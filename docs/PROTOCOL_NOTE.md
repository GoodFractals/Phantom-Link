# Phantom Link v1 — protocol note

Two Pico 2 W boards run the same `main.c`; `IS_AP` selects only the Wi-Fi role.
Both boards type, render, send, and receive.

## 1. Frame format (v1, frozen)

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | Magic `B`,`C` |
| 2 | 1 | Version = 1 |
| 3 | 1 | Type (HELLO, HELLO_ACK, DATA, ACK, PING, PONG, BYE, SFO, ERROR) |
| 4 | 1 | Flags (ACK_REQ, ENCRYPTED, FINAL, SFO_SYNC) |
| 5 | 1 | Payload length (0–64) |
| 6 | 4 | Session ID, little-endian |
| 10 | 4 | Sequence, little-endian |
| 14 | 4 | ACK, little-endian |
| 18 | n | Payload |
| 18+n | 8 | Auth field: CRC32 + zeros (plaintext) or 8-byte CMAC (encrypted) |

Max wire length: 90 bytes. DATA payloads are padded to a constant 64 bytes with
a length byte and random padding before encryption, so all DATA frames have the
same size.

## 2. Session and reliability

Each handshake generates a hardware-random 32-bit session ID. Every outgoing
frame consumes a per-direction sequence starting at 0. The receiver keeps a
32-bit sliding replay window with duplicate/stale/wrong-session counters.

Messages are 21 glyphs from a 49-code Base-6 transport alphabet (3 digits per
code). Whole messages are retransmitted from the first missing hole; ACKs occur
at every eighth symbol and on FINAL; retries are bounded with a `NO ACK` notice.
TX is a bounded 64-deep queue drained one frame per main-loop tick; DELETE
aborts and drains it.

## 3. Key schedule and crypto

The 32-byte PSK is a pairing boundary, not a secret against physical possession
(ADR 0004). `proto_psk.h` / `netcfg_local.h` hold a per-build override: SSID,
password, and enrolled AP BSSID.

- KDF: CMAC-based (SP 800-108 style), role-based directional keys. Label
  `0x01` = AP→STA, `0x02` = STA→AP; both boards derive the same key per direction.
- Session-ID and ECDH values are bound into the salt by the forward-secrecy
  layer to prevent cross-session keystream reuse.
- Encryption: Speck128/128 CTR, nonce `0x01 || seq(4) || 0^7`, 32 rounds.
- Authentication: RFC 4493 CMAC, truncated to the first 8 bytes (MSB-first),
  over the 18-byte header as AAD plus the ciphertext. Verify before decrypt;
  constant-time tag compare; failure increments `crypto` and drops the frame.
- HELLO/HELLO_ACK carry an 8-byte nonce + 8-byte confirmation tag bound to
  session ID, sequence, nonce, and direction label. Wrong PSK → `AUTH FAILED`.
- Header fields (session/seq/ack) are XOR-masked with a key-derived stream;
  this is obfuscation, not encryption. An AEAD suite slot is reserved but
  ChaCha20-Poly1305 is not wired in v1.

## 4. Identity and SFO caveat

The STA joins only the enrolled AP BSSID when one is configured, verifies the
connected BSSID at runtime, and counts `BSSID MISMATCH` events. SSID alone is not
an identity.

True StegoFrameOrder reorders 802.11 MAC frames and requires monitor mode and
raw frame injection; the CYW43439 cannot do this (ADR 0002). Phantom Link's SFO
is an **application-layer telemetry channel**: four UDP datagrams per burst,
arrival permutation encodes one nibble, 16 of 24 orders are used, CRC8 and
three-repeat majority voting reject corruption. It carries no user message
content and never gates delivery; failed bursts log `SFO degraded`.

## 5. Claims we do not make

No MAC-layer SFO, no covertness, no forward secrecy in the shipped v1 crypto
(forward-secrecy modules are host-green but not device-integrated), no secure
element, no independent audit, and no protection against physical possession
or coercion. See ticket 38's Limitations text.
