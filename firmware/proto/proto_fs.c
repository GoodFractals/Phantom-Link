#include "proto_fs.h"

#include <string.h>

#include "proto_crypto.h"

void fs_derive_keys(const uint8_t psk[FS_KEY_BYTES],
                    const uint8_t ecdh_shared[FS_KEY_BYTES],
                    uint32_t session_id,
                    const uint8_t *salt, size_t salt_len,
                    uint8_t ap_to_sta[FS_KEY_BYTES],
                    uint8_t sta_to_ap[FS_KEY_BYTES]) {
    uint8_t mixed[FS_KEY_BYTES];
    uint8_t bound[64];
    size_t total = 4u + salt_len;

    if (total > sizeof bound) total = sizeof bound;
    bound[0] = (uint8_t)session_id;
    bound[1] = (uint8_t)(session_id >> 8u);
    bound[2] = (uint8_t)(session_id >> 16u);
    bound[3] = (uint8_t)(session_id >> 24u);
    if (salt_len) memcpy(bound + 4u, salt, salt_len > sizeof bound - 4u ? sizeof bound - 4u : salt_len);

    /* Domain-separate the ephemeral ECDH secret from the long-term PSK, then
     * bind the session ID so keystream cannot be reused across sessions. */
    pc_kdf(psk, ecdh_shared, FS_KEY_BYTES, 0x03u, mixed);
    pc_kdf(mixed, bound, total, 0x01u, ap_to_sta);
    pc_kdf(mixed, bound, total, 0x02u, sta_to_ap);
}

void fs_session_init(fs_session_t *s) {
    if (!s) return;
    s->epoch = 0u;
    s->active = false;
}

bool fs_session_adopt(fs_session_t *s, uint32_t epoch) {
    if (!s || epoch == 0u) return false;
    if (s->active && epoch <= s->epoch) return false;
    s->epoch = epoch;
    s->active = true;
    return true;
}

bool fs_session_accept_epoch(const fs_session_t *s, uint32_t epoch) {
    return s && s->active && epoch == s->epoch;
}
