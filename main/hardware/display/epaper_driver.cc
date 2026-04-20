#include "epaper_driver.h"
#include "jd79661_driver.h"
#include <esp_log.h>

#define TAG "EPaperDriver"

EPaperDriver::~EPaperDriver() = default;

EPaperDriver* EPaperDriver::GetInstance() {
    static EPaperDriver* instance = nullptr;
    if (instance != nullptr) {
        return instance;
    }

    // Shared bus owned for the lifetime of the process.
    static EPaperBus bus;
    bus.EnablePower();

    instance = new Jd79661Driver(&bus);
    ESP_LOGI(TAG, "Instantiated driver: %s (%dx%d)", instance->name(),
             instance->max_width(), instance->max_height());
    return instance;
}

void EPaperDriver::SetResolution(uint16_t width, uint16_t height) {
    if (width > max_width() || height > max_height()) {
        ESP_LOGE(TAG, "Resolution %dx%d exceeds max %dx%d",
                 width, height, max_width(), max_height());
        return;
    }

    width_ = width;
    height_ = height;
    start_x_ = (max_width() - width) / 2;
    start_y_ = (max_height() - height) / 2;
    // Align start_x and logical width to 8-pixel (byte) boundaries so the
    // controller's column addressing lines up with our stride.
    start_x_ = (start_x_ + 7) & ~7;
    aligned_width_ = (width + 7) & ~7;
    stride_bytes_ = aligned_width_ / 8 * 2;

    ESP_LOGI(TAG, "Resolution set to %dx%d (stride=%d)", width, height, stride_bytes_);
}

void EPaperDriver::ResetToMaxResolution() {
    SetResolution(max_width(), max_height());
}
