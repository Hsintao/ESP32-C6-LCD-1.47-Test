#pragma once

typedef struct {
    char account[48];
    char work_status[16];
    char plan[12];
    char ip[16];
    char session_reset[32];
    char weekly_reset[32];
    char extra_usage[16];
    int session_percent;
    int weekly_percent;
} app_ui_codex_status_t;

void app_ui_init(void);
void app_ui_show_status(const char *title, const char *line1, const char *line2, const char *line3);
void app_ui_update_codex_status(const app_ui_codex_status_t *status);
