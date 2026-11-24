# Phantom Link security layer — quick reference

| Layer | Mechanism | Guarantees | Does NOT guarantee |
|---|---|---|---|
| Wi-Fi | WPA2-AES, SSID `TMOBILE-1219` | Link-layer encryption to the AP | Identity of which AP (SSID ambiguous) |
| Identity | BSSID lock + runtime `cyw43_wifi_get_bssid` check | Joins only the enrolled hardware address | Protection if BSSID is cloned |
| Handshake | HELLO/HELLO_ACK nonce + CMAC tag | PSK confirmation; wrong PSK → AUTH FAILED | Forward secrecy |
| Payload | Speck128/128 CTR, nonce `0x01||seq||0^7` | Confidentiality of payload bytes | Hiding traffic existence/size |
| Integrity | 8-byte truncated CMAC over header AAD + ciphertext | Tamper detection; verify before decrypt | Replay protection by itself |
| Replay | 32-bit sliding window + session IDs | Duplicate/stale rejection | Authentication (CMAC does that) |
| Metadata | XOR mask over session/seq/ack bytes | Less obvious metadata | Full header privacy |
| LPI | Fixed length, decoys, jitter | Less bursty traffic | Covertness |

Key facts:
- KDF is CMAC-based with two role-based keys: AP→STA (label 0x01) and STA→AP (label 0x02). Both boards derive the same key for a given direction.
- CTR keystream must never repeat for the same key; sequence is unique per direction within a session.
- CMAC construction is RFC 4493; tag is the first 8 bytes (MSB-first).
- AEAD suite slot is reserved but not implemented (`pc_suite_aead` is a stub).
- PSK is a pairing boundary; anyone with flash access can read it (ADR 0004).

Sources: ADR 0001/0004, RFC 4493, NSA Speck guide, `proto_crypto.c`, tickets 10–12.
