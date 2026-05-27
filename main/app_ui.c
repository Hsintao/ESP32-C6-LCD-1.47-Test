#include "app_ui.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

static lv_obj_t *title_label;
static lv_obj_t *line_labels[3];
static SemaphoreHandle_t ui_mutex;
static char pending_title[48] = "Starting";
static char pending_lines[3][64] = {"", "", ""};

static void set_label_text(lv_obj_t *label, const char *text)
{
    lv_label_set_text(label, text ? text : "");
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!ui_mutex || xSemaphoreTake(ui_mutex, 0) != pdTRUE) {
        return;
    }

    set_label_text(title_label, pending_title);
    for (int i = 0; i < 3; i++) {
        set_label_text(line_labels[i], pending_lines[i]);
    }

    xSemaphoreGive(ui_mutex);
}

void app_ui_init(void)
{
    ui_mutex = xSemaphoreCreateMutex();
    const lv_font_t *title_font = LV_FONT_DEFAULT;
    const lv_font_t *line_font = LV_FONT_DEFAULT;

#if LV_FONT_MONTSERRAT_18
    title_font = &lv_font_montserrat_18;
#endif
#if LV_FONT_MONTSERRAT_12
    line_font = &lv_font_montserrat_12;
#endif

    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x101820), 0);

    title_label = lv_label_create(lv_scr_act());
    lv_obj_set_width(title_label, 156);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, title_font, 0);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 18);

    for (int i = 0; i < 3; i++) {
        line_labels[i] = lv_label_create(lv_scr_act());
        lv_obj_set_width(line_labels[i], 156);
        lv_obj_set_style_text_color(line_labels[i], lv_color_hex(0xd8dee9), 0);
        lv_obj_set_style_text_font(line_labels[i], line_font, 0);
        lv_label_set_long_mode(line_labels[i], LV_LABEL_LONG_WRAP);
        lv_obj_align(line_labels[i], LV_ALIGN_TOP_LEFT, 8, 72 + i * 54);
    }

    lv_timer_create(ui_timer_cb, 100, NULL);
    app_ui_show_status("Starting", "Preparing network", "", "");
}

void app_ui_show_status(const char *title, const char *line1, const char *line2, const char *line3)
{
    if (!ui_mutex || xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    strlcpy(pending_title, title ? title : "", sizeof(pending_title));
    strlcpy(pending_lines[0], line1 ? line1 : "", sizeof(pending_lines[0]));
    strlcpy(pending_lines[1], line2 ? line2 : "", sizeof(pending_lines[1]));
    strlcpy(pending_lines[2], line3 ? line3 : "", sizeof(pending_lines[2]));

    xSemaphoreGive(ui_mutex);
}
