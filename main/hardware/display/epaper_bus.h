#pragma once

#include <cstddef>
#include <cstdint>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "config.h"

// Low-level communication layer for the e-paper panel.
// Owns the SPI bus/device handle and the RES/DC/CS/BUSY GPIO lines.
// Shared by all concrete EPaperDriver implementations.
class EPaperBus {
public:
    EPaperBus() = default;
    ~EPaperBus() = default;

    EPaperBus(const EPaperBus&) = delete;
    EPaperBus& operator=(const EPaperBus&) = delete;

    // Power + bus lifecycle (idempotent). No PMIC on this board, so power
    // gating here is really "SPI bus + GPIO up/down".
    void EnablePower();
    void DisablePower();
    bool IsPowerEnabled() const { return power_enabled_; }

    // Hardware reset via the RES pin. Caller should EnablePower() first.
    void Reset();

    // Low-level SPI transactions.
    void WriteCommand(uint8_t command);
    void WriteData(const uint8_t* data, size_t size);
    void WriteData(uint8_t data) { WriteData(&data, 1); }
    bool ReadBus(uint8_t* data, size_t size);

    // Raw BUSY pin level. Polarity is chip-specific and handled by the driver.
    int GetBusyLevel() const { return gpio_get_level(EPD_BUSY_PIN); }

    // vTaskDelay wrapper; with tickless idle enabled, long delays drop the
    // CPU into light sleep automatically.
    static void Delay(uint32_t ms);

private:
    void WriteBus(const uint8_t* data, size_t size);
    void WriteBus(uint8_t data) { WriteBus(&data, 1); }

    spi_device_handle_t spi_handle_ = nullptr;
    bool power_enabled_ = false;
};
