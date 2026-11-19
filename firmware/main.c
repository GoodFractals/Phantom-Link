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
#include "pico/rand.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/dhcp.h"

#include "oled.h"
#include "alpha_ui.h"
#include "numeric_ui.h"
#include "boot_ui.h"
#include "banner.h"
#include "proto/proto_frame.h"
#include "proto/proto_session.h"

#ifndef IS_AP
#define IS_AP 1
#endif

// The compile-time flag selects only the Wi-Fi role. Every other role-dependent
// decision reads this constant at runtime, so the UI and input path are never
// compiled out of either target.
static const bool is_ap = (IS_AP != 0);

#define BTN_SELECT 14
#define BTN_DELETE 15
#define BTN_BACK   12   // wired to the same delete action as BTN_DELETE

#define WIFI_SSID "Phantom_Link"
#define WIFI_PASSWORD "12345678"
#define UDP_PORT 4444

// The SDK build used here compiles the cyw43 AP interface without the optional
// DHCP server (CYW43_NETUTILS=0), so the AP never leases an address. The PoC
// therefore uses a fixed STA address in the AP's default subnet. Both numbers
// match the AP's built-in 192.168.4.1/24 interface.
#define STA_IP0 192
#define STA_IP1 168
#define STA_IP2 4
#define STA_IP3 2

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
#define SESSION_STATS_MS 30000

// --- Base-6 transport alphabet ------------------------------------------------
// 49 codes: 48 drawable font glyphs plus space. Space is code 0.
// The font itself has no space glyph; the renderer advances five pixels for it.
static const char GLYPHS[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789=+-.,?!()[]\"";
_Static_assert(sizeof(GLYPHS) - 1 == 49, "transport alphabet must have 49 codes");

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

// --- Frame counters -----------------------------------------------------------
// Ticket 20 turns these into a STATUS dump with persistence. For now they are
// live counters that make rejections visible without hiding them in a log.
static uint32_t rx_bad_frames = 0;
static uint32_t rx_unknown_type = 0;
static uint32_t rx_bad_payload = 0;

// --- Session and replay state --------------------------------------------------
static proto_session_t session;
static uint32_t expected_rx_seq = 0;
static uint32_t rx_gap = 0;
static uint32_t rx_out_of_order = 0;
static bool rx_message_dirty = false;
static uint32_t last_session_stats_ms = 0;

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
static const char *frame_type_name(uint8_t type) {
    switch (type) {
    case PROTO_TYPE_HELLO: return "HELLO";
    case PROTO_TYPE_HELLO_ACK: return "HELLO_ACK";
    case PROTO_TYPE_DATA: return "DATA";
    case PROTO_TYPE_ACK: return "ACK";
    case PROTO_TYPE_PING: return "PING";
    case PROTO_TYPE_PONG: return "PONG";
    case PROTO_TYPE_BYE: return "BYE";
    case PROTO_TYPE_SFO: return "SFO";
    case PROTO_TYPE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

static err_t send_frame_to(const ip_addr_t *dst, u16_t port, uint8_t type,
                           uint8_t flags, const uint8_t *payload,
                           uint8_t payload_len) {
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    proto_frame_view_t frame;
    size_t len = 0;
    proto_status_t status;

    if (!upcb || !dst) return ERR_VAL;

    if (!proto_session_active(&session) && type != PROTO_TYPE_HELLO) {
        printf("TX no session for %s\n", frame_type_name(type));
        return ERR_VAL;
    }

    frame.version = (uint8_t)PROTO_VERSION;
    frame.type = type;
    frame.flags = flags;
    frame.session_id = session.session_id;
    frame.seq = proto_session_next_seq(&session);
    frame.ack = 0;
    frame.payload_len = payload_len;
    frame.payload = payload;

    status = proto_frame_encode(buf, sizeof buf, &frame, &len);
    if (status == PROTO_OK) status = proto_frame_put_crc(buf, len);
    if (status != PROTO_OK) {
        printf("TX encode failed: %s\n", proto_status_str(status));
        return ERR_VAL;
    }

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
    if (!p) {
        printf("TX type=%s: pbuf_alloc failed\n", frame_type_name(type));
        return ERR_MEM;
    }

    memcpy(p->payload, buf, len);
    err_t err = udp_sendto(upcb, p, dst, port);
    pbuf_free(p);

    if (err != ERR_OK) {
        printf("TX type=%s len=%u failed: %d\n", frame_type_name(type),
               (unsigned)payload_len, (int)err);
    } else {
        printf("TX type=%s seq=%u len=%u\n", frame_type_name(type),
               (unsigned)frame.seq, (unsigned)payload_len);
    }
    return err;
}

static bool send_control_frame(uint8_t type) {
    if (!upcb) return false;

    if (peer_valid) {
        return send_frame_to(&peer_ip, peer_port, type, 0, NULL, 0) == ERR_OK;
    }

    if (!is_ap) {
        // The STA knows the AP's default address before HELLO_ACK arrives.
        return send_frame_to(&ap_addr, UDP_PORT, type, 0, NULL, 0) == ERR_OK;
    }

    printf("no peer yet; cannot send %s\n", frame_type_name(type));
    return false;
}

static bool send_data_frame(const uint8_t *payload, uint8_t payload_len,
                            uint8_t flags) {
    if (peer_valid) {
        return send_frame_to(&peer_ip, peer_port, PROTO_TYPE_DATA, flags,
                             payload, payload_len) == ERR_OK;
    }

    if (!is_ap) {
        return send_frame_to(&ap_addr, UDP_PORT, PROTO_TYPE_DATA, flags,
                             payload, payload_len) == ERR_OK;
    }

    printf("no peer yet; cannot send DATA\n");
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
    rx_message_dirty = false;
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

static void drop_rx_message(const char *why) {
    if (rx_index > 0 || sym_count > 0) {
        printf("RX partial message discarded: %s\n", why);
    }
    rx_index = 0;
    rx_buffer[0] = '\0';
    reset_rx_accumulator();
    last_symbol_ms = 0;
}

static void adopt_session(uint32_t id, const char *why) {
    uint32_t old = session.session_id;

    proto_session_init(&session, id);
    expected_rx_seq = 0;
    rx_message_dirty = false;
    drop_rx_message("session reset");

    printf("SESSION %s id=0x%08x old=0x%08x\n", why, (unsigned)id,
           (unsigned)old);
}

static void begin_handshake(void) {
    uint32_t id = get_rand_32();
    if (id == 0) id = 1; /* keep zero as the "no session" sentinel */
    adopt_session(id, "new");
}

static void handle_frame(const proto_frame_view_t *frame, const ip_addr_t *addr,
                         u16_t port) {
    note_peer(addr, port);

    /* HELLO may introduce a new session. Every other frame must match the
     * session already in place; the window check enforces that below. */
    if (frame->type == PROTO_TYPE_HELLO &&
        frame->session_id != session.session_id) {
        adopt_session(frame->session_id, "adopted");
    }

    if (!proto_session_active(&session) && frame->type != PROTO_TYPE_HELLO) {
        rx_bad_frames++;
        printf("RX %s before a session; dropped\n",
               frame_type_name(frame->type));
        return;
    }

    proto_rx_result_t verdict =
        proto_session_accept(&session, frame->session_id, frame->seq);
    if (verdict != PROTO_RX_ACCEPTED) {
        printf("RX %s type=%s seq=%u id=0x%08x "
               "(accepted=%u duplicate=%u stale=%u wrong=%u)\n",
               proto_rx_result_str(verdict), frame_type_name(frame->type),
               (unsigned)frame->seq, (unsigned)frame->session_id,
               (unsigned)session.accepted, (unsigned)session.duplicates,
               (unsigned)session.stale, (unsigned)session.wrong_session);
        return;
    }

    bool in_order = (frame->seq == expected_rx_seq);
    if (in_order) {
        expected_rx_seq++;
    } else if (frame->seq > expected_rx_seq) {
        rx_gap++;
        printf("RX sequence gap: expected=%u got=%u\n",
               (unsigned)expected_rx_seq, (unsigned)frame->seq);
        expected_rx_seq = frame->seq + 1;
    } else {
        rx_out_of_order++;
        printf("RX late frame: expected=%u got=%u\n",
               (unsigned)expected_rx_seq, (unsigned)frame->seq);
    }

    printf("RX type=%s seq=%u len=%u\n", frame_type_name(frame->type),
           (unsigned)frame->seq, (unsigned)frame->payload_len);

    switch (frame->type) {
    case PROTO_TYPE_HELLO:
        if (send_control_frame(PROTO_TYPE_HELLO_ACK)) {
            set_link_up("HELLO");
        } else {
            printf("HELLO_ACK send failed; link stays down\n");
        }
        break;

    case PROTO_TYPE_HELLO_ACK:
        set_link_up("HELLO_ACK");
        break;

    case PROTO_TYPE_PING: {
        bool pong_sent = send_control_frame(PROTO_TYPE_PONG);
        if (link_state != LINK_UP && pong_sent) set_link_up("PING");
        break;
    }

    case PROTO_TYPE_PONG:
        if (manual_ping) {
            printf("RTT: %u ms\n", (unsigned)(now_ms() - manual_ping_ms));
            manual_ping = false;
        }
        if (link_state != LINK_UP) set_link_up("PONG");
        break;

    case PROTO_TYPE_DATA:
        if (link_state != LINK_UP) break;

        if (!in_order) {
            if (!rx_message_dirty) {
                rx_message_dirty = true;
                printf("RX message invalidated: out-of-order symbol\n");
                drop_rx_message("out-of-order");
            }
            if (frame->payload_len == 0 &&
                (frame->flags & PROTO_FLAG_FINAL)) {
                rx_message_dirty = false;
            }
            break;
        }

        if (rx_message_dirty) {
            if (frame->payload_len == 0 &&
                (frame->flags & PROTO_FLAG_FINAL)) {
                rx_message_dirty = false;
                printf("RX message dropped after gap\n");
            }
            break;
        }

        if (frame->payload_len == 1 && frame->payload[0] < 6) {
            handle_symbol(frame->payload[0]);
        } else if (frame->payload_len == 0 &&
                   (frame->flags & PROTO_FLAG_FINAL)) {
            finish_rx_message();
        } else {
            rx_bad_payload++;
            rx_message_dirty = true;
            drop_rx_message("bad DATA payload");
            printf("RX DATA rejected: len=%u flags=0x%02x\n",
                   (unsigned)frame->payload_len, (unsigned)frame->flags);
        }
        break;

    default:
        printf("RX ignored type=%s (not handled yet)\n",
               frame_type_name(frame->type));
        break;
    }
}

static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;

    if (!p) return;

    uint16_t wire_len = p->tot_len;
    if (wire_len > PROTO_MAX_FRAME_LEN) {
        rx_bad_frames++;
        printf("RX oversized frame (%u bytes)\n", (unsigned)wire_len);
        pbuf_free(p);
        return;
    }

    uint8_t buf[PROTO_MAX_FRAME_LEN];
    if (pbuf_copy_partial(p, buf, wire_len, 0) != wire_len) {
        rx_bad_frames++;
        printf("RX truncated read (%u bytes)\n", (unsigned)wire_len);
        pbuf_free(p);
        return;
    }
    pbuf_free(p);

    proto_status_t status = proto_frame_check_crc(buf, wire_len);
    if (status != PROTO_OK) {
        if (status == PROTO_ERR_TYPE) {
            rx_unknown_type++;
        } else {
            rx_bad_frames++;
        }
        printf("RX rejected frame: %s (%u bytes)\n", proto_status_str(status),
               (unsigned)wire_len);
        return;
    }

    proto_frame_view_t frame;
    status = proto_frame_decode(buf, wire_len, &frame);
    if (status != PROTO_OK) {
        rx_bad_frames++;
        printf("RX decode failed after CRC: %s\n", proto_status_str(status));
        return;
    }

    handle_frame(&frame, addr, port);
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
        uint8_t sym = (uint8_t)symbol;

        if (!send_data_frame(&sym, 1, 0)) {
            tx_next_ms = now + SYMBOL_INTERVAL_MS;
            return;
        }

        printf("TX glyph '%c' sym=%d\n", tx_buf[tx_pos], symbol);
        tx_next_ms = now + SYMBOL_INTERVAL_MS;
        if (++tx_digit == 3) {
            tx_digit = 0;
            if (++tx_pos >= tx_len) tx_phase = TX_END;
        }
    } else if (tx_phase == TX_END) {
        if (!send_data_frame(NULL, 0, PROTO_FLAG_FINAL)) {
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

// --- Status HUD ---------------------------------------------------------------
// The baked numeric page reserves three cells that the cursor never visits:
// a tall signal box at x 96-120, y 20-39; "BAT:" at y 42-52; and "LINK" at
// x 105-122, y 56-63. Only the signal and link cells have live data today.
static int signal_bars = 0; // 0..4; 0 also means "no reading"
static uint32_t next_rssi_ms = 0;

static void hud_tick(uint32_t now) {
    // cyw43_wifi_get_rssi only reports a station's link to its AP, so the AP
    // side has no signal reading until the STA relays one.
    if (is_ap) return;

    if (link_state != LINK_UP) {
        signal_bars = 0;
        return;
    }
    if ((int32_t)(now - next_rssi_ms) < 0) return;
    next_rssi_ms = now + 2000;

    int32_t rssi = 0;
    if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) != 0 || rssi == 0) {
        signal_bars = 0;
    } else if (rssi >= -55) {
        signal_bars = 4;
    } else if (rssi >= -65) {
        signal_bars = 3;
    } else if (rssi >= -75) {
        signal_bars = 2;
    } else {
        signal_bars = 1;
    }
}

static void draw_status_hud(void) {
    static const uint8_t bar_x[4] = { 98, 103, 108, 113 };
    static const uint8_t bar_h[4] = { 5, 9, 13, 17 };
    const int baseline = 38; // bottom pixel row inside the bracket cell

    // Clear the cell interior every frame. The baked art still carries a
    // placeholder dot until the art pass; drawing over the top of it would
    // leave stray pixels between the bars.
    oled_fill_rect(98, 21, 20, 18, false);
    for (int i = 0; i < signal_bars && i < 4; i++) {
        oled_fill_rect(bar_x[i], baseline - bar_h[i], 4, bar_h[i], true);
    }

    // Filled square beside "LINK" means up; hollow square means down.
    if (link_state == LINK_UP) {
        oled_fill_rect(99, 59, 4, 4, true);
    } else {
        oled_draw_box(99, 59, 3, 3);
    }
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

    if (current_page == 1) draw_status_hud();

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
    if (send_control_frame(PROTO_TYPE_PING)) {
        manual_ping = true;
        manual_ping_ms = now_ms();
        printf("PING sent\n");
    } else {
        printf("PING send failed\n");
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
static Button btn_back = { .pin = BTN_BACK };

static void button_init(Button *b, uint8_t pin) {
    b->pin = pin;
    b->raw = false;
    b->stable = false;
    b->changed_ms = now_ms();

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

// Both boards carry the same BOM: OLED, joystick on ADC0/ADC1, and the
// active-low buttons. Initialization is unconditional.
static void input_init(void) {
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);

    button_init(&btn_select, BTN_SELECT);
    button_init(&btn_delete, BTN_DELETE);
    button_init(&btn_back, BTN_BACK);
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
    // DELETE and BACK share one action. Poll both before acting so pressing
    // them together cannot remove two characters on the same tick.
    bool delete_pressed = button_pressed(&btn_delete, now);
    bool back_pressed = button_pressed(&btn_back, now);
    if (delete_pressed || back_pressed) handle_delete_button();
}

// --- Network timers -----------------------------------------------------------
static const char *sta_status_name(int st) {
    switch (st) {
        case CYW43_LINK_DOWN:  return "down";
        case CYW43_LINK_JOIN:  return "joining";
        case CYW43_LINK_NOIP:  return "joined, no IP";
        case CYW43_LINK_UP:    return "up";
        case CYW43_LINK_FAIL:  return "join failed";
        case CYW43_LINK_NONET: return "network not found";
        case CYW43_LINK_BADAUTH: return "bad auth";
        default:               return "unknown";
    }
}

static void sta_use_static_ip(void) {
    struct netif *n = netif_default;
    if (!n) {
        printf("STA netif missing; static IP not set\n");
        return;
    }

    ip4_addr_t ip, mask, gw;
    IP_ADDR4(&ip, STA_IP0, STA_IP1, STA_IP2, STA_IP3);
    IP_ADDR4(&mask, 255, 255, 255, 0);
    IP_ADDR4(&gw, STA_IP0, STA_IP1, STA_IP2, 1);

    cyw43_arch_lwip_begin();
    dhcp_stop(n);
    netif_set_addr(n, &ip, &mask, &gw);
    cyw43_arch_lwip_end();

    printf("STA static IP: %s (DHCP disabled)\n", ip4addr_ntoa(&ip));
}

static void wifi_service(uint32_t now) {
    static int last_sta_status = -1;

    if (is_ap) {
        wifi_ready = true;
        return;
    }

    int st = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (st != last_sta_status) {
        printf("STA status: %s (%d)\n", sta_status_name(st), st);
        last_sta_status = st;
    }

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
            begin_handshake();
            send_control_frame(PROTO_TYPE_HELLO);
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
        send_control_frame(PROTO_TYPE_PING);
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

        // Keep the radio awake; power save adds latency and dropped packets.
        cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);

        // The AP build has no DHCP server, so do not wait for a lease.
        sta_use_static_ip();

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

static void session_stats_tick(uint32_t now) {
    if (!proto_session_active(&session)) return;
    if ((uint32_t)(now - last_session_stats_ms) < SESSION_STATS_MS) return;

    last_session_stats_ms = now;
    printf("SESSION stats: id=0x%08x tx=%u accepted=%u duplicate=%u "
           "stale=%u wrong=%u gap=%u late=%u\n",
           (unsigned)session.session_id, (unsigned)session.tx_seq,
           (unsigned)session.accepted, (unsigned)session.duplicates,
           (unsigned)session.stale, (unsigned)session.wrong_session,
           (unsigned)rx_gap, (unsigned)rx_out_of_order);
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500); // give the USB CDC monitor time to attach

    char banner[64];
    phantom_banner(banner, sizeof banner, is_ap);
    printf("\n%s\n", banner);

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
        hud_tick(now);
        session_stats_tick(now);

        if ((int32_t)(now - next_draw_ms) >= 0) {
            drawMainUI();
            next_draw_ms = now + 50;
        }

        sleep_ms(5);
    }
}
