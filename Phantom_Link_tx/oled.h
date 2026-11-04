#ifndef OLED_H
#define OLED_H

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define PIN_CLK  18
#define PIN_MOSI 19
#define PIN_RES  20
#define PIN_DC   16
#define PIN_CS   17

// Standard 5x7 font, mapped to 8-bit bytes for easy pixel plotting
const uint8_t font5x7[][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C}, 
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, 
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40}, 
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06}, 
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, 
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63}, 
    {0x07, 0x08, 0x08, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43} 
};

uint8_t oled_buffer[1024];

void oled_write(uint8_t data, bool is_data) {
    gpio_put(PIN_DC, is_data);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_CS, 1);
}

void oled_init() {
    spi_init(SPI_PORT, 10 * 1000 * 1000);
    gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_init(PIN_RES); gpio_set_dir(PIN_RES, GPIO_OUT);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1); 

    gpio_put(PIN_RES, 1); sleep_ms(10);
    gpio_put(PIN_RES, 0); sleep_ms(10);
    gpio_put(PIN_RES, 1); sleep_ms(10);

    uint8_t init_cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
        0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    for (int i = 0; i < sizeof(init_cmds); i++) oled_write(init_cmds[i], false);
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oled_show() {
    for (int page = 0; page < 8; page++) {
        oled_write(0xB0 + page, false);
        oled_write(0x02, false); // SH1106 +2 offset
        oled_write(0x10, false);
        for (int col = 0; col < 128; col++) {
            oled_write(oled_buffer[page * 128 + col], true);
        }
    }
}

void oled_clear() {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

// --- THE PIXEL ENGINE ---
// This is the magic. It calculates exactly which byte and bit to set.
void oled_draw_pixel(int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    oled_buffer[(y / 8) * 128 + x] |= (1 << (y % 8));
}

void oled_draw_char(int x, int y, char c) {
    if (c < 'A' || c > 'Z') return;
    int index = c - 'A';
    
    for (int i = 0; i < 5; i++) {
        uint8_t col_data = font5x7[index][i];
        for (int j = 0; j < 7; j++) {
            if (col_data & (1 << j)) {
                oled_draw_pixel(x + i, y + j);
            }
        }
    }
}

void oled_draw_string(int x, int y, char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        oled_draw_char(x + (i * 6), y, text[i]);
    }
}

void oled_draw_box(int x, int y, int w, int h) {
    for(int i=x; i<x+w; i++) {
        oled_draw_pixel(i, y);
        oled_draw_pixel(i, y+h);
    }
    for(int i=y; i<=y+h; i++) {
        oled_draw_pixel(x, i);
        oled_draw_pixel(x+w, i);
    }
}

void oled_draw_bitmap(int x, int y, int w, int h, const unsigned char *bitmap) {
    int bytes_per_row = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int byte_index = row * bytes_per_row + (col / 8);
            int bit_index = 7 - (col % 8);
            if (bitmap[byte_index] & (1 << bit_index)) {
                oled_draw_pixel(x + col, y + row);
            }
        }
    }
}
#endif