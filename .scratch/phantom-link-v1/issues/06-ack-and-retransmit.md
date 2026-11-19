# 06: ACK and whole-message retransmit

**What to build:** The receiver ACKs the highest contiguous sequence. The sender retransmits the whole message on timeout under the same message ID. Duplicates are suppressed end to end.

**Blocked by:** 05

**Status:** ready-for-human (code complete; 10% loss and lost-ACK criteria verified on the host harness)

- [x] With 10% induced loss, a 21-glyph message arrives exactly once in both directions.
- [x] No partial message is ever surfaced to the app.
- [x] Retransmit stops after a bounded number of attempts and the UI reports failure.
- [x] Memory use stays bounded across repeated retransmits.
- [x] The ACK path itself survives a lost ACK without duplicating the message.

## Verification

2026-11-19 — `proto_msg` adds position-indexed DATA frames (`msg_id`, `pos`, symbol)
and FINAL/ACK payloads. The sender retransmits the unacknowledged suffix from
the first missing position; with no ACK it retransmits the whole message. The
receiver ACKs the highest contiguous position and dedupes by position, so
retries never duplicate a character. `MAX_ATTEMPTS = 20`, `ACK_TIMEOUT_MS = 1200`.

`main.c` now uses `proto_msg` for TX and RX, sends `ACK` frames, and shows a
`NO ACK` notice on failure. `append_char` caps the draft at 21 characters.

Host `ctest` is 8/8. `test_msg` covers assembly, gaps, retries, bounds, and
bounded failure. `test_reliable` drives two real endpoints through a 10% loss
channel (593 datagrams, 69 dropped, 6 retries, 2084 ms), a lost-final-ACK
recovery, an all-lost failure (20 attempts, reported), and 40 messages at 10%
loss with no duplicate delivery. Both targets build clean under `-Werror`: AP
678,912 bytes, STA 685,056 bytes.

Still open: flash both boards and observe a 21-character message land once each
way at 10% induced loss and a `NO ACK` on the UI. That joins the ticket 09
hardware run.
