/*
 * Phantom Link - unified AP/STA firmware
 *
 * One source, two build targets:
 *   phantom_link_ap  (IS_AP=1) - hosts the private Wi-Fi network
 *   phantom_link_sta (IS_AP=0) - joins it and initiates HELLO
 *
 * IS_AP selects only the Wi-Fi role. The OLED UI, ADC joystick, buttons,
 * codec, parser, and transmit/receive paths are unconditional and identical
 * in both builds. Both boards type, send, receive, decode, and render.
 * Encryption is deliberately not implemented yet.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"

#include "oled.h"
#include "alpha_ui.h"
#include "numeric_ui.h"
#include "boot_ui.h"

#ifndef IS_AP
#define IS_AP 1
#endif

// The compile-time flag selects only the Wi-Fi role. Every other role-dependent
// decision reads this constant at runtime, so the UI and input path are never
// compiled out of either target.
static const bool is_ap = (IS_AP != 0);

#define BTN_SELECT 14
#define BTN_DELETE 15

#define WIFI_SSID "Phantom_Link"
#define WIFI_PASSWORD "12345678"
#define UDP_PORT 4444

#define WIFI_RETRY_MS 5000
#define HELLO_INTERVAL_MS 1000
#define HEARTBEAT_MS 1000
#define LINK_TIMEOUT_MS 3000
#define SYMBOL_INTERVAL_MS 80
#define SYMBOL_GAP_MS 500
#define TAP_TIMEOUT_MS 800
#define JOY_REPEAT_MS 220
#define JOY_LOW 1000
#define JOY_HIGH 3000
#define BUTTON_DEBOUNCE_MS 20
#define MAX_TEXT_CHARS 21

// --- Base-6 transport alphabet ------------------------------------------------
// 49 codes: 48 drawable font glyphs plus space. Space is code 0.
// The font itself has no space glyph; the renderer advances five pixels for it.
static const char GLYPHS[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789=+-.,?!()[]\"";
_Static_assert(sizeof(GLYPHS) - 1 == 49, "transport alphabet must have 49 codes");

static const char *const SYMBOLS[6] = {
    "PING", "ACK", "SYNC", "PONG", "DATA", "READY"
};

#define TOK_HELLO     "HELLO"
#define TOK_HELLO_ACK "HELLO_ACK"
#define TOK_LINK_PING "LINK_PING"
#define TOK_LINK_PONG "LINK_PONG"
#define TOK_END       "END"

// --- UI layout ---------------------------------------------------------------
typedef struct { int x1, y1, x2, y2; } BoxRect;

static const BoxRect alpha_boxes[4][3] = {
    { {7, 21, 31, 31}, {49, 21, 72, 31}, {93, 21, 116, 31} },
    { {7, 32, 29, 42}, {49, 32, 72, 42}, {93, 32, 117, 42} },
    { {7, 43, 30, 53}, {49, 43, 73, 53}, {88, 43, 120, 53} },
    { {7, 54, 30, 64}, {52, 55, 70, 64}, {84, 53, 124, 64} }
};

// Only non-zero rectangles are selectable. [0][2] is the [ . ] indicator and
// [2][2] is BAT:. The numeric bottom-right [3][2] is the LINK indicator, not a
// punctuation key; the alpha page is the only page with a punctuation key.
static const BoxRect numeric_boxes[4][3] = {
    { {7, 20, 40, 30}, {49, 20, 80, 30}, {0, 0, 0, 0} },       // 123   IN RANGE?   [ . ]
    { {7, 31, 40, 41}, {0, 0, 0, 0},     {0, 0, 0, 0} },       // 456
    { {7, 42, 40, 52}, {49, 42, 80, 52}, {0, 0, 0, 0} },       // 789   SOS         BAT:
    { {7, 53, 40, 63}, {52, 55, 70, 64}, {0, 0, 0, 0} }        // 0=+-  SPACE       LINK
};

// Group 8 is the alpha punctuation key. The numeric page has no punctuation
// key: its bottom-right cell is the LINK indicator. The baked alpha label says
// "WXYZ", so W is the first tap and V is the undocumented fifth tap; this keeps
// every A-Z letter typeable without rewriting alpha.png/alpha_ui.h.
static const char *const tap_groups[] = {
    "ABC", "DEF", "GHI", "JKL", "MNO", "PQR", "STU", "WXYZV",
    ".,()[]?!", "123", "456", "789", "0=+-\""
};

static int current_page = 0;
static int cursor_row = 0;
static int cursor_col = 0;
static bool ui_active = true;
static char current_message[64] = "";
static int msg_length = 0;
static int active_group = -1;
static int tap_count = 0;
static uint32_t last_tap_ms = 0;

// --- Received message ---------------------------------------------------------
// Kept separate from current_message so an inbound packet cannot overwrite a
// draft while the user is typing.
static char rx_message[64] = "";
static int rx_message_len = 0;
static char rx_buffer[64] = "";
static int rx_index = 0;
static int sym_acc = 0;
static int sym_count = 0;
static uint32_t last_symbol_ms = 0;

// --- Link state ---------------------------------------------------------------
typedef enum { LINK_DOWN = 0, LINK_UP } link_state_t;

static struct udp_pcb *upcb = NULL;
static ip_addr_t ap_addr;
static ip_addr_t peer_ip;
static u16_t peer_port = UDP_PORT;
static bool peer_valid = false;
static bool wifi_ready = false;
static uint32_t last_wifi_retry_ms = 0;
static link_state_t link_state = LINK_DOWN;
static uint32_t last_heard_ms = 0;
static uint32_t last_hello_ms = 0;
static uint32_t last_ping_ms = 0;
static bool manual_ping = false;
static uint32_t manual_ping_ms = 0;

// --- Transmit state machine ---------------------------------------------------
typedef enum { TX_IDLE = 0, TX_DATA, TX_END } tx_phase_t;

static tx_phase_t tx_phase = TX_IDLE;
static char tx_buf[64];
static int tx_len = 0;
static int tx_pos = 0;
static int tx_digit = 0;
static uint32_t tx_next_ms = 0;
static uint8_t tx_msg_id = 0;

// --- Time and codec -----------------------------------------------------------
static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static int code_from_glyph(char c) {
    if (c == '\0') return -1;
    const char *p = strchr(GLYPHS, c);
    return p ? (int)(p - GLYPHS) : -1;
}

static char glyph_from_code(int code) {
    if (code < 0 || code >= (int)sizeof(GLYPHS) - 1) return '?';
    return GLYPHS[code];
}

// --- UDP send / peer learning -------------------------------------------------
static err_t send_to(const ip_addr_t *dst, u16_t port, const char *text) {
    if (!upcb || !dst) return ERR_VAL;

    size_t len = strlen(text);
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
    if (!p) {
        printf("send: pbuf_alloc failed for '%s'\n", text);
        return ERR_MEM;
    }

    memcpy(p->payload, text, len);
    err_t err = udp_sendto(upcb, p, dst, port);
    pbuf_free(p);

    if (err != ERR_OK) {
        printf("send '%s' failed: %d\n", text, (int)err);
    }
    return err;
}

static bool send_control(const char *token) {
    if (!upcb) return false;

    if (peer_valid) {
        return send_to(&peer_ip, peer_port, token) == ERR_OK;
    }

    if (!is_ap) {
        // The STA knows the AP's default address before HELLO_ACK arrives.
        return send_to(&ap_addr, UDP_PORT, token) == ERR_OK;
    }

    printf("no peer yet; cannot send %s\n", token);
    return false;
}

static void note_peer(const ip_addr_t *addr, u16_t port) {
    if (addr && !ip_addr_isany(addr)) {
        peer_ip = *addr;
        peer_port = port;
        peer_valid = true;
    }
    last_heard_ms = now_ms();
}

static void set_link_up(const char *why) {
    uint32_t now = now_ms();
    last_heard_ms = now;

    if (link_state == LINK_UP) return;

    link_state = LINK_UP;
    last_ping_ms = now;
    last_hello_ms = now;
    ui_active = true;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    if (peer_valid) {
        printf("LINK UP (%s) peer=%s:%u\n", why, ipaddr_ntoa(&peer_ip),
               (unsigned)peer_port);
    } else {
        printf("LINK UP (%s)\n", why);
    }
}

static void set_link_down(const char *why) {
    if (link_state == LINK_DOWN) return;

    link_state = LINK_DOWN;
    peer_valid = false;
    manual_ping = false;
    sym_acc = 0;
    sym_count = 0;
    rx_index = 0;
    rx_buffer[0] = '\0';
    ui_active = true;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    if (tx_phase != TX_IDLE) {
        tx_phase = TX_IDLE;
        tx_pos = 0;
        tx_digit = 0;
        printf("TX aborted: no link\n");
    }

    printf("LINK LOST (%s)\n", why);
}

// --- Receive path -------------------------------------------------------------
static bool pkt_eq(const struct pbuf *p, const char *token) {
    size_t len = strlen(token);
    if (len >= 16 || p->tot_len != len) return false;

    char buf[16];
    if (pbuf_copy_partial(p, buf, (u16_t)len, 0) != (u16_t)len) return false;
    return memcmp(buf, token, len) == 0;
}

static int symbol_id(const struct pbuf *p) {
    for (int i = 0; i < 6; i++) {
        if (pkt_eq(p, SYMBOLS[i])) return i;
    }
    return -1;
}

static void reset_rx_accumulator(void) {
    sym_acc = 0;
    sym_count = 0;
}

static void finish_rx_message(void) {
    if (sym_count != 0) {
        printf("RX END with partial glyph (%d symbol%s); dropping it\n",
               sym_count, sym_count == 1 ? "" : "s");
        reset_rx_accumulator();
    }

    if (rx_index > 0) {
        rx_buffer[rx_index] = '\0';
        memcpy(rx_message, rx_buffer, (size_t)rx_index + 1);
        rx_message_len = rx_index;

        if (strcmp(rx_message, "SOS") == 0) {
            printf("\n!!! SOS SIGNAL RECEIVED !!!\n");
        } else {
            printf("\nSecret Msg: %s\n", rx_message);
        }
    } else {
        printf("RX END with no data; ignored\n");
    }

    rx_index = 0;
    rx_buffer[0] = '\0';
    reset_rx_accumulator();
}

static void handle_symbol(int id) {
    uint32_t now = now_ms();

    if ((uint32_t)(now - last_symbol_ms) > SYMBOL_GAP_MS) {
        if (sym_count > 0) printf("RX gap: Base-6 accumulator reset\n");
        reset_rx_accumulator();
    }
    last_symbol_ms = now;

    sym_acc = sym_acc * 6 + id;
    if (++sym_count < 3) return;

    char c = glyph_from_code(sym_acc);
    if (rx_index < (int)sizeof(rx_buffer) - 1) {
        rx_buffer[rx_index++] = c;
        rx_buffer[rx_index] = '\0';
    }
    reset_rx_accumulator();
}

static void handle_control(const char *token, const ip_addr_t *addr, u16_t port) {
    note_peer(addr, port);

    if (strcmp(token, TOK_HELLO) == 0) {
        printf("RX HELLO\n");
        if (send_control(TOK_HELLO_ACK)) {
            set_link_up("HELLO");
        } else {
            printf("HELLO_ACK send failed; link stays down\n");
        }
    } else if (strcmp(token, TOK_HELLO_ACK) == 0) {
        printf("RX HELLO_ACK\n");
        set_link_up("HELLO_ACK");
    } else if (strcmp(token, TOK_LINK_PING) == 0) {
        bool pong_sent = send_control(TOK_LINK_PONG);
        if (link_state != LINK_UP && pong_sent) set_link_up("LINK_PING");
    } else if (strcmp(token, TOK_LINK_PONG) == 0) {
        if (manual_ping) {
            printf("RTT: %u ms\n", (unsigned)(now_ms() - manual_ping_ms));
            manual_ping = false;
        }
        if (link_state != LINK_UP) set_link_up("LINK_PONG");
    }
}

static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;

    if (!p) return;

    if (pkt_eq(p, TOK_HELLO)) {
        handle_control(TOK_HELLO, addr, port);
    } else if (pkt_eq(p, TOK_HELLO_ACK)) {
        handle_control(TOK_HELLO_ACK, addr, port);
    } else if (pkt_eq(p, TOK_LINK_PING)) {
        handle_control(TOK_LINK_PING, addr, port);
    } else if (pkt_eq(p, TOK_LINK_PONG)) {
        handle_control(TOK_LINK_PONG, addr, port);
    } else if (pkt_eq(p, TOK_END)) {
        if (link_state == LINK_UP) {
            note_peer(addr, port);
            finish_rx_message();
        }
    } else {
        int id = symbol_id(p);
        if (id >= 0) {
            if (link_state == LINK_UP) {
                note_peer(addr, port);
                handle_symbol(id);
            }
        } else {
            printf("RX unknown packet (%u bytes)\n", (unsigned)p->tot_len);
        }
    }

    pbuf_free(p);
}

// --- Transmit path ------------------------------------------------------------
static void abort_tx(const char *why) {
    if (tx_phase != TX_IDLE) {
        printf("TX aborted: %s\n", why);
    }
    tx_phase = TX_IDLE;
    tx_pos = 0;
    tx_digit = 0;
    ui_active = true;
}

static void start_send(void) {
    if (tx_phase != TX_IDLE) {
        printf("TX busy\n");
        return;
    }
    if (link_state != LINK_UP) {
        printf("NO LINK\n");
        return;
    }
    if (msg_length <= 0) {
        printf("Nothing to send\n");
        return;
    }

    memcpy(tx_buf, current_message, (size_t)msg_length + 1);
    tx_len = msg_length;
    tx_pos = 0;
    tx_digit = 0;
    tx_msg_id++;
    tx_phase = TX_DATA;
    tx_next_ms = now_ms();
    ui_active = false;
    active_group = -1;
    tap_count = 0;

    printf("TX start msg=%u len=%d\n", (unsigned)tx_msg_id, tx_len);
}

static void tx_tick(uint32_t now) {
    if (tx_phase == TX_IDLE) return;
    if ((int32_t)(now - tx_next_ms) < 0) return;

    if (link_state != LINK_UP) {
        abort_tx("no link");
        return;
    }

    if (tx_phase == TX_DATA) {
        int code = code_from_glyph(tx_buf[tx_pos]);
        if (code < 0) code = code_from_glyph('?');

        int divisor = (tx_digit == 0) ? 36 : (tx_digit == 1) ? 6 : 1;
        int symbol = (code / divisor) % 6;

        if (!send_control(SYMBOLS[symbol])) {
            tx_next_ms = now + SYMBOL_INTERVAL_MS;
            return;
        }

        printf("TX %c -> %s\n", tx_buf[tx_pos], SYMBOLS[symbol]);
        tx_next_ms = now + SYMBOL_INTERVAL_MS;
        if (++tx_digit == 3) {
            tx_digit = 0;
            if (++tx_pos >= tx_len) tx_phase = TX_END;
        }
    } else if (tx_phase == TX_END) {
        if (!send_control(TOK_END)) {
            tx_next_ms = now + SYMBOL_INTERVAL_MS;
            return;
        }
        printf("TX END msg=%u\n", (unsigned)tx_msg_id);
        tx_phase = TX_IDLE;
        ui_active = true;
    }
}

// --- UI -----------------------------------------------------------------------
static void draw_text_tail(int y, const char *text) {
    if (!text || !*text) return;

    size_t len = strlen(text);
    if (len > MAX_TEXT_CHARS) {
        text += len - MAX_TEXT_CHARS;
        len = MAX_TEXT_CHARS;
    }

    char line[MAX_TEXT_CHARS + 1];
    memcpy(line, text, len);
    line[len] = '\0';
    oled_draw_custom_string(0, y, line);
}

static void drawMainUI(void) {
    memcpy(oled_buffer, (current_page == 0) ? alpha_image : numeric_image, 1024);

    // The header rows (0-15) are the live message area. Clearing them stops
    // text from chewing holes in the baked art and hides the stale labels.
    memset(oled_buffer, 0, 256);

    if (ui_active) {
        BoxRect b = (current_page == 0)
                        ? alpha_boxes[cursor_row][cursor_col]
                        : numeric_boxes[cursor_row][cursor_col];
        if (b.x2 > b.x1 && b.y2 > b.y1) {
            // w/h are extents, so +2 surrounds the hit box.
            oled_draw_box(b.x1 - 1, b.y1 - 1, b.x2 - b.x1 + 2, b.y2 - b.y1 + 2);
        }
    }

    if (msg_length > 0) {
        draw_text_tail(0, current_message);
        if (rx_message_len > 0) draw_text_tail(8, rx_message);
    } else if (rx_message_len > 0) {
        draw_text_tail(0, rx_message);
    }

    oled_show();
}

static void move_cursor(int d_row, int d_col) {
    if (current_page == 1 && d_col == -1 && cursor_col == 0) {
        current_page = 0;
        cursor_row = 0;
        cursor_col = 0;
        return;
    }

    for (int step = 0; step < 12; step++) {
        int r = (cursor_row + d_row + 4) % 4;
        int c = (cursor_col + d_col + 3) % 3;
        BoxRect b = (current_page == 0) ? alpha_boxes[r][c] : numeric_boxes[r][c];
        if (b.x2 > b.x1 && b.y2 > b.y1) {
            cursor_row = r;
            cursor_col = c;
            return;
        }
        // advance one more step in the same direction to skip empty slots
        cursor_row = r;
        cursor_col = c;
    }
}

static void reset_tap(void) {
    active_group = -1;
    tap_count = 0;
}

static void append_char(char c) {
    if (msg_length < (int)sizeof(current_message) - 1) {
        current_message[msg_length++] = c;
        current_message[msg_length] = '\0';
    }
}

static void request_ping(void) {
    if (link_state != LINK_UP) {
        printf("NO LINK\n");
        return;
    }
    if (send_control(TOK_LINK_PING)) {
        manual_ping = true;
        manual_ping_ms = now_ms();
        printf("LINK_PING sent\n");
    } else {
        printf("LINK_PING send failed\n");
    }
}

static void load_and_send_sos(void) {
    if (tx_phase != TX_IDLE) {
        printf("TX busy\n");
        return;
    }
    if (link_state != LINK_UP) {
        printf("NO LINK\n");
        return;
    }
    snprintf(current_message, sizeof(current_message), "SOS");
    msg_length = 3;
    reset_tap();
    start_send();
}

static void handle_select_button(uint32_t now) {
    if ((uint32_t)(now - last_tap_ms) > TAP_TIMEOUT_MS) {
        reset_tap();
    }

    if (current_page == 0) {
        if (cursor_row == 0 && cursor_col == 0) { // SEND
            reset_tap();
            start_send();
            return;
        }
        if (cursor_row == 3 && cursor_col == 0) { // 123+
            current_page = 1;
            cursor_row = 0;
            cursor_col = 0;
            reset_tap();
            return;
        }
        if (cursor_row == 3 && cursor_col == 1) { // SPACE
            append_char(' ');
            reset_tap();
            return;
        }
    } else {
        if (cursor_row == 0 && cursor_col == 1) { // IN RANGE?
            reset_tap();
            request_ping();
            return;
        }
        if (cursor_row == 2 && cursor_col == 1) { // SOS
            reset_tap();
            load_and_send_sos();
            return;
        }
        if (cursor_row == 3 && cursor_col == 1) { // SPACE
            append_char(' ');
            reset_tap();
            return;
        }
    }

    int group_index = -1;
    if (current_page == 0) {
        if (cursor_row == 0 && cursor_col == 1) group_index = 0;
        if (cursor_row == 0 && cursor_col == 2) group_index = 1;
        if (cursor_row == 1 && cursor_col == 0) group_index = 2;
        if (cursor_row == 1 && cursor_col == 1) group_index = 3;
        if (cursor_row == 1 && cursor_col == 2) group_index = 4;
        if (cursor_row == 2 && cursor_col == 0) group_index = 5;
        if (cursor_row == 2 && cursor_col == 1) group_index = 6;
        if (cursor_row == 2 && cursor_col == 2) group_index = 7;
        if (cursor_row == 3 && cursor_col == 2) group_index = 8;
    } else {
        if (cursor_row == 0 && cursor_col == 0) group_index = 9;
        if (cursor_row == 1 && cursor_col == 0) group_index = 10;
        if (cursor_row == 2 && cursor_col == 0) group_index = 11;
        if (cursor_row == 3 && cursor_col == 0) group_index = 12;
    }

    if (group_index >= 0) {
        if (group_index != active_group) {
            tap_count = 0;
            active_group = group_index;
        }
        tap_count++;

        const char *group = tap_groups[group_index];
        int group_len = (int)strlen(group);
        char selected = group[(tap_count - 1) % group_len];

        if (tap_count > 1 && msg_length > 0) {
            current_message[msg_length - 1] = selected;
        } else {
            append_char(selected);
        }
        last_tap_ms = now;
    }
}

static void handle_delete_button(void) {
    if (tx_phase != TX_IDLE) {
        abort_tx("DELETE");
        return;
    }
    if (msg_length > 0) {
        current_message[--msg_length] = '\0';
    }
    reset_tap();
}

// --- Input --------------------------------------------------------------------
typedef struct {
    uint8_t pin;
    bool raw;
    bool stable;
    uint32_t changed_ms;
} Button;

static Button btn_select = { .pin = BTN_SELECT };
static Button btn_delete = { .pin = BTN_DELETE };

static void button_init(Button *b, uint8_t pin) {
    b->pin = pin;
    b->raw = false;
    b->stable = false;
    b->changed_ms = now_ms();

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

// Both boards carry the same BOM: OLED, joystick on ADC0/ADC1, and the two
// active-low buttons. Initialization is unconditional.
static void input_init(void) {
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);

    button_init(&btn_select, BTN_SELECT);
    button_init(&btn_delete, BTN_DELETE);
}

static bool button_pressed(Button *b, uint32_t now) {
    bool raw = (gpio_get(b->pin) == 0);
    if (raw != b->raw) {
        b->raw = raw;
        b->changed_ms = now;
    }

    if ((uint32_t)(now - b->changed_ms) >= BUTTON_DEBOUNCE_MS &&
        b->stable != b->raw) {
        b->stable = b->raw;
        return b->stable; // fire once on the press edge
    }
    return false;
}

static int last_joy_dr = 0;
static int last_joy_dc = 0;
static uint32_t last_move_ms = 0;

static void input_tick(uint32_t now) {
    adc_select_input(0);
    uint16_t x = adc_read();
    adc_select_input(1);
    uint16_t y = adc_read();

    int dr = 0;
    int dc = 0;
    if (x > JOY_HIGH) dc = -1;       // original wiring: right is left
    else if (x < JOY_LOW) dc = 1;    // original wiring: left is right
    else if (y > JOY_HIGH) dr = 1;
    else if (y < JOY_LOW) dr = -1;

    bool direction_changed = (dr != last_joy_dr || dc != last_joy_dc);
    if (dr != 0 || dc != 0) {
        if (direction_changed ||
            (uint32_t)(now - last_move_ms) >= JOY_REPEAT_MS) {
            move_cursor(dr, dc);
            last_move_ms = now;
        }
    } else {
        last_move_ms = now; // make the next push immediate
    }
    last_joy_dr = dr;
    last_joy_dc = dc;

    if (button_pressed(&btn_select, now)) handle_select_button(now);
    if (button_pressed(&btn_delete, now)) handle_delete_button();
}

// --- Network timers -----------------------------------------------------------
static void wifi_service(uint32_t now) {
    if (is_ap) {
        wifi_ready = true;
        return;
    }

    int st = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (st == CYW43_LINK_UP) {
        const ip4_addr_t *ip =
            netif_default ? netif_ip4_addr(netif_default) : NULL;
        if (ip && !ip4_addr_isany(ip)) {
            if (!wifi_ready) {
                printf("STA IP: %s\n", ip4addr_ntoa(ip));
                wifi_ready = true;
            }
            last_wifi_retry_ms = now;
        }
        return;
    }

    if (wifi_ready) {
        wifi_ready = false;
        if (link_state == LINK_UP) set_link_down("wifi");
    }

    bool transient = (st == CYW43_LINK_JOIN || st == CYW43_LINK_NOIP);
    if (!transient &&
        (uint32_t)(now - last_wifi_retry_ms) >= WIFI_RETRY_MS) {
        last_wifi_retry_ms = now;
        printf("Wi-Fi reconnect attempt\n");
        cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD,
                                      CYW43_AUTH_WPA2_AES_PSK);
    }
}

static void net_tick(uint32_t now) {
    if (link_state == LINK_DOWN) {
        // Only the STA initiates the handshake; the AP waits and learns the
        // peer from the received HELLO.
        if (!is_ap && wifi_ready &&
            (uint32_t)(now - last_hello_ms) >= HELLO_INTERVAL_MS) {
            last_hello_ms = now;
            printf("TX HELLO\n");
            send_control(TOK_HELLO);
        }
        return;
    }

    if ((uint32_t)(now - last_heard_ms) >= LINK_TIMEOUT_MS) {
        set_link_down("timeout");
        return;
    }

    if (manual_ping && (uint32_t)(now - manual_ping_ms) >= LINK_TIMEOUT_MS) {
        manual_ping = false;
        printf("RTT timeout\n");
    }

    if ((uint32_t)(now - last_ping_ms) >= HEARTBEAT_MS) {
        last_ping_ms = now;
        send_control(TOK_LINK_PING);
    }
}

// --- Wi-Fi and UDP bring-up ---------------------------------------------------
static bool wifi_init(void) {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("CYW43 init failed\n");
        return false;
    }

    if (is_ap) {
        cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASSWORD,
                                  CYW43_AUTH_WPA2_AES_PSK);
        wifi_ready = true;
        printf("AP active: 192.168.4.1\n");
    } else {
        cyw43_arch_enable_sta_mode();
        printf("STA connecting to %s...\n", WIFI_SSID);

        last_wifi_retry_ms = now_ms();
        int err = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD,
                                                CYW43_AUTH_WPA2_AES_PSK);
        if (err) printf("wifi_connect_async failed: %d\n", err);
        printf("STA join continues in the main loop\n");
    }

    return true;
}

static bool udp_link_init(void) {
    upcb = udp_new();
    if (!upcb) {
        printf("udp_new failed\n");
        return false;
    }

    err_t err = udp_bind(upcb, IP_ADDR_ANY, UDP_PORT);
    if (err != ERR_OK) {
        printf("udp_bind failed: %d\n", (int)err);
        udp_remove(upcb);
        upcb = NULL;
        return false;
    }

    udp_recv(upcb, udp_recv_callback, NULL);
    printf("UDP listening on %u\n", (unsigned)UDP_PORT);
    return true;
}

static void halt_error(const char *label) {
    oled_clear();
    oled_draw_custom_string(0, 0, label);
    oled_show();
    printf("%s\n", label);
    while (true) sleep_ms(1000);
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500); // give the USB CDC monitor time to attach

    printf("\n--- PHANTOM LINK %s ---\n", is_ap ? "AP" : "STA");

    oled_init();
    memcpy(oled_buffer, boot_image, 1024);
    oled_show();
    sleep_ms(2500);

    // Both boards carry the same BOM. Bring up the full local UI and controls
    // before Wi-Fi so input works even while the STA is still joining.
    input_init();
    drawMainUI();

    IP_ADDR4(&ap_addr, 192, 168, 4, 1);

    if (!wifi_init()) halt_error("WIFI FAIL");
    if (!udp_link_init()) halt_error("UDP FAIL");

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    uint32_t next_draw_ms = 0;

    while (true) {
        cyw43_arch_poll();

        uint32_t now = now_ms();
        wifi_service(now);
        input_tick(now);
        net_tick(now);
        tx_tick(now);

        if ((int32_t)(now - next_draw_ms) >= 0) {
            drawMainUI();
            next_draw_ms = now + 50;
        }

        sleep_ms(5);
    }
}
