idf_component_register(
                         SRCS "main.c"
                              "app_http.c"
                              "app_led_state.c"
                              "app_storage.c"
                              "app_ui.c"
                              "app_wifi.c"
                              "LCD_Driver/Vernon_ST7789T/Vernon_ST7789T.c" 
                              "LCD_Driver/ST7789.c"
                              "LVGL_Driver/LVGL_Driver.c"
                              "RGB/RGB.c"

                         INCLUDE_DIRS 
                              "./LCD_Driver/Vernon_ST7789T" 
                              "./LCD_Driver" 
                              "./LVGL_Driver" 
                              "./RGB" 
                              "."
                         REQUIRES esp_wifi esp_http_server esp_netif esp_event nvs_flash
                                  esp_driver_gpio esp_driver_spi esp_driver_ledc esp_lcd esp_timer
                       )
