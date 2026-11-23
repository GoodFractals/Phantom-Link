# Speck / CMAC research notes for ticket 10

Research output from a read-only subagent, 2026-11-21. No implementation has
started; ticket 09 still gates ticket 10.

## Existing seam

- No `proto_crypto.*` exists yet. The only seam is documented intent in
  `firmware/proto/proto_frame.h:11-18`: CRC32 occupies the 8-byte auth field
  today; the crypto layer will replace it with a truncated CMAC.
- `docs/adr/0001` and `docs/GAMEPLAN.md:391` require a cipher-agnostic
  `proto_crypto` with an AEAD slot.
- Wire format is frozen: payload at offset 18, 8-byte auth at `18+n`, max frame
  90 bytes. Do not touch `proto_frame.*`, `proto_session.*`, or `proto_spec.h`.
- Host tests: C11, `-Wall -Wextra -Werror`, one exe per test, `phantom_proto`
  static lib, `CHECK()` macro style, KAT + fuzz conventions.

## Speck128/128 KAT vectors

**Vector A (official NSA paper Appendix C, byte form):**
- key `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f`
- plaintext `20 6d 61 64 65 20 69 74 20 65 71 75 69 76 61 6c` (`" made it equival"`)
- ciphertext `65 32 78 79 51 98 5d a6 18 0d 57 5c df fe 60 78`
- Sources: [IACR ePrint 2013/404](https://eprint.iacr.org/2013/404) Appendix C;
  [Linux ARM patch thread with author byte-order clarification](https://lists.infradead.org/pipermail/linux-arm-kernel/2018-February/559214.html)

**Vector B (Crypto++ independent vector):**
- key `75 28 9e 33 d1 8b df c9 c6 89 b2 9a 43 cb f3 f2`
- plaintext `51 c1 6c d9 af ab 4f 1d 32 66 58 f8 9c 06 c9 40`
- ciphertext `c0 03 c0 49 76 84 78 9b 3a 6c cb e8 e7 f9 8d 9e`
- Source: [Crypto++ TestVectors/speck.txt](https://github.com/weidai11/cryptopp/blob/master/TestVectors/speck.txt)

## CMAC known-answer tests (RFC 4493, AES-backed construction)

Key `2b7e1516 28aed2a6 abf71588 09cf4f3c`:
- empty → `bb1d6929 e9593728 7fa37d12 9b756746`
- 16 B `6bc1bee2 2e409f96 e93d7e11 7393172a` → `070a16b4 6b4d4144 f79bdd9d d04a287c`
- 40 B → `dfa66747 de9ae630 30ca3261 1497c827`
- 64 B → `51f0bebf 7e3b9d92 fc497417 79363cfe`
- Subkeys: K1 `fbeed618 35713366 7c85e08f 7236a8de`, K2 `f7ddac30 6ae266cc f90bc11e e46d513b`
- Source: [RFC 4493](https://www.rfc-editor.org/rfc/rfc4493.txt)

Caveat: these are AES-CMAC tags. They validate the CMAC construction only;
there is no authoritative Speck-CMAC vector. Validate the generic CMAC wrapper
with AES first (or pinned K1/K2), then pin locally generated Speck-CMAC tags as
KATs from an independent reference implementation.

KDF design reference: [NIST SP 800-108r1](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-108r1.pdf) counter-mode with CMAC as PRF.

## Minimal interface shape (sketch)

New SDK-free `firmware/proto/proto_crypto.h/.c`:
- primitive: `speck128_set_key`, `speck128_encrypt` (32 rounds)
- generic CMAC over any 128-bit block function: `pc_cmac_subkeys`, `pc_cmac`,
  `pc_cmac_trunc` (8-byte MSB-first tag)
- `pc_ctr_xcrypt(key, nonce[12], in, out, len)`; nonce = direction||seq||zero
- `pc_kdf(master, salt, label)` with `0x01`/`0x02` domain separation
- suite ops with `seal`/`open`; `pc_suite_speck_ctr_cmac` plus an AEAD slot
  (`pc_suite_aead`) reserved for ChaCha20-Poly1305; session layer never branches
  on cipher.
- `proto_crypto_self_test()` runs the KAT table; boot must block LINK UP on fail.

Add `proto_crypto.c` to `phantom_proto` and the firmware target; add
`tests/test_crypto.c` with KATs, CTR round-trip, K_send != K_recv, self-test.

## Embedded C gotchas

1. Speck byte/word order is the #1 trap: paper words are little-endian bytes in
   `(y,x)` order; Crypto++ shows the same bytes as big-endian words. Pin both
   KAT forms.
2. No unaligned `uint64_t*` casts; use `memcpy` or explicit byte shifts.
3. Rotation on `uint64_t` with counts 8/3; avoid `<< 64`.
4. CTR increments only the low 32 bits of the 96-bit nonce; never reuse
   `(direction, seq)` — retransmits reuse keystream (flag for ticket 12).
5. CMAC subkey shifts are MSB-first on the canonical byte string; handle
   empty / exact-multiple / partial block cases explicitly.
6. Truncate CMAC to first 8 bytes MSB-first for `PROTO_AUTH_LEN`; use
   constant-time tag compare; never log key/tag material.
7. Keep derived keys in one context struct, best-effort zeroize; no SDK headers
   in crypto so host and firmware share one source.
