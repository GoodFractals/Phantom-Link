# Phantom Link security layer — quick reference

| Layer | Mechanism | Guarantees | Does NOT guarantee |
|---|---|---|---|
| Wi-Fi | WPA2-AES, SSID `TMOBILE-1219` | Link-layer encryption to the AP | Identity of which AP (SSID ambiguous) |
| Identity | BSSID lock + runtime `cyw43_wifi_get_bssid` check | Joins only the enrolled hardware address | Protection if BSSID is cloned |
| Handshake | HELLO/HELLO_ACK confirm tag + ephemeral X25519 | PSK confirmation; ECDH session keys; wrong PSK → AUTH FAILED | Live impersonation by a PSK holder; full-pubkey binding |
| Payload | Speck CTR (or opt-in ChaCha20-Poly1305), per-frame ratchet key | Confidentiality of payload bytes; one frame key is not the session | Hiding traffic existence/size |
| Integrity | 8-byte truncated CMAC / Poly1305 over masked-header AAD + ciphertext | Tamper detection; verify before decrypt | 128-bit authentication; replay protection by itself |
| Replay | 32-bit sliding window + session IDs | Duplicate/stale rejection | Authentication (the tag does that) |
| Metadata | XOR mask over session/seq/ack with ECDH-derived keys after handshake | Less obvious metadata; PSK-only leak cannot strip it | Header privacy; hiding type, length, timing |
| LPI | Fixed length, decoys, jitter | Less bursty traffic | Covertness; measured detection resistance |

Key facts:
- KDF is CMAC-based with two role-based keys: AP→STA (label 0x01) and STA→AP (label 0x02). Both boards derive the same key for a given direction.
- Traffic keys mix a **fresh X25519 scalar per session** (generated in `adopt_session()` / the next HELLO) with the PSK and `session_id` (tickets 31, 37). A later PSK compromise cannot decrypt old sessions.
- DATA frames use a distinct key per frame from the ratchet (`pc_kdf(master, LE32(seq), 0x04)`), so retransmits with fresh padding never reuse a `(key, nonce)` pair (ticket 33).
- Header fields are masked with domain-separated ECDH keys; the old PSK-mask path is only for HELLO/HELLO_ACK. The separately-tagged `proto_hdr` envelope is host code, **not** on the wire (ticket 34).
- CTR keystream must never repeat for the same key; sequence is unique per direction within a session.
- CMAC construction is RFC 4493; tag is the first 8 bytes (MSB-first). Forgery bound is `2^-64` per attempt — adequate for this PoC, not a 128-bit claim.
- AEAD suite is implemented as a compile-time opt-in (`PHANTOM_USE_AEAD`) and hardware-verified; Speck remains the default (ticket 32).
- PSK is a pairing boundary; anyone with flash access can read it (ADR 0004, ticket 38).
- 32-bit sequence wrap is not handled by the replay window; document as a limitation.

Sources: ADR 0001/0004, RFC 4493, NSA Speck guide, `proto_crypto.c`, tickets 10–14, 31–39.
