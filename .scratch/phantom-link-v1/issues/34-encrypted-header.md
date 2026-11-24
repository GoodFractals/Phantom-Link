# 34: Encrypt header fields

**What to build:** Move beyond masking: encrypt session_id, seq, and ack inside
the AEAD envelope, leaving only routing-visible fields in clear.

**Blocked by:** 32

**Status:** ready-for-agent

- [ ] Wire format revised with documented encrypted-header envelope.
- [ ] Receiver decrypts header before session/replay processing.
- [ ] AEAD AAD covers only fields that must stay in clear (magic/version/type/length).
- [ ] Replay rejection still works after header decryption.
- [ ] Host tests cover tamper, replay, and wrong-key rejection.
