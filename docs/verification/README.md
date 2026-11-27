# Verification stack (ticket 37)

Layered evidence for the claim "the protocol logic is bounded and the link is
hardware-verified." These are **model-level proofs** of specific properties,
not a proof that the C implementation is bug-free. The C implementation is
covered by `ctest` (18/18), source assertions (`validate_assets.py`), and the
hardware acceptance logs.

## What is actually proven / run

| Layer | Tool | Status | What it checks |
|---|---|---|---|
| State machine | **TLA+ / TLC** | **RUN, PASS** (2026-11-26) | Session model: LINK UP never without ECDH keys, no zero session when up, keys need a session, pending needs HELLO. 13 distinct states, graph depth 5. |
| Arithmetic | **Z3** | **RUN, PASS** (2026-11-26) | 85-glyph length ceiling, TX queue depth, serial-number comparison asymmetry, wraparound, 8-slot window membership, ACK position order. |
| C codec | **CBMC** | **STAGED, NOT RUN** — Windows installer failed (MSI 1603) | Frame decode bounds and encode/decode round-trip for payload 0..64. Harness in `cbmc/frame_harness.c`. |
| C implementation | **ctest** | **RUN, PASS** 18/18 | Host tests for frame, session, msg, ratchet, AEAD, zeroize, reconnect, SFO. |
| Source assertions | `validate_assets.py` | **RUN, PASS** | Font, codec, tap groups, box coordinates, length constants. |
| Physical | Serial captures | **RUN, PASS** | RF capture, replay/tamper, watchdog, reconnect, soak, power cycle. |

## How to run what works

```powershell
# TLA+ model check
cd docs\verification\tla
java -cp <path-to>\tla2tools.jar tlc2.TLC session.tla -config session.cfg -workers 4

# Z3 arithmetic proofs
py -3 docs\verification\z3\protocol_bounds.py
```

`tla2tools.jar` is downloaded to the temp workspace; install it somewhere
stable if you want to keep re-running it. Z3 is installed as a user package
(`py -3 -m pip install z3-solver`).

## Honest boundary

- TLA+ proves properties of the **model**, not the firmware. The model is
  deliberately small (session IDs 0..3) so TLC can exhaust it.
- Z3 proves arithmetic facts about wire limits and serial-number rules. It
  cannot prove a KDF is collision-resistant or that the radio works.
- CBMC, once run, checks the C codec only within the given bounds.
- None of these replace hardware acceptance. They make the claim surface
  narrower and more defensible — not "the codebase has no bugs."

## TODO when CBMC is available

- Run `cbmc/frame_harness.c` and record the result here.
- Add harnesses for `proto_msg.c` (reassembly bounds) and `proto_ratchet.c`
  (window under uint32 wrap).
- Consider a Z3 encoding of the reconnect backoff bound.
