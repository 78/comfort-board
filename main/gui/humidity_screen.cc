#include "humidity_screen.h"

#include <cstdio>

#include <lvgl.h>

#include "fonts/fonts.h"
#include "misc/lv_area.h"

namespace gui {

namespace {

enum class Level { kOk, kWarn, kDanger };

// Baby room comfort ranges, tuned for subtropical climate (e.g. Guangdong)
// where summer AC is commonly set to 26°C. Thresholds are inclusive on the
// comfortable side (a reading of exactly 20 / 27 / 40 / 60 is OK) since the
// caller has already truncated fractions away.
//   Temp: 20-27°C ideal, 18-29°C acceptable, else dangerous
//         (upper danger >29°C guards against infant overheating / SIDS risk)
//   Humi: 40-60%  ideal, 30-65% acceptable, else dangerous
Level TempLevel(int t) {
    if (t < 18 || t > 29) return Level::kDanger;
    if (t < 20 || t > 27) return Level::kWarn;
    return Level::kOk;
}

Level HumiLevel(int h) {
    if (h < 30 || h > 65) return Level::kDanger;
    if (h < 40 || h > 60) return Level::kWarn;
    return Level::kOk;
}

// Panel cache so UpdateHumidityScreen can repaint backgrounds + values.
lv_obj_t* s_temp_panel = nullptr;
lv_obj_t* s_temp_value = nullptr;
lv_obj_t* s_humi_panel = nullptr;
lv_obj_t* s_humi_value = nullptr;

lv_obj_t* MakePanel(lv_obj_t* parent, int x, int w, int h) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, 0);
    lv_obj_set_style_pad_all(p, 4, 0);
    lv_obj_set_style_bg_color(p, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    // Labels inherit text_color from the panel; we flip it along with the bg
    // so OK text is black on white, Warn is black on yellow (high contrast)
    // and Danger is white on red (readable on the dark fill).
    lv_obj_set_style_text_color(p, lv_color_black(), 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t* MakeLabel(lv_obj_t* parent, const lv_font_t* font, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}

void ApplyLevel(lv_obj_t* panel, Level lv) {
    lv_color_t bg;
    lv_color_t fg;
    switch (lv) {
        case Level::kWarn:
            bg = lv_color_hex(0xFFFF00);  // yellow fill
            fg = lv_color_hex(0x000000);
            break;
        case Level::kDanger:
            bg = lv_color_hex(0xFF0000);  // red fill
            fg = lv_color_hex(0xFFFFFF);  // white digits for contrast on red
            break;
        case Level::kOk:
        default:
            bg = lv_color_hex(0xFFFFFF);
            fg = lv_color_hex(0x000000);
            break;
    }
    lv_obj_set_style_bg_color(panel, bg, 0);
    lv_obj_set_style_text_color(panel, fg, 0);
}

}  // namespace

void CreateHumidityScreen() {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    // Screen is 250x122 landscape. Split it into two equal 125-wide half-panels.
    // Each panel stacks header / big digits / unit vertically so the unit
    // never collides with the digit glyphs.
    static constexpr int kHalfW = 125;
    static constexpr int kH = 122;

    // ---------- Temperature panel ----------
    s_temp_panel = MakePanel(scr, 0, kHalfW, kH);
    lv_obj_t* t_hdr = MakeLabel(s_temp_panel, &font_label, "Temp");
    lv_obj_align(t_hdr, LV_ALIGN_TOP_LEFT, 0, 2);

    s_temp_value = MakeLabel(s_temp_panel, &font_digits, "--");
    lv_obj_align(s_temp_value, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t* t_unit = MakeLabel(s_temp_panel, &font_label, "°C");
    lv_obj_align(t_unit, LV_ALIGN_BOTTOM_RIGHT, 0, -2);

    // ---------- Humidity panel ----------
    s_humi_panel = MakePanel(scr, kHalfW, kHalfW, kH);
    lv_obj_t* h_hdr = MakeLabel(s_humi_panel, &font_label, "Humidity");
    lv_obj_align(h_hdr, LV_ALIGN_TOP_LEFT, 0, 2);

    s_humi_value = MakeLabel(s_humi_panel, &font_digits, "--");
    lv_obj_align(s_humi_value, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t* h_unit = MakeLabel(s_humi_panel, &font_label, "%");
    lv_obj_align(h_unit, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
}

void UpdateHumidityScreen(int temp_c, int humidity_pct, bool sensor_ok) {
    if (!sensor_ok) {
        lv_label_set_text(s_temp_value, "--");
        lv_label_set_text(s_humi_value, "--");
        ApplyLevel(s_temp_panel, Level::kDanger);
        ApplyLevel(s_humi_panel, Level::kDanger);
        return;
    }

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", temp_c);
    lv_label_set_text(s_temp_value, buf);
    ApplyLevel(s_temp_panel, TempLevel(temp_c));

    std::snprintf(buf, sizeof(buf), "%d", humidity_pct);
    lv_label_set_text(s_humi_value, buf);
    ApplyLevel(s_humi_panel, HumiLevel(humidity_pct));
}

}  // namespace gui
