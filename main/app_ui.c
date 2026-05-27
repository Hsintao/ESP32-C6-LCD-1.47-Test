#include "app_ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

static SemaphoreHandle_t ui_mutex;
static app_ui_codex_status_t pending = {
    .account = "2nw*@*.com",
    .work_status = "Active",
    .plan = "Plus",
    .ip = "0.0.0.0",
    .session_reset = "4h 19m to reset",
    .weekly_reset = "Wed 21:17 reset",
    .extra_usage = "Off",
    .session_percent = 16,
    .weekly_percent = 3,
};

static lv_obj_t *account_label;
static lv_obj_t *status_label;
static lv_obj_t *plan_label;
static lv_obj_t *ip_label;
static lv_obj_t *session_reset_label;
static lv_obj_t *session_percent_label;
static lv_obj_t *weekly_reset_label;
static lv_obj_t *weekly_percent_label;
static lv_obj_t *session_bar;
static lv_obj_t *weekly_bar;

static int clamp_percent(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static void label_set(lv_obj_t *label, const char *text)
{
    lv_label_set_text(label, text ? text : "");
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

static lv_obj_t *make_pill(lv_obj_t *parent, const lv_font_t *font, lv_color_t bg, int width)
{
    lv_obj_t *pill = lv_label_create(parent);
    lv_obj_set_size(pill, width, 22);
    lv_obj_set_style_radius(pill, 11, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(pill, bg, 0);
    lv_obj_set_style_text_color(pill, lv_color_white(), 0);
    lv_obj_set_style_text_font(pill, font, 0);
    lv_obj_set_style_text_align(pill, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(pill, 3, 0);
    lv_label_set_long_mode(pill, LV_LABEL_LONG_DOT);
    return pill;
}

static lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int width)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, width, 9);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2b6d9f), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2fffe2), LV_PART_INDICATOR);
    return bar;
}

static void draw_codex_bitmap(lv_obj_t *parent)
{
    static lv_color_t icon_buf[30 * 30];
    static const char *icon_rows[30] = {
        "..........#######.............",
        ".........#########............",
        "........###....########.......",
        ".......###......#########.....",
        "......###......#####.#####....",
        ".....###.....####.......###...",
        "...#####...#####.........###..",
        "..######..####....###.....##..",
        ".###..##..##.....#####....##..",
        ".##...##..##...###..####..##..",
        "###...##..##.####....#######..",
        "##....##..#########....#####..",
        "##....##..###....###.....###..",
        "##....##..##......####....###.",
        "###...##..##......######...##.",
        ".##...######......##..##...###",
        ".###....####......##..##....##",
        "..###.....###....###..##....##",
        "..#####....#########..##....##",
        "..#######....####.##..##...###",
        "..##..####..###...##..##...##.",
        "..##....#####.....##..##..###.",
        "..##.....###....####..######..",
        "..###.........#####...#####...",
        "...###.......####.....###.....",
        "....#####.#####......###......",
        ".....#########......###.......",
        ".......########...####........",
        "............#########.........",
        ".............#######..........",
    };

    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, icon_buf, 30, 30, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 30; x++) {
            lv_color_t color = icon_rows[y][x] == '#' ? lv_color_black() : lv_color_white();
            lv_canvas_set_px_color(canvas, x, y, color);
        }
    }
}

static void pull_ip_from_line(const char *line)
{
    if (!line) {
        return;
    }

    const char *ip = strstr(line, "IP:");
    if (ip) {
        ip += 3;
        while (*ip == ' ') {
            ip++;
        }
        strlcpy(pending.ip, ip, sizeof(pending.ip));
    }
}

static void update_screen_locked(void)
{
    char buf[40];

    label_set(account_label, pending.account);
    label_set(status_label, pending.work_status);
    label_set(plan_label, pending.plan);

    snprintf(buf, sizeof(buf), "IP %s", pending.ip);
    label_set(ip_label, buf);

    label_set(session_reset_label, pending.session_reset);
    label_set(weekly_reset_label, pending.weekly_reset);
    int session = clamp_percent(pending.session_percent);
    int weekly = clamp_percent(pending.weekly_percent);
    lv_bar_set_value(session_bar, session, LV_ANIM_OFF);
    lv_bar_set_value(weekly_bar, weekly, LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%d%%", session);
    label_set(session_percent_label, buf);
    snprintf(buf, sizeof(buf), "%d%%", weekly);
    label_set(weekly_percent_label, buf);
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!ui_mutex || xSemaphoreTake(ui_mutex, 0) != pdTRUE) {
        return;
    }

    update_screen_locked();
    xSemaphoreGive(ui_mutex);
}

void app_ui_init(void)
{
    ui_mutex = xSemaphoreCreateMutex();
    const lv_font_t *font_large = LV_FONT_DEFAULT;
    const lv_font_t *font_normal = LV_FONT_DEFAULT;

#if LV_FONT_MONTSERRAT_18
    font_large = &lv_font_montserrat_18;
#endif
#if LV_FONT_MONTSERRAT_12
    font_normal = &lv_font_montserrat_12;
#endif

    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x021633), 0);

    lv_obj_t *panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(panel, 316, 146);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x1679ff), 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x062a52), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *avatar = lv_obj_create(panel);
    lv_obj_set_size(avatar, 30, 30);
    lv_obj_align(avatar, LV_ALIGN_TOP_LEFT, 9, 9);
    lv_obj_set_style_radius(avatar, 6, 0);
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0xf8fbff), 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(avatar, 0, 0);
    lv_obj_set_style_pad_all(avatar, 0, 0);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);

    draw_codex_bitmap(avatar);

    account_label = make_label(panel, font_large, lv_color_white(), 120);
    lv_obj_align(account_label, LV_ALIGN_TOP_LEFT, 46, 7);

    status_label = make_pill(panel, font_normal, lv_color_hex(0x00d47e), 64);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 170, 10);

    plan_label = make_pill(panel, font_normal, lv_color_hex(0x20dccc), 52);
    lv_obj_align(plan_label, LV_ALIGN_TOP_RIGHT, -9, 10);

    ip_label = make_label(panel, font_normal, lv_color_hex(0xbde7ff), 170);
    lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, 46, 28);

    lv_obj_t *session_label = make_label(panel, font_large, lv_color_white(), 100);
    lv_label_set_text(session_label, "Session");
    lv_obj_align(session_label, LV_ALIGN_TOP_LEFT, 10, 52);

    session_reset_label = make_label(panel, font_normal, lv_color_white(), 130);
    lv_obj_set_style_text_align(session_reset_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(session_reset_label, LV_ALIGN_TOP_RIGHT, -10, 55);

    session_bar = make_bar(panel, 10, 78, 248);
    session_percent_label = make_label(panel, font_normal, lv_color_hex(0x40ffd5), 44);
    lv_obj_set_style_text_align(session_percent_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(session_percent_label, LV_ALIGN_TOP_RIGHT, -10, 74);

    lv_obj_t *weekly_label = make_label(panel, font_large, lv_color_white(), 100);
    lv_label_set_text(weekly_label, "Weekly");
    lv_obj_align(weekly_label, LV_ALIGN_TOP_LEFT, 10, 101);

    weekly_reset_label = make_label(panel, font_normal, lv_color_white(), 140);
    lv_obj_set_style_text_align(weekly_reset_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(weekly_reset_label, LV_ALIGN_TOP_RIGHT, -10, 104);

    weekly_bar = make_bar(panel, 10, 127, 248);
    weekly_percent_label = make_label(panel, font_normal, lv_color_hex(0x40ffd5), 44);
    lv_obj_set_style_text_align(weekly_percent_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(weekly_percent_label, LV_ALIGN_TOP_RIGHT, -10, 123);

    lv_timer_create(ui_timer_cb, 100, NULL);
    update_screen_locked();
}

void app_ui_show_status(const char *title, const char *line1, const char *line2, const char *line3)
{
    if (!ui_mutex || xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    pull_ip_from_line(title);
    pull_ip_from_line(line1);
    pull_ip_from_line(line2);
    pull_ip_from_line(line3);

    xSemaphoreGive(ui_mutex);
}

void app_ui_update_codex_status(const app_ui_codex_status_t *status)
{
    if (!status || !ui_mutex || xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    pending = *status;
    pending.session_percent = clamp_percent(pending.session_percent);
    pending.weekly_percent = clamp_percent(pending.weekly_percent);

    xSemaphoreGive(ui_mutex);
}
