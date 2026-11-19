# 06: ACK and whole-message retransmit

**What to build:** The receiver ACKs the highest contiguous sequence. The sender retransmits the whole message on timeout under the same message ID. Duplicates are suppressed end to end.

**Blocked by:** 05

**Status:** ready-for-agent

- [ ] With 10% induced loss, a 21-glyph message arrives exactly once in both directions.
- [ ] No partial message is ever surfaced to the app.
- [ ] Retransmit stops after a bounded number of attempts and the UI reports failure.
- [ ] Memory use stays bounded across repeated retransmits.
- [ ] The ACK path itself survives a lost ACK without duplicating the message.
