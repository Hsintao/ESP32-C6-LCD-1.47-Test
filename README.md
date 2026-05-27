# ESP32-C6-LCD-1.47 WiFi RGB 控灯

基于 Waveshare ESP32-C6-LCD-1.47 的 ESP-IDF 示例工程，增加了手机配网、横屏 Codex 状态屏、HTTP 控制板载 RGB 灯、灯光状态掉电保存等功能。

## 功能

- 首次启动或 WiFi 连接失败时，开发板会开启热点 `ESP32-C6-SETUP`。
- 手机连接热点后访问 `http://192.168.4.1/`，提交家庭 WiFi 名称和密码。
- 开发板连接家庭 WiFi 后，屏幕顶部显示 IP 地址。
- PC 可通过浏览器或命令行控制 RGB 灯。
- RGB 灯状态保存到 NVS，重启后自动恢复。
- LCD 使用 320 x 172 横屏显示 Codex 状态仪表盘。
- PC 可通过 `/api/codex` 推送 Codex 账户、工作状态和限额进度。
- 仓库提供 `scripts/codex_status_push.py`，可在 PC 端自动读取本机 Codex 状态并实时推送到板子。
- Codex 状态屏左上角使用黑白 Codex 图标，并关闭 LVGL 右下角帧率/CPU 性能监视显示。

## 配网

1. 烧录并启动开发板。
2. 手机连接热点：
   - SSID: `ESP32-C6-SETUP`
   - 密码: `12345678`
3. 手机浏览器打开 `http://192.168.4.1/`。
4. 输入家庭 WiFi 的 SSID 和密码并提交。
5. 开发板联网成功后，屏幕会在 Codex 状态屏顶部显示获取到的 IP 地址。

如果手机提示“无互联网连接”，这是正常现象，选择继续连接即可。

## PC 端控制 RGB

把下面命令里的 `板子IP` 替换为屏幕显示的 IP 地址。

查询当前 RGB 状态：

```bash
curl "http://板子IP/api/rgb"
```

通过 URL 参数设置 RGB：

```bash
curl "http://板子IP/api/rgb?r=255&g=0&b=128"
```

通过 JSON 设置 RGB：

```bash
curl -X POST "http://板子IP/api/rgb" -H "Content-Type: application/json" -d "{\"r\":255,\"g\":0,\"b\":128}"
```

返回示例：

```json
{"state":"#FF0080","r":255,"g":0,"b":128}
```

## 兼容的灯光 API

原来的简单状态接口仍然可用：

```bash
curl -X POST "http://板子IP/api/light" -H "Content-Type: application/json" -d "{\"state\":\"red\"}"
curl -X POST "http://板子IP/api/light" -H "Content-Type: application/json" -d "{\"state\":\"green\"}"
curl -X POST "http://板子IP/api/light" -H "Content-Type: application/json" -d "{\"state\":\"off\"}"
```

## Web 页面

PC 浏览器打开：

```text
http://板子IP/
```

页面支持红灯、绿灯、关闭和自定义 RGB 输入。

## Codex 横屏状态屏

LCD 会以横屏仪表盘方式显示 Codex 账户、工作状态、套餐、IP、5 小时限额和一周限额进度。开发板无法直接读取 Codex 登录账户或限额信息，需要 PC 主动推送状态。IP 由开发板自动填充，其他字段由 PC 推送。

界面细节：

- 左上角显示黑白 Codex 图标。
- 顶部显示脱敏账户、工作状态、套餐和当前 IP。
- 中部显示 Session 与 Weekly 两条限额进度条。
- 已关闭 LVGL 内置性能监视，因此右下角不会显示帧率和 CPU 占用率。

显示字段：

- `account`: 登录账户，建议传脱敏邮箱，例如 `2nw*@*.com`。
- `status`: 工作状态，例如 `Active`、`Idle`。
- `plan`: 套餐标识，例如 `Plus`。
- `session`: 5 小时限额进度，范围 `0` 到 `100`。
- `session_reset`: 5 小时限额重置时间文案。
- `weekly`: 一周限额进度，范围 `0` 到 `100`。
- `weekly_reset`: 一周限额重置时间文案。

推送示例：

```bash
curl -X POST "http://板子IP/api/codex" -H "Content-Type: application/json" -d "{\"account\":\"2nw*@*.com\",\"status\":\"Active\",\"plan\":\"Plus\",\"session\":16,\"session_reset\":\"4h 19m to reset\",\"weekly\":3,\"weekly_reset\":\"Wed 21:17 reset\"}"
```

也可以用 URL 参数快速更新：

```bash
curl "http://板子IP/api/codex?account=2nw*@*.com&status=Active&plan=Plus&session=16&session_reset=4h%2019m%20to%20reset&weekly=3&weekly_reset=Wed%2021:17%20reset"
```

查询当前显示状态：

```bash
curl "http://板子IP/api/codex"
```

如果只想更新部分字段，也可以只传变化的字段，例如：

```bash
curl "http://板子IP/api/codex?session=28&session_reset=3h%2012m%20to%20reset"
```

注意：`session_reset`、`weekly_reset` 里有空格时，URL 参数需要做 URL 编码；使用 POST JSON 时不需要手动编码。

## PC 端自动推送 Codex 状态

仓库自带 `scripts/codex_status_push.py`，用于在 Windows / macOS / Linux 上读取本机 Codex Desktop 的登录信息与最新会话限额状态，并自动推送到板子的 `/api/codex`。

脚本特性：

- 零第三方依赖，只使用 Python 标准库。
- 自动读取 `~/.codex/auth.json`，提取邮箱并做脱敏显示。
- 自动读取最新的 `~/.codex/sessions/.../rollout-*.jsonl`，提取 5 小时与 7 天用量百分比、重置时间和套餐信息。
- 当最近 90 秒内有会话活动时显示 `Active`，否则显示 `Idle`。

先打印一次当前 payload，确认读取结果：

```bash
python scripts/codex_status_push.py --board http://板子IP --once --print-only
```

推送一次到板子：

```bash
python scripts/codex_status_push.py --board http://板子IP --once
```

持续实时推送，每 10 秒刷新一次：

```bash
python scripts/codex_status_push.py --board http://板子IP --interval 10
```

常用参数：

- `--board`: 板子的基础地址，例如 `http://192.168.1.88`
- `--interval`: 连续运行时的推送间隔，默认 `10`
- `--once`: 只执行一次
- `--print-only`: 仅打印 payload，不发送 HTTP 请求
- `--codex-home`: 自定义 Codex 数据目录，默认是 `~/.codex`

返回的 payload 形如：

```json
{"account":"2nw*@p*.com","status":"Active","plan":"Plus","session":21,"session_reset":"4h 12m to reset","weekly":40,"weekly_reset":"Sun 13:53 reset"}
```

如果你想在 Windows 开机后自动常驻运行，可以把下面命令放进计划任务或启动脚本：

```powershell
python C:\path\to\scripts\codex_status_push.py --board http://板子IP --interval 10
```

## 构建与烧录

进入工程目录后执行：

```bash
idf.py build
idf.py -p COM3 flash monitor
```

如果串口不是 `COM3`，请替换成实际串口。

## 主要代码

- `main/app_wifi.c`: SoftAP 配网、STA 联网和 IP 状态管理。
- `main/app_http.c`: 配网页、Web 控灯页面、RGB API 和 Codex 状态推送 API。
- `main/app_led_state.c`: RGB 状态应用与持久化封装。
- `main/app_storage.c`: NVS 读写 WiFi 凭据和 RGB 状态。
- `main/app_ui.c`: LCD 横屏 Codex 状态仪表盘。
- `scripts/codex_status_push.py`: PC 端读取本机 Codex 状态并推送到板子的脚本。
- `main/LVGL_Driver/LVGL_Driver.c`: LVGL 横屏分辨率、ST7789 硬件横屏和 flush 偏移。
- `main/LCD_Driver/ST7789.c`: LCD SPI bus 初始化和 ST7789 面板初始化。

## 默认参数

- 配网热点 SSID: `ESP32-C6-SETUP`
- 配网热点密码: `12345678`
- RGB GPIO: `GPIO8`
- LCD: ST7789, 172 x 320 物理屏，应用界面使用 320 x 172 横屏
