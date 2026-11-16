 #include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "oled.h"
#include "alpha_ui.h"
#include "numeric_ui.h"

// --- NETWORK ROLE ---
// SET TO 1 FOR THE JANK BOX (AP)
// SET TO 0 FOR THE RX PICO (STA)
#define IS_AP 0 

#define BTN_SELECT 14
#define BTN_DELETE 15

// --- PACKET STRUCTURE ---
typedef struct {
    uint16_t sync_word;
    uint8_t length;
    char payload[64];
    uint8_t crc;
} PhantomPacket;

uint8_t calculate_crc(const char* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) crc ^= data[i];
    return crc;
}

// --- SFO LOGIC ---
const char* dummy_packets[] = {"PING", "ACK", "SYNC", "PONG", "DATA", "READY", "END"};
struct udp_pcb *upcb;

// --- UI VARIABLES ---
typedef struct { int x1, y1, x2, y2; } BoxRect;

BoxRect alpha_boxes[4][3] = {
    { {7, 21, 31, 31}, {49, 21, 72, 31}, {93, 21, 116, 31} },
    { {7, 32, 29, 42}, {49, 32, 72, 42}, {93, 32, 117, 42} },
    { {7, 43, 30, 53}, {49, 43, 73, 53}, {88, 43, 120, 53} },
    { {7, 54, 30, 64}, {52, 55, 70, 64}, {84, 53, 124, 64} }
};
BoxRect numeric_boxes[4][3] = {
    { {7, 20, 40, 30}, {49, 20, 80, 30}, {0, 0, 0, 0} },
    { {7, 31, 40, 41}, {49, 31, 80, 41}, {0, 0, 0, 0} },
    { {7, 42, 40, 52}, {0, 0, 0, 0}, {0, 0, 0, 0} },
    { {7, 53, 40, 63}, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

int current_page = 0; 
int cursor_col = 0; 
int cursor_row = 0; 
bool ui_active = true; 
char current_message[64] = ""; 
int msg_length = 0;

const char* tap_groups[] = {
    "ABC", "DEF", "GHI", "JKL", "MNO", "PQR", "STU", "WXYZ",
    ".,?!", "123", "456", "789", "0=+\"", "()[]"
};
int active_group = -1;
int tap_count = 0;
uint32_t last_tap_time = 0;
char rx_buffer[64] = "";
int rx_index = 0;

// --- UDP FUNCTIONS ---
void udp_send_packet(const char *msg) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(msg), PBUF_RAM);
    if (!p) return;
    memcpy(p->payload, msg, strlen(msg));
    
    ip_addr_t dest_ip;
    IP4_ADDR(&dest_ip, 192, 168, 4, 1); // Broadcast to both Picos
    
    udp_sendto(upcb, p, &dest_ip, 4444);
    pbuf_free(p);
}

static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        char *payload = (char *)p->payload;
        int packet_id = -1;
        
        for (int i = 0; i < 7; i++) {
            if (strncmp(payload, dummy_packets[i], p->len) == 0) {
                packet_id = i;
                break;
            }
        }
              static int high_val = -1;
        
        if (packet_id >= 0 && packet_id < 6) {
            if (high_val == -1) {
                high_val = packet_id; // Store high digit
            } else {
                int low_val = packet_id; // Got low digit
                char decoded_char = 'A' + (high_val * 6 + low_val);
                if (rx_index < 63) {
                    rx_buffer[rx_index++] = decoded_char;
                    rx_buffer[rx_index] = '\0';
                }
                high_val = -1; // Reset for next character
            }
        } else if (packet_id == 6) { // END packet
            high_val = -1; // Reset state
            strcpy(current_message, rx_buffer);
            msg_length = rx_index;
            printf("\n!!! SOS SIGNAL RECEIVED !!!\n");
            rx_index = 0;
            rx_buffer[0] = '\0';
        }
        pbuf_free(p);
    }
}

// --- UI FUNCTIONS ---
void drawMainUI(void) {
    if (current_page == 0) memcpy(oled_buffer, alpha_image, 1024);
    else memcpy(oled_buffer, numeric_image, 1024);
    
    if (ui_active) {
        BoxRect b = (current_page == 0) ? alpha_boxes[cursor_row][cursor_col] : numeric_boxes[cursor_row][cursor_col];
        if (b.x2 > b.x1 && b.y2 > b.y1) {
            oled_draw_box(b.x1 - 1, b.y1 - 1, b.x2 - b.x1, b.y2 - b.y1);
        }
    }

    if (msg_length > 0) {
        oled_draw_custom_string(0, 30, current_message); // Top Left UI
    }
    oled_show();
}

void move_cursor(int d_row, int d_col) {
    if (current_page == 1 && d_col == -1) { current_page = 0; cursor_row = 0; cursor_col = 0; return; }
    int next_row = (cursor_row + d_row + 4) % 4;
    int next_col = (cursor_col + d_col + 3) % 3;
    
    BoxRect next_box = (current_page == 0) ? alpha_boxes[next_row][next_col] : numeric_boxes[next_row][next_col];
    if (next_box.x2 > next_box.x1 && next_box.y2 > next_box.y1) {
        cursor_row = next_row;
        cursor_col = next_col;
    }
}

void handle_select_button() {
    uint32_t now = time_us_32();
    if (now - last_tap_time > 800000) { tap_count = 0; active_group = -1; }

    if (current_page == 0) {
        if (cursor_row == 0 && cursor_col == 0) { // SEND
            printf("\n--- INITIATING BONNIE & CLYDE PROTOCOL ---\n");
            for (int i = 0; i < msg_length; i++) {
                int char_index = current_message[i] - 'A'; 
                if (char_index < 0 || char_index > 25) char_index = 0; 
                
                // Split the character into two base-6 digits
                int high = char_index / 6;
                int low = char_index % 6;
                
                // Send high packet
                printf("TX High: %s\n", dummy_packets[high]);
                udp_send_packet(dummy_packets[high]);
                sleep_ms(100);
                
                // Send low packet
                printf("TX Low: %s\n", dummy_packets[low]);
                udp_send_packet(dummy_packets[low]);
                sleep_ms(100);
            }
            
            // Send the terminator so the RX knows the message is done
            printf("TX Packet: END\n");
            udp_send_packet(dummy_packets[6]);
            sleep_ms(100);
            
            // Clear the local UI message
            msg_length = 0; 
            current_message[0] = '\0';
            return;
        } // <--- THIS CLOSES THE SEND BLOCK
        
        if (cursor_row == 3 && cursor_col == 0) { current_page = 1; cursor_row = 0; cursor_col = 0; return; }
        if (cursor_row == 3 && cursor_col == 1) { if (msg_length < 63) { current_message[msg_length++] = ' '; current_message[msg_length] = '\0'; } return; }
    } else {
        if (cursor_row == 0 && cursor_col == 1) { current_page = 0; cursor_row = 0; cursor_col = 0; return; }
        if (cursor_row == 1 && cursor_col == 1) { if (msg_length < 63) { current_message[msg_length++] = ' '; current_message[msg_length] = '\0'; } return; }
    }

    int group_index = -1;
    if (current_page == 0) {
        if (cursor_row == 0 && cursor_col == 1) group_index = 0; if (cursor_row == 0 && cursor_col == 2) group_index = 1;
        if (cursor_row == 1 && cursor_col == 0) group_index = 2; if (cursor_row == 1 && cursor_col == 1) group_index = 3;
        if (cursor_row == 1 && cursor_col == 2) group_index = 4; if (cursor_row == 2 && cursor_col == 0) group_index = 5;
        if (cursor_row == 2 && cursor_col == 1) group_index = 6; if (cursor_row == 2 && cursor_col == 2) group_index = 7;
        if (cursor_row == 3 && cursor_col == 2) group_index = 8;
    } else {
        if (cursor_row == 0 && cursor_col == 0) group_index = 9; if (cursor_row == 1 && cursor_col == 0) group_index = 10;
        if (cursor_row == 2 && cursor_col == 0) group_index = 11; if (cursor_row == 3 && cursor_col == 0) group_index = 12;
        if (cursor_row == 1 && cursor_col == 2) group_index = 13;
    }

    if (group_index != -1) {
        if (group_index != active_group) { tap_count = 0; active_group = group_index; }
        tap_count++;
        const char* group_str = tap_groups[group_index];
        int group_len = strlen(group_str);
        char selected_char = group_str[(tap_count - 1) % group_len];
        
        if (msg_length < 63) {
            if (tap_count > 1 && msg_length > 0) { current_message[msg_length - 1] = selected_char; } 
            else { current_message[msg_length] = selected_char; msg_length++; }
            current_message[msg_length] = '\0';
        }
        last_tap_time = now;
    }
}

void handle_delete_button() {
    if (msg_length > 0) { msg_length--; current_message[msg_length] = '\0'; }
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Give Windows 2 seconds to open the COM port

    // 1. Initialize the screen
    oled_init();
    oled_clear();
    oled_show();
    sleep_ms(500);

    printf("\n\n--- BOOTING PHANTOM LINK ---\n");
    
    // 2. Draw the UI FIRST so we have visual feedback
    drawMainUI();
    oled_show();

    // 3. Initialize Wi-Fi (ONCE)
    printf("--- REACHED WIFI INIT ---\n");
    stdio_flush();
    
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) { 
        printf("Wi-Fi init failed\n"); 
        return -1; 
    }
    printf("CYW43 Core Initialized.\n");
    stdio_flush();
    sleep_ms(1000); // Let the driver breathe

#if IS_AP
    cyw43_arch_enable_ap_mode("Phantom_Link", "12345678", CYW43_AUTH_WPA2_AES_PSK);
    printf("AP Active! IP: 192.168.4.1\n");
    stdio_flush();
#else
    cyw43_arch_enable_sta_mode();
    printf("Connecting to Phantom_Link (Async)...\n");
    stdio_flush();
    
    cyw43_arch_wifi_connect_async("Phantom_Link", "12345678", CYW43_AUTH_WPA2_AES_PSK);
    
    int timeout = 30; 
    while (timeout > 0) {
        cyw43_arch_poll(); // Keep the Wi-Fi alive
        
        if (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN) {
            printf("STA Connected! IP: 192.168.4.2\n");
            stdio_flush();
            break;
        }
        sleep_ms(100);
        timeout--;
    }
    
    if (timeout == 0) {
        printf("Failed to connect. Running offline mode.\n");
        stdio_flush();
    }
#endif

     // 4. Initialize UDP
    printf("Survived WiFi. Before UDP Init...\n");
    stdio_flush();
    upcb = udp_new();
    udp_bind(upcb, IP_ADDR_ANY, 4444);
    udp_recv(upcb, udp_recv_callback, NULL);
    printf("UDP Initialized!\n");
    stdio_flush();

         // --- RE-INITIALIZE ADC & BUTTONS AFTER WIFI ---
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);

    // Re-initialize buttons and force internal pull-ups
    gpio_init(BTN_SELECT);
    gpio_set_dir(BTN_SELECT, GPIO_IN);
    gpio_pull_up(BTN_SELECT);

    gpio_init(BTN_DELETE);
    gpio_set_dir(BTN_DELETE, GPIO_IN);
    gpio_pull_up(BTN_DELETE);

    // 5. Main Loop
    while (true) {
        // printf("Loop Start...\n"); // Commented out so it doesn't spam
        // stdio_flush();
        
        cyw43_arch_poll(); // Keep Wi-Fi alive
        
        sleep_ms(10); // CRITICAL: Let the Wi-Fi breathe before reading ADC

        // Joystick Logic (RE-ENABLED)
        adc_select_input(0); uint16_t x = adc_read();
        adc_select_input(1); uint16_t y = adc_read();
        
        // Debug print (Uncomment this if the joystick still doesn't move)
        // printf("X: %d, Y: %d\n", x, y); 
        // stdio_flush();

        if (x > 3000) { move_cursor(0, -1); sleep_ms(200); }
        if (x < 1000) { move_cursor(0, 1); sleep_ms(200); }
        if (y > 3000) { move_cursor(1, 0); sleep_ms(200); }
        if (y < 1000) { move_cursor(-1, 0); sleep_ms(200); }

               // Button Logic with DEBUG PRINTS
        if (gpio_get(BTN_SELECT) == 0) { 
            printf("SELECT PRESSED!\n"); 
            stdio_flush();
            handle_select_button(); 
            sleep_ms(200); // Long debounce to prevent double triggers
        }
        if (gpio_get(BTN_DELETE) == 0) { 
            printf("DELETE PRESSED!\n"); 
            stdio_flush();
            handle_delete_button(); 
            sleep_ms(200); 
        }

        // Draw UI
        drawMainUI();
        sleep_ms(50);
    } // <-- CLOSES WHILE LOOP
} // <-- CLOSES INT MAIN()