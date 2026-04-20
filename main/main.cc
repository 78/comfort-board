/*
 * comfort-board app entry.
 *
 * On each wakeup: init the e-paper panel + LVGL, read the DHT22 once,
 * render the humidity screen, trigger a single LVGL refresh (which quantizes
 * the RGB565 buffer and drives the panel over SPI), then put the panel and
 * the chip itself into deep sleep for DEEP_SLEEP_INTERVAL_US.
 *
 * While the panel is busy (~5-10 s for a 4-color refresh) the only long
 * waits are vTaskDelay() calls in ReadBusy(), which cooperate with FreeRTOS
 * tickless idle + esp_pm to automatically enter light sleep.
 */

#include <dht.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

#include "config.h"
#include "display/epaper_driver.h"
#include "humidity_screen.h"
#include "lvgl_display.h"

#define TAG "main"

namespace {

// Enable automatic light sleep. CONFIG_PM_ENABLE only links the power
// management framework in; we still have to hand it a concrete DFS +
// light-sleep policy at runtime. With light_sleep_enable=true the kernel
// drops the CPU into light sleep whenever all tasks are blocked on
// vTaskDelay / semaphores / etc. (tickless idle) -- which is most of our
// uptime because ReadBusy() and the DHT22 retry path both sit in
// vTaskDelay.
void EnableAutoLightSleep() {
    esp_pm_config_t cfg = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = 10,  // XTAL / 4, the lowest the C3 DFS supports
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&cfg));
    ESP_LOGI(TAG, "Auto light sleep enabled (%d-%d MHz DFS)",
             cfg.min_freq_mhz, cfg.max_freq_mhz);
}

void LogWakeCause() {
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wake cause: RTC timer");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "Wake cause: cold boot / reset");
            break;
        default:
            ESP_LOGI(TAG, "Wake cause: %d", esp_sleep_get_wakeup_cause());
            break;
    }
}

// DHT22 needs a short warm-up and at least one retry in practice. Up to 3
// attempts at 2 s spacing keeps the worst case under ~6 s.
//
// Values are truncated toward zero (NOT rounded) before returning: DHT22
// datasheet accuracy is +/-0.5 C and +/-2-5 %RH, so showing the 0.x digit
// would be noise. Truncating (vs. rounding) also keeps the displayed number
// monotonic with the raw reading -- the number only ticks up when the
// sensor really crosses the next whole degree.
bool ReadSensor(int* temp_c, int* humidity_pct) {
    float t_f = 0.f;
    float h_f = 0.f;
    for (int attempt = 0; attempt < 3; ++attempt) {
        esp_err_t err = dht_read_float_data(DHT_TYPE_AM2301, DHT22_DATA_PIN,
                                            &h_f, &t_f);
        if (err == ESP_OK) {
            *temp_c = static_cast<int>(t_f);
            *humidity_pct = static_cast<int>(h_f);
            ESP_LOGI(TAG, "DHT22: %d C, %d %%RH (raw %.1f / %.1f, attempt %d)",
                     *temp_c, *humidity_pct, t_f, h_f, attempt + 1);
            return true;
        }
        ESP_LOGW(TAG, "DHT22 read failed (attempt %d): %s",
                 attempt + 1, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    return false;
}

}  // namespace

extern "C" void app_main(void) {
    LogWakeCause();
    EnableAutoLightSleep();

    EPaperDriver* epd = EPaperDriver::GetInstance();
    epd->Reset();

    lv_display_t* disp = gui::InitLvglDisplay();
    gui::CreateHumidityScreen();

    int temp_c = 0;
    int humidity_pct = 0;
    const bool ok = ReadSensor(&temp_c, &humidity_pct);
    gui::UpdateHumidityScreen(temp_c, humidity_pct, ok);

    // Drive LVGL's render + flush pipeline synchronously. flush_cb blocks
    // for the duration of the 4-color panel refresh (~5-10 s), during which
    // ReadBusy() vTaskDelay calls will permit light sleep.
    lv_refr_now(disp);

    ESP_LOGI(TAG, "Display updated; parking panel and entering deep sleep");
    epd->Sleep();
    epd->DisablePower();

    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_INTERVAL_US);
    ESP_LOGI(TAG, "Sleeping for %llu us", DEEP_SLEEP_INTERVAL_US);
    esp_deep_sleep_start();
}
