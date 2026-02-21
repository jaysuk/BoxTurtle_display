#include "network/network_manager.h"
#include "ui/ui.h"
#include "ui/screens/ui_lane_info.h"
#include <Arduino.h>

// We need access to the Lane Info screen object, declared in ui.cpp
extern lv_obj_t *ui_ScreenLaneInfo;

#define LI_MAX_LANES 16

static lv_obj_t *li_lane_wrappers[LI_MAX_LANES] = {NULL};
static lv_obj_t *li_label_tool[LI_MAX_LANES]    = {NULL};
static lv_obj_t *li_label_first[LI_MAX_LANES]   = {NULL};
static lv_obj_t *li_label_full[LI_MAX_LANES]    = {NULL};

// G-code callback data
struct LaneBtnData {
  int lane;
  int macroType; // 0=first, 2=full
};
static LaneBtnData li_btn_data[LI_MAX_LANES * 2];

static void measure_btn_cb(lv_event_t *e) {
  LaneBtnData *d = (LaneBtnData *)lv_event_get_user_data(e);
  if (!d) return;

  int unit = DataManager.getActiveAFCUnit();
  int tool = DataManager.getLaneToTool(unit, d->lane);

  char buf[96];
  if (d->macroType == 0) {
    snprintf(buf, sizeof(buf), "M98 P\"0:/macros/Lane - Measure First\" A%d", tool);
  } else {
    snprintf(buf, sizeof(buf), "M98 P\"0:/macros/Lane - Measure Main Length\" A%d", tool);
  }
  DataManager.sendGCode(buf);
}

void ui_screen_lane_info_init() {
  ui_ScreenLaneInfo = lv_obj_create(NULL);
  lv_obj_add_style(ui_ScreenLaneInfo, &style_base_screen, 0);

  /* ── Header ── */
  lv_obj_t *header = lv_obj_create(ui_ScreenLaneInfo);
  lv_obj_set_size(header, 480, 50);
  lv_obj_add_style(header, &style_header, 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(header, 0, 0);

  // Back button
  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 50, 50);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_set_style_bg_opa(btn_back, 0, 0);
  lv_obj_set_style_shadow_width(btn_back, 0, 0);
  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
  lv_obj_add_style(lbl_back, &style_header, 0);
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_20, 0);
  lv_obj_center(lbl_back);
  lv_obj_add_event_cb(btn_back,
    [](lv_event_t *e) { lv_scr_load(ui_ScreenDashboard); },
    LV_EVENT_CLICKED, NULL);

  // Title
  lv_obj_t *lbl_title = lv_label_create(header);
  lv_label_set_text(lbl_title, "Lane Measurements");
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
  lv_obj_add_style(lbl_title, &style_header, 0);
  lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

  /* ── Scrollable lane container ── */
  lv_obj_t *cont = lv_obj_create(ui_ScreenLaneInfo);
  lv_obj_set_size(cont, 480, 270);
  lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(cont, 5, 0);
  lv_obj_set_style_pad_column(cont, 10, 0);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE); // enable later per-axis
  lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scroll_dir(cont, LV_DIR_HOR);
  lv_obj_set_style_bg_opa(cont, 0, 0);
  lv_obj_set_style_border_width(cont, 0, 0);

  for (int i = 0; i < LI_MAX_LANES; i++) {
    // Lane wrapper card
    lv_obj_t *card = lv_obj_create(cont);
    lv_obj_set_size(card, 110, 255);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(card, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(card, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    li_lane_wrappers[i] = card;

    // Lane label (line 1: Lane N)
    lv_obj_t *lbl_lane_num = lv_label_create(card);
    lv_label_set_text_fmt(lbl_lane_num, "Lane %d", i);
    lv_obj_add_style(lbl_lane_num, &style_text_title, 0);
    lv_obj_set_style_text_font(lbl_lane_num, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_lane_num, 94);
    lv_obj_set_style_text_align(lbl_lane_num, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_lane_num, LV_LABEL_LONG_DOT);
    li_label_tool[i] = lbl_lane_num;

    // Tool sub-label (line 2: T0, T1, ...)
    lv_obj_t *lbl_tool_sub = lv_label_create(card);
    lv_label_set_text(lbl_tool_sub, "T?");
    lv_obj_add_style(lbl_tool_sub, &style_text_title, 0);
    lv_obj_set_style_text_font(lbl_tool_sub, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_tool_sub, 94);
    lv_obj_set_style_text_align(lbl_tool_sub, LV_TEXT_ALIGN_CENTER, 0);

    // Divider
    lv_obj_t *div1 = lv_obj_create(card);
    lv_obj_set_size(div1, 94, 1);
    lv_obj_set_style_bg_color(div1, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(div1, 0, 0);
    lv_obj_set_style_pad_all(div1, 0, 0);

    // --- First Length Section ---
    lv_obj_t *lbl_first_title = lv_label_create(card);
    lv_label_set_text(lbl_first_title, "First Len");
    lv_obj_add_style(lbl_first_title, &style_text_title, 0);
    lv_obj_set_style_text_font(lbl_first_title, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_first_title, 94);
    lv_obj_set_style_text_align(lbl_first_title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl_first = lv_label_create(card);
    lv_label_set_text(lbl_first, "---");
    lv_obj_add_style(lbl_first, &style_text_value, 0);
    lv_obj_set_style_text_font(lbl_first, &lv_font_montserrat_16, 0);
    lv_obj_set_width(lbl_first, 94);
    lv_obj_set_style_text_align(lbl_first, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_first, LV_LABEL_LONG_DOT);
    li_label_first[i] = lbl_first;

    // Remeasure First button
    li_btn_data[i * 2].lane = i;
    li_btn_data[i * 2].macroType = 0;
    lv_obj_t *btn_first = lv_btn_create(card);
    lv_obj_set_size(btn_first, 94, 28);
    lv_obj_add_style(btn_first, &style_btn_primary, 0);
    lv_obj_t *btn_first_lbl = lv_label_create(btn_first);
    lv_label_set_text(btn_first_lbl, "Measure");
    lv_obj_set_style_text_font(btn_first_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_first_lbl);
    lv_obj_add_event_cb(btn_first, measure_btn_cb, LV_EVENT_CLICKED, &li_btn_data[i * 2]);

    // Divider
    lv_obj_t *div2 = lv_obj_create(card);
    lv_obj_set_size(div2, 94, 1);
    lv_obj_set_style_bg_color(div2, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(div2, 0, 0);
    lv_obj_set_style_pad_all(div2, 0, 0);

    // --- Full Length Section ---
    lv_obj_t *lbl_full_title = lv_label_create(card);
    lv_label_set_text(lbl_full_title, "Full Len");
    lv_obj_add_style(lbl_full_title, &style_text_title, 0);
    lv_obj_set_style_text_font(lbl_full_title, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_full_title, 94);
    lv_obj_set_style_text_align(lbl_full_title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl_full = lv_label_create(card);
    lv_label_set_text(lbl_full, "---");
    lv_obj_add_style(lbl_full, &style_text_value, 0);
    lv_obj_set_style_text_font(lbl_full, &lv_font_montserrat_16, 0);
    lv_obj_set_width(lbl_full, 94);
    lv_obj_set_style_text_align(lbl_full, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_full, LV_LABEL_LONG_DOT);
    li_label_full[i] = lbl_full;

    // Remeasure Full button
    li_btn_data[i * 2 + 1].lane = i;
    li_btn_data[i * 2 + 1].macroType = 2;
    lv_obj_t *btn_full = lv_btn_create(card);
    lv_obj_set_size(btn_full, 94, 28);
    lv_obj_add_style(btn_full, &style_btn_primary, 0);
    lv_obj_t *btn_full_lbl = lv_label_create(btn_full);
    lv_label_set_text(btn_full_lbl, "Measure");
    lv_obj_set_style_text_font(btn_full_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_full_lbl);
    lv_obj_add_event_cb(btn_full, measure_btn_cb, LV_EVENT_CLICKED, &li_btn_data[i * 2 + 1]);

    // Hide unused lanes by default
    lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
  }
}

void ui_lane_info_update() {
  int unit = DataManager.getActiveAFCUnit();
  int laneCount = DataManager.getLanesForUnit(unit);

  for (int i = 0; i < LI_MAX_LANES; i++) {
    if (!li_lane_wrappers[i]) continue;

    if (i < laneCount) {
      lv_obj_clear_flag(li_lane_wrappers[i], LV_OBJ_FLAG_HIDDEN);

      int tool = DataManager.getLaneToTool(unit, i);

      // Lane label
      lv_label_set_text_fmt(li_label_tool[i], "Lane %d", i);
      // Tool sub-label: find it as next sibling
      lv_obj_t *card = li_lane_wrappers[i];
      lv_obj_t *tool_sub = lv_obj_get_child(card, 1); // second child
      if (tool_sub) lv_label_set_text_fmt(tool_sub, "T%d", tool);

      // First length — LVGL's lv_snprintf has no %f support, split manually
      float firstLen = DataManager.getLaneLength(unit, i, 1);
      if (firstLen < 0) {
        lv_label_set_text(li_label_first[i], "---");
      } else {
        int f_whole = (int)firstLen;
        int f_frac  = (int)((firstLen - f_whole) * 10);
        lv_label_set_text_fmt(li_label_first[i], "%d.%d mm", f_whole, f_frac);
      }

      // Full length
      float fullLen = DataManager.getLaneLength(unit, i, 2);
      if (fullLen < 0) {
        lv_label_set_text(li_label_full[i], "---");
      } else {
        int l_whole = (int)fullLen;
        int l_frac  = (int)((fullLen - l_whole) * 10);
        lv_label_set_text_fmt(li_label_full[i], "%d.%d mm", l_whole, l_frac);
      }
    } else {
      lv_obj_add_flag(li_lane_wrappers[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}
