# 10: Speck128/128, KDF, CMAC

**What to build:** A cipher-agnostic crypto module with Speck128/128 in CTR mode, a KDF for per-direction keys, and CMAC truncated to 8 bytes. Published vectors run on host and as an on-device self-test.

**Blocked by:** 09

**Status:** ready-for-agent

- [ ] Published Speck128/128 vectors pass on host and on-device.
- [ ] CMAC vectors pass for empty, short, and multi-block inputs.
- [ ] The KDF produces different send and receive keys for the same session.
- [ ] The interface exposes an AEAD slot that the session layer could adopt without changes.
- [ ] A failed boot self-test is loud and blocks LINK UP.
- [ ] The module compiles for both host tests and the firmware targets.
