#include "network/network_manager.h"
#include "ui/ui.h"
#include <Arduino.h>

/* Widgets */
lv_obj_t *label_status;
lv_obj_t *label_ip;
lv_obj_t *label_unit;
lv_obj_t *bar_progress;
lv_obj_t *label_lane_tool[4];
lv_obj_t *label_lane_status[4];
lv_obj_t *btn_lane_load[4];
lv_obj_t *btn_lane_unload[4];
lv_obj_t *btn_lane_filament[4];
lv_obj_t *label_lane_filament[4];
lv_obj_t *btn_lane_more[4];
lv_obj_t *led_indicator[4];

// Footer labels
lv_obj_t *label_wifi_name;
lv_obj_t *label_printer_status;

static lv_obj_t *filament_modal = NULL;
static lv_obj_t *filament_dropdown = NULL;
static int selected_lane_for_filament = -1;

static lv_obj_t *lane_options_modal = NULL;
static int selected_lane_for_options = -1;

static lv_obj_t *kb_modal = NULL;
static lv_obj_t *kb_ta = NULL;

void ui_screen_dashboard_init() {
  ui_ScreenDashboard = lv_obj_create(NULL);
  lv_obj_add_style(ui_ScreenDashboard, &style_base_screen, 0);

  /* Top Header */
  lv_obj_t *header = lv_obj_create(ui_ScreenDashboard);
  lv_obj_set_size(header, 480, 50);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x1a1a2e), 0); // Richer header
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(header, 2, 0); // Thicker border
  lv_obj_set_style_border_color(header, lv_color_hex(0x3a3a4a),
                                0); // Lighter border
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);        // Disable scrolling
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF); // Hide scrollbar
  lv_obj_set_style_pad_all(header, 0, 0);                   // Remove padding

  label_printer_name = lv_label_create(header);
  lv_label_set_text(label_printer_name, "PanelDue SC01+");
  lv_obj_set_style_text_font(label_printer_name, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label_printer_name, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(label_printer_name, LV_ALIGN_LEFT_MID, 10, 0);

  /* Unit Label (Center of Header) */
  label_unit = lv_label_create(header);
  lv_label_set_text(label_unit, "Unit 0");
  lv_obj_set_style_text_font(label_unit, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(label_unit, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(label_unit, LV_ALIGN_CENTER, 0, 0);

  label_ip = lv_label_create(header);
  lv_label_set_text(label_ip, "Disconnected");
  lv_obj_set_style_text_font(label_ip, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_ip, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(label_ip, LV_ALIGN_RIGHT_MID, -10, -10);

  label_clock = lv_label_create(header);
  lv_label_set_text(label_clock, "00:00:00");
  lv_obj_set_style_text_font(label_clock, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_clock, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(label_clock, LV_ALIGN_RIGHT_MID, -10, 10);

  /* Navigation (Gear Icon in Header) */
  lv_obj_t *btn_settings = lv_btn_create(header);
  lv_obj_set_size(btn_settings, 60, 50);
  lv_obj_align(btn_settings, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_set_style_bg_opa(btn_settings, 0,
                          0); // Transparent background for icon feel
  lv_obj_set_style_shadow_width(btn_settings, 0, 0);

  lv_obj_t *lbl_set = lv_label_create(btn_settings);
  lv_label_set_text(lbl_set, LV_SYMBOL_SETTINGS);
  lv_obj_set_style_text_font(lbl_set, &lv_font_montserrat_20, 0);
  lv_obj_center(lbl_set);

  lv_obj_add_event_cb(
      btn_settings, [](lv_event_t *e) { lv_scr_load(ui_ScreenSettings); },
      LV_EVENT_CLICKED, NULL);

  /* Adjust header labels to not overlap with gear icon */
  lv_obj_align(label_ip, LV_ALIGN_RIGHT_MID, -60, -10);
  lv_obj_align(label_clock, LV_ALIGN_RIGHT_MID, -60, 10);

  /* Filament Lane Cards (4 lanes - One Row) */
  for (int i = 0; i < 4; i++) {
    int x = 5 + i * 120; // Tighter horizontal spacing
    int y = 95;          // More space from header

    lv_obj_t *card = lv_obj_create(ui_ScreenDashboard);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, 115, 145); // Compact cards for footer
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    label_lane_tool[i] =
        lv_label_create(ui_ScreenDashboard); // Parent to screen
    lv_obj_set_width(label_lane_tool[i], 115);
    lv_obj_set_style_text_align(label_lane_tool[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_lane_tool[i], lv_color_hex(0xFFFFFF),
                                0); // Explicit White
    lv_obj_align(label_lane_tool[i], LV_ALIGN_TOP_LEFT, x,
                 68); // Centered between header and cards
    lv_label_set_text(label_lane_tool[i], "L?");
    lv_obj_set_style_text_font(label_lane_tool[i], &lv_font_montserrat_20,
                               0); // Larger font

    label_lane_status[i] = lv_label_create(card);
    lv_obj_align(label_lane_status[i], LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(label_lane_status[i], "Unloaded");
    lv_obj_set_style_text_font(label_lane_status[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_lane_status[i], lv_color_hex(0xFFFFFF),
                                0);

    btn_lane_unload[i] = lv_btn_create(card);
    lv_obj_set_size(btn_lane_unload[i], 90, 35);
    lv_obj_align(btn_lane_unload[i], LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_t *lbl_unload = lv_label_create(btn_lane_unload[i]);
    lv_label_set_text(lbl_unload, "Unload");
    lv_obj_set_style_text_font(lbl_unload, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_unload);

    lv_obj_add_event_cb(
        btn_lane_unload[i],
        [](lv_event_t *e) {
          int idx = (int)(intptr_t)lv_event_get_user_data(e);
          int unit = DataManager.getActiveAFCUnit();
          int toolIdxForLane = unit * 4 + idx;

          // Only send unload command if lane is actually loaded
          if (!DataManager.isLaneLoaded(toolIdxForLane)) {
            Serial.printf("UI: Lane %d not loaded, ignoring unload request\n",
                          idx);
            return;
          }

          // Get tool number from AFC_lane_to_tool[unit][lane]
          int toolNum = DataManager.getLaneToTool(unit, idx);

          char buf[128];
          snprintf(buf, sizeof(buf), "M98 P\"0:/macros/Lane - Unload\" A%d",
                   toolNum);
          DataManager.sendGCode(buf);
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);

    // Filament Selection Button (small button at bottom of card)
    btn_lane_filament[i] = lv_btn_create(card);
    lv_obj_set_size(btn_lane_filament[i], 90, 25);
    lv_obj_align(btn_lane_filament[i], LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(btn_lane_filament[i], lv_color_hex(0x444444), 0);

    label_lane_filament[i] = lv_label_create(btn_lane_filament[i]);
    lv_label_set_text(label_lane_filament[i], "");
    lv_obj_set_style_text_font(label_lane_filament[i], &lv_font_montserrat_14,
                               0);
    lv_obj_center(label_lane_filament[i]);

    lv_obj_add_event_cb(
        btn_lane_filament[i],
        [](lv_event_t *e) {
          int idx = (int)(intptr_t)lv_event_get_user_data(e);
          selected_lane_for_filament = idx;

          // Fetch filament list if not already loaded
          DataManager.fetchFilamentList();

          // Create modal dialog
          if (filament_modal) {
            lv_obj_del(filament_modal);
          }

          filament_modal = lv_obj_create(lv_scr_act());
          lv_obj_set_size(filament_modal, 300, 250);
          lv_obj_center(filament_modal);
          lv_obj_set_style_bg_color(filament_modal, lv_color_hex(0x1E1E1E), 0);
          lv_obj_set_style_border_color(filament_modal, lv_color_hex(0x444444),
                                        0);
          lv_obj_set_style_border_width(filament_modal, 2, 0);

          // Title
          lv_obj_t *title = lv_label_create(filament_modal);
          char titleBuf[32];
          snprintf(titleBuf, sizeof(titleBuf), "Select Filament - Lane %d",
                   idx);
          lv_label_set_text(title, titleBuf);
          lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
          lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

          // Dropdown
          filament_dropdown = lv_dropdown_create(filament_modal);
          lv_obj_set_width(filament_dropdown, 260);
          lv_obj_align(filament_dropdown, LV_ALIGN_TOP_MID, 0, 40);

          // Populate dropdown with filament list
          String options = "";
          int count = DataManager.getFilamentCount();
          for (int i = 0; i < count; i++) {
            if (i > 0)
              options += "\n";
            options += DataManager.getFilamentName(i);
          }
          if (count == 0) {
            options = "No filaments found";
          }
          lv_dropdown_set_options(filament_dropdown, options.c_str());

          // OK Button
          lv_obj_t *btn_ok = lv_btn_create(filament_modal);
          lv_obj_set_size(btn_ok, 120, 40);
          lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_LEFT, 10, -10);
          lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x4CD964), 0);

          lv_obj_t *lbl_ok = lv_label_create(btn_ok);
          lv_label_set_text(lbl_ok, "OK");
          lv_obj_center(lbl_ok);

          lv_obj_add_event_cb(
              btn_ok,
              [](lv_event_t *e) {
                if (filament_dropdown && selected_lane_for_filament >= 0) {
                  uint16_t sel = lv_dropdown_get_selected(filament_dropdown);
                  String filamentName = DataManager.getFilamentName(sel);
                  int unit = DataManager.getActiveAFCUnit();
                  DataManager.setLaneFilament(unit, selected_lane_for_filament,
                                              filamentName);
                }
                if (filament_modal) {
                  lv_obj_del(filament_modal);
                  filament_modal = NULL;
                }
              },
              LV_EVENT_CLICKED, NULL);

          // Cancel Button
          lv_obj_t *btn_cancel = lv_btn_create(filament_modal);
          lv_obj_set_size(btn_cancel, 120, 40);
          lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
          lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xFF6B6B), 0);

          lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
          lv_label_set_text(lbl_cancel, "Cancel");
          lv_obj_center(lbl_cancel);

          lv_obj_add_event_cb(
              btn_cancel,
              [](lv_event_t *e) {
                if (filament_modal) {
                  lv_obj_del(filament_modal);
                  filament_modal = NULL;
                }
              },
              LV_EVENT_CLICKED, NULL);
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  /* LED Indicators (below cards) */
  for (int i = 0; i < 4; i++) {
    int x = 5 + i * 120; // Match new card spacing
    int y = 250;         // Below the cards (95 + 145 + 10)

    led_indicator[i] = lv_obj_create(ui_ScreenDashboard);
    lv_obj_set_size(led_indicator[i], 24, 24);   // Slightly larger
    lv_obj_set_pos(led_indicator[i], x + 46, y); // Center under card
    lv_obj_set_style_radius(led_indicator[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(led_indicator[i], lv_color_hex(0x888888),
                              0); // Default gray

    // Remove border and outline but add subtle glow
    lv_obj_set_style_border_width(led_indicator[i], 0, 0);
    lv_obj_set_style_outline_width(led_indicator[i], 0, 0);
    lv_obj_set_style_pad_all(led_indicator[i], 0, 0);
    lv_obj_set_style_shadow_width(led_indicator[i], 8, 0); // Subtle glow
    lv_obj_set_style_shadow_color(led_indicator[i], lv_color_hex(0x888888), 0);
    lv_obj_set_style_shadow_opa(led_indicator[i], 40, 0);

    // More button (next to LED)
    btn_lane_more[i] = lv_btn_create(ui_ScreenDashboard);
    lv_obj_set_size(btn_lane_more[i], 30, 20);
    lv_obj_set_pos(btn_lane_more[i], x + 73, y); // Right of LED
    lv_obj_set_style_bg_color(btn_lane_more[i], lv_color_hex(0x555555), 0);
    lv_obj_set_style_radius(btn_lane_more[i], 4, 0);

    lv_obj_t *lbl_more = lv_label_create(btn_lane_more[i]);
    lv_label_set_text(lbl_more, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_font(lbl_more, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_more);

    lv_obj_add_event_cb(
        btn_lane_more[i],
        [](lv_event_t *e) {
          int idx = (int)(intptr_t)lv_event_get_user_data(e);
          selected_lane_for_options = idx;

          // Create modal if it doesn't exist
          if (lane_options_modal == NULL) {
            lane_options_modal = lv_obj_create(lv_scr_act());
            lv_obj_set_size(lane_options_modal, 280, 220);
            lv_obj_center(lane_options_modal);
            lv_obj_set_style_bg_color(lane_options_modal,
                                      lv_color_hex(0x1e1e2e),
                                      0); // Richer background
            lv_obj_set_style_border_color(
                lane_options_modal, lv_color_hex(0x7c3aed), 0); // Purple border
            lv_obj_set_style_border_width(lane_options_modal, 2, 0);
            lv_obj_set_style_radius(lane_options_modal, 14, 0); // More rounded
            lv_obj_set_style_shadow_width(lane_options_modal, 30,
                                          0); // Deeper shadow
            lv_obj_set_style_shadow_color(lane_options_modal,
                                          lv_color_hex(0x000000), 0);
            lv_obj_set_style_shadow_opa(lane_options_modal, 80,
                                        0); // Prominent shadow

            // Title
            lv_obj_t *title = lv_label_create(lane_options_modal);
            lv_label_set_text(title, "Lane Options");
            lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

            // Mark Unloaded button
            lv_obj_t *btn_mark_unloaded = lv_btn_create(lane_options_modal);
            lv_obj_set_size(btn_mark_unloaded, 240, 40);
            lv_obj_align(btn_mark_unloaded, LV_ALIGN_TOP_MID, 0, 45);
            lv_obj_set_style_bg_color(btn_mark_unloaded, lv_color_hex(0xFF6B6B),
                                      0);

            lv_obj_t *lbl_mark = lv_label_create(btn_mark_unloaded);
            lv_label_set_text(lbl_mark, "Mark Lane Unloaded");
            lv_obj_set_style_text_font(lbl_mark, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl_mark);

            lv_obj_add_event_cb(
                btn_mark_unloaded,
                [](lv_event_t *e) {
                  int lane = selected_lane_for_options;
                  int unit = DataManager.getActiveAFCUnit();
                  int toolIdxForLane = unit * 4 + lane;

                  // Only allow if lane is loaded
                  if (!DataManager.isLaneLoaded(toolIdxForLane)) {
                    Serial.printf(
                        "UI: Lane %d not loaded, cannot mark unloaded\n", lane);
                    return;
                  }

                  int toolNum = DataManager.getLaneToTool(unit, lane);

                  char buf[128];
                  snprintf(buf, sizeof(buf),
                           "M98 P\"0:/macros/Lane - Mark Unloaded\" A%d",
                           toolNum);
                  DataManager.sendGCode(buf);

                  // Close modal
                  lv_obj_del(lane_options_modal);
                  lane_options_modal = NULL;
                },
                LV_EVENT_CLICKED, NULL);

            // Measure Lane button
            lv_obj_t *btn_measure = lv_btn_create(lane_options_modal);
            lv_obj_set_size(btn_measure, 240, 40);
            lv_obj_align(btn_measure, LV_ALIGN_TOP_MID, 0, 95);
            lv_obj_set_style_bg_color(btn_measure, lv_color_hex(0x4A90E2), 0);

            lv_obj_t *lbl_measure = lv_label_create(btn_measure);
            lv_label_set_text(lbl_measure, "Measure This Lane");
            lv_obj_set_style_text_font(lbl_measure, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl_measure);

            lv_obj_add_event_cb(
                btn_measure,
                [](lv_event_t *e) {
                  int lane = selected_lane_for_options;
                  int unit = DataManager.getActiveAFCUnit();
                  int toolIdxForLane = unit * 4 + lane;

                  // Only allow if lane is loaded
                  if (!DataManager.isLaneLoaded(toolIdxForLane)) {
                    Serial.printf("UI: Lane %d not loaded, cannot measure\n",
                                  lane);
                    return;
                  }

                  int toolNum = DataManager.getLaneToTool(unit, lane);

                  char buf[128];
                  snprintf(buf, sizeof(buf),
                           "M98 P\"0:/macros/Lane - Measure First\" A%d",
                           toolNum);
                  DataManager.sendGCode(buf);

                  // Close modal
                  lv_obj_del(lane_options_modal);
                  lane_options_modal = NULL;
                },
                LV_EVENT_CLICKED, NULL);

            // Close button
            lv_obj_t *btn_close = lv_btn_create(lane_options_modal);
            lv_obj_set_size(btn_close, 240, 35);
            lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, -10);
            lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x555555), 0);

            lv_obj_t *lbl_close = lv_label_create(btn_close);
            lv_label_set_text(lbl_close, "Close");
            lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl_close);

            lv_obj_add_event_cb(
                btn_close,
                [](lv_event_t *e) {
                  lv_obj_del(lane_options_modal);
                  lane_options_modal = NULL;
                },
                LV_EVENT_CLICKED, NULL);
          }

          // Update title with lane number
          lv_obj_t *title = lv_obj_get_child(lane_options_modal, 0);
          char buf[32];
          snprintf(buf, sizeof(buf), "Lane %d Options", idx);
          lv_label_set_text(title, buf);
        },
        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  /* Footer Bar */
  lv_obj_t *footer = lv_obj_create(ui_ScreenDashboard);
  lv_obj_set_size(footer, 480, 35);
  lv_obj_set_style_bg_color(footer, lv_color_hex(0x1a1a2e), 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(footer, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(footer, 0, 0);
  lv_obj_set_style_border_width(footer, 0, 0);

  // WiFi Name (left)
  label_wifi_name = lv_label_create(footer);
  lv_label_set_text(label_wifi_name, "WiFi: --");
  lv_obj_set_style_text_font(label_wifi_name, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_wifi_name, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(label_wifi_name, LV_ALIGN_LEFT_MID, 10, 0);

  // Printer Connection Status (right)
  label_printer_status = lv_label_create(footer);
  lv_label_set_text(label_printer_status, "Printer: Disconnected");
  lv_obj_set_style_text_font(label_printer_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_printer_status, lv_color_hex(0xFF6B6B),
                              0); // Red for disconnected
  lv_obj_align(label_printer_status, LV_ALIGN_RIGHT_MID, -10, 0);
}

void ui_dashboard_update(const char *status, float progress, const char *name,
                         const char *time, int toolIdx) {
  Serial.printf("UI: dashboard_update called - status='%s'\n", status);
  if (label_status)
    lv_label_set_text(label_status, status);
  if (label_ip)
    lv_label_set_text(label_ip, DataManager.getIP().c_str());
  if (bar_progress)
    lv_bar_set_value(bar_progress, (int)progress, LV_ANIM_OFF);
  if (label_printer_name && name)
    lv_label_set_text(label_printer_name, name);
  if (label_clock && time)
    lv_label_set_text(label_clock, time);

  // Update Lane Cards
  int activeUnit = DataManager.getActiveAFCUnit();

  // Update unit label
  if (label_unit) {
    char unitBuf[16];
    snprintf(unitBuf, sizeof(unitBuf), "Unit %d", activeUnit);
    lv_label_set_text(label_unit, unitBuf);
  }

  for (int i = 0; i < 4; i++) {
    int toolIdxForLane = activeUnit * 4 + i;

    // Update lane label (above card) - just "Lane X"
    if (label_lane_tool[i]) {
      char buf[16];
      snprintf(buf, sizeof(buf), "Lane %d", i);
      lv_label_set_text(label_lane_tool[i], buf);
    }

    // Update loaded status (top of card)
    if (label_lane_status[i]) {
      bool loaded = DataManager.isLaneLoaded(toolIdxForLane);
      lv_label_set_text(label_lane_status[i], loaded ? "LOADED" : "Unloaded");
      lv_obj_set_style_text_color(
          label_lane_status[i],
          loaded ? lv_color_hex(0x4CD964) : lv_color_hex(0xFF6B6B), 0);
    }

    // Update filament button label
    if (label_lane_filament[i]) {
      String filament = DataManager.getLaneFilament(activeUnit, i);
      lv_label_set_text(label_lane_filament[i], filament.c_str());
    }

    // Update LED indicator
    if (led_indicator[i]) {
      int ledColor = DataManager.getLEDColor(activeUnit, i);
      uint32_t color = 0x888888; // Default gray

      switch (ledColor) {
      case 0:
        color = 0xFF0000;
        break; // Red
      case 1:
        color = 0x00FF00;
        break; // Green
      case 2:
        color = 0x0000FF;
        break; // Blue
      case 3:
        color = 0xFFFFFF;
        break; // White
      case 4:
        color = 0xFFFF00;
        break; // Yellow
      case 5:
        color = 0xFF00FF;
        break; // Magenta
      case 6:
        color = 0x00FFFF;
        break; // Cyan
      }

      lv_obj_set_style_bg_color(led_indicator[i], lv_color_hex(color), 0);
    }
  }

  // Update footer labels
  if (label_wifi_name) {
    String ssid = WiFi.SSID();
    if (ssid.length() > 0) {
      char buf[32];
      snprintf(buf, sizeof(buf), "WiFi: %s", ssid.c_str());
      lv_label_set_text(label_wifi_name, buf);
    } else {
      lv_label_set_text(label_wifi_name, "WiFi: --");
    }
  }

  if (label_printer_status) {
    String status = DataManager.getStatus();
    bool connected = (status != "Offline" && WiFi.status() == WL_CONNECTED);

    // Debug: Log status check
    static String lastStatus = "";
    if (status != lastStatus) {
      Serial.printf("Footer status check: '%s' -> %s\n", status.c_str(),
                    connected ? "Connected" : "Disconnected");
      lastStatus = status;
    }

    if (connected) {
      lv_label_set_text(label_printer_status, "Printer: Connected");
      lv_obj_set_style_text_color(label_printer_status, lv_color_hex(0x4CD964),
                                  0); // Green
    } else {
      lv_label_set_text(label_printer_status, "Printer: Disconnected");
      lv_obj_set_style_text_color(label_printer_status, lv_color_hex(0xFF6B6B),
                                  0); // Red
    }
  }
}
