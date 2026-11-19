# 11: Key confirmation and PSK handling

**What to build:** `HELLO` and `HELLO_ACK` carry nonces and a key-confirmation tag over the session. A peer with a different PSK reaches an explicit AUTH FAILED state and never LINK UP.

**Blocked by:** 04, 10

**Status:** ready-for-agent

- [ ] Same-PSK boards reach LINK UP.
- [ ] Different-PSK builds reach an explicit AUTH FAILED state.
- [ ] A missing or malformed tag drops the handshake and is counted.
- [ ] PSK override is documented, including that flash access defeats it.
- [ ] Replaying an old confirmation tag cannot establish a new session.
