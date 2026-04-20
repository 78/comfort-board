#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "epaper_bus.h"

// 4-color palette used by JD79661-class panels. Each pixel occupies 2 bits:
//   00 -> Black, 01 -> White, 10 -> Yellow, 11 -> Red
#define EPD_BLACK  0
#define EPD_WHITE  1
#define EPD_YELLOW 2
#define EPD_RED    3

// Abstract base for all e-paper panel drivers on this board.
class EPaperDriver {
public:
    // Singleton accessor. Owns the shared EPaperBus and the concrete driver.
    static EPaperDriver* GetInstance();

    virtual ~EPaperDriver();

    // Hardware reset + chip initialization sequence (re-entrant; safe to call
    // after wake from deep sleep).
    virtual void Reset() = 0;

    // Fill the entire panel with a single palette color.
    virtual void Clear(int color) = 0;

    // Push a full framebuffer. Buffer must be exactly stride_bytes() * height()
    // and hold 2-bit-per-pixel data in the panel's native order.
    virtual void UpdateDisplay(std::unique_ptr<std::vector<uint8_t>> data) = 0;

    // Enter deep sleep (~uA, contents retained until the next Reset()).
    virtual void Sleep() = 0;

    // Panel identity and limits.
    virtual const char* name() const = 0;
    virtual uint16_t max_width() const = 0;
    virtual uint16_t max_height() const = 0;

    // Current logical resolution (may be smaller than max_*).
    uint16_t width() const { return width_; }
    uint16_t height() const { return height_; }
    uint16_t stride_bytes() const { return stride_bytes_; }

    // Applies an 8-pixel-aligned centered window inside the panel's max area.
    void SetResolution(uint16_t width, uint16_t height);
    void ResetToMaxResolution();

    // Forwarded to the shared bus so the app can power-gate the panel before
    // entering deep sleep.
    void EnablePower() { bus_->EnablePower(); }
    void DisablePower() { bus_->DisablePower(); }
    bool IsPowerEnabled() const { return bus_->IsPowerEnabled(); }

protected:
    explicit EPaperDriver(EPaperBus* bus) : bus_(bus) {}

    virtual void ReadBusy() = 0;

    EPaperBus* bus_ = nullptr;

    uint16_t width_ = 0;
    uint16_t height_ = 0;
    uint16_t start_x_ = 0;
    uint16_t start_y_ = 0;
    uint16_t aligned_width_ = 0;
    // For 4-color panels stride is aligned_width_ * 2 bits = aligned_width_/8*2 bytes.
    uint16_t stride_bytes_ = 0;
};
