# 34: Encrypt header fields

**What to build:** Move beyond masking: encrypt session_id, seq, and ack inside
the AEAD envelope, leaving only routing-visible fields in clear.

**Blocked by:** 32

**Status:** closed (hardware verified 2026-11-26: header fields masked with ECDH-derived keys after handshake, peer decrypts with zero wrong-session/crypto errors). See Verification for the deliberate scope boundary.

- [x] Documented encrypted-header envelope in `proto_hdr.h` (6-byte clear AAD, 12-byte encrypted secret, 8-byte tag).
- [x] Firmware masks bytes 6..17 with ECDH-derived keys for all frames after the handshake; HELLO/HELLO_ACK keep the PSK mask because the peer has no ECDH keys yet.
- [x] `PROTO_FLAG_EPH_MASK` (clear prefix, bit 4) tells the receiver which key to use, so handshake transitions cannot desync the mask state.
- [x] Mask keys are domain-separated from traffic keys via `pc_kdf(..., 0x05)`; the mask nonce is bound to `session_id`, so the keystream changes each handshake.
- [x] Receiver unmasks before session/replay processing and verifies with the existing AEAD/CRC tag (no wire-length change).
- [x] AAD covers only clear fields; `hdr_open` requires seq from session state to rebuild the nonce.
- [x] Replay rejection compatible: decrypted seq is checked against the supplied session seq.
- [x] Host tests cover tamper, wrong-key, wrong-seq, and sentinel-unchanged-on-failure.

## Verification

2026-11-23 — `proto_hdr` host module with exhaustive single-bit AAD/CT/tag
tamper coverage, TDD red→green, MSVC /W4 /WX clean, `ctest` 18/18.

2026-11-26 — Firmware integration. The gap this closes is concrete: the old
`mask_header_fields()` used the **static PSK keys**, so anyone with the leaked
PSK could read session_id/seq/ack even after ticket 31 moved payload keys to
ECDH. Header masking now uses domain-separated ECDH-derived keys for every
post-handshake frame. Verified with the `T` DATA probe and a 95 s soak:
STA/AP `link=UP`, shared session `0x82473266`, `wrong=0`, `crypto=0`,
`hdr_mask eph_tx/eph_rx` incrementing on both boards, DATA frame decrypted on
the peer (`ratchet rx_key=1`, `tx_key=1`).

**Scope boundary (do not overclaim):** the separately-tagged `proto_hdr`
envelope (6 clear + 12 ciphertext + 8-byte tag) is host-green but is **not**
deployed on the wire. The firmware keeps the v1 18-byte header and 8-byte auth
field; header ciphertext is covered by the existing AEAD tag for DATA frames
and by CRC32 for control frames (which carry no secrets). Wire length and all
timing constants are unchanged, so every existing tool stays compatible.

**Residual limitation (v1):** the mask nonce is fixed per session (`session_id`),
so the keystream repeats across frames in one session. The traffic payload does
not rely on it — DATA frames get a distinct per-frame key from ticket 33's
ratchet — and the masked fields are low-sensitivity metadata (random session
id, incrementing seq, ack). Fixing this completely needs a per-frame clear
nonce and therefore a wire-format change; deferred, not claimed.
