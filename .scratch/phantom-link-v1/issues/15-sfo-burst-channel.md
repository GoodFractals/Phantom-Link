# 15: SFO burst channel

**What to build:** Four-datagram bursts whose arrival permutation carries one nibble, with CRC8 and 3× repeat. Invalid or duplicate permutations are rejected.

**Blocked by:** 14

**Status:** ready-for-agent

- [ ] All 24 permutations map deterministically; the 8 unused codes are rejected.
- [ ] A loopback harness decodes all 16 nibbles correctly on an in-order queue.
- [ ] CRC8 catches a corrupted burst.
- [ ] Three repeats resolve to a majority decode.
- [ ] A repeated or duplicated index inside one burst is rejected.
- [ ] Bounded jitter on the TX scheduler varies inter-frame delay without
  changing protocol timing constants or breaking ACK/retransmit bounds.
