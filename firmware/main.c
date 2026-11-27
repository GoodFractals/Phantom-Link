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
#include "pico/bootrom.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"

#include "oled.h"
#include "alpha_ui.h"
#include "numeric_ui.h"
#include "boot_ui.h"
#include "banner.h"
#include "proto/proto_frame.h"
#include "proto/proto_msg.h"
#include "proto/proto_session.h"
#include "proto/proto_txq.h"
#include "proto/proto_crypto.h"
#include "proto/proto_psk.h"
#include "proto/proto_lpi.h"

#include "proto/proto_sfo.h"
#include "proto/proto_sfo_telemetry.h"
#include "proto/proto_netcfg.h"
#include "proto/proto_reconnect.h"
#include "proto/proto_zeroize.h"
#include "proto/proto_aead.h"
#include "proto/proto_fs.h"
#include "proto/proto_x25519.h"
#include "proto/proto_ratchet.h"

#define WIFI_SSID PHANTOM_WIFI_SSID
#define WIFI_PASSWORD PHANTOM_WIFI_PASSWORD

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
#define DECOY_BASE_MS 700u
#define DECOY_JITTER_MS 300u
#define SFO_BURST_MS 1000u
#define SFO_CRC_CONTEXT 0xA5u
#ifdef PHANTOM_WATCHDOG_TEST
#define WATCHDOG_TEST_TRIGGER_MS 5000u
static uint32_t watchdog_test_start = 0;
#endif
#define LINK_TIMEOUT_MS 3000
#define SYMBOL_INTERVAL_MS 80
#define ACK_TIMEOUT_MS 1200
#define TX_NOTICE_MS 5000
#define TAP_TIMEOUT_MS 800
#define JOY_REPEAT_MS 220
#define JOY_LOW 1000
#define JOY_HIGH 3000
#define BUTTON_DEBOUNCE_MS 20
#define MAX_TEXT_CHARS 21
#define SESSION_STATS_MS 30000

// --- Base-6 transport alphabet ------------------------------------------------
// 49 codes: 48 drawable font glyphs plus space. Space is code 0.
// The font itself has no space glyph; the renderer adds two extra pixels,
// so words sit three pixels apart.
static const char GLYPHS[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789=+-.,?!()[]\"";
_Static_assert(sizeof(GLYPHS) - 1 == 49, "transport alphabet must have 49 codes");

// --- UI layout ---------------------------------------------------------------
typedef struct { int x1, y1, x2, y2; } BoxRect;

/* Ticket 24: named UI regions. Drawing code uses these instead of scattering
 * literal coordinates; no protocol or timing constant lives here. */
typedef struct { uint8_t x, y, w, h; } UiRect;
static const UiRect UI_SIGNAL_CELL = { 98, 21, 20, 18 };
static const UiRect UI_BAT_CELL = { 88, 42, 32, 11 };
static const UiRect UI_LINK_MARK = { 99, 59, 4, 4 };

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
    { {7, 20, 40, 30}, {0, 0, 0, 0},     {0, 0, 0, 0} },       // 123   (dead)      [ . ]
    { {7, 31, 40, 41}, {0, 0, 0, 0},     {0, 0, 0, 0} },       // 456
    { {7, 42, 40, 52}, {0, 0, 0, 0},     {0, 0, 0, 0} },       // 789   (dead)      BAT:
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
// draft while the user is typing. The assembler holds at most 21 characters.
static char rx_message[64] = "";
static int rx_message_len = 0;
static proto_msg_rx_t rx_msg;

// --- Link state ---------------------------------------------------------------
typedef enum { LINK_DOWN = 0, LINK_UP } link_state_t;

static struct udp_pcb *upcb = NULL;
static uint8_t psk_tx_key[16];
static uint8_t psk_rx_key[16];
/* Ephemeral per-session traffic keys from X25519 + fs_derive_keys. HELLO
 * confirmation keeps using the static PSK keys; DATA/SFO use these instead.
 * FS_KEY_BYTES is 32, so keep separate buffers and pick the right suffix in
 * the suite calls (Speck uses 16, AEAD uses 32). */
static uint8_t sess_tx_key[FS_KEY_BYTES];
static uint8_t sess_rx_key[FS_KEY_BYTES];
/* Ticket 34: dedicated 16-byte header-mask keys, KDF-separated from the
 * traffic keys so the same material is never reused for two purposes. */
static uint8_t hdr_mask_tx_key[SPECK128_KEY_BYTES];
static uint8_t hdr_mask_rx_key[SPECK128_KEY_BYTES];
static bool sess_keys_ready = false;
static bool ecdh_secrets_registered = false;
static x25519_ctx_t ecdh_ctx;
static uint8_t our_pubkey[X25519_KEY_BYTES];
static bool ecdh_ready = false;
/* Per-frame ratchets: each DATA frame gets a distinct key from its frame seq.
 * Masters are the ECDH-derived session keys, initialized on each handshake. */
static ratchet_t tx_ratchet;
static ratchet_t rx_ratchet;
static bool ratchets_ready = false;
/* ECDH derivation is deferred from the lwIP callback (deep stack) to the main
 * loop: mbedTLS Curve25519 uses several KB of stack and overflowed the 8 KB
 * stack when called from handle_frame. */
static bool ecdh_derive_pending = false;
static uint8_t pending_peer_pub[X25519_KEY_BYTES];
static const char *pending_link_reason = NULL;
static uint32_t auth_failures = 0;
static proto_session_t session;  /* used by derive_session_keys above */
static uint32_t crypto_errors = 0;
static uint32_t hdr_eph_tx = 0;
static uint32_t hdr_eph_rx = 0;
static uint32_t decoy_tx_count = 0;
static uint32_t last_decoy_ms = 0;
static uint32_t next_decoy_ms = DECOY_BASE_MS;
#ifdef PHANTOM_DATA_PROBE
static bool probe_tamper_next = false;
#endif

static sfo_telemetry_t sfo_tel;
static uint32_t next_sfo_ms = SFO_BURST_MS;
static uint8_t sfo_nibble = 0;
static uint8_t sfo_repeat = 0;
static uint8_t sfo_perm_pos = 0;
static uint8_t sfo_order[4] = {0};
static uint8_t sfo_crc[4] = {0};
static uint8_t sfo_arrival = 0;
static uint8_t sfo_nibbles[SFO_REPEATS] = {0};
static uint8_t sfo_nibble_count = 0;
static uint32_t next_sfo_report_ms = 30000u;

static const uint8_t ap_bssid[6] = PHANTOM_AP_BSSID;
static uint32_t bssid_failures = 0;
static bool bssid_enrolled = false;
static reconnect_t rc;
static uint32_t rc_epoch = 0;
static zeroize_tracker_t zeroizer;
static bool keys_cleared = false;

static const pc_suite_ops_t *active_suite = &pc_suite_speck_ctr_cmac;

#ifdef PHANTOM_USE_AEAD
static void aead_expand_key(const uint8_t in[16], uint8_t out[32]) {
    uint8_t master[32] = {0};
    static const uint8_t salt[] = "AEAD-KEY";
    memcpy(master, in, 16u);
    pc_kdf(master, salt, sizeof salt - 1u, 0x05u, out);
}

static void aead_seal_adapter(const uint8_t key[16],
                              const uint8_t nonce[12],
                              const uint8_t *aad, size_t aad_len,
                              uint8_t *buf, size_t len,
                              uint8_t tag[8]) {
    uint8_t expanded[32];
    aead_expand_key(key, expanded);
    aead_seal(expanded, nonce, aad, aad_len, buf, len, tag);
}

static int aead_open_adapter(const uint8_t key[16],
                             const uint8_t nonce[12],
                             const uint8_t *aad, size_t aad_len,
                             uint8_t *buf, size_t len,
                             const uint8_t tag[8]) {
    uint8_t expanded[32];
    aead_expand_key(key, expanded);
    return aead_open(expanded, nonce, aad, aad_len, buf, len, tag);
}

static const pc_suite_ops_t pc_suite_aead_adapter = {
    aead_seal_adapter, aead_open_adapter
};
#endif

static void print_bssid(const char *label, const uint8_t bssid[6]) {
    printf("%s %02x:%02x:%02x:%02x:%02x:%02x\n", label,
           bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}

static int wifi_join_sta(void) {
    const uint8_t *target = bssid_enrolled ? ap_bssid : NULL;
    return cyw43_wifi_join(&cyw43_state, strlen(WIFI_SSID),
                           (const uint8_t *)WIFI_SSID,
                           strlen(WIFI_PASSWORD),
                           (const uint8_t *)WIFI_PASSWORD,
                           CYW43_AUTH_WPA2_AES_PSK, target, CYW43_CHANNEL_NONE);
}

static void frame_nonce(uint32_t seq, uint8_t nonce[PC_NONCE_BYTES]) {
    nonce[0] = 0x01u; /* outbound direction */
    memcpy(nonce + 1u, &seq, 4u);
    memset(nonce + 5u, 0, PC_NONCE_BYTES - 5u);
}

/* PSK-masked handshake frames use a fixed zero nonce (the session id is not
 * established yet). Ephemeral-masked frames bind the nonce to the session id
 * so the keystream changes on every handshake. */
static const uint8_t HEADER_MASK_NONCE[PC_NONCE_BYTES] = {0};

static void header_mask_nonce(uint8_t nonce[PC_NONCE_BYTES]) {
    memset(nonce, 0, PC_NONCE_BYTES);
    memcpy(nonce, &session.session_id, sizeof session.session_id);
}

#define STATUS_SLOTS 16u
#define STATUS_SLOT_SIZE FLASH_PAGE_SIZE
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4u * 1024u * 1024u)
#endif
#define STATUS_FLASH_BASE (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define STATUS_MAGIC 0x50534C32u

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint32_t tx, accepted, duplicates, stale, wrong, gap, late;
    uint32_t msg, msgdup, msgdrop, retry, fail;
    uint32_t auth, crypto, decoy, bssid;
    uint32_t sfo_good, sfo_total, sfo_reorders;
    uint32_t crc;
} status_record_t;

#ifdef PHANTOM_STATUS_SAVE
#ifdef PHANTOM_BRINGUP_TRACE
#define STATUS_SAVE_INTERVAL_MS 8000u
#else
#define STATUS_SAVE_INTERVAL_MS 60000u
#endif
static uint32_t status_next_save_ms = STATUS_SAVE_INTERVAL_MS;
#endif
static uint32_t status_record_seq = 0;
static uint32_t status_slot = 0;
static status_record_t status_loaded;
static bool status_valid = false;

/* Ticket 34: mask bytes 6..17 (session_id, seq, ack). Handshake frames stay on
 * the static PSK because the peer has no ECDH keys yet; every later frame uses
 * the ECDH-derived header key, signalled by a clear-prefix flag bit. */
static void mask_header_fields(uint8_t *buf, bool ephemeral) {
    if (ephemeral) {
        uint8_t nonce[PC_NONCE_BYTES];
        buf[4] |= (uint8_t)PROTO_FLAG_EPH_MASK;
        hdr_eph_tx++;
        header_mask_nonce(nonce);
        pc_ctr_xcrypt(hdr_mask_tx_key, nonce, buf + 6u, 12u);
    } else {
        pc_ctr_xcrypt(psk_tx_key, HEADER_MASK_NONCE, buf + 6u, 12u);
    }
}

static void unmask_header_fields(uint8_t *buf) {
    if ((buf[4] & (uint8_t)PROTO_FLAG_EPH_MASK) != 0u) {
        uint8_t nonce[PC_NONCE_BYTES];
        hdr_eph_rx++;
        header_mask_nonce(nonce);
        pc_ctr_xcrypt(hdr_mask_rx_key, nonce, buf + 6u, 12u);
    } else {
        pc_ctr_xcrypt(psk_rx_key, HEADER_MASK_NONCE, buf + 6u, 12u);
    }
}

static void register_key_secrets(void) {
    zeroize_tracker_register(&zeroizer, psk_tx_key, sizeof psk_tx_key);
    zeroize_tracker_register(&zeroizer, psk_rx_key, sizeof psk_rx_key);
    keys_cleared = false;
}

/* Derive ephemeral session keys from our ECDH context and the peer's public
 * key. Registers the new secrets with the zeroizer. Returns false on failure. */
/* Ticket 37 review finding: a fresh X25519 scalar for EVERY session, not one
 * reused until teardown. mbedTLS keeps the private scalar's limbs on the heap,
 * so clear the old context first — zeroize_tracker_clear_all() wipes struct
 * bytes but does not free the limbs. */
static void ecdh_new_session_keypair(void) {
    x25519_clear(&ecdh_ctx);
    memset(our_pubkey, 0, sizeof our_pubkey);
    ecdh_ready = false;
    if (!x25519_gen_keypair(&ecdh_ctx, our_pubkey)) {
        printf("ECDH keypair generation failed\n");
        return;
    }
    ecdh_ready = true;
}

static bool derive_session_keys(const uint8_t peer_pub[X25519_KEY_BYTES]) {
    uint8_t shared[X25519_KEY_BYTES];
    /* HELLO_ACK already sent our public key for this session, so do NOT
     * regenerate here — that would invalidate the key the peer is using.
     * Freshness is enforced by adopt_session() clearing ecdh_ready. */
    if (!ecdh_ready) ecdh_new_session_keypair();
    if (!ecdh_ready) return false;
    /* Register the ECDH/session secrets once. These are static buffers, so
     * re-registering on every handshake would fill the 16-slot tracker. */
    if (!ecdh_secrets_registered) {
        zeroize_tracker_register(&zeroizer, ecdh_ctx.opaque,
                                 sizeof ecdh_ctx.opaque);
        zeroize_tracker_register(&zeroizer, sess_tx_key, sizeof sess_tx_key);
        zeroize_tracker_register(&zeroizer, sess_rx_key, sizeof sess_rx_key);
        zeroize_tracker_register(&zeroizer, hdr_mask_tx_key,
                                 sizeof hdr_mask_tx_key);
        zeroize_tracker_register(&zeroizer, hdr_mask_rx_key,
                                 sizeof hdr_mask_rx_key);
        ecdh_secrets_registered = true;
    }
    if (!x25519_shared_secret(&ecdh_ctx, peer_pub, shared)) return false;
    {
        static const uint8_t master_psk[FS_KEY_BYTES] = PHANTOM_PSK_BYTES;
        uint8_t ap_to_sta[FS_KEY_BYTES], sta_to_ap[FS_KEY_BYTES];
        fs_derive_keys(master_psk, shared, session.session_id,
                       (const uint8_t *)"BONNIE-CLYDE-v1",
                       sizeof("BONNIE-CLYDE-v1") - 1u,
                       ap_to_sta, sta_to_ap);
        /* Role-based, not "my send / my receive": both boards must use the
         * same key for a given direction. The STA swaps, mirroring
         * psk_derive_keys(). Getting this wrong makes DATA frames undecryptable
         * while PING/PONG still work (they are CRC-only), hiding the bug. */
        if (is_ap) {
            memcpy(sess_tx_key, ap_to_sta, FS_KEY_BYTES);
            memcpy(sess_rx_key, sta_to_ap, FS_KEY_BYTES);
        } else {
            memcpy(sess_tx_key, sta_to_ap, FS_KEY_BYTES);
            memcpy(sess_rx_key, ap_to_sta, FS_KEY_BYTES);
        }
        memset(ap_to_sta, 0, sizeof ap_to_sta);
        memset(sta_to_ap, 0, sizeof sta_to_ap);
    }
    memset(shared, 0, sizeof shared);
    /* Ticket 34: domain-separated header-mask keys. The PSK-derived keys are
     * still used for HELLO/HELLO_ACK only; everything after the handshake is
     * masked with these ECDH-derived keys so a leaked PSK cannot read
     * session_id/seq/ack. */
    {
        static const uint8_t salt[] = "BONNIE-CLYDE-hdr-v1";
        uint8_t material[PC_DERIVED_KEY_BYTES];
        /* The SAME label on both sides: directional separation comes from
         * ap_to_sta vs sta_to_ap, not from the label. */
        pc_kdf(sess_tx_key, salt, sizeof salt - 1u, 0x05u, material);
        memcpy(hdr_mask_tx_key, material, SPECK128_KEY_BYTES);
        pc_kdf(sess_rx_key, salt, sizeof salt - 1u, 0x05u, material);
        memcpy(hdr_mask_rx_key, material, SPECK128_KEY_BYTES);
        memset(material, 0, sizeof material);
    }
    sess_keys_ready = true;
    /* Ticket 33: each DATA frame key comes from a per-frame ratchet whose
     * master is the ECDH session key for that direction. */
    ratchet_init(&tx_ratchet, sess_tx_key);
    ratchet_init(&rx_ratchet, sess_rx_key);
    ratchets_ready = true;
    printf("SESSION KEYS derived (X25519 + ratchet)\n");
    return true;
}

static void psk_derive_keys(void) {
    static const uint8_t salt[] = "BONNIE-CLYDE-v1";
    static const uint8_t psk[PC_MASTER_KEY_BYTES] = PHANTOM_PSK_BYTES;
    uint8_t ap_to_sta[PC_DERIVED_KEY_BYTES], sta_to_ap[PC_DERIVED_KEY_BYTES];

    pc_kdf(psk, salt, sizeof salt - 1u, 0x01u, ap_to_sta);
    pc_kdf(psk, salt, sizeof salt - 1u, 0x02u, sta_to_ap);
    /* Keys are role-based, not "my send / my receive". Both boards must use
     * the same key for a given direction or the peer cannot verify/decrypt. */
    if (is_ap) {
        memcpy(psk_tx_key, ap_to_sta, sizeof psk_tx_key);
        memcpy(psk_rx_key, sta_to_ap, sizeof psk_rx_key);
    } else {
        memcpy(psk_tx_key, sta_to_ap, sizeof psk_tx_key);
        memcpy(psk_rx_key, ap_to_sta, sizeof psk_rx_key);
    }
}

static ip_addr_t ap_addr;
static ip_addr_t peer_ip;
static u16_t peer_port = UDP_PORT;
static bool peer_valid = false;
static bool wifi_ready = false;
static uint32_t last_wifi_retry_ms = 0;
static bool sta_ever_connected = false;
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
static uint32_t expected_rx_seq = 0;
static uint32_t rx_gap = 0;
static uint32_t rx_out_of_order = 0;
static uint32_t last_session_stats_ms = 0;

// --- Transmit state machine ---------------------------------------------------
static proto_msg_tx_t tx_msg;
static txq_t tx_queue;
static uint32_t tx_next_ms = 0;
static uint32_t tx_frames = 0;
static char tx_notice[12] = "";
static uint32_t tx_notice_until = 0;

// --- Time and codec -----------------------------------------------------------
static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static bool tx_active(void) {
    return tx_msg.phase == PROTO_TX_SENDING ||
           tx_msg.phase == PROTO_TX_WAITING;
}

// Confirmed delivery clears the draft, but only when the user has not typed
// over it during the send. Input stays live while transmitting, so a changed
// draft is newer than the message that just landed and must survive.
static void tx_clear_delivered_draft(void) {
    if (msg_length != (int)tx_msg.len) return;
    if (memcmp(current_message, tx_msg.text, (size_t)tx_msg.len) != 0) return;

    msg_length = 0;
    current_message[0] = '\0';
    active_group = -1;
    tap_count = 0;
}

// Frame payloads carry 16-bit message IDs in network byte order.
static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void write_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)(value & 0xFF);
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
                           uint8_t payload_len, uint32_t ack) {
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
    frame.ack = ack;
    frame.payload_len = payload_len;
    frame.payload = payload;

    if (type == PROTO_TYPE_DATA) frame.flags |= PROTO_FLAG_ENCRYPTED;

    status = proto_frame_encode(buf, sizeof buf, &frame, &len);
    {
        bool eph = sess_keys_ready && type != PROTO_TYPE_HELLO &&
                   type != PROTO_TYPE_HELLO_ACK;
        if (status == PROTO_OK) mask_header_fields(buf, eph);
    }
    if (status == PROTO_OK && type == PROTO_TYPE_DATA) {
        uint8_t nonce[PC_NONCE_BYTES];
        uint8_t frame_key[PC_DERIVED_KEY_BYTES];
        frame_nonce(frame.seq, nonce);
        /* Ticket 33: derive a distinct key per frame (msg_id = frame seq).
         * The tx ratchet master is the ECDH session key for our direction. */
        if (!ratchets_ready ||
            !ratchet_advance(&tx_ratchet, frame.seq, frame_key)) {
            printf("RATCHET TX failed seq=%u\n", (unsigned)frame.seq);
            return ERR_VAL;
        }
        active_suite->seal(frame_key, nonce, buf, PROTO_HEADER_LEN,
                           buf + PROTO_HEADER_LEN, payload_len,
                           buf + PROTO_HEADER_LEN + payload_len);
        memset(frame_key, 0, sizeof frame_key);
    } else if (status == PROTO_OK) {
        status = proto_frame_put_crc(buf, len);
    }
    if (status != PROTO_OK) {
        printf("TX encode failed: %s\n", proto_status_str(status));
        return ERR_VAL;
    }
#ifdef PHANTOM_DATA_PROBE
    /* 'X' probe: flip one auth-tag byte after sealing so the peer must reject
     * the frame with CRYPTO FAIL. Exercises the real AEAD verify path. */
    if (probe_tamper_next) {
        probe_tamper_next = false;
        buf[len - 1u] ^= 0x01u;
    }
#endif

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
    uint8_t payload[PC_CONFIRM_NONCE_BYTES + PC_CONFIRM_TAG_BYTES + X25519_KEY_BYTES];
    const ip_addr_t *dst = NULL;
    u16_t port = UDP_PORT;
    err_t err;

    if (!upcb) return false;

    if (peer_valid) {
        dst = &peer_ip;
        port = peer_port;
    } else if (!is_ap) {
        dst = &ap_addr;
    } else {
        printf("no peer yet; cannot send %s\n", frame_type_name(type));
        return false;
    }

    if (type == PROTO_TYPE_HELLO || type == PROTO_TYPE_HELLO_ACK) {
        uint8_t label = (type == PROTO_TYPE_HELLO) ? 0x01u : 0x02u;
        const uint8_t *key = (type == PROTO_TYPE_HELLO) ? psk_tx_key : psk_rx_key;
        size_t plen = PC_CONFIRM_NONCE_BYTES + PC_CONFIRM_TAG_BYTES +
                      X25519_KEY_BYTES;
        /* Fresh ephemeral keypair for this session. Only the first 8 bytes of
         * the public key are the confirm nonce; the full key is appended and
         * is implicitly authenticated by the DATA tag, not MACed here. */
        ecdh_new_session_keypair();
        if (!ecdh_ready) return false;
        memcpy(payload, our_pubkey, PC_CONFIRM_NONCE_BYTES);
        pc_confirm_tag(key, label, session.session_id, session.tx_seq,
                       payload, payload + PC_CONFIRM_NONCE_BYTES);
        memcpy(payload + PC_CONFIRM_NONCE_BYTES + PC_CONFIRM_TAG_BYTES,
               our_pubkey, X25519_KEY_BYTES);

        err = send_frame_to(dst, port, type, 0, payload,
                            (uint8_t)plen, 0);
    } else {
        err = send_frame_to(dst, port, type, 0, NULL, 0, 0);
    }
    return err == ERR_OK;
}

static bool send_data_frame(const uint8_t *payload, uint8_t payload_len,
                            uint8_t flags) {
    uint8_t padded[LPI_WIRE_PAYLOAD];
    err_t err;

    lpi_pad(payload, payload_len, padded, get_rand_32);
    if (peer_valid) {
        err = send_frame_to(&peer_ip, peer_port, PROTO_TYPE_DATA, flags,
                            padded, LPI_WIRE_PAYLOAD, 0);
    } else if (!is_ap) {
        err = send_frame_to(&ap_addr, UDP_PORT, PROTO_TYPE_DATA, flags,
                            padded, LPI_WIRE_PAYLOAD, 0);
    } else {
        printf("no peer yet; cannot send DATA\n");
        return false;
    }
    return err == ERR_OK;
}

static bool send_ack_frame(uint16_t msg_id, uint8_t next_pos) {
    uint8_t payload[PROTO_MSG_ACK_PAYLOAD];
    uint32_t ack = (expected_rx_seq > 0) ? expected_rx_seq - 1 : 0;

    write_u16(payload, msg_id);
    payload[2] = next_pos;

    if (peer_valid) {
        return send_frame_to(&peer_ip, peer_port, PROTO_TYPE_ACK, 0, payload,
                             PROTO_MSG_ACK_PAYLOAD, ack) == ERR_OK;
    }

    if (!is_ap) {
        return send_frame_to(&ap_addr, UDP_PORT, PROTO_TYPE_ACK, 0, payload,
                             PROTO_MSG_ACK_PAYLOAD, ack) == ERR_OK;
    }

    printf("no peer yet; cannot send ACK\n");
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

static void sta_use_static_ip(void);  // forward decl for set_link_down()

static void set_link_up(const char *why) {
    uint32_t now = now_ms();
    last_heard_ms = now;

    if (link_state == LINK_UP) return;

    link_state = LINK_UP;
    last_ping_ms = now;
    last_hello_ms = now;
    ui_active = true;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
#ifdef PHANTOM_WATCHDOG_TEST
    // Arm only now that the link is up - association can exceed 8 s.
    watchdog_enable(8000u, false);
#endif

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
    proto_session_init(&session, 0);
    proto_msg_rx_reset(&rx_msg);
    ui_active = true;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    if (tx_active()) {
        proto_msg_tx_abort(&tx_msg);
        tx_frames = 0;
        tx_next_ms = 0;
        txq_clear(&tx_queue);
        printf("TX aborted: no link\n");
    }

    printf("LINK LOST (%s)\n", why);

    // On a STA, a lost link means the AP may have rebooted. CYW43 can keep
    // reporting LINK_UP to a dead AP, so force a fresh association and
    // restore our static IP before the next HELLO goes out.
    if (!is_ap && strcmp(why, "timeout") == 0) {
        wifi_ready = false;
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        last_wifi_retry_ms = now_ms();
        wifi_join_sta();
        sta_use_static_ip();
    }
}

// --- Receive path -------------------------------------------------------------
static void adopt_session(uint32_t id, const char *why) {
    uint32_t old = session.session_id;

    /* Ticket 37 review finding: a new session must get a new X25519 scalar.
     * The next HELLO / HELLO_ACK will generate it. */
    x25519_clear(&ecdh_ctx);
    ecdh_ready = false;

    proto_session_init(&session, id);
    expected_rx_seq = 0;
    proto_msg_rx_reset(&rx_msg);
    rc_epoch++;
    reconnect_adopt_epoch(&rc, now_ms(), rc_epoch, link_state == LINK_UP);

    if (tx_active()) {
        printf("TX aborted: session reset\n");
        proto_msg_tx_abort(&tx_msg);
        tx_frames = 0;
        txq_clear(&tx_queue);
    }

    printf("SESSION %s id=0x%08x old=0x%08x\n", why, (unsigned)id,
           (unsigned)old);
}

static void begin_handshake(void) {
    uint32_t id = get_rand_32();
    if (id == 0) id = 1; /* keep zero as the "no session" sentinel */
    if (keys_cleared) {
        psk_derive_keys();
        register_key_secrets();
    }
    adopt_session(id, "new");
}

static void request_teardown(const char *reason) {
    if (link_state != LINK_UP) return;

    printf("TEARDOWN requested: %s\n", reason);
    if (send_control_frame(PROTO_TYPE_BYE)) {
        printf("BYE sent\n");
    } else {
        printf("BYE send failed\n");
    }
    set_link_down("BYE");
    /* Free mbedTLS heap limbs BEFORE the tracker wipes the struct, otherwise
     * x25519_clear() sees a zeroed 'ready' flag and leaks them. */
    x25519_clear(&ecdh_ctx);
    zeroize_tracker_clear_all(&zeroizer);
    keys_cleared = true;
    /* The tracker just wiped these buffers; invalidate their state flags so
     * the next handshake regenerates fresh ECDH keys instead of using a wiped
     * context (which caused endless link churn after a BYE). */
    ecdh_ready = false;
    ecdh_secrets_registered = false;
    sess_keys_ready = false;
    ratchets_ready = false;
    printf("SECRETS zeroized\n");
}

static void sfo_send_burst_step(void) {
    uint8_t perm[SFO_BURST_SIZE];
    uint8_t payload[2];

    sfo_nibble_to_perm(sfo_nibble, perm);
    payload[0] = perm[sfo_perm_pos];
    payload[1] = sfo_crc8(SFO_CRC_CONTEXT, sfo_nibble);
    if (peer_valid) {
        send_frame_to(&peer_ip, peer_port, PROTO_TYPE_SFO, 0,
                      payload, sizeof payload, 0);
    } else if (!is_ap) {
        send_frame_to(&ap_addr, UDP_PORT, PROTO_TYPE_SFO, 0,
                      payload, sizeof payload, 0);
    }

    sfo_perm_pos++;
    if (sfo_perm_pos >= SFO_BURST_SIZE) {
        sfo_perm_pos = 0;
        sfo_repeat++;
        if (sfo_repeat >= SFO_REPEATS) {
            sfo_repeat = 0;
            sfo_nibble = (uint8_t)((sfo_nibble + 1u) & 0x0Fu);
        }
    }
}

static void handle_sfo_frame(const proto_frame_view_t *frame) {
    uint8_t nibble = 0;
    uint8_t decoded = 0;

    if (frame->payload_len != 2u || !frame->payload) return;
    if (sfo_arrival >= SFO_BURST_SIZE) sfo_arrival = 0;
    sfo_order[sfo_arrival] = frame->payload[0];
    sfo_crc[sfo_arrival] = frame->payload[1];
    sfo_arrival++;

    if (sfo_arrival < SFO_BURST_SIZE) return;
    sfo_arrival = 0;

    if (!sfo_perm_to_nibble(sfo_order, &decoded) ||
        sfo_crc[0] != sfo_crc8(SFO_CRC_CONTEXT, decoded)) {
        sfo_telemetry_record_failure(&sfo_tel);
        return;
    }
    if (sfo_nibble_count < SFO_REPEATS) {
        sfo_nibbles[sfo_nibble_count++] = decoded;
    }
    if (sfo_nibble_count < SFO_REPEATS) return;

    if (sfo_majority(sfo_nibbles, &nibble)) {
        sfo_telemetry_record_success(&sfo_tel, nibble);
    } else {
        sfo_telemetry_record_failure(&sfo_tel);
    }
    sfo_nibble_count = 0;
}

static void sfo_report_tick(uint32_t now) {
    sfo_telemetry_snapshot_t snap;
    char line[96];

#ifdef PHANTOM_WATCHDOG_TEST
    if (watchdog_test_start == 0u && link_state == LINK_UP) {
        watchdog_test_start = now;
        printf("WATCHDOG TEST armed\n");
    }
    if (watchdog_test_start != 0u &&
        (int32_t)(now - watchdog_test_start) >= (int32_t)WATCHDOG_TEST_TRIGGER_MS) {
        printf("WATCHDOG TEST forcing hang\n");
        while (true) { tight_loop_contents(); }
    }
#endif

    if ((int32_t)(now - next_sfo_report_ms) >= 0) {
        next_sfo_report_ms = now + 30000u;
        sfo_telemetry_snapshot(&sfo_tel, &snap);
        sfo_telemetry_format(&snap, line, sizeof line);
        printf("%s\n", line);
        if (snap.degraded) printf("SFO degraded\n");
    }
}

static void status_load(void) {
    uint32_t i, best = 0;
    for (i = 0; i < STATUS_SLOTS; i++) {
        // STATUS_FLASH_BASE is a flash *offset*. A raw cast lands at
        // 0x003FF000, which is unmapped on RP2350 and hard-faults. Read
        // through the XIP window instead; flash_range_* below correctly take
        // the offset form.
        const status_record_t *r = (const status_record_t *)
            (XIP_BASE + STATUS_FLASH_BASE + i * STATUS_SLOT_SIZE);
        if (r->magic != STATUS_MAGIC) continue;
        if (r->crc != proto_crc32((const uint8_t *)r,
                                  sizeof(*r) - sizeof(uint32_t))) continue;
        if (status_valid && r->seq <= best) continue;
        status_loaded = *r;
        status_valid = true;
        best = r->seq;
        status_record_seq = r->seq;
        status_slot = i;
    }
    printf("STATUS record %s seq=%u\n",
           status_valid ? "loaded" : "none", (unsigned)status_record_seq);
}

/* Full 256-byte flash page in RAM. status_record_t is only 88 bytes, so the
 * page buffer avoids the stack over-read that the old &rec programming did. */
static uint8_t status_page[FLASH_PAGE_SIZE];

/* flash_safe_execute() callback: runs with IRQs disabled and code out of XIP,
 * the SDK-documented pattern. */
typedef struct {
    uint32_t offset;
    bool erase;
} flash_write_ctx_t;

static void status_flash_write(void *arg) {
    flash_write_ctx_t *ctx = (flash_write_ctx_t *)arg;
    if (ctx->erase) flash_range_erase(STATUS_FLASH_BASE, FLASH_SECTOR_SIZE);
    flash_range_program(ctx->offset, status_page, FLASH_PAGE_SIZE);
}

static void status_save(uint32_t now) {
#ifndef PHANTOM_STATUS_SAVE
    (void)now;
    // Flash writes are off by default pending the RP2350 flash/CYW43
    // quiescing fix (ticket 20). Reading persisted records still works.
    return;
#else
    status_record_t rec;
    sfo_telemetry_snapshot_t sfo;
    uint32_t offset;
    bool erase = false;

#ifdef PHANTOM_BRINGUP_TRACE
    {
        static uint32_t last_check = 0;
        if ((int32_t)(now - last_check) >= 1000) {
            last_check = now;
            printf("SSAVE-CHECK now=%u next=%u\n", (unsigned)now,
                   (unsigned)status_next_save_ms);
        }
    }
#endif

    if ((int32_t)(now - status_next_save_ms) < 0) return;
    /* Schedule the NEXT save relative to now. Setting it to `now` made every
     * subsequent loop iteration write a page (thousands of writes per minute). */
    status_next_save_ms = now + STATUS_SAVE_INTERVAL_MS;

    status_slot = (status_slot + 1u) % STATUS_SLOTS;
    if (status_slot == 0u) erase = true;
    offset = STATUS_FLASH_BASE + status_slot * STATUS_SLOT_SIZE;

    memset(&rec, 0, sizeof rec);
    rec.magic = STATUS_MAGIC;
    rec.seq = ++status_record_seq;
    rec.tx = session.tx_seq;
    rec.accepted = session.accepted;
    rec.duplicates = session.duplicates;
    rec.stale = session.stale;
    rec.wrong = session.wrong_session;
    rec.gap = rx_gap;
    rec.late = rx_out_of_order;
    rec.msg = rx_msg.completed;
    rec.msgdup = rx_msg.duplicates;
    rec.msgdrop = rx_msg.dropped;
    rec.retry = tx_msg.retransmits;
    rec.fail = tx_msg.failed;
    rec.auth = auth_failures;
    rec.crypto = crypto_errors;
    rec.decoy = decoy_tx_count;
    rec.bssid = bssid_failures;
    sfo_telemetry_snapshot(&sfo_tel, &sfo);
    rec.sfo_good = sfo.good;
    rec.sfo_total = sfo.total;
    rec.sfo_reorders = sfo.reorders;
    rec.crc = proto_crc32((const uint8_t *)&rec, sizeof rec - sizeof(uint32_t));
    memset(status_page, 0, sizeof status_page);
    memcpy(status_page, &rec, sizeof rec);
#ifdef PHANTOM_BRINGUP_TRACE
    printf("SSAVE crc done=%08x page_bytes=%u\n", (unsigned)rec.crc,
           (unsigned)sizeof status_page);
#endif

#ifdef PHANTOM_BRINGUP_TRACE
    printf("SSAVE enter slot=%u off=%06x erase=%d seq=%u base=%06x\n",
           (unsigned)status_slot, (unsigned)offset, (int)erase,
           (unsigned)rec.seq, (unsigned)STATUS_FLASH_BASE);
#endif
    // SDK-documented API: flash_safe_execute() handles IRQ disable + XIP
    // re-entry correctly. cyw43_arch_lwip_begin/end are no-ops in the poll
    // build, so they were removed. Feed the watchdog immediately before.
    watchdog_update();
    flash_write_ctx_t ctx = { .offset = offset, .erase = erase };
    int rc = flash_safe_execute(status_flash_write, &ctx, 5000u);
    watchdog_update();
    if (rc != 0) {
        printf("SSAVE flash failed rc=%d\n", rc);
    } else {
        /* The record is now on flash; reflect that in the STATUS dump. */
        status_valid = true;
    }
#ifdef PHANTOM_BRINGUP_TRACE
    printf("SSAVE post-write rc=%d\n", rc);
#endif
#endif /* PHANTOM_STATUS_SAVE */
}

static void status_dump(void) {
    sfo_telemetry_snapshot_t sfo;
    sfo_telemetry_snapshot(&sfo_tel, &sfo);
    printf("=== STATUS ===\n");
    printf("link=%s role=%s session=0x%08x\n",
           link_state == LINK_UP ? "UP" : "DOWN", is_ap ? "AP" : "STA",
           (unsigned)session.session_id);
    printf("seq tx=%u accepted=%u dup=%u stale=%u wrong=%u gap=%u late=%u\n",
           (unsigned)session.tx_seq, (unsigned)session.accepted,
           (unsigned)session.duplicates, (unsigned)session.stale,
           (unsigned)session.wrong_session, (unsigned)rx_gap,
           (unsigned)rx_out_of_order);
    printf("msg complete=%u dup=%u drop=%u retry=%u fail=%u\n",
           (unsigned)rx_msg.completed, (unsigned)rx_msg.duplicates,
           (unsigned)rx_msg.dropped, (unsigned)tx_msg.retransmits,
           (unsigned)tx_msg.failed);
    printf("errors auth=%u crypto=%u bssid=%u decoy=%u\n",
           (unsigned)auth_failures, (unsigned)crypto_errors,
           (unsigned)bssid_failures, (unsigned)decoy_tx_count);
    printf("sfo good=%u/%u reorder=%u degraded=%u\n",
           (unsigned)sfo.good, (unsigned)sfo.total,
           (unsigned)sfo.reorders, sfo.degraded ? 1u : 0u);
    printf("reconnect state=%s attempts=%u replacements=%u stale_rejected=%u\n",
           reconnect_state_name(rc.state), (unsigned)rc.attempts,
           (unsigned)rc.replacements, (unsigned)rc.stale_rejected);
    printf("persist seq=%u valid=%u slot=%u\n",
           (unsigned)status_record_seq, status_valid ? 1u : 0u,
           (unsigned)status_slot);
    printf("ratchet tx_key=%u rx_key=%u rollbacks=%u ready=%d\n",
           (unsigned)tx_ratchet.accepted, (unsigned)rx_ratchet.accepted,
           (unsigned)rx_ratchet.rollbacks_rejected, (int)ratchets_ready);
    printf("hdr_mask eph_tx=%u eph_rx=%u ready=%d\n",
           (unsigned)hdr_eph_tx, (unsigned)hdr_eph_rx, (int)sess_keys_ready);
    printf("=== END STATUS ===\n");
}

static void handle_frame(const proto_frame_view_t *frame, const ip_addr_t *addr,
                         u16_t port) {
    note_peer(addr, port);

    uint8_t peer_pub[X25519_KEY_BYTES] = {0};
    if (frame->type == PROTO_TYPE_HELLO ||
        frame->type == PROTO_TYPE_HELLO_ACK) {
        /* After BYE the PSK role keys were wiped. The AP has no
         * begin_handshake(), so restore them here BEFORE checking the confirm
         * tag — otherwise no pair can ever re-link without a reboot. */
        if (keys_cleared) {
            psk_derive_keys();
            register_key_secrets();
        }
        uint8_t label = (frame->type == PROTO_TYPE_HELLO) ? 0x01u : 0x02u;
        const uint8_t *key = (frame->type == PROTO_TYPE_HELLO)
                                 ? psk_rx_key : psk_tx_key;
        size_t plen = PC_CONFIRM_NONCE_BYTES + PC_CONFIRM_TAG_BYTES +
                      X25519_KEY_BYTES;
        /* The first 8 bytes of the peer's public key are the confirm nonce,
         * so the tag is bound to the ephemeral key it accompanies. */
        if (frame->payload_len != plen || !frame->payload ||
            !pc_confirm_check(key, label, frame->session_id, frame->seq,
                              frame->payload,
                              frame->payload + PC_CONFIRM_NONCE_BYTES)) {
            auth_failures++;
            printf("AUTH FAILED: %s bad tag (id=0x%08x seq=%u)\n",
                   frame_type_name(frame->type), (unsigned)frame->session_id,
                   (unsigned)frame->seq);
            return;
        }
        memcpy(peer_pub,
               frame->payload + PC_CONFIRM_NONCE_BYTES + PC_CONFIRM_TAG_BYTES,
               X25519_KEY_BYTES);
    }

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
        /* Reply immediately (HELLO_ACK is CRC-only), then derive keys in the
         * main loop where there is enough stack for mbedTLS Curve25519. */
        memcpy(pending_peer_pub, peer_pub, X25519_KEY_BYTES);
        pending_link_reason = "HELLO";
        ecdh_derive_pending = true;
        if (!send_control_frame(PROTO_TYPE_HELLO_ACK)) {
            printf("HELLO_ACK send failed; link stays down\n");
        }
        break;

    case PROTO_TYPE_HELLO_ACK:
        memcpy(pending_peer_pub, peer_pub, X25519_KEY_BYTES);
        pending_link_reason = "HELLO_ACK";
        ecdh_derive_pending = true;
        break;

    case PROTO_TYPE_PING: {
        bool pong_sent = send_control_frame(PROTO_TYPE_PONG);
        if (link_state != LINK_UP && pong_sent) set_link_up("PING");
        break;
    }

    case PROTO_TYPE_PONG:
        if (manual_ping) {
            printf("RTT: %u ms\n", (unsigned)(int32_t)(now_ms() - manual_ping_ms));
            manual_ping = false;
        }
        if (link_state != LINK_UP) set_link_up("PONG");
        break;

    case PROTO_TYPE_BYE:
        printf("RX BYE: peer closing link\n");
        set_link_down("BYE");
        // Mirror the initiator's teardown: clear secrets on this side too.
        // set_link_down deliberately does not zeroize when called from
        // wifi_service on a transient outage, so do it explicitly for BYE.
        x25519_clear(&ecdh_ctx);
        zeroize_tracker_clear_all(&zeroizer);
        keys_cleared = true;
        ecdh_ready = false;
        ecdh_secrets_registered = false;
        sess_keys_ready = false;
        ratchets_ready = false;
        printf("SECRETS zeroized (peer BYE)\n");
        break;

    case PROTO_TYPE_ACK: {
        if (frame->payload_len != PROTO_MSG_ACK_PAYLOAD) {
            rx_bad_payload++;
            printf("RX ACK bad len=%u\n", (unsigned)frame->payload_len);
            break;
        }

        uint16_t acked_id = read_u16(frame->payload);
        uint8_t next_pos = frame->payload[2];
        bool delivered = proto_msg_tx_ack(&tx_msg, acked_id, next_pos);

        if (delivered) {
            printf("TX delivered msg=%u attempts=%u retransmits=%u\n",
                   (unsigned)acked_id, (unsigned)tx_msg.attempt,
                   (unsigned)tx_msg.retransmits);
            tx_clear_delivered_draft();
            ui_active = true;
        } else if (tx_active() && acked_id == tx_msg.msg_id) {
            printf("TX ACK msg=%u next=%u resume=%u\n", (unsigned)acked_id,
                   (unsigned)next_pos, (unsigned)tx_msg.resume_pos);
        }
        break;
    }

    case PROTO_TYPE_SFO:
#ifndef SFO_DISABLED
        handle_sfo_frame(frame);
#endif
        break;

    case PROTO_TYPE_DATA: {
        if (link_state != LINK_UP) break;

        bool final = (frame->flags & PROTO_FLAG_FINAL) != 0;
        uint8_t need = final ? PROTO_MSG_FINAL_PAYLOAD
                             : PROTO_MSG_SYMBOL_PAYLOAD;
        if (frame->payload_len != need) {
            rx_bad_payload++;
            printf("RX DATA bad len=%u flags=0x%02x\n",
                   (unsigned)frame->payload_len, (unsigned)frame->flags);
            break;
        }

        uint16_t rx_id = read_u16(frame->payload);
        uint8_t pos = frame->payload[2];
        uint8_t symbol = final ? 0 : frame->payload[3];

        proto_msg_event_t event =
            proto_msg_rx_frame(&rx_msg, rx_id, pos, symbol, final);

        if (event == PROTO_MSG_EV_COMPLETE) {
            uint8_t len = proto_msg_rx_len(&rx_msg);
            memcpy(rx_message, proto_msg_rx_text(&rx_msg), (size_t)len + 1);
            rx_message_len = len;

            if (strcmp(rx_message, "SOS") == 0) {
                printf("\n!!! SOS SIGNAL RECEIVED !!!\n");
            } else {
                printf("\nSecret Msg: %s\n", rx_message);
            }
            printf("RX msg=%u complete len=%u\n", (unsigned)rx_id,
                   (unsigned)len);
            ui_active = true;
        } else if (event == PROTO_MSG_EV_DUPLICATE) {
            printf("RX msg=%u duplicate final; re-ACK\n", (unsigned)rx_id);
        } else if (event == PROTO_MSG_EV_BAD) {
            rx_bad_payload++;
            printf("RX DATA bad msg=%u pos=%u sym=%u\n", (unsigned)rx_id,
                   (unsigned)pos, (unsigned)symbol);
        } else if (event == PROTO_MSG_EV_IGNORED && pos > 0) {
            printf("RX DATA gap msg=%u pos=%u have=%u\n", (unsigned)rx_id,
                   (unsigned)pos, (unsigned)rx_msg.symbol_count);
        }

        if ((frame->flags & PROTO_FLAG_ACK_REQ) &&
            event != PROTO_MSG_EV_BAD) {
            uint8_t next_pos = proto_msg_rx_next_pos(&rx_msg, rx_id);
            if (send_ack_frame(rx_id, next_pos)) {
                printf("TX ACK msg=%u next=%u\n", (unsigned)rx_id,
                       (unsigned)next_pos);
            } else {
                printf("ACK send failed msg=%u\n", (unsigned)rx_id);
            }
        }
        break;
    }

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

    uint8_t masked_header[PROTO_HEADER_LEN];
    proto_frame_view_t frame, masked_view;
    proto_status_t status;

    memcpy(masked_header, buf, PROTO_HEADER_LEN);
    status = proto_frame_decode(buf, wire_len, &masked_view);
    if (status != PROTO_OK) {
        rx_bad_frames++;
        printf("RX decode failed: %s\n", proto_status_str(status));
        return;
    }

    if (masked_view.flags & PROTO_FLAG_ENCRYPTED) {
        uint8_t nonce[PC_NONCE_BYTES];
        unmask_header_fields(buf);
        status = proto_frame_decode(buf, wire_len, &frame);
        if (status != PROTO_OK) {
            rx_bad_frames++;
            printf("RX decode failed after unmask: %s\n", proto_status_str(status));
            return;
        }
        frame_nonce(frame.seq, nonce);
        /* Ticket 33: recover the per-frame key from the ratchet using the
         * seq from the (unmasked) header. Out-of-order frames inside the
         * 8-slot window resolve to their cached key; rollbacks are rejected. */
        uint8_t frame_key[PC_DERIVED_KEY_BYTES];
        /* Ticket 37 review finding: ratchet_advance() mutates the tip/window.
         * Snapshot it so a forged high-seq frame with a bad tag cannot slide
         * the tip and wedge every later legitimate frame (DoS). */
        ratchet_t rx_before = rx_ratchet;
        bool ratchet_ok = ratchets_ready &&
            ratchet_advance(&rx_ratchet, frame.seq, frame_key);
        bool opened = ratchet_ok &&
            wire_len == PROTO_HEADER_LEN + frame.payload_len + PROTO_AUTH_LEN &&
            active_suite->open(
                frame_key, nonce, masked_header, PROTO_HEADER_LEN,
                buf + PROTO_HEADER_LEN, frame.payload_len,
                buf + PROTO_HEADER_LEN + frame.payload_len) == 0;
        if (!opened) {
            rx_ratchet = rx_before;   /* discard unauthenticated state change */
            memset(frame_key, 0, sizeof frame_key);
            crypto_errors++;
            printf("CRYPTO FAIL: bad tag (%u bytes) ratchet=%d\n",
                   (unsigned)wire_len, (int)ratchet_ok);
            return;
        }
        memset(frame_key, 0, sizeof frame_key);
    } else {
        status = proto_frame_check_crc(buf, wire_len);
        if (status != PROTO_OK) {
            if (status == PROTO_ERR_TYPE) rx_unknown_type++;
            else rx_bad_frames++;
            printf("RX rejected frame: %s (%u bytes)\n",
                   proto_status_str(status), (unsigned)wire_len);
            return;
        }
        unmask_header_fields(buf);
        status = proto_frame_decode(buf, wire_len, &frame);
        if (status != PROTO_OK) {
            rx_bad_frames++;
            printf("RX decode failed after unmask: %s\n", proto_status_str(status));
            return;
        }
    }

    if ((frame.flags & PROTO_FLAG_ENCRYPTED) && frame.type == PROTO_TYPE_DATA) {
        uint8_t real[LPI_WIRE_PAYLOAD];
        size_t real_len = 0;
        if (frame.payload_len != LPI_WIRE_PAYLOAD ||
            !lpi_unpad(buf + PROTO_HEADER_LEN, LPI_WIRE_PAYLOAD,
                       real, &real_len)) {
            crypto_errors++;
            printf("CRYPTO FAIL: bad padding\n");
            return;
        }
        memcpy(buf + PROTO_HEADER_LEN, real, real_len);
        frame.payload_len = (uint8_t)real_len;
    }

    handle_frame(&frame, addr, port);
}

static bool tx_enqueue_message(void) {
    uint8_t pos;

    tx_frames = 0;
    for (pos = tx_msg.pos; pos < tx_msg.total_symbols; pos++) {
        txq_item_t item;
        memset(&item, 0, sizeof item);

        tx_frames++;
        if ((tx_frames % 8u) == 0u) item.flags |= PROTO_FLAG_ACK_REQ;

        item.payload_len = PROTO_MSG_SYMBOL_PAYLOAD;
        write_u16(item.payload, tx_msg.msg_id);
        item.payload[2] = pos;
        item.payload[3] = proto_msg_tx_symbol_at(&tx_msg, pos);

        if (!txq_enqueue(&tx_queue, &item)) {
            printf("TX queue overflow (depth=%u)\n", (unsigned)TXQ_DEPTH);
            return false;
        }
    }

    {
        txq_item_t item;
        memset(&item, 0, sizeof item);
        item.flags = PROTO_FLAG_FINAL | PROTO_FLAG_ACK_REQ;
        item.payload_len = PROTO_MSG_FINAL_PAYLOAD;
        write_u16(item.payload, tx_msg.msg_id);
        item.payload[2] = tx_msg.total_symbols;

        if (!txq_enqueue(&tx_queue, &item)) {
            printf("TX queue overflow (depth=%u)\n", (unsigned)TXQ_DEPTH);
            return false;
        }
    }

    return true;
}

static void abort_tx(const char *why) {
    if (tx_active()) {
        printf("TX aborted: %s\n", why);
    }
    txq_clear(&tx_queue);
    proto_msg_tx_abort(&tx_msg);
    tx_frames = 0;
    tx_next_ms = 0;
    ui_active = true;
}

static void start_send(void) {
    if (tx_active()) {
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

    if (!proto_msg_tx_begin(&tx_msg, current_message, (uint8_t)msg_length)) {
        printf("TX begin failed\n");
        return;
    }

    txq_clear(&tx_queue);
    if (!tx_enqueue_message()) {
        txq_clear(&tx_queue);
        proto_msg_tx_abort(&tx_msg);
        ui_active = true;
        return;
    }

    tx_next_ms = now_ms();
    tx_notice[0] = '\0';
    // Keep the cursor visible while sending: ticket 07 requires the UI and
    // input to stay responsive during transmit. Hiding the cursor made a
    // harmless 5 s send look like a freeze.
    active_group = -1;
    tap_count = 0;

    printf("TX start msg=%u len=%u queued=%u\n", (unsigned)tx_msg.msg_id,
           (unsigned)tx_msg.len, (unsigned)txq_count(&tx_queue));
}

static void tx_failed(void) {
    printf("TX FAILED msg=%u attempts=%u\n", (unsigned)tx_msg.msg_id,
           (unsigned)tx_msg.attempt);

    snprintf(tx_notice, sizeof tx_notice, "NO ACK");
    tx_notice_until = now_ms() + TX_NOTICE_MS;

    txq_clear(&tx_queue);
    proto_msg_tx_abort(&tx_msg);
    tx_frames = 0;
    ui_active = true;
}

static void tx_tick(uint32_t now) {
    txq_item_t item;

    if (!tx_active()) return;

    if (tx_msg.phase == PROTO_TX_WAITING) {
        if (!proto_msg_tx_timed_out(&tx_msg, now)) return;

        if (!proto_msg_tx_retry(&tx_msg)) {
            tx_failed();
            return;
        }

        txq_clear(&tx_queue);
        if (!tx_enqueue_message()) {
            tx_failed();
            return;
        }

        tx_next_ms = now;
        printf("TX retry msg=%u attempt=%u from=%u queued=%u\n",
               (unsigned)tx_msg.msg_id, (unsigned)tx_msg.attempt,
               (unsigned)tx_msg.pos, (unsigned)txq_count(&tx_queue));
        return;
    }

    if (txq_empty(&tx_queue)) return;
    if ((int32_t)(now - tx_next_ms) < 0) return;

    if (link_state != LINK_UP) {
        abort_tx("no link");
        return;
    }

    if (!txq_peek(&tx_queue, &item)) return;

    // send_data_frame returns bool, not err_t. Do not compare it with ERR_OK:
    // a successful send is true (1), which would look like a failure.
    if (!send_data_frame(item.payload, item.payload_len, item.flags)) {
        tx_next_ms = now + SYMBOL_INTERVAL_MS;
        return;
    }

    txq_dequeue(&tx_queue, NULL);

    if (item.flags & PROTO_FLAG_FINAL) {
        printf("TX END msg=%u total=%u\n", (unsigned)tx_msg.msg_id,
               (unsigned)tx_msg.total_symbols);
        proto_msg_tx_final_sent(&tx_msg, now, ACK_TIMEOUT_MS);
        tx_next_ms = tx_msg.deadline_ms;
    } else {
        tx_msg.pos++;
        tx_next_ms = now + SYMBOL_INTERVAL_MS;
    }
}

// --- UI -----------------------------------------------------------------------
/* Ticket 24/26: the old fixed "last 21 characters" tail could clip because the
 * font is variable-width (worst case 21 x 6 = 126 of 128 px). Walk back from
 * the end using real glyph widths and stop before a right margin, so every
 * end-character fits. Prefixes are empty until the user's baked TX:/RX: art
 * lands; swapping them is then a two-line change. */
#define UI_TEXT_RIGHT_PAD 4u
static const char *ui_prefix_tx = "";
static const char *ui_prefix_rx = "";

static int ui_glyph_width(char ch) {
    int idx = font_index_for_char(ch);
    if (idx < 0 || idx >= font_char_count) return (ch == ' ') ? 3 : 6;
    return font_widths[idx] + 1; /* renderer advances width + 1 */
}

static int ui_text_width(const char *text) {
    int w = 0;
    for (const char *p = text; *p; p++) w += ui_glyph_width(*p);
    return w > 0 ? w - 1 : 0;      /* last glyph has no trailing gap */
}

static void draw_text_tail(int y, const char *text, const char *prefix) {
    if (!text || !*text) return;

    int prefix_w = prefix ? ui_text_width(prefix) : 0;
    int budget = 128 - (int)UI_TEXT_RIGHT_PAD - prefix_w;
    size_t len = strlen(text);
    size_t start = len;
    int used = 0;

    while (start > 0) {
        int cw = ui_glyph_width(text[start - 1u]);
        if (used + cw > budget) break;
        used += cw;
        start--;
    }

    char line[MAX_TEXT_CHARS + 8u];
    size_t n = 0;
    if (prefix && *prefix) {
        size_t plen = strlen(prefix);
        if (plen > MAX_TEXT_CHARS) plen = MAX_TEXT_CHARS;
        memcpy(line, prefix, plen);
        n = plen;
    }
    size_t tail = len - start;
    if (n + tail > sizeof line - 1u) tail = sizeof line - 1u - n;
    memcpy(line + n, text + start, tail);
    line[n + tail] = '\0';
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
    const int baseline = UI_SIGNAL_CELL.y + UI_SIGNAL_CELL.h - 1;

    /* Clear baked placeholders every frame: the signal cell still carries a
     * dot and the BAT cell a fake label until this art pass. */
    oled_fill_rect(UI_SIGNAL_CELL.x, UI_SIGNAL_CELL.y,
                   UI_SIGNAL_CELL.w, UI_SIGNAL_CELL.h, false);
    oled_fill_rect(UI_BAT_CELL.x, UI_BAT_CELL.y,
                   UI_BAT_CELL.w, UI_BAT_CELL.h, false);

    for (int i = 0; i < signal_bars && i < 4; i++) {
        oled_fill_rect(bar_x[i], baseline - bar_h[i], 4, bar_h[i], true);
    }

    /* BAT cell shows TX/RX activity, never a fake charge gauge. */
    if (tx_active()) {
        oled_fill_rect(UI_BAT_CELL.x + 2u, UI_BAT_CELL.y + 3u, 8u, 4u, true);
    } else if (rx_message_len > 0) {
        oled_fill_rect(UI_BAT_CELL.x + 20u, UI_BAT_CELL.y + 3u, 8u, 4u, true);
    }

    /* LINK: filled square = UP, X = NO LINK (a different shape, not just a
     * missing fill). The baked "LINK" label stays visible beside the mark. */
    if (link_state == LINK_UP) {
        oled_fill_rect(UI_LINK_MARK.x, UI_LINK_MARK.y,
                       UI_LINK_MARK.w, UI_LINK_MARK.h, true);
    } else {
        for (int i = 0; i < 4; i++) {
            oled_fill_rect((uint8_t)(UI_LINK_MARK.x + i),
                           (uint8_t)(UI_LINK_MARK.y + i), 1u, 1u, true);
            oled_fill_rect((uint8_t)(UI_LINK_MARK.x + i),
                           (uint8_t)(UI_LINK_MARK.y + 3u - i), 1u, 1u, true);
        }
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
            /* Main (alpha) page: visible outline sits 1 px up and 1 px left
             * of the stored hit box, per the user's pixel-perfect spec.
             * Numeric page is unchanged until its own alignment pass. */
            int ox = (current_page == 0) ? b.x1 - 2 : b.x1 - 1;
            int oy = (current_page == 0) ? b.y1 - 2 : b.y1 - 1;
            oled_draw_box(ox, oy, b.x2 - b.x1 + 2, b.y2 - b.y1 + 2);
        }
    }

    bool notice = tx_notice[0] != '\0' &&
                  (int32_t)(now_ms() - tx_notice_until) < 0;
    if (msg_length > 0) {
        draw_text_tail(0, current_message, "");
        if (notice) {
            draw_text_tail(8, tx_notice, ui_prefix_tx);
        } else if (rx_message_len > 0) {
            draw_text_tail(8, rx_message, ui_prefix_rx);
        }
    } else if (notice) {
        draw_text_tail(0, tx_notice, ui_prefix_tx);
    } else if (rx_message_len > 0) {
        draw_text_tail(0, rx_message, ui_prefix_rx);
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
    if (msg_length < MAX_TEXT_CHARS) {
        current_message[msg_length++] = c;
        current_message[msg_length] = '\0';
    }
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
        // Rows 0 and 2 column 1 are dead (no cursor, no handler, no baked
        // label drawn). Only SPACE remains selectable here.
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
    if (tx_active()) {
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

    if ((int32_t)(now - b->changed_ms) >= (int32_t)BUTTON_DEBOUNCE_MS &&
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
    static uint32_t teardown_hold_start = 0;
    static bool teardown_triggered = false;

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
            (int32_t)(now - last_move_ms) >= (int32_t)JOY_REPEAT_MS) {
            move_cursor(dr, dc);
            last_move_ms = now;
        }
    } else {
        last_move_ms = now; // make the next push immediate
    }
    last_joy_dr = dr;
    last_joy_dc = dc;

    bool sel_raw = (gpio_get(BTN_SELECT) == 0);
    bool del_raw = (gpio_get(BTN_DELETE) == 0);
    bool back_raw = (gpio_get(BTN_BACK) == 0);
    /* Hold both physical buttons together for 2.5 s to request a clean
     * teardown. The chord cannot fire while typing or erasing, and it does
     * not depend on joystick orientation or the active page. */
    bool teardown_hold = sel_raw && (del_raw || back_raw);

    bool select_pressed = button_pressed(&btn_select, now);
    bool delete_pressed = button_pressed(&btn_delete, now);
    bool back_pressed = button_pressed(&btn_back, now);
    if (!teardown_hold) {
        if (select_pressed) handle_select_button(now);
        // DELETE and BACK share one action. Poll both before acting so
        // pressing them together cannot remove two characters on one tick.
        if (delete_pressed || back_pressed) handle_delete_button();
    }

    if (teardown_hold) {
        if (teardown_hold_start == 0) teardown_hold_start = now;
        if (!teardown_triggered &&
            (int32_t)(now - teardown_hold_start) >= 2500) {
            teardown_triggered = true;
            request_teardown("user");
        }
    } else {
        teardown_hold_start = 0;
        teardown_triggered = false;
    }
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
    static uint32_t transient_since = 0;

    if (is_ap) {
        wifi_ready = true;
        return;
    }

    int st = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (st != last_sta_status) {
        printf("STA status: %s (%d)\n", sta_status_name(st), st);
        last_sta_status = st;
        transient_since = now;
    }

    if (st == CYW43_LINK_UP) {
        const ip4_addr_t *ip =
            netif_default ? netif_ip4_addr(netif_default) : NULL;
        if (ip && !ip4_addr_isany(ip)) {
            if (!wifi_ready) {
                if (bssid_enrolled) {
                    uint8_t cur[6];
                    if (cyw43_wifi_get_bssid(&cyw43_state, cur) != 0 ||
                        memcmp(cur, ap_bssid, 6u) != 0) {
                        bssid_failures++;
                        printf("BSSID MISMATCH: expected/current\n");
                        print_bssid("  expected", ap_bssid);
                        print_bssid("  current ", cur);
                        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
                        last_wifi_retry_ms = now;
                        return;
                    }
                }
                printf("STA IP: %s\n", ip4addr_ntoa(ip));
                /* An AP reboot can change its BSSID/L2 mapping. Drop any ARP
                 * entry learned for the old AP before sending HELLO. */
                cyw43_arch_lwip_begin();
#if LWIP_ARP
                if (netif_default) etharp_cleanup_netif(netif_default);
#endif
                cyw43_arch_lwip_end();
                wifi_ready = true;
                sta_ever_connected = true;
            }
            last_wifi_retry_ms = now;
            return;
        }
        // CYW43 says LINK_UP but the static IP was not restored after the
        // AP restarted. Re-apply it and let the retry logic run if needed.
        sta_use_static_ip();
        wifi_ready = true;
        return;
    }

    if (wifi_ready) {
        wifi_ready = false;
        if (link_state == LINK_UP) set_link_down("wifi");
    }

    bool transient = (st == CYW43_LINK_JOIN || st == CYW43_LINK_NOIP);
    // A transient state must not suppress retries forever: if the AP
    // disappears and returns, CYW43 can park in JOIN/NOIP indefinitely.
    // After WIFI_RETRY_MS in that state, leave and re-join explicitly.
    bool transient_stuck = transient &&
        (int32_t)(now - transient_since) >= (int32_t)WIFI_RETRY_MS;
    if ((!transient || transient_stuck) &&
        (int32_t)(now - last_wifi_retry_ms) >= (int32_t)WIFI_RETRY_MS) {
        last_wifi_retry_ms = now;
        transient_since = now;
        printf("Wi-Fi reconnect attempt (st=%d stuck=%d)\n", st,
               (int)transient_stuck);
        if (transient_stuck) cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        wifi_join_sta();
    }
}

static void net_tick(uint32_t now) {
    if (link_state == LINK_DOWN) {
        reconnect_result_t rr = reconnect_tick(&rc, now, false);
        if (!is_ap && (rr.action == RECONNECT_ACTION_START ||
                       rr.action == RECONNECT_ACTION_RETRY)) {
            wifi_join_sta();
            printf("RECONNECTING attempt=%u backoff=%ums state=%s\n",
                   (unsigned)rc.attempts, (unsigned)rr.backoff_ms,
                   reconnect_state_name(rr.state));
        }
        if (!is_ap) {
            // peer from the received HELLO.
            if (wifi_ready &&
                (int32_t)(now - last_hello_ms) >= (int32_t)HELLO_INTERVAL_MS) {
                last_hello_ms = now;
                begin_handshake();
                send_control_frame(PROTO_TYPE_HELLO);
            }
        }
        return;
    }

    reconnect_result_t up = reconnect_tick(&rc, now, true);
    if (up.action == RECONNECT_ACTION_LINK_UP) {
        printf("RECONNECT LINK UP epoch=%u state=%s\n",
               (unsigned)up.epoch, reconnect_state_name(up.state));
    }

    if ((int32_t)(now - last_heard_ms) >= (int32_t)LINK_TIMEOUT_MS) {
        set_link_down("timeout");
        return;
    }

    if (manual_ping && (int32_t)(now - manual_ping_ms) >= (int32_t)LINK_TIMEOUT_MS) {
        manual_ping = false;
        printf("RTT timeout\n");
    }

    if ((int32_t)(now - last_ping_ms) >= (int32_t)HEARTBEAT_MS) {
        last_ping_ms = now;
        send_control_frame(PROTO_TYPE_PING);
    }

    /* Constant-rate-ish decoy chatter while no user message is in flight.
     * Control frames only: never DATA, so message assembly and msgdrop stay
     * untouched. Jitter is bounded and the next delay is chosen up front. */
    if (!tx_active() && (int32_t)(now - last_decoy_ms) >= (int32_t)next_decoy_ms) {
        last_decoy_ms = now;
        next_decoy_ms = lpi_decoy_delay(DECOY_BASE_MS, DECOY_JITTER_MS,
                                        get_rand_32);
        if (send_control_frame(PROTO_TYPE_PING)) decoy_tx_count++;
    }
#ifndef SFO_DISABLED
    if ((int32_t)(now - next_sfo_ms) >= (int32_t)SFO_BURST_MS) {
        next_sfo_ms = now;
        sfo_send_burst_step();
    }
#endif
}

// --- Bring-up trace (diagnostic build only) ----------------------------------
#ifdef PHANTOM_BRINGUP_TRACE
static void trace_checkpoint(const char *label) {
    printf("TRACE %s\n", label);
    oled_clear();
    oled_draw_custom_string(0, 0, label);
    oled_show();
}
#else
#define trace_checkpoint(label) ((void)0)
#endif

// --- Wi-Fi and UDP bring-up ---------------------------------------------------
static bool wifi_init(void) {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("CYW43 init failed\n");
        return false;
    }
    trace_checkpoint("T4");

    if (is_ap) {
        uint8_t own[6];
        cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASSWORD,
                                  CYW43_AUTH_WPA2_AES_PSK);
        trace_checkpoint("T5");
        wifi_ready = true;
        printf("AP active: 192.168.4.1\n");
        if (cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_AP, own) == 0) {
            print_bssid("AP BSSID:", own);
        }
    } else {
        cyw43_arch_enable_sta_mode();
        trace_checkpoint("T5");

        // Keep the radio awake; power save adds latency and dropped packets.
        cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);

        // The AP build has no DHCP server, so do not wait for a lease.
        sta_use_static_ip();

        printf("STA connecting to %s (BSSID %s)...\n", WIFI_SSID,
               bssid_enrolled ? "enrolled" : "unbound");

        last_wifi_retry_ms = now_ms();
        int err = wifi_join_sta();
        trace_checkpoint("T6");
        if (err) printf("wifi_join failed: %d\n", err);
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
           "stale=%u wrong=%u gap=%u late=%u msg=%u msgdup=%u msgdrop=%u "
           "retry=%u fail=%u auth=%u crypto=%u decoy=%u bssid=%u\n",
           (unsigned)session.session_id, (unsigned)session.tx_seq,
           (unsigned)session.accepted, (unsigned)session.duplicates,
           (unsigned)session.stale, (unsigned)session.wrong_session,
           (unsigned)rx_gap, (unsigned)rx_out_of_order,
           (unsigned)rx_msg.completed, (unsigned)rx_msg.duplicates,
           (unsigned)rx_msg.dropped, (unsigned)tx_msg.retransmits,
           (unsigned)tx_msg.failed, (unsigned)auth_failures, (unsigned)crypto_errors, (unsigned)decoy_tx_count, (unsigned)bssid_failures);
}

// Tooling hook: two 'B' characters within 1 s on the CDC port reboot into
// BOOTSEL. This lets tools/flash_pico.ps1 flash without a physical button;
// picotool's --ser filter only matches BOOTSEL devices and cannot target an
// app-mode board when both Picos are connected.
static void bootsel_command(uint32_t now) {
    static uint32_t armed_ms = 0;
    if (armed_ms != 0u && (uint32_t)(now - armed_ms) < 1000u) {
        reset_usb_boot(0, 0);
    }
    armed_ms = now;
}

#ifdef PHANTOM_BRINGUP_TRACE
static uint32_t stage_debug_iter = 0;
#define LOOP_DONE(s) do { if (stage_debug_iter < 3u) printf("STAGE %s\n", (s)); } while(0)
#define LOOP_ITER() (stage_debug_iter++)
#else
#define LOOP_DONE(s) ((void)0)
#define LOOP_ITER() ((void)0)
#endif

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
    txq_init(&tx_queue);
    proto_msg_rx_init(&rx_msg, GLYPHS, (uint8_t)(sizeof(GLYPHS) - 1));
    proto_msg_tx_init(&tx_msg, GLYPHS, (uint8_t)(sizeof(GLYPHS) - 1));
    drawMainUI();
    trace_checkpoint("T0");

    IP_ADDR4(&ap_addr, 192, 168, 4, 1);

    if (!proto_crypto_self_test()) halt_error("CRYPTO FAIL");
    if (!x25519_selftest()) halt_error("ECDH FAIL");
    printf("ECDH selftest PASS\n");
    trace_checkpoint("T1");
    psk_derive_keys();
    trace_checkpoint("T1a");
    zeroize_tracker_init(&zeroizer);
    register_key_secrets();
    trace_checkpoint("T1b");
    sfo_telemetry_init(&sfo_tel);
    trace_checkpoint("T1c");
    reconnect_init(&rc);
    trace_checkpoint("T1d");
    if (watchdog_enable_caused_reboot()) {
        printf("RESET reason: watchdog\n");
    }
#ifdef PHANTOM_USE_AEAD
    if (!aead_self_test()) halt_error("AEAD FAIL");
    active_suite = &pc_suite_aead_adapter;
    printf("SUITE ChaCha20-Poly1305\n");
#endif
    status_load();
    trace_checkpoint("T2");
    {
        uint8_t i;
        for (i = 0; i < 6u; i++) {
            if (ap_bssid[i] != 0u) { bssid_enrolled = true; break; }
        }
    }

    trace_checkpoint("T3");
    if (!wifi_init()) halt_error("WIFI FAIL");
    trace_checkpoint("T7");
    if (!udp_link_init()) halt_error("UDP FAIL");
    trace_checkpoint("T8");

    // Arm only after CYW43/UDP bring-up. Enabling before wifi_init() made a
    // slow first association trip the 8 s watchdog and boot-loop the pair.
#ifdef PHANTOM_BRINGUP_TRACE
    // Diagnostic: watchdog off entirely so we can observe a full boot cycle
    // without resets masking the real behavior.
#else
#ifdef PHANTOM_WATCHDOG_TEST
    // Arm at LINK UP (not at boot) in the test build: Wi-Fi association can
    // exceed 8 s, which would reset the board before the loop feeds it.
    // Production arms here because it has no forced hang; the test build
    // defers arming until set_link_up().
#else
    watchdog_enable(8000u, false);
#endif
#endif

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    uint32_t next_draw_ms = 0;

    while (true) {
        uint32_t now = now_ms();
        LOOP_ITER();

        LOOP_DONE("poll");
        cyw43_arch_poll();
        watchdog_update();

        /* Complete deferred ECDH here on the shallow main-loop stack. */
        if (ecdh_derive_pending) {
            ecdh_derive_pending = false;
            sess_keys_ready = false;
            if (derive_session_keys(pending_peer_pub)) {
                set_link_up(pending_link_reason ? pending_link_reason : "ECDH");
            } else {
                printf("ECDH derive failed (deferred)\n");
            }
            memset(pending_peer_pub, 0, sizeof pending_peer_pub);
            pending_link_reason = NULL;
            watchdog_update();
        }

        int ch = getchar_timeout_us(0);
        LOOP_DONE("char");
        if (ch == 'S' || ch == 's') status_dump();
        if (ch == 'Z' || ch == 'z') request_teardown("serial-test");
        if (ch == 'B' || ch == 'b') bootsel_command(now);
#ifdef PHANTOM_DATA_PROBE
        // 'T' injects a short message through the real DATA/ratchet TX path
        // so the per-frame key derivation can be verified on hardware.
        if (ch == 'T' || ch == 't') {
            static const uint8_t probe[] = {0x01u, 0x00u, 0x41u, 0x42u};
            printf("DATA probe: %s\n",
                   send_data_frame(probe, sizeof probe, PROTO_FLAG_FINAL)
                       ? "sent" : "failed");
        }
        // 'X' sends the same probe with a flipped auth byte; the peer must
        // reject it (CRYPTO FAIL) rather than deliver it.
        if (ch == 'X' || ch == 'x') {
            static const uint8_t probe[] = {0x01u, 0x00u, 0x41u, 0x42u};
            probe_tamper_next = true;
            printf("DATA tamper probe: %s\n",
                   send_data_frame(probe, sizeof probe, PROTO_FLAG_FINAL)
                       ? "sent" : "failed");
        }
#endif

        LOOP_DONE("wifi");
        wifi_service(now);
        LOOP_DONE("input");
        input_tick(now);
        LOOP_DONE("net");
        net_tick(now);
        LOOP_DONE("tx");
        tx_tick(now);
        LOOP_DONE("hud");
        hud_tick(now);
        LOOP_DONE("stats");
        session_stats_tick(now);
        LOOP_DONE("sfo");
        sfo_report_tick(now);
        LOOP_DONE("status");
        watchdog_update();
        status_save(now);
        watchdog_update();
        LOOP_DONE("poststatus");

        if ((int32_t)(now - next_draw_ms) >= 0) {
            LOOP_DONE("draw");
            drawMainUI();
#ifdef PHANTOM_BRINGUP_TRACE
            {
                char tb[16];
                snprintf(tb, sizeof tb, "L%lu", (unsigned long)(now / 1000u));
                oled_draw_custom_string(0, 0, tb);
                oled_show();
            }
#endif
            next_draw_ms = now + 50;
        }

        LOOP_DONE("sleep");
        sleep_ms(5);
    }
}

