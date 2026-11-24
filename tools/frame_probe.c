/* Builds Phantom Link frames for hardware rejection probes. Prints hex only.
 * Modes:
 *   replay <session_hex> <seq>        stale PING, valid CRC, rejected by window
 *   tamper <session_hex> <seq>        DATA with valid CMAC then one bit flipped
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proto_frame.h"
#include "proto_crypto.h"
#include "proto_lpi.h"
#include "proto_psk.h"

#define SALT "BONNIE-CLYDE-v1"

static const uint8_t ZERO_NONCE[PC_NONCE_BYTES] = {0};

static void frame_nonce(uint32_t seq, uint8_t nonce[PC_NONCE_BYTES]) {
    nonce[0] = 0x01u;
    memcpy(nonce + 1u, &seq, 4u);
    memset(nonce + 5u, 0, PC_NONCE_BYTES - 5u);
}

static void derive_sta_to_ap(uint8_t key[16]) {
    static const uint8_t psk[PC_MASTER_KEY_BYTES] = PHANTOM_PSK_BYTES;
    uint8_t derived[PC_DERIVED_KEY_BYTES];
    pc_kdf(psk, (const uint8_t *)SALT, strlen(SALT), 0x02u, derived);
    memcpy(key, derived, 16u);
}

static void print_hex(const uint8_t *buf, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) printf("%02X", buf[i]);
    printf("\n");
}

int main(int argc, char **argv) {
    uint8_t key[16], buf[PROTO_MAX_FRAME_LEN], padded[LPI_WIRE_PAYLOAD];
    proto_frame_view_t view;
    size_t len = 0;
    uint32_t session, seq;
    uint8_t nonce[PC_NONCE_BYTES];

    if (argc < 4) {
        fprintf(stderr, "usage: %s replay|tamper <session_hex> <seq>\n", argv[0]);
        return 2;
    }
    session = (uint32_t)strtoul(argv[2], NULL, 16);
    seq = (uint32_t)strtoul(argv[3], NULL, 10);
    derive_sta_to_ap(key);

    memset(&view, 0, sizeof view);
    view.version = (uint8_t)PROTO_VERSION;
    view.session_id = session;
    view.seq = seq;
    view.ack = 0;

    if (strcmp(argv[1], "replay") == 0) {
        view.type = PROTO_TYPE_PING;
        view.payload_len = 0;
        view.payload = NULL;
        if (proto_frame_encode(buf, sizeof buf, &view, &len) != PROTO_OK) return 3;
        pc_ctr_xcrypt(key, ZERO_NONCE, buf + 6u, 12u);
        if (proto_frame_put_crc(buf, len) != PROTO_OK) return 4;
        print_hex(buf, len);
        return 0;
    }

    if (strcmp(argv[1], "tamper") == 0) {
        static const uint8_t msg[] = "HELLO";
        view.type = PROTO_TYPE_DATA;
        view.flags = PROTO_FLAG_ENCRYPTED;
        view.payload_len = LPI_WIRE_PAYLOAD;
        view.payload = padded;
        lpi_pad(msg, sizeof msg - 1u, padded, NULL);
        if (proto_frame_encode(buf, sizeof buf, &view, &len) != PROTO_OK) return 5;
        pc_ctr_xcrypt(key, ZERO_NONCE, buf + 6u, 12u);
        frame_nonce(seq, nonce);
        pc_suite_speck_ctr_cmac.seal(key, nonce, buf, PROTO_HEADER_LEN,
                                     buf + PROTO_HEADER_LEN, LPI_WIRE_PAYLOAD,
                                     buf + PROTO_HEADER_LEN + LPI_WIRE_PAYLOAD);
        buf[PROTO_HEADER_LEN] ^= 0x80u; /* single-bit ciphertext tamper */
        print_hex(buf, len);
        return 0;
    }

    fprintf(stderr, "unknown mode: %s\n", argv[1]);
    return 2;
}
