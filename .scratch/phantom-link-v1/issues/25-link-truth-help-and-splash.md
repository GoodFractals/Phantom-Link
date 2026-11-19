# 25: Link truth, help screen, boot splash

**What to build:** The link label tells the truth, a lock glyph appears only for an authenticated session, a one-screen legend explains the controls, and boot shows a skippable splash.

**Blocked by:** 24

**Status:** ready-for-agent

- [ ] `LINK` and `NO LINK` reflect the real session state.
- [ ] A lock glyph appears only when key confirmation succeeded.
- [ ] Long-press opens a legend covering type, send, delete, back, and power.
- [ ] The splash shows version and role, lasts 2.5 s, and any button skips it.
- [ ] No protocol or timing constant is touched.
