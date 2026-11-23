# 07: TX queue replaces the manual state machine

**What to build:** Sending becomes a queue of frames driven by the main loop. DELETE aborts the current message and drains the queue without leaving partial state on the receiver.

**Blocked by:** 06

**Status:** closed

- [x] DELETE mid-send aborts and the receiver never assembles the aborted message.
- [x] The UI and input stay responsive during send and abort.
- [x] A second send after an abort starts cleanly.
- [x] The baseline non-blocking-send check still passes.
- [x] Queue depth is bounded and overflow is reported, not silently dropped.
- [x] A delivered message clears the sender's draft; an aborted or failed send leaves it on screen.

## Verification

2026-11-19 — `proto_txq` is a 64-deep bounded ring buffer with overflow
counting. `main.c` now enqueues a whole message up front, drains one frame per
tick, and aborts/drains on DELETE. `tx_enqueue_message()` reports overflow and
aborts. Host `test_txq` covers FIFO, wraparound, overflow, clear, and null
safety. `ctest` 9/9. Both targets build clean under `-Werror`.

2026-11-19 — hardware: user confirmed send and DELETE mid-send abort work.
That run exposed a bool/`err_t` mismatch in `tx_tick` (fixed in `8772dcf`);
the first build re-sent one frame forever. Draft clearing on delivery was then
added and needs one hardware check before this ticket closes.

2026-11-21 — hardware PASS on build 54618e9d. User confirms: 21-glyph
delivery clears the sender's draft; a short-press BACK mid-send aborts with no
partial text on the receiver and keeps the draft; the following send starts
cleanly and delivers. Logs show `TX delivered ... attempts=1 retransmits=0`
and `Secret Msg: HELLO WORLD ABCDEFGHI` with msgdrop=0, retry=0, fail=0.

2026-11-20 — control mapping: the rig has no DELETE label on the OLED. GP12
BACK is the erase and abort control; use BACK mid-send for the remaining check.
Firmware keeps DELETE on GP15 as an alias, but hardware instructions use BACK.
