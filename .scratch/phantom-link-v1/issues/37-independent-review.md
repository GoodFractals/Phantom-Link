# 37: Independent security review

**What to build:** Commission or conduct an external cryptographic/code review
before any public "secure" claim. Treat this as a security control, not a formality.

**Blocked by:** 32

**Status:** ready-for-human

- [ ] Freeze a tagged build for review.
- [ ] Provide reviewer with threat model, ADRs, frame spec, and source.
- [ ] Review questions: KDF domain separation, nonce reuse, CMAC truncation,
      header masking, key confirmation, BSSID binding, replay interaction.
- [ ] Publish findings and remediate critical/high issues.
- [ ] Datasheet may say "reviewed" only after this completes.
