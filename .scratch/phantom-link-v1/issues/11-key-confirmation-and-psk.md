# 11: Key confirmation and PSK handling

**What to build:** `HELLO` and `HELLO_ACK` carry nonces and a key-confirmation tag over the session. A peer with a different PSK reaches an explicit AUTH FAILED state and never LINK UP.

**Blocked by:** 04, 10 (10 closed)

## Verification

2026-11-22 — host tests green: key confirmation binds session ID, sequence,
nonce, and direction label; wrong key, wrong label, wrong session, and tampered
tag are all rejected. Firmware builds clean; `ctest` 10/10 and
`HOST VALIDATION: PASS`. Hardware same-PSK/wrong-PSK checks join ticket 14.

2026-11-22 — fixed a tag-construction bug: `pc_confirm_tag()` wrote the label
at input byte 12 (clobbering a nonce byte) while byte 16 stayed uninitialized,
making tags nondeterministic and handshakes fail with `AUTH FAILED`. Label now
at byte 16, with a pinned KAT and determinism check. Derived keys are also
role-based (AP→STA / STA→AP) so both boards use the same key per direction.
Hardware verified on build a6cdff30: LINK UP, `auth=0`, `crypto=0`, decoys
running.

**Status:** ready-for-human (host green; hardware AUTH tests join ticket 14)

- [x] Same-PSK boards exchange key confirmation in HELLO/HELLO_ACK (code complete).
- [x] Different-PSK builds reach an explicit AUTH FAILED state (code complete).
- [x] A missing or malformed tag drops the handshake and is counted (`auth` counter).
- [x] PSK override is documented in `proto_psk.h`, with `tools/gen_psk.ps1`.
- [x] Replaying an old confirmation tag cannot establish a new session (tag binds session_id + seq).
