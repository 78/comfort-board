#include "jd79661_driver.h"

#include <esp_log.h>
#include <cstring>
#include <vector>

#define TAG "Jd79661Driver"

Jd79661Driver::Jd79661Driver(EPaperBus* bus) : EPaperDriver(bus) {
    ResetToMaxResolution();
}

void Jd79661Driver::ReadBusy() {
    // Controller pulls BUSY low while occupied; poll with tickless-idle-friendly
    // delays so the CPU drops into light sleep between checks.
    while (bus_->GetBusyLevel() == 0) {
        EPaperBus::Delay(10);
    }
}

void Jd79661Driver::Reset() {
    bus_->Reset();
    ReadBusy();

    // Register sequence reproduced verbatim from the reference Ep2in13ry driver.
    bus_->WriteCommand(0x4D);
    bus_->WriteData(0x78);

    bus_->WriteCommand(0x00);
    bus_->WriteData(0x0F);
    bus_->WriteData(0x09);

    bus_->WriteCommand(0x01);
    bus_->WriteData(0x07);
    bus_->WriteData(0x00);
    bus_->WriteData(0x22);
    bus_->WriteData(0x78);
    bus_->WriteData(0x0A);
    bus_->WriteData(0x22);

    bus_->WriteCommand(0x03);
    bus_->WriteData(0x10);
    bus_->WriteData(0x54);
    bus_->WriteData(0x44);

    bus_->WriteCommand(0x06);
    bus_->WriteData(0x0F);
    bus_->WriteData(0x0A);
    bus_->WriteData(0x2F);
    bus_->WriteData(0x25);
    bus_->WriteData(0x22);
    bus_->WriteData(0x2E);
    bus_->WriteData(0x21);

    bus_->WriteCommand(0x30);  // Frame rate from OTP
    bus_->WriteData(0x02);

    bus_->WriteCommand(0x41);
    bus_->WriteData(0x00);

    bus_->WriteCommand(0x50);
    bus_->WriteData(0x37);

    bus_->WriteCommand(0x60);
    bus_->WriteData(0x02);
    bus_->WriteData(0x02);

    bus_->WriteCommand(0x61);
    bus_->WriteData(aligned_width_ / 256);
    bus_->WriteData(aligned_width_ % 256);
    bus_->WriteData(height_ / 256);
    bus_->WriteData(height_ % 256);

    bus_->WriteCommand(0x65);
    bus_->WriteData(start_x_ >> 8);
    bus_->WriteData(start_x_ & 0xFF);
    bus_->WriteData(start_y_ >> 8);
    bus_->WriteData(start_y_ & 0xFF);

    bus_->WriteCommand(0xE7);
    bus_->WriteData(0x1C);

    bus_->WriteCommand(0xE3);
    bus_->WriteData(0x22);

    bus_->WriteCommand(0xE0);
    bus_->WriteData(0x00);

    bus_->WriteCommand(0xB4);
    bus_->WriteData(0xD0);

    bus_->WriteCommand(0xB5);
    bus_->WriteData(0x03);

    bus_->WriteCommand(0xE9);
    bus_->WriteData(0x01);
}

void Jd79661Driver::Clear(int color) {
    uint8_t byte;
    switch (color) {
        case EPD_BLACK:  byte = 0x00; break;
        case EPD_WHITE:  byte = 0x55; break;
        case EPD_YELLOW: byte = 0xAA; break;
        case EPD_RED:    byte = 0xFF; break;
        default:
            ESP_LOGE(TAG, "Unknown color %d, defaulting to white", color);
            byte = 0x55;
            break;
    }

    std::vector<uint8_t> line(stride_bytes_, byte);

    bus_->WriteCommand(0x10);
    for (int i = 0; i < height_; i++) {
        bus_->WriteData(line.data(), line.size());
    }

    bus_->WriteCommand(0x04);  // Power on
    ReadBusy();
    bus_->WriteCommand(0x12);  // Display refresh
    bus_->WriteData(0x00);
    ReadBusy();
}

void Jd79661Driver::UpdateDisplay(std::unique_ptr<std::vector<uint8_t>> data) {
    const size_t expected = static_cast<size_t>(stride_bytes_) * height_;
    if (!data || data->size() != expected) {
        ESP_LOGE(TAG, "Invalid buffer size: %zu (expected %zu)",
                 data ? data->size() : 0, expected);
        return;
    }

    bus_->WriteCommand(0x10);
    bus_->WriteData(data->data(), data->size());

    bus_->WriteCommand(0x04);  // Power on
    ReadBusy();
    bus_->WriteCommand(0x12);  // Display refresh
    bus_->WriteData(0x00);
    ReadBusy();
}

void Jd79661Driver::Sleep() {
    bus_->WriteCommand(0x02);  // Power off
    bus_->WriteData(0x00);
    ReadBusy();

    bus_->WriteCommand(0x07);  // Deep sleep
    bus_->WriteData(0xA5);
}
