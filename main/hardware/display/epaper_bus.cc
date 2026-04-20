#include "epaper_bus.h"
#include <esp_log.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "EPaperBus"

void EPaperBus::EnablePower() {
    if (power_enabled_) {
        return;
    }

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = EPD_SDA_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = EPD_SCL_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = 4000;
    ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 4 * 1000 * 1000;
    devcfg.spics_io_num = EPD_CS_PIN;  // SPI driver toggles CS around each transaction
    devcfg.queue_size = 8;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    ESP_ERROR_CHECK(spi_bus_add_device(EPD_SPI_HOST, &devcfg, &spi_handle_));

    gpio_config_t output = {};
    output.pin_bit_mask = (1ULL << EPD_RES_PIN) | (1ULL << EPD_DC_PIN);
    output.mode = GPIO_MODE_OUTPUT;
    output.intr_type = GPIO_INTR_DISABLE;
    output.pull_down_en = GPIO_PULLDOWN_DISABLE;
    output.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&output));

    gpio_set_level(EPD_DC_PIN, 1);
    gpio_set_level(EPD_RES_PIN, 1);

    gpio_config_t input = {};
    input.pin_bit_mask = (1ULL << EPD_BUSY_PIN);
    input.mode = GPIO_MODE_INPUT;
    input.intr_type = GPIO_INTR_DISABLE;
    input.pull_down_en = GPIO_PULLDOWN_DISABLE;
    input.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&input));

    power_enabled_ = true;
    ESP_LOGI(TAG, "Display bus enabled");
}

void EPaperBus::DisablePower() {
    if (!power_enabled_) {
        return;
    }

    // The panel has already been commanded into deep sleep (cmd 0x07 / 0xA5)
    // by Jd79661Driver::Sleep() before this call. All we need to do here is
    // release the SPI driver; we deliberately do NOT touch the GPIOs after
    // that.
    //
    // Why "do nothing" is the right answer:
    //   - esp_deep_sleep_start() automatically disables every digital pad
    //     driver on ESP32-C3, so all six panel-facing lines become Hi-Z
    //     the moment we actually fall asleep -- exactly what a sleeping
    //     JD79661 wants to see on its inputs.
    //   - RES / DC are already output-HIGH from EnablePower(), so no
    //     spurious reset pulse is generated on the way into sleep.
    //   - spi_bus_free() restores CS / SCLK / MOSI to input-with-pullup,
    //     which is also fine.
    //
    // Two tempting "improvements" that made things worse in testing:
    //   1) Reconfiguring RES/DC/CS as INPUT+PULLDOWN before deep sleep.
    //      The transient LOW on RES was long enough to reset the still-
    //      sleeping panel, leaving it half-initialised and drawing ~1 mA
    //      until the next power cycle.
    //   2) Driving pins HIGH and latching with gpio_hold_en() +
    //      gpio_deep_sleep_hold_en(). That kept the ESP32-C3 IO pad
    //      domain powered through deep sleep and sourced current into the
    //      panel's input pins, resulting in ~1.5 mA.
    spi_bus_remove_device(spi_handle_);
    spi_bus_free(EPD_SPI_HOST);
    spi_handle_ = nullptr;

    power_enabled_ = false;
    ESP_LOGI(TAG, "Display bus released");
}

void EPaperBus::Reset() {
    gpio_set_level(EPD_RES_PIN, 1);
    Delay(10);
    gpio_set_level(EPD_RES_PIN, 0);
    Delay(10);
    gpio_set_level(EPD_RES_PIN, 1);
    Delay(10);
}

bool EPaperBus::ReadBus(uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return false;
    }

    spi_transaction_t t = {};
    t.rxlength = size * 8;
    t.rx_buffer = data;

    esp_err_t ret = spi_device_transmit(spi_handle_, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

void EPaperBus::Delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void EPaperBus::WriteBus(const uint8_t* data, size_t size) {
    // DMA chunking: SPI driver enforces max_transfer_sz; split large frames.
    const size_t max_chunk_size = 4000;
    size_t remaining = size;
    size_t offset = 0;

    while (remaining > 0) {
        size_t chunk_size = (remaining > max_chunk_size) ? max_chunk_size : remaining;

        spi_transaction_t t = {};
        t.length = chunk_size * 8;
        t.tx_buffer = data + offset;
        ESP_ERROR_CHECK(spi_device_transmit(spi_handle_, &t));

        remaining -= chunk_size;
        offset += chunk_size;
    }
}

void EPaperBus::WriteCommand(uint8_t command) {
    gpio_set_level(EPD_DC_PIN, 0);
    WriteBus(command);
    gpio_set_level(EPD_DC_PIN, 1);
}

void EPaperBus::WriteData(const uint8_t* data, size_t size) {
    gpio_set_level(EPD_DC_PIN, 1);
    WriteBus(data, size);
}
