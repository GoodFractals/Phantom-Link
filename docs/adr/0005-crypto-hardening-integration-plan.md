# ADR 0005 — Crypto-hardening integration plan (tickets 31, 33, 34, 32)

Status: proposed (host modules green; device integration not started)
Date: 2026-11-25

## Context

Four host-only crypto modules exist and pass their tests but are **not** in the
firmware build and **not** wired into `main.c`:

| Ticket | Module | Host test | What it adds |
|---|---|---|---|
| 31 | `proto_fs.c` / `proto_fs.h` | `test_fs` | Forward secrecy: session keys from ECDH + PSK, ephemeral, not stored |
| 33 | `proto_ratchet.c` / `proto_ratchet.h` | `test_ratchet` | Per-message key derivation (label 0x04, 8-slot window) |
| 34 | `proto_hdr.c` / `proto_hdr.h` | `test_hdr` | Encrypted header (session/seq/ack) with AAD + tag |
| 32 | `proto_aead.c` / `proto_aead.h` | `test_aead` | ChaCha20-Poly1305 suite option |

`firmware/CMakeLists.txt` builds only: frame, session, msg, txq, crypto, lpi,
sfo, sfo_telemetry, reconnect, zeroize, aead — note **aead.c is already in the
firmware build** (behind `PHANTOM_USE_AEAD`), but fs/ratchet/hdr are not.

## Why this is a wave, not four independent commits

All four change the wire format or the key schedule. A board with any one of
them enabled cannot talk to a board without it. So they must be integrated,
flashed to **both** boards together, and verified as one acceptance pass.

## Integration surface (exact call sites in main.c)

1. **Build**: add `proto/proto_fs.c`, `proto/proto_ratchet.c`, `proto/proto_hdr.c`
   to `add_phantom_target()` sources. Link `pico_mbedtls` for X25519 on device.
2. **Includes**: add `proto_fs.h`, `proto_ratchet.h`, `proto_hdr.h`.
3. **Key derivation** (`psk_derive_keys`, ~line 287): after the static directional
   derivation, on each new session call `fs_derive_keys(psk, ecdh_shared,
   session_id, salt, ap_to_sta, sta_to_ap)`. The ECDH shared secret comes from
   an ephemeral key pair generated per session (X25519 via mbedTLS).
4. **Handshake** (`PROTO_TYPE_HELLO` / `HELLO_ACK` handling, ~line 922): carry
   the ephemeral public key in the handshake payload; run `fs_session_adopt`
   with a new epoch on each accepted handshake. HELLO_ACK must include the
   responder's public key so both sides derive the same shared secret.
5. **Frame seal** (`send_frame_to`, ~line 398): before `pc_ctr_xcrypt` on the
   payload, call `hdr_seal` on the 18-byte header to encrypt session/seq/ack;
   derive the per-message key with `ratchet_key_for(&tx_ratchet, msg_id)`.
6. **Frame open** (`udp_recv_callback`, ~line 1048): call `hdr_open` with the
   session seq the receiver expects; reject on tag mismatch **before** session
   or replay processing. Then `ratchet_key_for(&rx_ratchet, msg_id)`.
7. **Zeroization** (`request_teardown`, ~line 634): register the ephemeral
   private key, ECDH shared secret, and both ratchet states with `zeroizer` so
   BYE clears them along with the static keys.

## Wire-format changes

- Header bytes 6..17 (session_id, seq, ack) become ciphertext + tag instead of
  masked/clear. Frames grow by the header tag size — `PROTO_MAX_FRAME_LEN` must
  be checked and the SFO burst payload re-verified against it.
- HELLO/HELLO_ACK payloads grow by 32 bytes (public key). The existing 16-byte
  handshake payload and key-confirmation tag must be extended consistently on
  both sides.
- Because `hdr_open` needs the expected seq supplied by the caller, the receive
  path must pass `expected_rx_seq` from `proto_session` state, not read it from
  the wire.

## Acceptance (per ticket)

- 31: a captured session cannot be decrypted with a later PSK compromise.
  Device self-test must gate LINK UP on KATs.
- 33: per-message keys distinct; a compromised message key does not reveal
  others; 8-slot out-of-order window works on hardware.
- 34: receiver decrypts header before session/replay processing; tampered
  header rejected before any state mutation.
- 32: `PHANTOM_USE_AEAD=1` on both boards, T2–T12 + tamper re-run.

## Order and risk

Do 31 → 33 → 34 → 32. Each step changes the handshake or frame format, so
**never flash one board at a time** with a new step — the pair must match.
Keep the current production build (`f6a8114`) recoverable; back up both UF2s
before each step. Verify LINK UP + a full T2–T12 pass after each step.

## Decision

Defer implementation until the user is present for the paired flash + hardware
verification. This document is the prep work; it is not an implementation.
