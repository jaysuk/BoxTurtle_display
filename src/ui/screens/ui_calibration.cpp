#include "ui_calibration.h"
#include <Arduino.h>

// External bypass flag from main.cpp
extern bool g_bypass_calibration;

lv_obj_t *ui_ScreenCalibration = NULL;

// Simple 4-point calibration targets
struct CalibTarget {
  int x;
  int y;
  const char *label;
};

static CalibTarget targets[] = {
    {20, 20, "1"}, {460, 20, "2"}, {20, 300, "3"}, {460, 300, "4"}};

static int currentTarget = 0;
static const int numTargets = sizeof(targets) / sizeof(targets[0]);

void ui_calibration_screen_init() {
  // Enable raw coordinate mode for calibration screen
  g_bypass_calibration = true;

  ui_ScreenCalibration = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenCalibration, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_ScreenCalibration, lv_color_hex(0x000000), 0);

  // Title
  lv_obj_t *title = lv_label_create(ui_ScreenCalibration);
  lv_label_set_text(title, "TOUCH CALIBRATION");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

  // Instructions
  lv_obj_t *instructions = lv_label_create(ui_ScreenCalibration);
  lv_label_set_text(instructions,
                    "Touch each numbered dot.\nCheck Serial for coordinates.");
  lv_obj_set_style_text_color(instructions, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_style_text_font(instructions, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(instructions, LV_ALIGN_TOP_MID, 0, 35);

  // Draw all 4 dots
  for (int i = 0; i < numTargets; i++) {
    // Dot (circle)
    lv_obj_t *dot = lv_obj_create(ui_ScreenCalibration);
    lv_obj_set_size(dot, 20, 20);
    lv_obj_set_pos(dot, targets[i].x - 10,
                   targets[i].y - 10); // Center the 20px dot
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(dot,
                              i == currentTarget ? lv_color_hex(0x00FF00)
                                                 : lv_color_hex(0xFF0000),
                              0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);

    // Label next to dot
    lv_obj_t *label = lv_label_create(ui_ScreenCalibration);
    lv_label_set_text(label, targets[i].label);
    lv_obj_set_style_text_color(label,
                                i == currentTarget ? lv_color_hex(0x00FF00)
                                                   : lv_color_hex(0xFFFFFF),
                                0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(label, targets[i].x + 15, targets[i].y - 8);
  }

  Serial.println("\n=== TOUCH CALIBRATION MODE (RAW) ===");
  Serial.printf("Touch dot %s at (%d, %d)\n", targets[currentTarget].label,
                targets[currentTarget].x, targets[currentTarget].y);
  Serial.println(
      "Expected format: Raw(xr=XXX, yr=YYY) -> Calibrated(x=XXX, y=YYY)");
  Serial.println("====================================\n");
}
