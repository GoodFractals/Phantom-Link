#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "oled.h"
#include "alpha_ui.h"   // Your Alpha page array (alpha_image)
#include "numeric_ui.h" // Your Numeric page array (numeric_image)

// --- The Bounding Box Structure ---
typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
} BoxRect;

// ==========================================
// FILL IN THESE COORDINATES FROM PIXILART
// Format: {x1, y1, x2, y2}
// ==========================================

// --- ALPHA PAGE GRID (12 Buttons) ---
BoxRect alpha_boxes[4][3] = {
    // Row 0: SEND, ABC, DEF
    { 
        {0, 0, 0, 0}, // SEND
        {0, 0, 0, 0}, // ABC
        {0, 0, 0, 0}  // DEF
    },
    // Row 1: GHI, JKL, MNO
    { 
        {0, 0, 0, 0}, // GHI
        {0, 0, 0, 0}, // JKL
        {0, 0, 0, 0}  // MNO
    },
    // Row 2: PQR, STU, WXYZ
    { 
        {0, 0, 0, 0}, // PQR
        {0, 0, 0, 0}, // STU
        {0, 0, 0, 0}  // WXYZ
    },
    // Row 3: 123+, SPACE, .,?!
    { 
        {0, 0, 0, 0}, // 123+
        {0, 0, 0, 0}, // SPACE
        {0, 0, 0, 0}  // .,?!
    }
};

// --- NUMERIC PAGE GRID (6 Active Buttons) ---
// The {0,0,0,0} spots are the empty areas the joystick will skip.
BoxRect numeric_boxes[4][3] = {
    // Row 0: 123, SOS, (Empty)
    { 
        {0, 0, 0, 0}, // 123
        {0, 0, 0, 0}, // SOS
        {0, 0, 0, 0}  // Empty (Status bar area)
    },
    // Row 1: 456, SPC, (Empty)
    { 
        {0, 0, 0, 0}, // 456
        {0, 0, 0, 0}, // SPC
        {0, 0, 0, 0}  // Empty (Status bar area)
    },
    // Row 2: 789, (Empty), (Empty)
    { 
        {0, 0, 0, 0}, // 789
        {0, 0, 0, 0}, // Empty
        {0, 0, 0, 0}  // Empty
    },
    // Row 3: 0=+-, (Empty), (Empty)
    { 
        {0, 0, 0, 0}, // 0=+-
        {0, 0, 0, 0}, // Empty
        {0, 0, 0, 0}  // Empty
    }
};

// --- UI State Variables ---
int current_page = 0; // 0 = Alpha, 1 = Numeric
int cursor_col = 0; 
int cursor_row = 0; 
bool ui_active = false; 

// --- Helper to get the current box based on the page ---
BoxRect get_current_box() {
    if (current_page == 0) return alpha_boxes[cursor_row][cursor_col];
    else return numeric_boxes[cursor_row][cursor_col];
}

// --- The Spatial Navigation Logic ---
void move_cursor(int d_row, int d_col) {
    int next_row = cursor_row + d_row;
    int next_col = cursor_col + d_col;

    // Screen boundaries
    if (next_row < 0 || next_row > 3 || next_col < 0 || next_col > 2) return;

    // Check if the box actually exists (is not 0,0,0,0)
    BoxRect next_box;
    if (current_page == 0) next_box = alpha_boxes[next_row][next_col];
    else next_box = numeric_boxes[next_row][next_col];

    if (next_box.x2 > next_box.x1 && next_box.y2 > next_box.y1) {
        cursor_row = next_row;
        cursor_col = next_col;
        ui_active = true; 
    }
}

void drawMainUI(void) {
    // 1. Load the correct background
    if (current_page == 0) memcpy(oled_buffer, alpha_image, 1024);
    else memcpy(oled_buffer, numeric_image, 1024);
    
    // 2. Draw the box if active
    if (ui_active) {
        BoxRect b = get_current_box();
        if (b.x2 > b.x1 && b.y2 > b.y1) {
            int width = b.x2 - b.x1;
            int height = b.y2 - b.y1;
            // Draw box (we use -1 offset if needed from previous testing)
            oled_draw_box(b.x1, b.y1, width, height);
        }
    }
    
    // 3. Send to screen
    oled_show();
}

int main() {
    stdio_init_all();
    oled_init();
    
    adc_init();
    adc_gpio_init(26); // X
    adc_gpio_init(27); // Y

    while (true) {
        adc_select_input(0); uint16_t x = adc_read();
        adc_select_input(1); uint16_t y = adc_read();

        // Joystick Movement
        if (x > 3000) { move_cursor(0, -1); sleep_ms(200); } // Left
        if (x < 1000) { move_cursor(0, 1); sleep_ms(200); }  // Right
        if (y > 3000) { move_cursor(1, 0); sleep_ms(200); }  // Down
        if (y < 1000) { move_cursor(-1, 0); sleep_ms(200); } // Up

        // TODO: Add Button Logic Here (Page switching, typing, backspace)

        drawMainUI();
        sleep_ms(50);
    }
    return 0;
}