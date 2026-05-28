#!/usr/bin/env python3
import argparse
import base64
import datetime as dt
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any, Dict, Optional


ACTIVE_SECONDS = 90
TRANSIENT_HTTP_CODES = {429, 502, 503, 504}
PUSH_COMPARE_FIELDS = ("account", "status", "plan", "session", "weekly")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Push local Codex usage status to the ESP32 board."
    )
    parser.add_argument(
        "--board",
        required=True,
        help="Board base URL, for example http://192.168.1.88",
    )
    parser.add_argument(
        "--interval",
        type=int,
        default=10,
        help="Push interval in seconds when running continuously. Default: 10",
    )
    parser.add_argument(
        "--codex-home",
        default=None,
        help="Override the Codex data directory. By default the script auto-detects CODEX_HOME or ~/.codex.",
    )
    parser.add_argument(
        "--once",
        action="store_true",
        help="Push once and exit.",
    )
    parser.add_argument(
        "--print-only",
        action="store_true",
        help="Print the payload without sending it to the board.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="HTTP timeout in seconds. Default: 5",
    )
    parser.add_argument(
        "--max-retries",
        type=int,
        default=3,
        help="Retry count for transient push failures. Default: 3",
    )
    parser.add_argument(
        "--retry-delay",
        type=float,
        default=1.0,
        help="Initial retry delay in seconds. Default: 1",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print every payload and push response while running continuously.",
    )
    return parser.parse_args()


def decode_jwt_payload(token: str) -> Dict[str, Any]:
    parts = token.split(".")
    if len(parts) != 3:
        return {}

    payload = parts[1]
    payload += "=" * ((4 - len(payload) % 4) % 4)
    try:
        decoded = base64.urlsafe_b64decode(payload.encode("ascii"))
        return json.loads(decoded.decode("utf-8"))
    except (ValueError, json.JSONDecodeError):
        return {}


def mask_email(email: str) -> str:
    if "@" not in email:
        return email[:3] + "*" if len(email) > 3 else email

    local, domain = email.split("@", 1)
    masked_local = local[:3] + "*" if len(local) > 3 else (local[:1] + "*" if local else "*")

    if "." in domain:
        first, rest = domain.split(".", 1)
        masked_domain = (first[:1] + "*") if first else "*"
        masked_tld = "." + ("*" if rest else "")
        return masked_local + "@" + masked_domain + masked_tld

    masked_domain = domain[:1] + "*" if domain else "*"
    return masked_local + "@" + masked_domain


def read_json(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def read_cached_payload(path: Path) -> Optional[Dict[str, Any]]:
    if not path.is_file():
        return None

    try:
        data = read_json(path)
    except (OSError, json.JSONDecodeError):
        return None

    return data if isinstance(data, dict) else None


def write_cached_payload(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=2)


def is_codex_home(path: Path) -> bool:
    return (path / "auth.json").is_file() and (path / "sessions").is_dir()


def resolve_codex_home(override: Optional[str]) -> Path:
    candidates = []

    if override:
        candidates.append(Path(os.path.expanduser(override)))

    env_home = os.environ.get("CODEX_HOME")
    if env_home:
        candidates.append(Path(os.path.expanduser(env_home)))

    candidates.append(Path.home() / ".codex")

    for path in candidates:
        expanded = path.resolve()
        if is_codex_home(expanded):
            return expanded

    checked = ", ".join(str(path) for path in candidates)
    raise FileNotFoundError(
        "Could not find Codex data directory. "
        f"Checked: {checked}. "
        "Run Codex once on this computer, set CODEX_HOME, or pass --codex-home."
    )


def load_auth_profile(codex_home: Path) -> Dict[str, str]:
    auth = read_json(codex_home / "auth.json")
    tokens = auth.get("tokens", {})

    id_claims = decode_jwt_payload(tokens.get("id_token", ""))
    access_claims = decode_jwt_payload(tokens.get("access_token", ""))

    auth_claims = id_claims.get("https://api.openai.com/auth", {})
    access_auth_claims = access_claims.get("https://api.openai.com/auth", {})

    email = id_claims.get("email") or access_claims.get("https://api.openai.com/profile", {}).get("email", "")
    plan = auth_claims.get("chatgpt_plan_type") or access_auth_claims.get("chatgpt_plan_type") or "unknown"

    return {
        "account": mask_email(email) if email else "unknown",
        "plan": plan.title(),
    }


def latest_session_file(codex_home: Path) -> Path:
    sessions_dir = codex_home / "sessions"
    candidates = list(sessions_dir.rglob("rollout-*.jsonl"))
    if not candidates:
        raise FileNotFoundError(f"No Codex session files found under {sessions_dir}")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def parse_timestamp(text: str) -> Optional[dt.datetime]:
    try:
        if text.endswith("Z"):
            text = text[:-1] + "+00:00"
        return dt.datetime.fromisoformat(text)
    except ValueError:
        return None


def format_duration_until(reset_at: dt.datetime, now: dt.datetime) -> str:
    remaining = reset_at - now
    if remaining.total_seconds() <= 0:
        return "Reset now"

    total_minutes = int(remaining.total_seconds() // 60)
    hours, minutes = divmod(total_minutes, 60)
    if hours > 0:
        return f"{hours}h {minutes}m to reset"
    return f"{minutes}m to reset"


def format_weekly_reset(reset_at: dt.datetime, now: dt.datetime) -> str:
    local_reset = reset_at.astimezone(now.tzinfo)
    return local_reset.strftime("%a %H:%M reset")


def load_session_status(codex_home: Path) -> Dict[str, Any]:
    session_path = latest_session_file(codex_home)
    latest_rate_limits: Optional[Dict[str, Any]] = None
    latest_timestamp: Optional[dt.datetime] = None

    with session_path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue

            event_ts = parse_timestamp(event.get("timestamp", ""))
            if event_ts:
                latest_timestamp = event_ts

            if event.get("type") != "event_msg":
                continue

            payload = event.get("payload", {})
            if payload.get("type") != "token_count":
                continue

            rate_limits = payload.get("rate_limits")
            if isinstance(rate_limits, dict):
                latest_rate_limits = rate_limits

    if not latest_rate_limits:
        raise RuntimeError(f"No token_count event found in {session_path}")

    now = dt.datetime.now(dt.timezone.utc).astimezone()
    last_seen_local = latest_timestamp.astimezone(now.tzinfo) if latest_timestamp else None
    active = bool(last_seen_local and (now - last_seen_local).total_seconds() <= ACTIVE_SECONDS)

    primary = latest_rate_limits.get("primary", {})
    secondary = latest_rate_limits.get("secondary", {})

    primary_reset = dt.datetime.fromtimestamp(primary.get("resets_at", 0), dt.timezone.utc).astimezone()
    secondary_reset = dt.datetime.fromtimestamp(secondary.get("resets_at", 0), dt.timezone.utc).astimezone()

    return {
        "status": "Active" if active else "Idle",
        "session": int(round(float(primary.get("used_percent", 0)))),
        "session_reset": format_duration_until(primary_reset, now),
        "weekly": int(round(float(secondary.get("used_percent", 0)))),
        "weekly_reset": format_weekly_reset(secondary_reset, now),
        "session_file": str(session_path),
        "last_seen": last_seen_local.isoformat() if last_seen_local else "",
    }


def build_payload(codex_home: Path) -> Dict[str, Any]:
    profile = load_auth_profile(codex_home)
    session = load_session_status(codex_home)
    return {
        "account": profile["account"],
        "status": session["status"],
        "plan": profile["plan"],
        "session": session["session"],
        "session_reset": session["session_reset"],
        "weekly": session["weekly"],
        "weekly_reset": session["weekly_reset"],
    }


def cache_file_path() -> Path:
    return Path(__file__).resolve().parent / "codex-status-push-cache.json"


def comparable_payload(payload: Optional[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    if payload is None:
        return None
    return {field: payload.get(field) for field in PUSH_COMPARE_FIELDS}


def post_status(board: str, payload: Dict[str, Any], timeout: float) -> str:
    url = board.rstrip("/") + "/api/codex"
    body = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read().decode("utf-8", errors="replace")


def push_status_with_retry(
    board: str,
    payload: Dict[str, Any],
    timeout: float,
    max_retries: int,
    retry_delay: float,
) -> str:
    attempts = max(max_retries, 0) + 1
    delay = max(retry_delay, 0.1)
    last_error: Optional[BaseException] = None

    for attempt in range(1, attempts + 1):
        try:
            return post_status(board, payload, timeout)
        except urllib.error.HTTPError as exc:
            if exc.code not in TRANSIENT_HTTP_CODES or attempt >= attempts:
                raise
            last_error = exc
        except (urllib.error.URLError, TimeoutError, OSError) as exc:
            if attempt >= attempts:
                raise
            last_error = exc

        print(
            f"[codex-status] push retry {attempt}/{attempts - 1}: {last_error}",
            file=sys.stderr,
        )
        time.sleep(delay)
        delay = min(delay * 2, 10.0)

    raise RuntimeError(f"push failed after retries: {last_error}")


def run_once(args: argparse.Namespace) -> int:
    codex_home = resolve_codex_home(args.codex_home)
    payload = build_payload(codex_home)
    last_payload = read_cached_payload(cache_file_path())
    if args.print_only or args.once or args.verbose:
        print(json.dumps(payload, ensure_ascii=False))

    if args.print_only:
        return 0

    if comparable_payload(last_payload) == comparable_payload(payload):
        if args.once or args.verbose:
            print("[codex-status] key fields unchanged, skip push")
        return 0

    response = push_status_with_retry(
        args.board,
        payload,
        args.timeout,
        args.max_retries,
        args.retry_delay,
    )
    write_cached_payload(cache_file_path(), payload)
    if args.once or args.verbose:
        print(response)
    else:
        print(
            "[codex-status] pushed "
            f"status={payload['status']} session={payload['session']} weekly={payload['weekly']}"
        )
    return 0


def main() -> int:
    args = parse_args()

    while True:
        try:
            result = run_once(args)
        except FileNotFoundError as exc:
            print(f"[codex-status] {exc}", file=sys.stderr)
            result = 1
        except RuntimeError as exc:
            print(f"[codex-status] {exc}", file=sys.stderr)
            result = 1
        except (urllib.error.URLError, TimeoutError, OSError) as exc:
            print(f"[codex-status] push failed: {exc}", file=sys.stderr)
            result = 1
        except Exception as exc:  # noqa: BLE001
            print(f"[codex-status] unexpected error: {exc}", file=sys.stderr)
            result = 1

        if args.once:
            return result

        time.sleep(max(args.interval, 1))


if __name__ == "__main__":
    raise SystemExit(main())
