# 07: TX queue replaces the manual state machine

**What to build:** Sending becomes a queue of frames driven by the main loop. DELETE aborts the current message and drains the queue without leaving partial state on the receiver.

**Blocked by:** 06

**Status:** ready-for-agent

- [ ] DELETE mid-send aborts and the receiver never assembles the aborted message.
- [ ] The UI and input stay responsive during send and abort.
- [ ] A second send after an abort starts cleanly.
- [ ] The baseline non-blocking-send check still passes.
- [ ] Queue depth is bounded and overflow is reported, not silently dropped.
