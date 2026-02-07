#pragma once
#include <lvgl.h>

/* UI Events and Global State */
void ui_init();
void ui_update_status(); // Call this periodically or on event

/* Theme & Styles */
void ui_theme_init();
extern lv_style_t style_base_screen;
extern lv_style_t style_card;
extern lv_style_t style_text_title;
extern lv_style_t style_text_value;
extern lv_style_t style_btn_primary;

/* Screens */
extern lv_obj_t *ui_ScreenDashboard;
extern lv_obj_t *ui_ScreenSettings;
extern lv_obj_t *ui_ScreenCalibration;

void ui_screen_dashboard_init();
void ui_dashboard_update(const char *status, float progress, const char *name,
                         const char *time, int toolIdx);
void ui_screen_settings_init();

/* Navigation */
void ui_nav_create(lv_obj_t *parent);
extern lv_obj_t *label_printer_name;
extern lv_obj_t *label_clock;
