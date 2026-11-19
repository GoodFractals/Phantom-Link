# 19: Hardware watchdog

**What to build:** A watchdog supervises the main loop. A forced hang resets the board and it returns to LINK UP without human intervention.

**Blocked by:** 14

**Status:** ready-for-agent

- [ ] A forced infinite loop resets in under 8 s.
- [ ] After reset the board re-links and resumes sending.
- [ ] The reset reason is logged on the next boot.
- [ ] Normal operation never trips the watchdog during a 1 h run.
- [ ] Long blocking work is split or fed to the watchdog explicitly.
