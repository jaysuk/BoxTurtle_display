#include "ui.h"
#include "../network/network_manager.h"

// External bypass flag from main.cpp
extern bool g_bypass_calibration;
#include "screens/ui_calibration.h"
#include "screens/ui_lane_info.h"

/* Global Styles */
lv_style_t style_base_screen;
lv_style_t style_card;
lv_style_t style_text_title;
lv_style_t style_text_value;
lv_style_t style_btn_primary;
lv_style_t style_header;

/* Objects */
lv_obj_t *ui_ScreenDashboard;
lv_obj_t *ui_ScreenSettings;
lv_obj_t *ui_ScreenLaneInfo;
lv_obj_t *label_printer_name = NULL;
lv_obj_t *label_clock = NULL;

void ui_theme_apply(int theme_idx) {
  lv_style_set_bg_opa(&style_base_screen, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&style_base_screen, LV_GRAD_DIR_VER);
  
  lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
  lv_style_set_radius(&style_card, 14);
  lv_style_set_border_width(&style_card, 1);
  lv_style_set_pad_all(&style_card, 10);
  lv_style_set_shadow_width(&style_card, 25);
  lv_style_set_shadow_opa(&style_card, 60);

  lv_style_set_text_font(&style_text_title, &lv_font_montserrat_14);
  lv_style_set_text_font(&style_text_value, &lv_font_montserrat_28);

  lv_style_set_bg_grad_dir(&style_btn_primary, LV_GRAD_DIR_VER);
  lv_style_set_radius(&style_btn_primary, 10);
  lv_style_set_shadow_width(&style_btn_primary, 15);
  lv_style_set_shadow_opa(&style_btn_primary, 50);

  lv_style_set_bg_opa(&style_header, LV_OPA_COVER);
  lv_style_set_border_side(&style_header, LV_BORDER_SIDE_BOTTOM);
  lv_style_set_border_width(&style_header, 2);
  lv_style_set_pad_all(&style_header, 0);
  lv_style_set_radius(&style_header, 0);

  if (theme_idx == 1) { // Light
    lv_style_set_bg_color(&style_base_screen, lv_color_hex(0xf3f4f6));
    lv_style_set_bg_grad_color(&style_base_screen, lv_color_hex(0xe5e7eb));
    lv_style_set_text_color(&style_base_screen, lv_color_hex(0x1f2937));
    
    lv_style_set_bg_color(&style_card, lv_color_hex(0xffffff));
    lv_style_set_border_color(&style_card, lv_color_hex(0xcccccc));
    lv_style_set_shadow_color(&style_card, lv_color_hex(0xaaaaaa));
    
    lv_style_set_text_color(&style_text_title, lv_color_hex(0x4b5563));
    lv_style_set_text_color(&style_text_value, lv_color_hex(0x1f2937));
    
    lv_style_set_bg_color(&style_btn_primary, lv_color_hex(0x3b82f6));
    lv_style_set_bg_grad_color(&style_btn_primary, lv_color_hex(0x2563eb));
    lv_style_set_shadow_color(&style_btn_primary, lv_color_hex(0x3b82f6));
    
    lv_style_set_bg_color(&style_header, lv_color_hex(0xffffff));
    lv_style_set_border_color(&style_header, lv_color_hex(0xcccccc));
    lv_style_set_text_color(&style_header, lv_color_hex(0x1f2937));
    
  } else if (theme_idx == 2) { // Dark Green
    lv_style_set_bg_color(&style_base_screen, lv_color_hex(0x064e3b));
    lv_style_set_bg_grad_color(&style_base_screen, lv_color_hex(0x022c22));
    lv_style_set_text_color(&style_base_screen, lv_color_hex(0xd1fae5));
    
    lv_style_set_bg_color(&style_card, lv_color_hex(0x065f46));
    lv_style_set_border_color(&style_card, lv_color_hex(0x34d399));
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
    
    lv_style_set_text_color(&style_text_title, lv_color_hex(0xa7f3d0));
    lv_style_set_text_color(&style_text_value, lv_color_hex(0xd1fae5));
    
    lv_style_set_bg_color(&style_btn_primary, lv_color_hex(0x10b981));
    lv_style_set_bg_grad_color(&style_btn_primary, lv_color_hex(0x059669));
    lv_style_set_shadow_color(&style_btn_primary, lv_color_hex(0x10b981));
    
    lv_style_set_bg_color(&style_header, lv_color_hex(0x064e3b));
    lv_style_set_border_color(&style_header, lv_color_hex(0x34d399));
    lv_style_set_text_color(&style_header, lv_color_hex(0xd1fae5));
    
  } else if (theme_idx == 3) { // Cyberpunk
    lv_style_set_bg_color(&style_base_screen, lv_color_hex(0x2a0a2a));
    lv_style_set_bg_grad_color(&style_base_screen, lv_color_hex(0x000022));
    lv_style_set_text_color(&style_base_screen, lv_color_hex(0x00ffcc));
    
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1a0026));
    lv_style_set_border_color(&style_card, lv_color_hex(0xff00ff));
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
    
    lv_style_set_text_color(&style_text_title, lv_color_hex(0xff00ff));
    lv_style_set_text_color(&style_text_value, lv_color_hex(0x00ffcc));
    
    lv_style_set_bg_color(&style_btn_primary, lv_color_hex(0xff00ff));
    lv_style_set_bg_grad_color(&style_btn_primary, lv_color_hex(0xcc00cc));
    lv_style_set_shadow_color(&style_btn_primary, lv_color_hex(0xff00ff));
    
    lv_style_set_bg_color(&style_header, lv_color_hex(0x1a0026));
    lv_style_set_border_color(&style_header, lv_color_hex(0xff00ff));
    lv_style_set_text_color(&style_header, lv_color_hex(0x00ffcc));
    
  } else { // Dark Purple / Default (0)
    lv_style_set_bg_color(&style_base_screen, lv_color_hex(0x1a2332));
    lv_style_set_bg_grad_color(&style_base_screen, lv_color_hex(0x2d1b4e));
    lv_style_set_text_color(&style_base_screen, lv_color_hex(0xFFFFFF));
    
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1e1e2e));
    lv_style_set_border_color(&style_card, lv_color_hex(0x3a3a4a));
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
    
    lv_style_set_text_color(&style_text_title, lv_color_hex(0xAAAAAA));
    lv_style_set_text_color(&style_text_value, lv_color_hex(0xFFFFFF));
    
    lv_style_set_bg_color(&style_btn_primary, lv_color_hex(0x7c3aed));
    lv_style_set_bg_grad_color(&style_btn_primary, lv_color_hex(0x5b21b6));
    lv_style_set_shadow_color(&style_btn_primary, lv_color_hex(0x7c3aed));

    lv_style_set_bg_color(&style_header, lv_color_hex(0x1a1a2e));
    lv_style_set_border_color(&style_header, lv_color_hex(0x3a3a4a));
    lv_style_set_text_color(&style_header, lv_color_hex(0xFFFFFF));
  }

  lv_obj_report_style_change(NULL);
}

void ui_theme_init() {
  lv_style_init(&style_base_screen);
  lv_style_init(&style_card);
  lv_style_init(&style_text_title);
  lv_style_init(&style_text_value);
  lv_style_init(&style_btn_primary);
  lv_style_init(&style_header);

  ui_theme_apply(DataManager.getTheme());
}

void ui_init() {
  ui_theme_init();
  ui_screen_dashboard_init();
  ui_screen_settings_init();
  ui_calibration_screen_init();
  ui_screen_lane_info_init();

  /* Load dashboard screen with calibration */
  g_bypass_calibration = false;
  lv_scr_load(ui_ScreenDashboard);
}

void ui_update_status() {
  static float lastProg = -1.0f;
  static String lastStatus = "";
  static String lastName = "";
  static String lastTime = "";
  static int lastToolIdx = -1;
  static int lastActiveUnit = -1;
  static bool lastLaneLoaded[4] = {false, false, false, false};
  static String lastLaneNames[4] = {"", "", "", ""};

  float progress = DataManager.getProgress();
  String status = DataManager.getStatus();
  String name = DataManager.getPrinterName();
  String time = DataManager.getFormattedTime();
  int toolIdx = DataManager.getSelectedTool();
  int activeUnit = DataManager.getActiveAFCUnit();

  bool laneChanged = (activeUnit != lastActiveUnit);
  for (int i = 0; i < 4; i++) {
    int idx = activeUnit * 4 + i;
    if (DataManager.isLaneLoaded(idx) != lastLaneLoaded[i] ||
        DataManager.getLaneName(idx) != lastLaneNames[i]) {
      laneChanged = true;
      lastLaneLoaded[i] = DataManager.isLaneLoaded(idx);
      lastLaneNames[i] = DataManager.getLaneName(idx);
    }
  }
  lastActiveUnit = activeUnit;

  if (progress != lastProg || status != lastStatus || name != lastName ||
      time != lastTime || toolIdx != lastToolIdx || laneChanged) {
    ui_dashboard_update(status.c_str(), progress, name.c_str(), time.c_str(),
                        toolIdx);
    lastProg = progress;
    lastStatus = status;
    lastName = name;
    lastTime = time;
    lastToolIdx = toolIdx;
  }

  // Also sync the theme if it changed in background or web portal
  static int lastTheme = -1;
  int currentTheme = DataManager.getTheme();
  if (currentTheme != lastTheme && lastTheme != -1) {
    ui_theme_apply(currentTheme);
  }
  lastTheme = currentTheme;

  // Refresh settings screen to update unit buttons when new units discovered
  if (lv_scr_act() == ui_ScreenSettings) {
    extern void ui_settings_refresh();
    ui_settings_refresh();
  }

  // Update lane info screen if visible
  if (lv_scr_act() == ui_ScreenLaneInfo) {
    ui_lane_info_update();
  }
}
