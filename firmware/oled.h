#ifndef OLED_H
#define OLED_H

#include "font_ui.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define PIN_CLK  18
#define PIN_MOSI 19
#define PIN_RES  20
#define PIN_DC   16
#define PIN_CS   17

// The generated font is the UI font (48 glyphs, quote merged into one 3-px
// glyph by the Phase 2 asset fix). These asserts stop a bad regeneration from
// silently shifting every character.
_Static_assert(sizeof(font_widths) / sizeof(font_widths[0]) == 48,
               "font_widths must contain 48 glyphs");
_Static_assert(sizeof(font_offsets) / sizeof(font_offsets[0]) == 48,
               "font_offsets must contain 48 glyphs");
_Static_assert(sizeof(font_image) == 180,
               "font_image must contain 180 bytes for 48 glyphs");

// Legacy 5x7 block font. Kept for oled_draw_string(); the main UI uses the
// variable-width generated font above.
static const uint8_t font5x7[][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x08, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}
};

uint8_t oled_buffer[1024];

static void oled_write(uint8_t data, bool is_data) {
    gpio_put(PIN_DC, is_data);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_CS, 1);
}

static void oled_write_data(const uint8_t *data, size_t len) {
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, data, len);
    gpio_put(PIN_CS, 1);
}

void oled_init(void) {
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

    const uint8_t init_cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
        0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    for (size_t i = 0; i < sizeof(init_cmds); i++) oled_write(init_cmds[i], false);
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oled_show(void) {
    for (int page = 0; page < 8; page++) {
        oled_write(0xB0 + page, false);
        oled_write(0x02, false); // SH1106 +2 column offset
        oled_write(0x10, false);
        oled_write_data(&oled_buffer[page * 128], 128);
    }
}

void oled_clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oled_draw_pixel(int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    oled_buffer[(y / 8) * 128 + x] |= (uint8_t)(1u << (y % 8));
}

void oled_draw_char(int x, int y, char c) {
    if (c < 'A' || c > 'Z') return;
    int index = c - 'A';
    for (int i = 0; i < 5; i++) {
        uint8_t col_data = font5x7[index][i];
        for (int j = 0; j < 7; j++) {
            if (col_data & (1u << j)) oled_draw_pixel(x + i, y + j);
        }
    }
}

static int font_index_for_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return (c - '0') + 26;
    switch (c) {
        case '=': return 36;
        case '+': return 37;
        case '-': return 38;
        case '.': return 39;
        case ',': return 40;
        case '?': return 41;
        case '!': return 42;
        case '(': return 43;
        case ')': return 44;
        case '[': return 45;
        case ']': return 46;
        case '"': return 47;
        default:  return -1;
    }
}

// OR the glyph into the framebuffer so text does not erase the UI art.
// y is an absolute pixel row, not a page index.
void oled_draw_custom_string(int x, int y, const char *text) {
    if (!text || y < 0 || y >= 64) return;
    int page = y / 8;
    int shift = y % 8;
    int cursor_x = x;

    while (*text) {
        char ch = *text++;
        int index = font_index_for_char(ch);
        if (index < 0 || index >= font_char_count) {
            // The font has no space glyph. Every normal glyph already leaves a
            // one-pixel gap, so two more pixels give a three-pixel word gap.
            cursor_x += (ch == ' ') ? 2 : 5;
            continue;
        }

        int char_width = font_widths[index];
        int char_offset = font_offsets[index];
        for (int col = 0; col < char_width; col++) {
            int screen_x = cursor_x + col;
            if (screen_x < 0 || screen_x >= 128) continue;
            uint16_t bits = (uint16_t)font_image[char_offset + col] << shift;
            oled_buffer[page * 128 + screen_x] |= (uint8_t)(bits & 0xFF);
            if (page + 1 < 8) {
                oled_buffer[(page + 1) * 128 + screen_x] |= (uint8_t)(bits >> 8);
            }
        }
        cursor_x += char_width + 1;
    }
}

void oled_draw_string(int x, int y, const char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        oled_draw_char(x + (i * 6), y, text[i]);
    }
}

// w and h are extents: the box includes x+w and y+h.
void oled_draw_box(int x, int y, int w, int h) {
    for (int i = 0; i <= w; i++) {
        oled_draw_pixel(x + i, y);
        oled_draw_pixel(x + i, y + h);
    }
    for (int i = 0; i <= h; i++) {
        oled_draw_pixel(x, y + i);
        oled_draw_pixel(x + w, y + i);
    }
}

// w and h are dimensions: the filled area is [x, x+w) by [y, y+h).
void oled_fill_rect(int x, int y, int w, int h, bool on) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= 128 || py < 0 || py >= 64) continue;
            int page = py / 8;
            uint8_t mask = (uint8_t)(1u << (py % 8));
            if (on) oled_buffer[page * 128 + px] |= mask;
            else    oled_buffer[page * 128 + px] &= (uint8_t)~mask;
        }
    }
}

void oled_draw_bitmap(int x, int y, int w, int h, const unsigned char *bitmap) {
    int bytes_per_row = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int byte_index = row * bytes_per_row + (col / 8);
            int bit_index = 7 - (col % 8);
            if (bitmap[byte_index] & (1u << bit_index)) {
                oled_draw_pixel(x + col, y + row);
            }
        }
    }
}

#endif
