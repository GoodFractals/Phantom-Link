 #include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "oled.h"
#include "alpha_ui.h"   
#include "numeric_ui.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"

// Button Definitions
#define BTN_SELECT 14
#define BTN_DELETE 15

// --- Phantom Link Packet Structure ---
typedef struct {
    uint16_t sync_word;  // 0xAA55
    uint8_t length;
    char payload[64];
    uint8_t crc;
} PhantomPacket;

// Simple checksum for CRC error checking
uint8_t calculate_crc(const char* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
    }
    return crc;
}

// --- The Bounding Box Structure ---
typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
} BoxRect;

// --- ALPHA PAGE GRID (12 Buttons) ---
BoxRect alpha_boxes[4][3] = {
    { {7, 21, 31, 31}, {49, 21, 72, 31}, {93, 21, 116, 31} }, // Row 0 (SEND, ABC, DEF)
    { {7, 32, 29, 42}, {49, 32, 72, 42}, {93, 32, 117, 42} }, // Row 1 (GHI, JKL, MNO)
    { {7, 43, 30, 53}, {49, 43, 73, 53}, {88, 43, 120, 53} }, // Row 2 (PQR, STU, WXYZ)
    { {7, 54, 30, 64}, {52, 55, 70, 64}, {84, 53, 124, 64} }  // Row 3 (123, SPC, .,?!)
};

// --- NUMERIC PAGE GRID (6 Active Buttons) ---
BoxRect numeric_boxes[4][3] = {
    { {7, 20, 40, 30}, {49, 20, 80, 30}, {0, 0, 0, 0} }, // Row 0 (123, SOS, Empty)
    { {7, 31, 40, 41}, {49, 31, 80, 41}, {0, 0, 0, 0} }, // Row 1 (456, SPC, Empty)
    { {7, 42, 40, 52}, {0, 0, 0, 0}, {0, 0, 0, 0} },     // Row 2 (789, Empty, Empty)
    { {7, 53, 40, 63}, {0, 0, 0, 0}, {0, 0, 0, 0} }      // Row 3 (0=+-, Empty, Empty)
};

// --- UI State Variables ---
int current_page = 0; // 0 = Alpha, 1 = Numeric
int cursor_col = 0; 
int cursor_row = 0; 
bool ui_active = true; 

// --- Text Buffer Variables ---
char current_message[64] = ""; 
int msg_length = 0;

void drawMainUI(void) {
    // 1. Load the correct background
    if (current_page == 0) memcpy(oled_buffer, alpha_image, 1024);
    else memcpy(oled_buffer, numeric_image, 1024);
    
    // 2. Draw the selection box
    if (ui_active) {
        BoxRect b = (current_page == 0) ? alpha_boxes[cursor_row][cursor_col] : numeric_boxes[cursor_row][cursor_col];
        if (b.x2 > b.x1 && b.y2 > b.y1) {
            oled_draw_box(b.x1 - 1, b.y1 - 1, b.x2 - b.x1, b.y2 - b.y1);
        }
    }

    // 3. Draw the typed message on the screen
    // Adjust the X, Y coordinates if the text overlaps the "CURR:" label
        if (msg_length > 0) {
        oled_draw_custom_string(24, 12, current_message); 
    }
    
    oled_show();
}

void move_cursor(int d_row, int d_col) {
    // --- 1. The "Left Flick Back" Logic for Page 2 ---
    // If we are on the numeric page and the user flicks left, take them back to Alpha
    if (current_page == 1 && d_col == -1) {
        current_page = 0;
        cursor_row = 0; // Reset to top left of the Alpha page
        cursor_col = 0;
        ui_active = true;
        return;
    }

    int next_row = cursor_row + d_row;
    int next_col = cursor_col + d_col;

    // --- 2. The Wrap-Around Logic ---
    // Horizontal Wrap (Left/Right)
    if (next_col < 0) next_col = 2;
    if (next_col > 2) next_col = 0;

    // Vertical Wrap (Up/Down)
    if (next_row < 0) next_row = 3;
    if (next_row > 3) next_row = 0;

    // --- 3. Check if the new box is valid ---
    BoxRect next_box = (current_page == 0) ? alpha_boxes[next_row][next_col] : numeric_boxes[next_row][next_col];

    if (next_box.x2 > next_box.x1 && next_box.y2 > next_box.y1) {
        // Box is valid, move there
        cursor_row = next_row;
        cursor_col = next_col;
        ui_active = true;
    } else {
        // Box is empty (happens a lot on the numeric page). 
        // Let's skip over the empty boxes in the same direction.
        int test_row = next_row;
        int test_col = next_col;
        int steps = 0;

        while (steps < 3) { // Max 3 columns to check
            if (d_col != 0) { // Moving horizontally
                test_col += d_col;
                if (test_col < 0) test_col = 2;
                if (test_col > 2) test_col = 0;
            } else if (d_row != 0) { // Moving vertically
                test_row += d_row;
                if (test_row < 0) test_row = 3;
                if (test_row > 3) test_row = 0;
            }

            // Check if this skipped box is valid
            BoxRect test_box = (current_page == 0) ? alpha_boxes[test_row][test_col] : numeric_boxes[test_row][test_col];
            
            if (test_box.x2 > test_box.x1 && test_box.y2 > test_box.y1) {
                cursor_row = test_row;
                cursor_col = test_col;
                ui_active = true;
                return;
            }
            steps++;
        }
    }
}

// --- Multi-Tap State Variables ---
const char* tap_groups[] = {
    "ABC", "DEF", "GHI", "JKL", "MNO", "PQR", "STU", "WXYZ", // Alpha page (0-7)
    ".,?!", // Punctuation (8)
    "123", "456", "789", "0=+\"" // Numeric page (9-12) - Note the escaped quote!
};

int active_group = -1;
int tap_count = 0;
uint32_t last_tap_time = 0;

void handle_select_button() {
    uint32_t now = time_us_32();

    // Reset tap count if it's been more than 800ms since the last press
    if (now - last_tap_time > 800000) {
        tap_count = 0;
        active_group = -1;
    }

    // --- 1. Handle Utility Buttons First ---
    if (current_page == 0) {
        if (cursor_row == 0 && cursor_col == 0) { // SEND
            // 1. Build the packet
            PhantomPacket pkt;
            pkt.sync_word = 0xAA55;
            pkt.length = msg_length;
            strncpy(pkt.payload, current_message, 64);
            pkt.crc = calculate_crc(pkt.payload, pkt.length);
            
            // 2. Print to serial (for testing)
            printf("\n--- SENDING PACKET ---\n");
            printf("Sync: 0x%04X\n", pkt.sync_word);
            printf("Length: %d\n", pkt.length);
            printf("Payload: %s\n", pkt.payload);
            printf("CRC: 0x%02X\n", pkt.crc);
            
            // 3. Clear the local UI message
            msg_length = 0;
            current_message[0] = '\0';
            return;
        }
        if (cursor_row == 3 && cursor_col == 0) { // SWITCH TO 123
            current_page = 1; cursor_row = 0; cursor_col = 0; active_group = -1; return;
        }
        if (cursor_row == 3 && cursor_col == 1) { // SPACE
            if (msg_length < 63) { current_message[msg_length++] = ' '; current_message[msg_length] = '\0'; }
            active_group = -1; return;
        }
    } else {
        if (cursor_row == 0 && cursor_col == 1) { // SOS (Switch back to ALPHA)
            current_page = 0; cursor_row = 0; cursor_col = 0; active_group = -1; return;
        }
        if (cursor_row == 1 && cursor_col == 1) { // SPACE on numeric page
            if (msg_length < 63) { current_message[msg_length++] = ' '; current_message[msg_length] = '\0'; }
            active_group = -1; return;
        }
    }

    // --- 2. Map Grid Position to Tap Group Index ---
    int group_index = -1;
    if (current_page == 0) {
        if (cursor_row == 0 && cursor_col == 1) group_index = 0; // ABC
        if (cursor_row == 0 && cursor_col == 2) group_index = 1; // DEF
        if (cursor_row == 1 && cursor_col == 0) group_index = 2; // GHI
        if (cursor_row == 1 && cursor_col == 1) group_index = 3; // JKL
        if (cursor_row == 1 && cursor_col == 2) group_index = 4; // MNO
        if (cursor_row == 2 && cursor_col == 0) group_index = 5; // PQR
        if (cursor_row == 2 && cursor_col == 1) group_index = 6; // STU
        if (cursor_row == 2 && cursor_col == 2) group_index = 7; // WXYZ
        if (cursor_row == 3 && cursor_col == 2) group_index = 8; // .,?!
    } else {
        if (cursor_row == 0 && cursor_col == 0) group_index = 9;  // 123
        if (cursor_row == 1 && cursor_col == 0) group_index = 10; // 456
        if (cursor_row == 2 && cursor_col == 0) group_index = 11; // 789
        if (cursor_row == 3 && cursor_col == 0) group_index = 12; // 0=+"
        if (cursor_row == 1 && cursor_col == 2) group_index = 13; // ()[]
    }

    // --- 3. Process Multi-Tap ---
    if (group_index != -1) {
        if (group_index != active_group) {
            tap_count = 0; 
            active_group = group_index;
        }
        
        tap_count++;
        const char* group_str = tap_groups[group_index];
        int group_len = strlen(group_str);
        
        char selected_char = group_str[(tap_count - 1) % group_len];
        
        if (msg_length < 63) {
            if (tap_count > 1 && msg_length > 0) {
                current_message[msg_length - 1] = selected_char;
            } else {
                current_message[msg_length] = selected_char;
                msg_length++;
            }
            current_message[msg_length] = '\0';
        }
        last_tap_time = now;
    }
}

void handle_delete_button() {
    if (msg_length > 0) {
        msg_length--;
        current_message[msg_length] = '\0'; // Null terminate
    }
}

int main() {
    stdio_init_all();

        // --- AP MODE (Phantom Link creates its own network) ---
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    
    cyw43_arch_enable_ap_mode("Phantom_Link", "12345678", CYW43_AUTH_WPA2_AES_PSK);
    printf("AP Active! IP: 192.168.4.1\n");

    oled_init();
    
    // Initialize Joystick
    adc_init();
    adc_gpio_init(26); // X
    adc_gpio_init(27); // Y

    // Initialize Buttons
    gpio_init(BTN_SELECT);
    gpio_set_dir(BTN_SELECT, GPIO_IN);
    gpio_pull_up(BTN_SELECT);

    gpio_init(BTN_DELETE);
    gpio_set_dir(BTN_DELETE, GPIO_IN);
    gpio_pull_up(BTN_DELETE);

    while (true) {
        // --- 1. JOYSTICK NAVIGATION ---
        adc_select_input(0); uint16_t x = adc_read();
        adc_select_input(1); uint16_t y = adc_read();

        if (x > 3000) { move_cursor(0, -1); sleep_ms(200); }
        if (x < 1000) { move_cursor(0, 1); sleep_ms(200); }
        if (y > 3000) { move_cursor(1, 0); sleep_ms(200); }
        if (y < 1000) { move_cursor(-1, 0); sleep_ms(200); }

        // --- 2. BUTTON READS ---
        if (gpio_get(BTN_SELECT) == 0) {
            handle_select_button();
            sleep_ms(50); // Debounce delay
        }

        if (gpio_get(BTN_DELETE) == 0) {
            handle_delete_button();
            sleep_ms(50); // Debounce delay
        }

        // --- 3. DRAW ---
        drawMainUI();
        sleep_ms(50);
    }
    return 0;
}