# 36: Device zeroization on teardown

**What to build:** Clear secrets from device RAM and any persistent stores when
a link closes or the device powers down, so a captured board contains no live
key material.

**Blocked by:** 14

**Status:** ready-for-agent

- [ ] Zeroize derived keys, PSK copies, ECDH state, and nonce buffers on BYE.
- [ ] Best-effort zeroize on watchdog reboot and brownout path.
- [ ] No secret material remains in retained RAM across warm resets.
- [ ] Any future flash logger must not store keys or plaintext.
- [ ] Host tests assert zeroized buffers; hardware check on cold/warm reset.
