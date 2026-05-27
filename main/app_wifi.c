#include "app_wifi.h"

#include <string.h>
#include "app_storage.h"
#include "app_ui.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

#define WIFI_MAX_RETRY 10

static const char *TAG = "app_wifi";
static bool wifi_started;
static bool portal_active;
static bool sta_connected;
static bool connecting_sta;
static int retry_count;
static char current_ssid[APP_WIFI_SSID_MAX_LEN + 1];
static char current_pass[APP_WIFI_PASS_MAX_LEN + 1];
static char current_ip[16] = "0.0.0.0";

static void start_config_portal(void);

static void show_connecting_status(void)
{
    char line1[64];
    snprintf(line1, sizeof(line1), "SSID: %s", current_ssid);
    app_ui_show_status("Connecting WiFi", line1, "Please wait...", "");
}

static void connect_sta(void)
{
    wifi_config_t sta_config = {0};
    strlcpy((char *)sta_config.sta.ssid, current_ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, current_pass, sizeof(sta_config.sta.password));
    sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    retry_count = 0;
    sta_connected = false;
    connecting_sta = true;
    current_ip[0] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_mode(portal_active ? WIFI_MODE_APSTA : WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    show_connecting_status();
}

static void configure_ap_compatibility(void)
{
    wifi_country_t country = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_MANUAL,
    };

    esp_err_t ret = esp_wifi_set_country(&country);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set country failed: %s", esp_err_to_name(ret));
    }

    ret = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set AP protocol failed: %s", esp_err_to_name(ret));
    }

    ret = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set AP bandwidth failed: %s", esp_err_to_name(ret));
    }
}

static void start_config_portal(void)
{
    wifi_config_t ap_config = {
        .ap = {
            .ssid = APP_CONFIG_AP_SSID,
            .ssid_len = strlen(APP_CONFIG_AP_SSID),
            .password = APP_CONFIG_AP_PASS,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    sta_connected = false;
    connecting_sta = false;
    portal_active = true;
    current_ip[0] = '\0';
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    configure_ap_compatibility();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    app_ui_show_status("WiFi Setup",
                       "AP: " APP_CONFIG_AP_SSID,
                       "Password: " APP_CONFIG_AP_PASS,
                       "Open: 192.168.4.1");
    ESP_LOGI(TAG, "Config portal started: %s", APP_CONFIG_AP_SSID);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!connecting_sta) {
            return;
        }
        sta_connected = false;
        if (retry_count < WIFI_MAX_RETRY) {
            retry_count++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/%d", retry_count, WIFI_MAX_RETRY);
            esp_wifi_connect();
            show_connecting_status();
        } else {
            ESP_LOGE(TAG, "WiFi connect failed, returning to config portal");
            app_storage_clear_wifi();
            start_config_portal();
        }
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base != IP_EVENT || event_id != IP_EVENT_STA_GOT_IP) {
        return;
    }

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&event->ip_info.ip));
    sta_connected = true;
    connecting_sta = false;
    portal_active = false;
    retry_count = 0;

    char line1[64];
    char line2[64];
    snprintf(line1, sizeof(line1), "SSID: %s", current_ssid);
    snprintf(line2, sizeof(line2), "IP: %s", current_ip);
    app_ui_show_status("WiFi Connected", line1, line2, "PC open this IP");
    ESP_LOGI(TAG, "Connected, IP: %s", current_ip);
    esp_wifi_set_mode(WIFI_MODE_STA);
}

esp_err_t app_wifi_start(void)
{
    if (wifi_started) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_started = true;

    if (app_storage_get_wifi(current_ssid, sizeof(current_ssid), current_pass, sizeof(current_pass))) {
        start_config_portal();
        connect_sta();
    } else {
        start_config_portal();
    }

    return ESP_OK;
}

esp_err_t app_wifi_set_credentials_and_connect(const char *ssid, const char *pass)
{
    if (!ssid || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(ssid) > APP_WIFI_SSID_MAX_LEN || (pass && strlen(pass) > APP_WIFI_PASS_MAX_LEN)) {
        return ESP_ERR_INVALID_SIZE;
    }

    strlcpy(current_ssid, ssid, sizeof(current_ssid));
    strlcpy(current_pass, pass ? pass : "", sizeof(current_pass));
    esp_err_t ret = app_storage_set_wifi(current_ssid, current_pass);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "save wifi failed: %s", esp_err_to_name(ret));
        return ret;
    }
    connect_sta();
    return ESP_OK;
}

bool app_wifi_is_connected(void)
{
    return sta_connected;
}

const char *app_wifi_get_ip(void)
{
    return current_ip[0] ? current_ip : "0.0.0.0";
}

const char *app_wifi_get_ssid(void)
{
    return current_ssid;
}
