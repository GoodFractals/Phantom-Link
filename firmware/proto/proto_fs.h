#ifndef PHANTOM_FS_H
#define PHANTOM_FS_H

/* Forward secrecy for Phantom Link sessions.
 *
 * The X25519 primitive is supplied by mbedTLS on the device (Pico SDK 2.3.1
 * ships lib/mbedtls). This module is SDK-free and validates the KDF/epoch
 * binding with RFC 7748 shared-secret vectors on the host.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FS_KEY_BYTES 32u

void fs_derive_keys(const uint8_t psk[FS_KEY_BYTES],
                    const uint8_t ecdh_shared[FS_KEY_BYTES],
                    uint32_t session_id,
                    const uint8_t *salt, size_t salt_len,
                    uint8_t ap_to_sta[FS_KEY_BYTES],
                    uint8_t sta_to_ap[FS_KEY_BYTES]);

typedef struct {
    uint32_t epoch;
    bool active;
} fs_session_t;

void fs_session_init(fs_session_t *s);
bool fs_session_adopt(fs_session_t *s, uint32_t epoch);
bool fs_session_accept_epoch(const fs_session_t *s, uint32_t epoch);

#endif
