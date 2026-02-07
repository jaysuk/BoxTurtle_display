#include <Arduino.h>
#define LGFX_USE_V1
#include "LGFX_SC01_Plus.hpp"
#include "network/network_manager.h"
#include "ui/ui.h"
#include <Wire.h> // Custom Touch Driver
#include <lvgl.h>

#define TP_ADDR 0x38

static LGFX tft;

/* Change to your screen resolution */
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

// Global touch state
static bool g_touched = false;
static int32_t g_last_x = 0;
static int32_t g_last_y = 0;
static uint16_t g_raw_x = 0; // Raw touch sensor values
static uint16_t g_raw_y = 0;
bool g_bypass_calibration = false; // Set to true to use raw coordinates

/* Read the touchpad */
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (g_touched) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = (lv_coord_t)g_last_x;
    data->point.y = (lv_coord_t)g_last_y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("\n--- WT32-SC01 PLUS v%s ---\n", FIRMWARE_VERSION);

  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH);

  tft.init();
  tft.initDMA();
  tft.setRotation(1); // Standard Landscape
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);

  Wire.begin(6, 5, 100000);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();
  DataManager.init();
  Serial.println("System Ready - Polling I2C...");
}

void loop() {
  // 1. READ I2C (Centralized)
  Wire.beginTransmission(0x38);
  Wire.write(0x02);
  Wire.endTransmission();

  if (Wire.requestFrom(0x38, 5) == 5) {
    uint8_t p = Wire.read();
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();

    if ((p & 0x0F) > 0) {
      uint16_t xr = ((xh & 0x0F) << 8) | xl;
      uint16_t yr = ((yh & 0x0F) << 8) | yl;

      // Store raw values
      g_raw_x = xr;
      g_raw_y = yr;

      if (g_bypass_calibration) {
        // RAW MODE - No calibration, direct mapping for calibration screen
        // Touch sensor is 320x480 (portrait), display is rotated to 480x320
        // (landscape) With rotation=1: sensor Y becomes display X, inverted
        // sensor X becomes display Y Sensor ranges: xr (0-320), yr (0-480)
        // Display ranges: x (0-479), y (0-319)
        g_last_x = yr;       // Sensor Y (0-480) → Display X (0-479)
        g_last_y = 319 - xr; // Inverted Sensor X (320-0) → Display Y (0-319)
      } else {
        // CALIBRATED MATH - Based on 4-point calibration data
        // Dot 1 (20,20): Raw(xr=305, yr=19)
        // Dot 2 (460,20): Raw(xr=315, yr=478)
        // Dot 3 (20,300): Raw(xr=5, yr=39)
        // Dot 4 (460,300): Raw(xr=27, yr=474)

        // X-axis: yr (raw sensor Y) maps to display X
        // yr range: 19-478 (459 units) → x range: 20-460 (440 pixels)
        // Linear mapping: x = (yr - 19) * 440 / 459 + 20
        g_last_x = ((int32_t)yr - 19) * 440 / 459 + 20;

        // Y-axis: xr (raw sensor X) maps to display Y (inverted)
        // xr range: 305-5 (inverted) → y range: 20-300 (280 pixels)
        // Average top xr: (305+315)/2 = 310 → y=20
        // Average bottom xr: (5+27)/2 = 16 → y=300
        // Linear mapping: y = (310 - xr) * 280 / 294 + 20
        g_last_y = (310 - (int32_t)xr) * 280 / 294 + 20;
      }

      if (g_last_x < 0)
        g_last_x = 0;
      if (g_last_x > 479)
        g_last_x = 479;
      if (g_last_y < 0)
        g_last_y = 0;
      if (g_last_y > 319)
        g_last_y = 319;

      // Calibration logging - outputs raw and calibrated coordinates
      static uint32_t lastLog = 0;
      if (millis() - lastLog > 500) { // Log every 500ms when touched
        Serial.printf("TOUCH: Raw(xr=%d, yr=%d) -> Calibrated(x=%d, y=%d)\n",
                      g_raw_x, g_raw_y, g_last_x, g_last_y);
        lastLog = millis();
      }

      g_touched = true;
    } else {
      g_touched = false;
    }
  }

  // 2. LVGL HANDLER
  DataManager.loop();
  ui_update_status();
  lv_tick_inc(5); // Add 5ms as we have a 5ms delay
  lv_timer_handler();

  delay(5);
}
