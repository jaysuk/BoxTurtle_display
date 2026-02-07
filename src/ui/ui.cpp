#include "ui.h"
#include "../network/network_manager.h"

// External bypass flag from main.cpp
extern bool g_bypass_calibration;
#include "screens/ui_calibration.h"

/* Global Styles */
lv_style_t style_base_screen;
lv_style_t style_card;
lv_style_t style_text_title;
lv_style_t style_text_value;
lv_style_t style_btn_primary;

/* Objects */
lv_obj_t *ui_ScreenDashboard;
lv_obj_t *ui_ScreenSettings;
lv_obj_t *label_printer_name = NULL;
lv_obj_t *label_clock = NULL;

void ui_theme_init() {
  /* Base Screen Style */
  lv_style_init(&style_base_screen);
  lv_style_set_bg_color(&style_base_screen,
                        lv_color_hex(0x1a2332)); // Dark blue
  lv_style_set_bg_grad_color(&style_base_screen,
                             lv_color_hex(0x2d1b4e)); // Purple
  lv_style_set_bg_grad_dir(&style_base_screen, LV_GRAD_DIR_VER);
  lv_style_set_bg_opa(&style_base_screen, LV_OPA_COVER);
  lv_style_set_text_color(&style_base_screen, lv_color_hex(0xFFFFFF));

  /* Card Style */
  lv_style_init(&style_card);
  lv_style_set_bg_color(&style_card,
                        lv_color_hex(0x1e1e2e)); // Richer card color
  lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
  lv_style_set_radius(&style_card, 14); // Slightly more rounded
  lv_style_set_border_width(&style_card, 1);
  lv_style_set_border_color(&style_card,
                            lv_color_hex(0x3a3a4a)); // Lighter border
  lv_style_set_pad_all(&style_card,
                       12); // Tighter padding for more content space
  lv_style_set_shadow_width(&style_card, 25); // Deeper shadow
  lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
  lv_style_set_shadow_opa(&style_card, 60); // More prominent shadow

  /* Text Styles */
  lv_style_init(&style_text_title);
  lv_style_set_text_font(&style_text_title, &lv_font_montserrat_14);
  lv_style_set_text_color(&style_text_title, lv_color_hex(0xAAAAAA));

  lv_style_init(&style_text_value);
  lv_style_set_text_font(&style_text_value, &lv_font_montserrat_28);
  lv_style_set_text_color(&style_text_value, lv_color_hex(0xFFFFFF));

  /* Button Style */
  lv_style_init(&style_btn_primary);
  lv_style_set_bg_color(&style_btn_primary,
                        lv_color_hex(0x7c3aed)); // Vibrant purple
  lv_style_set_bg_grad_color(&style_btn_primary,
                             lv_color_hex(0x5b21b6)); // Deeper purple
  lv_style_set_bg_grad_dir(&style_btn_primary, LV_GRAD_DIR_VER);
  lv_style_set_radius(&style_btn_primary, 10);       // More rounded
  lv_style_set_shadow_width(&style_btn_primary, 15); // Bigger shadow
  lv_style_set_shadow_color(&style_btn_primary,
                            lv_color_hex(0x7c3aed)); // Purple glow
  lv_style_set_shadow_opa(&style_btn_primary, 50);   // Visible glow
}

void ui_init() {
  ui_theme_init();
  ui_screen_dashboard_init();
  ui_screen_settings_init();
  ui_calibration_screen_init();

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

  // Refresh settings screen to update unit buttons when new units discovered
  if (lv_scr_act() == ui_ScreenSettings) {
    extern void ui_settings_refresh();
    ui_settings_refresh();
  }
}
