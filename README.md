# ESP32-C6-LCD-1.47 WiFi RGB 控灯

基于 Waveshare ESP32-C6-LCD-1.47 的 ESP-IDF 示例工程，增加了手机配网、LCD 显示 IP、HTTP 控制板载 RGB 灯、灯光状态掉电保存等功能。

## 功能

- 首次启动或 WiFi 连接失败时，开发板会开启热点 `ESP32-C6-SETUP`。
- 手机连接热点后访问 `http://192.168.4.1/`，提交家庭 WiFi 名称和密码。
- 开发板连接家庭 WiFi 后，屏幕显示 SSID、IP 地址和控制提示。
- PC 可通过浏览器或命令行控制 RGB 灯。
- RGB 灯状态保存到 NVS，重启后自动恢复。

## 配网

1. 烧录并启动开发板。
2. 手机连接热点：
   - SSID: `ESP32-C6-SETUP`
   - 密码: `12345678`
3. 手机浏览器打开 `http://192.168.4.1/`。
4. 输入家庭 WiFi 的 SSID 和密码并提交。
5. 开发板联网成功后，屏幕会显示获取到的 IP 地址。

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

## 构建与烧录

进入工程目录后执行：

```bash
idf.py build
idf.py -p COM3 flash monitor
```

如果串口不是 `COM3`，请替换成实际串口。

## 主要代码

- `main/app_wifi.c`: SoftAP 配网、STA 联网和 IP 状态管理。
- `main/app_http.c`: 配网页、Web 控灯页面和 HTTP API。
- `main/app_led_state.c`: RGB 状态应用与持久化封装。
- `main/app_storage.c`: NVS 读写 WiFi 凭据和 RGB 状态。
- `main/app_ui.c`: LCD 状态显示页面。
- `main/LCD_Driver/ST7789.c`: LCD SPI bus 初始化和 ST7789 面板初始化。

## 默认参数

- 配网热点 SSID: `ESP32-C6-SETUP`
- 配网热点密码: `12345678`
- RGB GPIO: `GPIO8`
- LCD: ST7789, 172 x 320
