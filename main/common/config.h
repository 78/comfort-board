/*
 * Configuration - project-wide hardware/GPIO definitions for comfort-board.
 *
 * Target MCU: ESP32-C3
 * Peripherals: JD79661 2.13" 4-color e-paper (122x250), DHT22 1-wire sensor.
 */

#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>

// ==================== EPaper (JD79661, 2.13" 4-color) ====================

// Native panel resolution (portrait)
#define EPD_2IN13_WIDTH  122
#define EPD_2IN13_HEIGHT 250

// GPIO assignments on ESP32-C3.
// Avoids strapping pins (2/8/9), USB-JTAG (18/19) and UART0 (20/21).
#define EPD_BUSY_PIN GPIO_NUM_3
#define EPD_RES_PIN  GPIO_NUM_4
#define EPD_DC_PIN   GPIO_NUM_5
#define EPD_CS_PIN   GPIO_NUM_6
#define EPD_SCL_PIN  GPIO_NUM_7
#define EPD_SDA_PIN  GPIO_NUM_10

#define EPD_SPI_HOST SPI2_HOST

// ==================== DHT22 ====================

// DHT22 single-wire data line. GPIO0 is a strapping pin but safe to use for
// a pulled-up 1-Wire-style sensor after boot.
#define DHT22_DATA_PIN GPIO_NUM_0

// ==================== Deep sleep policy ====================

// Wake every hour to take a new measurement.
#define DEEP_SLEEP_INTERVAL_US (3600ULL * 1000ULL * 1000ULL)
