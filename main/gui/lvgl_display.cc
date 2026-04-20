#include "lvgl_display.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <esp_log.h>
#include <esp_timer.h>

#include "common/config.h"
#include "hardware/display/epaper_driver.h"

#define TAG "LvglDisplay"

namespace gui {

// We run LVGL in landscape (wider than tall). The physical panel is 122x250
// portrait, so flush_cb rotates 90° CCW before handing the frame to the driver.
static constexpr int kLvWidth  = EPD_2IN13_HEIGHT;  // 250
static constexpr int kLvHeight = EPD_2IN13_WIDTH;   // 122

// LVGL 9.5's software renderer cannot blend directly into a 2-bpp indexed
// buffer (only I1 is supported among indexed formats). So we render to
// RGB565 full-screen and quantize inside flush_cb.
//   Buffer: 250 * 122 * 2 B = 61000 B (~60 KiB in internal DRAM).
alignas(16) static uint16_t s_draw_buf[kLvWidth * kLvHeight];

static uint32_t LvTickCb(void) {
    // Millisecond tick derived from the monotonic esp_timer. Survives light
    // sleep transitions cleanly, unlike xTaskGetTickCount.
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

// Nearest-neighbor quantization to the JD79661 palette (black, white,
// yellow, red) using squared Euclidean distance in RGB888 space.
static inline uint8_t QuantizeRgb565(uint16_t px) {
    const int r = ((px >> 11) & 0x1F) << 3;  // 0..248
    const int g = ((px >> 5)  & 0x3F) << 2;  // 0..252
    const int b = ( px        & 0x1F) << 3;  // 0..248

    const int d_black  =       r * r             +       g * g             +       b * b;
    const int d_white  = (255 - r) * (255 - r)   + (255 - g) * (255 - g)   + (255 - b) * (255 - b);
    const int d_yellow = (255 - r) * (255 - r)   + (255 - g) * (255 - g)   +       b * b;
    const int d_red    = (255 - r) * (255 - r)   +       g * g             +       b * b;

    int best = d_black;
    uint8_t idx = EPD_BLACK;
    if (d_white  < best) { best = d_white;  idx = EPD_WHITE;  }
    if (d_yellow < best) { best = d_yellow; idx = EPD_YELLOW; }
    if (d_red    < best) { best = d_red;    idx = EPD_RED;    }
    return idx;
}

static void FlushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    EPaperDriver* epd = EPaperDriver::GetInstance();
    const uint16_t panel_w = epd->width();         // 122
    const uint16_t panel_h = epd->height();        // 250
    const uint16_t stride  = epd->stride_bytes();  // 32 B per panel row

    auto out = std::make_unique<std::vector<uint8_t>>(
        static_cast<size_t>(stride) * panel_h, 0x55);  // preload white

    const uint16_t* src = reinterpret_cast<const uint16_t*>(px_map);
    const int src_w = area->x2 - area->x1 + 1;

    // Rotate 90° CCW while quantizing:
    //   panel_x = lvgl_y
    //   panel_y = (kLvWidth - 1) - lvgl_x
    for (int ly = area->y1; ly <= area->y2; ++ly) {
        for (int lx = area->x1; lx <= area->x2; ++lx) {
            const uint16_t rgb = src[(ly - area->y1) * src_w + (lx - area->x1)];
            const uint8_t color = QuantizeRgb565(rgb);

            const int px = ly;
            const int py = (kLvWidth - 1) - lx;
            if (px >= panel_w || py < 0 || py >= panel_h) {
                continue;
            }

            const int byte_idx = py * stride + (px >> 2);
            const int shift = (3 - (px & 0x3)) * 2;
            uint8_t& cell = (*out)[byte_idx];
            cell = (cell & ~(0x3 << shift)) | (color << shift);
        }
    }

    epd->UpdateDisplay(std::move(out));
    lv_display_flush_ready(disp);
}

lv_display_t* InitLvglDisplay() {
    lv_init();
    lv_tick_set_cb(LvTickCb);

    lv_display_t* disp = lv_display_create(kLvWidth, kLvHeight);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, s_draw_buf, nullptr, sizeof(s_draw_buf),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, FlushCb);

    ESP_LOGI(TAG, "LVGL display ready: %dx%d RGB565, buf=%u B",
             kLvWidth, kLvHeight, static_cast<unsigned>(sizeof(s_draw_buf)));
    return disp;
}

}  // namespace gui
