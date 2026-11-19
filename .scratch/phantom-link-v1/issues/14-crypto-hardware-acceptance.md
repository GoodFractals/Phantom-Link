# 14: Crypto hardware acceptance

**What to build:** Flash the encrypted build and prove the baseline still works, the wrong key fails, and a packet capture shows no readable text.

**Blocked by:** 13

**Status:** ready-for-human

- [ ] T2–T12 pass on both boards.
- [ ] T15–T17 pass: zero replays accepted, 100% of tampered frames rejected, wrong PSK cannot link.
- [ ] A packet capture contains no readable message text.
- [ ] Both serial logs are archived with the build hashes.
- [ ] `poc/plaintext-v1` still builds and flashes from the tagged tree.
