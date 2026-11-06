#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "oled.h"
#include "alpha_ui.h" 

// --- The Bounding Box Structure ---
typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
} BoxRect;

// --- THE MASTER GRID ---
BoxRect keypad_boxes[4][3] = {
    // Row 0
    { 
        {8, 21, 27, 31}, // SOS (Your perfect coordinates!)
        {49, 21, 72, 31},    // ABC (Empty)
        {93, 21, 116, 31}     // DEF (Empty)
    },
    // Row 1
    { 
        {7, 32, 29, 42},    // GHI
        {49, 32, 72, 42},    // JKL
        {93, 32, 117, 42}     // MNO
    },
    // Row 2
    { 
        {7, 43, 30, 53},    // PQR
        {49, 43, 73, 53},    // STU
        {88, 43, 120, 53}     // WXYZ
    },
    // Row 3
    { 
        {7, 54, 30, 64},    // 123
        {52, 55, 70, 64},    // SPC
        {84,53,124,64}     // .,?!
    }
};

// --- Grid Variables ---
int cursor_col = 0; 
int cursor_row = 0; 
bool ui_active = false; // NEW: Keeps the screen blank until you touch the stick

// --- NEW: The Organic Movement Logic ---
void move_cursor(int d_row, int d_col) {
    int next_row = cursor_row + d_row;
    int next_col = cursor_col + d_col;

    // 1. Check screen boundaries
    if (next_row < 0 || next_row > 3 || next_col < 0 || next_col > 2) return;

    // 2. Check if the box actually exists (is not 0,0,0,0)
    BoxRect next_box = keypad_boxes[next_row][next_col];
    if (next_box.x2 > next_box.x1 && next_box.y2 > next_box.y1) {
        // It exists! Move the cursor.
        cursor_row = next_row;
        cursor_col = next_col;
        ui_active = true; // Wake up the UI
    }
}

void drawMainUI(void) {
    // 1. Wipe screen with the background image
    memcpy(oled_buffer, alpha_image, 1024);
    
    // 2. Get the current box
    BoxRect b = keypad_boxes[cursor_row][cursor_col];
    
    // 3. THE GLOBAL OFFSET HACK
    // Shift the entire box 1 pixel left and 1 pixel up
    int offset_x = -1;
    int offset_y = -1;
    
    // 4. Only draw if the coordinates have been filled in
    if (b.x2 > b.x1 && b.y2 > b.y1) {
        int width = b.x2 - b.x1;
        int height = b.y2 - b.y1;
        
        // Apply the offset when drawing
        oled_draw_box(b.x1 + offset_x, b.y1 + offset_y, width, height);
    }
    
    // 5. Send to screen
    oled_show();
}

int main() {
    stdio_init_all();
    oled_init();
    
    adc_init();
    adc_gpio_init(26); // X
    adc_gpio_init(27); // Y

    while (true) {
        // --- 1. JOYSTICK MOVEMENT ---
        adc_select_input(0); uint16_t x = adc_read();
        adc_select_input(1); uint16_t y = adc_read();

        // Use the new organic movement function
        if (x > 3000) { move_cursor(0, -1); sleep_ms(200); } // Left
        if (x < 1000) { move_cursor(0, 1); sleep_ms(200); }  // Right
        if (y > 3000) { move_cursor(1, 0); sleep_ms(200); }  // Down
        if (y < 1000) { move_cursor(-1, 0); sleep_ms(200); } // Up

        // --- 2. DRAW SCREEN ---
        drawMainUI();
        sleep_ms(50);
    }
    return 0;
}