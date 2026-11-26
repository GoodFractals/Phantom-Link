# 33: Per-message key ratchet

**What to build:** Rotate or evolve traffic keys per message so compromise of
one key does not expose the whole session.

**Blocked by:** 32

**Status:** closed (hardware verified 2026-11-26: per-frame ratchet keys derived on TX and recovered on RX, zero rollbacks/crypto errors)

- [x] Host-testable ratchet with pinned KAT vectors and rollback rejection.
- [x] Key evolution binds message ID (direction supplied by the caller's master key).
- [x] Out-of-order delivery within the 8-slot window still decrypts, including uint32 wraparound.
- [x] Ratchet state is RAM-only; document that it does not survive reboot (session does not either).
- [x] No change to wire length budget or timing constants.

## Verification

2026-11-23 — `proto_ratchet` host module with 7 test functions, TDD red→green,
MSVC /W4 /WX clean, `ctest` 17/17.

2026-11-26 — Firmware wired: `ratchet_init` on each ECDH handshake with the
directional session keys; DATA frames use `ratchet_advance(tx, frame.seq)` to
seal and `ratchet_advance(rx, frame.seq)` to open. Verified with the `T` serial
probe (PHANTOM_DATA_PROBE build): STA `tx_key=1`, AP `rx_key=1`, both
`rollbacks=0`, `errors auth=0 crypto=0`, shared session `0xb91fd74e`. Each
frame gets a distinct key from `pc_kdf(master, LE32(seq), 0x04)`.
