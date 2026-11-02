#ifndef OLED_H
#define OLED_H

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Pin Definitions for SPI
#define SPI_PORT spi0
#define PIN_CLK  18
#define PIN_MOSI 19
#define PIN_RES  20
#define PIN_DC   16
#define PIN_CS   17

// --- FONT ARRAY ---
const uint8_t font5x7[][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x08, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}  // Z
};

// Helper function to write command or data
void oled_write(uint8_t data, bool is_data) {
    gpio_put(PIN_DC, is_data); // DC high for data, low for command
    gpio_put(PIN_CS, 0);       // CS low to select chip
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_CS, 1);       // CS high to deselect
}

void oled_init() {
    spi_init(SPI_PORT, 10 * 1000 * 1000);
    gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_RES); gpio_set_dir(PIN_RES, GPIO_OUT);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1); // Deselect initially

    // Hardware Reset
    gpio_put(PIN_RES, 1); sleep_ms(10);
    gpio_put(PIN_RES, 0); sleep_ms(10);
    gpio_put(PIN_RES, 1); sleep_ms(10);

    // SH1106 Initialization Sequence
    uint8_t init_cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
        0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    for (int i = 0; i < sizeof(init_cmds); i++) {
        oled_write(init_cmds[i], false);
    }
}

void oled_clear() {
    for (int page = 0; page < 8; page++) {
        oled_write(0xB0 + page, false);
        oled_write(0x02, false); // SH1106 +2 offset
        oled_write(0x10, false);
        for (int col = 0; col < 128; col++) {
            oled_write(0x00, true);
        }
    }
}

void oled_print(char *text) {
    oled_clear();
    oled_write(0xB0, false); // Page 0
    oled_write(0x02, false); // SH1106 +2 offset
    oled_write(0x10, false); // Higher column

    for (int i = 0; text[i] != '\0' && i < 20; i++) {
        char c = text[i];
        if (c == ' ') {
            for(int j=0; j<6; j++) oled_write(0x00, true);
        } else if (c >= 'A' && c <= 'Z') {
            int char_index = c - 'A'; // Correctly get the row index for the letter
            for(int j=0; j<5; j++) {
                // FIXED: Use two brackets to get the specific pixel byte
                oled_write(font5x7[char_index][j], true); 
            }
            oled_write(0x00, true); // Spacing
        }
    }
}

#endif