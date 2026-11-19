# 20: Error counters, STATUS dump, flash persistence

**What to build:** One STATUS report shows link state, sequence window, crypto errors, and SFO stats. Counters survive a power cycle so an untethered run can be audited later.

**Blocked by:** 18, 19

**Status:** ready-for-agent

- [ ] STATUS prints every required field in one block.
- [ ] Counters survive a power cycle and a reboot without reflashing.
- [ ] Flash writes are bounded and do not wear one sector during normal traffic.
- [ ] A corrupt counter record falls back to zero instead of blocking boot.
- [ ] The trace is readable after a run with no serial console attached.
