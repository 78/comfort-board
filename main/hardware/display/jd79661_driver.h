#pragma once

#include "epaper_driver.h"

// 2.13" 4-color e-paper panel (122x250) driven by the JD79661 controller.
class Jd79661Driver : public EPaperDriver {
public:
    explicit Jd79661Driver(EPaperBus* bus);

    void Reset() override;
    void Clear(int color) override;
    void UpdateDisplay(std::unique_ptr<std::vector<uint8_t>> data) override;
    void Sleep() override;

    const char* name() const override { return "JD79661"; }
    uint16_t max_width() const override { return EPD_2IN13_WIDTH; }
    uint16_t max_height() const override { return EPD_2IN13_HEIGHT; }

private:
    void ReadBusy() override;
};
