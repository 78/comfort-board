#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Large digits for temperature / humidity values.
// Glyphs: 0-9 . - ° %
extern const lv_font_t font_digits;

// Small labels (ASCII 0x20-0x7E + degree sign).
extern const lv_font_t font_label;

#ifdef __cplusplus
}
#endif
