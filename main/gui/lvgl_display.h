#pragma once

#include <lvgl.h>

namespace gui {

// Initialize LVGL (lv_init + tick source + display object) for the JD79661
// 4-color panel. Must be called exactly once after
// EPaperDriver::GetInstance()->Reset().
//
// The returned display is owned by LVGL; callers normally just trigger a
// refresh via lv_refr_now(disp).
lv_display_t* InitLvglDisplay();

}  // namespace gui
