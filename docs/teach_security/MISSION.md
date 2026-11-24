# Mission: Phantom Link security layer

## Why
The user is about to ship a datasheet describing the crypto stack and wants to
understand exactly what each mechanism does and does not guarantee, so the
documentation is honest and the demo claims are defensible.

## Success looks like
- Explain the layer stack: WPA2 link, Speck CTR+CMAC payload, key confirmation,
  BSSID lock, header masking, LPI shaping.
- State what the PSK protects and what flash access defeats.
- Explain role-based KDF keys and why sender-relative keys broke the link.
- Explain encrypt-then-MAC and verify-before-decrypt in one paragraph.
- Name the honest limits: no forward secrecy, no MAC-layer covertness, no secure element.

## Constraints
- User wants full explanations, no token budget concern.
- Teaching artifacts live in `docs/teach_security/`, separate from SFO lessons.
- Must cite primary/local sources; no parametric guessing.

## Out of scope
- Implementing crypto changes (tickets are closed; this is understanding only).
- Claiming "encrypted" or "covert" on hardware before ticket 14 is green.
- UI presentation of security state (parked UI tickets).
