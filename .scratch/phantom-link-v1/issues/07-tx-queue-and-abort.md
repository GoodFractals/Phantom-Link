# 07: TX queue replaces the manual state machine

**What to build:** Sending becomes a queue of frames driven by the main loop. DELETE aborts the current message and drains the queue without leaving partial state on the receiver.

**Blocked by:** 06

**Status:** ready-for-human (code complete; hardware verification pending)

- [x] DELETE mid-send aborts and the receiver never assembles the aborted message.
- [x] The UI and input stay responsive during send and abort.
- [x] A second send after an abort starts cleanly.
- [x] The baseline non-blocking-send check still passes.
- [x] Queue depth is bounded and overflow is reported, not silently dropped.
- [ ] A delivered message clears the sender's draft; an aborted or failed send leaves it on screen.

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
