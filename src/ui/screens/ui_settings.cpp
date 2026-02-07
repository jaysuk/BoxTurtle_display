#include "network/network_manager.h"
#include "ui/ui.h"

static lv_obj_t *ta_ssid;
static lv_obj_t *ta_pass;
static lv_obj_t *ta_ip;
static lv_obj_t *ta_poll;
static lv_obj_t *kb;
static lv_obj_t *cont_units = NULL;     // Container for unit buttons
static lv_obj_t *btn_units[8] = {NULL}; // Support up to 8 units
static int last_unit_count = 0;

// Forward declaration
void ui_settings_refresh();

static void ta_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    /*Focus on the clicked text area*/
    if (kb != NULL) {
      lv_keyboard_set_textarea(kb, ta);
      lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (code == LV_EVENT_READY) {
    LV_LOG_USER("Ready, do something");
  }
}

static void btn_connect_wifi_event_cb(lv_event_t *e) {
  const char *ssid = lv_textarea_get_text(ta_ssid);
  const char *pass = lv_textarea_get_text(ta_pass);
  DataManager.connectWiFi(ssid, pass);

  // Hide keyboard
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_state(ta_ssid, LV_STATE_FOCUSED);
  lv_obj_clear_state(ta_pass, LV_STATE_FOCUSED);
}

static void btn_save_ip_event_cb(lv_event_t *e) {
  const char *ip = lv_textarea_get_text(ta_ip);
  DataManager.setPrinterIP(ip);
  DataManager.updatePrinterStatus(); // Trigger immediate check

  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_state(ta_ip, LV_STATE_FOCUSED);
}

static void btn_save_poll_event_cb(lv_event_t *e) {
  const char *txt = lv_textarea_get_text(ta_poll);
  uint32_t val = (uint32_t)atoi(txt);
  if (val < 500)
    val = 500; // Min 500ms
  DataManager.setPollInterval(val);

  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_state(ta_poll, LV_STATE_FOCUSED);
}

static void btn_back_event_cb(lv_event_t *e) {
  lv_scr_load(ui_ScreenDashboard);
}

static void btn_unit_event_cb(lv_event_t *e) {
  int unit = (int)(intptr_t)lv_event_get_user_data(e);
  DataManager.setActiveAFCUnit(unit);
  ui_settings_refresh(); // Refresh to update button states
}

static void kb_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}

void ui_settings_refresh() {
  // NOTE: Do NOT refresh text areas here - it clears user input while typing!
  // Text areas are only populated on initial screen load in
  // ui_screen_settings_init() if (!ta_ssid || !ta_pass || !ta_ip || !ta_poll)
  //   return;
  // lv_textarea_set_text(ta_ssid, DataManager.getSSID().c_str());
  // lv_textarea_set_text(ta_pass, DataManager.getPass().c_str());
  // lv_textarea_set_text(ta_ip, DataManager.getPrinterIP().c_str());
  // char buf[16];
  // sprintf(buf, "%u", DataManager.getPollInterval());
  // lv_textarea_set_text(ta_poll, buf);

  // Update unit buttons if count changed
  int unit_count = DataManager.getUnitCount();
  int active_unit = DataManager.getActiveAFCUnit();

  if (cont_units && (unit_count != last_unit_count || btn_units[0] == NULL)) {
    // Clear existing buttons
    for (int i = 0; i < 8; i++) {
      if (btn_units[i]) {
        lv_obj_del(btn_units[i]);
        btn_units[i] = NULL;
      }
    }

    // Create buttons for all units (ensure active unit is included)
    int buttons_to_create = unit_count;
    if (active_unit >= unit_count) {
      buttons_to_create = active_unit + 1; // Ensure active unit button exists
    }

    for (int i = 0; i < buttons_to_create && i < 8; i++) {
      btn_units[i] = lv_btn_create(cont_units);
      lv_obj_set_size(btn_units[i], 80, 40);
      lv_obj_set_pos(btn_units[i], 10 + i * 90, 35);

      if (i == active_unit) {
        lv_obj_add_style(btn_units[i], &style_btn_primary, 0);
      }

      lv_obj_add_event_cb(btn_units[i], btn_unit_event_cb, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);

      lv_obj_t *lbl = lv_label_create(btn_units[i]);
      lv_label_set_text_fmt(lbl, "Unit %d", i);
      lv_obj_center(lbl);
    }

    last_unit_count = unit_count;
  } else if (cont_units) {
    // Just update button styles (highlight active unit)
    for (int i = 0; i < unit_count && i < 8; i++) {
      if (btn_units[i]) {
        // Remove primary style from all buttons first
        lv_obj_remove_style(btn_units[i], &style_btn_primary, LV_PART_MAIN);

        // Add primary style only to active unit
        if (i == active_unit) {
          lv_obj_add_style(btn_units[i], &style_btn_primary, 0);
        }
      }
    }
  }
}

void ui_screen_settings_init() {
  ui_ScreenSettings = lv_obj_create(NULL);
  lv_obj_add_style(ui_ScreenSettings, &style_base_screen, 0);
  lv_obj_set_scrollbar_mode(
      ui_ScreenSettings,
      LV_SCROLLBAR_MODE_OFF); // Disable main screen scrollbar

  /* Header (Fixed) */
  lv_obj_t *header = lv_obj_create(ui_ScreenSettings);
  lv_obj_set_size(header, 480, 50);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x252526), 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);        // Disable scrolling
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF); // Hide scrollbar
  lv_obj_set_style_pad_all(header, 0, 0);                   // Remove padding
  lv_obj_set_style_border_width(header, 0, 0);              // Remove border

  lv_obj_t *lbl_header = lv_label_create(header);
  lv_label_set_text_fmt(lbl_header, "Settings (v%s)", FIRMWARE_VERSION);
  lv_obj_set_style_text_font(lbl_header, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(lbl_header, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(lbl_header, LV_ALIGN_LEFT_MID, 10, 0);

  /* Back Button */
  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 100, 50);
  lv_obj_align(btn_back, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, "Back");
  lv_obj_center(lbl_back);

  /* Scrollable Container */
  lv_obj_t *cont_main = lv_obj_create(ui_ScreenSettings);
  lv_obj_set_size(cont_main, 480, 270);
  lv_obj_set_pos(cont_main, 0, 50);
  lv_obj_set_style_bg_opa(cont_main, 0, 0);
  lv_obj_set_style_border_width(cont_main, 0, 0);
  lv_obj_set_scrollbar_mode(cont_main, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(cont_main, LV_DIR_VER);

  /* WiFi Input Area */
  lv_obj_t *cont_wifi = lv_obj_create(cont_main);
  lv_obj_set_size(cont_wifi, 440, 200); // Wider for better input
  lv_obj_set_pos(cont_wifi, 20, 10);
  lv_obj_add_style(cont_wifi, &style_card, 0);

  lv_obj_t *lbl_wifi = lv_label_create(cont_wifi);
  lv_label_set_text(lbl_wifi, "WiFi Connection");
  lv_obj_add_style(lbl_wifi, &style_text_title, 0);
  lv_obj_align(lbl_wifi, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_ssid = lv_textarea_create(cont_wifi);
  lv_textarea_set_placeholder_text(ta_ssid, "SSID");
  lv_obj_set_size(ta_ssid, 400, 40);
  lv_obj_align(ta_ssid, LV_ALIGN_TOP_LEFT, 0, 30);
  lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_ALL, NULL);

  ta_pass = lv_textarea_create(cont_wifi);
  lv_textarea_set_placeholder_text(ta_pass, "Password");
  lv_textarea_set_password_mode(ta_pass, true);
  lv_obj_set_size(ta_pass, 400, 40);
  lv_obj_align(ta_pass, LV_ALIGN_TOP_LEFT, 0, 80);
  lv_obj_add_event_cb(ta_pass, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *btn_connect = lv_btn_create(cont_wifi);
  lv_obj_add_style(btn_connect, &style_btn_primary, 0);
  lv_obj_set_size(btn_connect, 120, 40);
  lv_obj_align(btn_connect, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(btn_connect, btn_connect_wifi_event_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_t *lbl_conn = lv_label_create(btn_connect);
  lv_label_set_text(lbl_conn, "Connect");
  lv_obj_center(lbl_conn);

  /* Printer IP Area - Stacked vertically */
  lv_obj_t *cont_ip = lv_obj_create(cont_main);
  lv_obj_set_size(cont_ip, 440, 150);
  lv_obj_set_pos(cont_ip, 20, 230); // 10px margin below WiFi card
  lv_obj_add_style(cont_ip, &style_card, 0);

  lv_obj_t *lbl_ip = lv_label_create(cont_ip);
  lv_label_set_text(lbl_ip, "Printer Address (IP or Hostname)");
  lv_obj_add_style(lbl_ip, &style_text_title, 0);
  lv_obj_align(lbl_ip, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_ip = lv_textarea_create(cont_ip);
  lv_textarea_set_placeholder_text(ta_ip, "printer.local or 192.168.1.100");
  lv_obj_set_size(ta_ip, 400, 40);
  lv_obj_align(ta_ip, LV_ALIGN_TOP_LEFT, 0, 30);
  lv_obj_add_event_cb(ta_ip, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *btn_save = lv_btn_create(cont_ip);
  lv_obj_add_style(btn_save, &style_btn_primary, 0);
  lv_obj_set_size(btn_save, 120, 40);
  lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(btn_save, btn_save_ip_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_save = lv_label_create(btn_save);
  lv_label_set_text(lbl_save, "Save IP");
  lv_obj_center(lbl_save);

  /* Poll Rate Area */
  lv_obj_t *cont_poll = lv_obj_create(cont_main);
  lv_obj_set_size(cont_poll, 440, 150);
  lv_obj_set_pos(cont_poll, 20, 390); // Scroll further down
  lv_obj_add_style(cont_poll, &style_card, 0);

  lv_obj_t *lbl_poll_title = lv_label_create(cont_poll);
  lv_label_set_text(lbl_poll_title, "Poll Interval (ms)");
  lv_obj_add_style(lbl_poll_title, &style_text_title, 0);
  lv_obj_align(lbl_poll_title, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_poll = lv_textarea_create(cont_poll);
  lv_textarea_set_accepted_chars(ta_poll, "0123456789");
  lv_obj_set_size(ta_poll, 400, 40);
  lv_obj_align(ta_poll, LV_ALIGN_TOP_LEFT, 0, 30);
  lv_obj_add_event_cb(ta_poll, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *btn_save_poll = lv_btn_create(cont_poll);
  lv_obj_add_style(btn_save_poll, &style_btn_primary, 0);
  lv_obj_set_size(btn_save_poll, 120, 40);
  lv_obj_align(btn_save_poll, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(btn_save_poll, btn_save_poll_event_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_t *lbl_save_poll = lv_label_create(btn_save_poll);
  lv_label_set_text(lbl_save_poll, "Save Rate");
  lv_obj_center(lbl_save_poll);

  /* AFC Unit Selection */
  cont_units = lv_obj_create(cont_main);
  lv_obj_set_size(cont_units, 440, 100);
  lv_obj_set_pos(cont_units, 20, 550); // Below poll rate area
  lv_obj_add_style(cont_units, &style_card, 0);

  lv_obj_t *lbl_afc = lv_label_create(cont_units);
  lv_label_set_text(lbl_afc, "AFC Unit Selection");
  lv_obj_add_style(lbl_afc, &style_text_title, 0);
  lv_obj_align(lbl_afc, LV_ALIGN_TOP_LEFT, 0, 0);

  // Buttons will be created dynamically in ui_settings_refresh()

  /* Keyboard - Stays on root screen to remain on top */
  kb = lv_keyboard_create(ui_ScreenSettings);
  lv_obj_set_size(kb, 480, 160);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);

  /* Populate with saved values */
  lv_textarea_set_text(ta_ssid, DataManager.getSSID().c_str());
  lv_textarea_set_text(ta_pass, DataManager.getPass().c_str());
  lv_textarea_set_text(ta_ip, DataManager.getPrinterIP().c_str());
  char buf[16];
  sprintf(buf, "%u", DataManager.getPollInterval());
  lv_textarea_set_text(ta_poll, buf);

  // Create initial unit buttons
  ui_settings_refresh();
}
