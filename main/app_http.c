#include "app_http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_led_state.h"
#include "app_storage.h"
#include "app_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "app_http";
static httpd_handle_t server;

static esp_err_t read_body(httpd_req_t *req, char *buf, size_t buf_len)
{
    size_t remaining = req->content_len;
    size_t offset = 0;
    if (remaining >= buf_len) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body too large");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + offset, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        offset += ret;
        remaining -= ret;
    }
    buf[offset] = '\0';
    return ESP_OK;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *dst, size_t dst_len, const char *src)
{
    size_t out = 0;
    for (size_t i = 0; src[i] && out + 1 < dst_len; i++) {
        if (src[i] == '+') {
            dst[out++] = ' ';
        } else if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            dst[out++] = (char)((hex_value(src[i + 1]) << 4) | hex_value(src[i + 2]));
            i += 2;
        } else {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
}

static bool form_value(const char *body, const char *key, char *out, size_t out_len)
{
    size_t key_len = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *value = p + key_len + 1;
            const char *end = strchr(value, '&');
            char encoded[APP_WIFI_PASS_MAX_LEN * 3 + 1] = {0};
            size_t len = end ? (size_t)(end - value) : strlen(value);
            if (len >= sizeof(encoded)) {
                len = sizeof(encoded) - 1;
            }
            memcpy(encoded, value, len);
            encoded[len] = '\0';
            url_decode(out, out_len, encoded);
            return true;
        }
        p = strchr(p, '&');
        if (p) {
            p++;
        }
    }
    return false;
}

static bool parse_uint8_text(const char *text, uint8_t *value)
{
    if (!text || text[0] == '\0') {
        return false;
    }

    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || parsed < 0 || parsed > 255) {
        return false;
    }

    *value = (uint8_t)parsed;
    return true;
}

static bool query_rgb(httpd_req_t *req, uint8_t *r, uint8_t *g, uint8_t *b)
{
    char query[96];
    char value[8];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }

    if (httpd_query_key_value(query, "r", value, sizeof(value)) != ESP_OK || !parse_uint8_text(value, r)) {
        return false;
    }
    if (httpd_query_key_value(query, "g", value, sizeof(value)) != ESP_OK || !parse_uint8_text(value, g)) {
        return false;
    }
    if (httpd_query_key_value(query, "b", value, sizeof(value)) != ESP_OK || !parse_uint8_text(value, b)) {
        return false;
    }
    return true;
}

static bool json_rgb_value(const char *body, const char *key, uint8_t *value)
{
    char needle[8];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(body, needle);
    if (!p) {
        return false;
    }
    p = strchr(p, ':');
    if (!p) {
        return false;
    }
    p++;
    while (*p == ' ' || *p == '\t' || *p == '"') {
        p++;
    }
    return parse_uint8_text(p, value);
}

static bool body_rgb(const char *body, uint8_t *r, uint8_t *g, uint8_t *b)
{
    char value[8];
    if (form_value(body, "r", value, sizeof(value)) && parse_uint8_text(value, r) &&
        form_value(body, "g", value, sizeof(value)) && parse_uint8_text(value, g) &&
        form_value(body, "b", value, sizeof(value)) && parse_uint8_text(value, b)) {
        return true;
    }

    return json_rgb_value(body, "r", r) &&
           json_rgb_value(body, "g", g) &&
           json_rgb_value(body, "b", b);
}

static esp_err_t send_rgb_json(httpd_req_t *req)
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    app_led_state_get_rgb(&r, &g, &b);

    char json[96];
    snprintf(json, sizeof(json),
             "{\"state\":\"%s\",\"r\":%u,\"g\":%u,\"b\":%u}",
             app_led_state_get(), r, g, b);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static const char *config_page =
    "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32-C6 WiFi Setup</title><style>body{font-family:Arial;margin:24px;max-width:420px}input,button{font-size:18px;width:100%;padding:10px;margin:8px 0;box-sizing:border-box}button{background:#1f7aec;color:white;border:0}</style></head>"
    "<body><h2>ESP32-C6 WiFi Setup</h2><form method='post' action='/configure'>"
    "<input name='ssid' placeholder='Home WiFi SSID' maxlength='32' required>"
    "<input name='password' placeholder='Home WiFi Password' maxlength='64' type='password'>"
    "<button type='submit'>Connect</button></form></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (!app_wifi_is_connected()) {
        return httpd_resp_sendstr(req, config_page);
    }

    uint8_t r;
    uint8_t g;
    uint8_t b;
    app_led_state_get_rgb(&r, &g, &b);

    char page[2200];
    snprintf(page, sizeof(page),
             "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>ESP32-C6 Light</title><style>body{font-family:Arial;margin:24px;max-width:460px}button,input{font-size:18px;padding:12px;margin:6px 0;box-sizing:border-box}button{width:100%%;border:0;color:white}.row{display:flex;gap:8px}.row input{width:33%%}.red{background:#d92d20}.green{background:#099250}.off{background:#475467}.set{background:#1f7aec}code{font-size:16px}</style></head>"
             "<body><h2>ESP32-C6 Light</h2><p>SSID: <code>%s</code></p><p>IP: <code>%s</code></p><p>State: <code id='state'>%s</code> <code id='rgb'>(%u,%u,%u)</code></p>"
             "<button class='red' onclick=\"setLight('red')\">Red</button><button class='green' onclick=\"setLight('green')\">Green</button><button class='off' onclick=\"setLight('off')\">Off</button>"
             "<div class='row'><input id='r' type='number' min='0' max='255' value='%u'><input id='g' type='number' min='0' max='255' value='%u'><input id='b' type='number' min='0' max='255' value='%u'></div><button class='set' onclick='setRgb()'>Set RGB</button>"
             "<script>function show(j){document.getElementById('state').textContent=j.state||'error';document.getElementById('rgb').textContent='('+j.r+','+j.g+','+j.b+')';}"
             "async function setLight(s){let r=await fetch('/api/light',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({state:s})});show(await r.json());}"
             "async function setRgb(){let body={r:+document.getElementById('r').value,g:+document.getElementById('g').value,b:+document.getElementById('b').value};let r=await fetch('/api/rgb',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});show(await r.json());}</script>"
             "</body></html>",
             app_wifi_get_ssid(), app_wifi_get_ip(), app_led_state_get(), r, g, b, r, g, b);
    return httpd_resp_sendstr(req, page);
}

static esp_err_t configure_post_handler(httpd_req_t *req)
{
    char body[256];
    char ssid[APP_WIFI_SSID_MAX_LEN + 1] = {0};
    char pass[APP_WIFI_PASS_MAX_LEN + 1] = {0};

    if (read_body(req, body, sizeof(body)) != ESP_OK ||
        !form_value(body, "ssid", ssid, sizeof(ssid)) ||
        !form_value(body, "password", pass, sizeof(pass)) ||
        ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid form");
        return ESP_FAIL;
    }

    esp_err_t ret = app_wifi_set_credentials_and_connect(ssid, pass);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "wifi config failed");
        return ret;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, "<html><body><h2>Saved</h2><p>Device is connecting to WiFi. Check the screen for the IP address.</p></body></html>");
}

static esp_err_t light_get_handler(httpd_req_t *req)
{
    return send_rgb_json(req);
}

static bool parse_light_state(const char *body, char *state, size_t state_len)
{
    if (strstr(body, "\"state\":\"red\"") || strstr(body, "state=red")) {
        strlcpy(state, "red", state_len);
        return true;
    }
    if (strstr(body, "\"state\":\"green\"") || strstr(body, "state=green")) {
        strlcpy(state, "green", state_len);
        return true;
    }
    if (strstr(body, "\"state\":\"off\"") || strstr(body, "state=off")) {
        strlcpy(state, "off", state_len);
        return true;
    }
    return false;
}

static esp_err_t light_post_handler(httpd_req_t *req)
{
    char body[128];
    char state[APP_LED_STATE_MAX_LEN] = {0};
    if (read_body(req, body, sizeof(body)) != ESP_OK || !parse_light_state(body, state, sizeof(state))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid state");
        return ESP_FAIL;
    }

    esp_err_t ret = app_led_state_set(state, true);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
        return ret;
    }

    return light_get_handler(req);
}

static esp_err_t rgb_get_handler(httpd_req_t *req)
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    if (query_rgb(req, &r, &g, &b)) {
        esp_err_t ret = app_led_state_set_rgb(r, g, b, true);
        if (ret != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
            return ret;
        }
    }

    return send_rgb_json(req);
}

static esp_err_t rgb_post_handler(httpd_req_t *req)
{
    char body[128];
    uint8_t r;
    uint8_t g;
    uint8_t b;
    if (read_body(req, body, sizeof(body)) != ESP_OK || !body_rgb(body, &r, &g, &b)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid rgb");
        return ESP_FAIL;
    }

    esp_err_t ret = app_led_state_set_rgb(r, g, b, true);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
        return ret;
    }

    return send_rgb_json(req);
}

esp_err_t app_http_start(void)
{
    if (server) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "http server start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
    httpd_uri_t configure = {.uri = "/configure", .method = HTTP_POST, .handler = configure_post_handler};
    httpd_uri_t light_get = {.uri = "/api/light", .method = HTTP_GET, .handler = light_get_handler};
    httpd_uri_t light_post = {.uri = "/api/light", .method = HTTP_POST, .handler = light_post_handler};
    httpd_uri_t rgb_get = {.uri = "/api/rgb", .method = HTTP_GET, .handler = rgb_get_handler};
    httpd_uri_t rgb_post = {.uri = "/api/rgb", .method = HTTP_POST, .handler = rgb_post_handler};

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &configure));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &light_get));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &light_post));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &rgb_get));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &rgb_post));
    ESP_LOGI(TAG, "http server started");
    return ESP_OK;
}
