# Phantom Link SFO Resources

## Knowledge

- [Lesson 005: Three kinds of proof](lessons/0005-three-kinds-of-proof.html)
  What TLA+, Z3 and CBMC actually prove, what they don't, and which to reach
  for. Use for: understanding the verification stack without overclaiming.
- [Reference: Verification cheat sheet](reference/verification-cheat-sheet.html)
  Commands and status for each tool — what ran, what is staged.
- [Reference: The black-box experiment](reference/the-black-box-experiment.html)
  The early story: proving a reliable two-board transport over a proprietary
  Wi-Fi firmware blob before any crypto existed. Use for: explaining what was
  actually hard, and why the encryption was the reward, not the starting point.
- [Reference: Proof, then brag](reference/claims-brag-sheet.html)
  Fifteen plain-English claims in the form &ldquo;Can we prove it? / Proven / Brag&rdquo;.
  Every brag is bolted to a named test or log. Use for: interview answers,
  demo narration and release copy &mdash; never as a substitute for the ledger.
- [Lesson 004: Where the project stands](lessons/0004-where-the-project-stands.html)
  A four-minute status board: 27 closed, 12 open, what each is waiting for, and
  the one sentence you can safely say. Uses the reusable `assets/status.css`.
- [Lesson 003: The Claims Ledger](lessons/0003-the-claims-ledger.html)
  All 39 tickets in one print-friendly page: what each proves (PROVEN), what a
  reader might infer (DERIVED), and the line that must not be crossed. Use for:
  writing datasheet copy, interview answers, or release notes without
  overclaiming.
- [Lesson 002: The Moats of Phantom Link](lessons/0002-the-moats.html)
  Every defence layer outside-in, what each proves and does not prove, the
  one-line PSK override, and the honest forward-secrecy boundary. Use for:
  explaining the architecture to a technical non-RF reader.
- [Ticket 14 RF evidence](../../../logs/capture14/capture14_20261125_rf_evidence.md)
  ESP32 no-PSK capture: 73 DATA frames from the AP BSSID, zero plaintext hits.
- [Paper: StegoFrameOrder—MAC Layer Covert Network Channel for Wireless IEEE 802.11 Networks (Sawicki et al., 2021)](https://pmc.ncbi.nlm.nih.gov/articles/PMC8471875/)
  Primary source for true SFO: covert data is carried by the order of MAC-layer
  frames using non-deterministic MAC behavior. Use for: what real SFO requires.
- [Project ADR 0002: SFO is an application-layer telemetry channel](../../adr/0002-sfo-is-an-application-layer-telemetry-channel.md)
  Authoritative local decision: the CYW43439 exposes only normal STA/AP
  interfaces, so MAC-layer SFO is unavailable. Use for: why we do it one layer up.
- [GAMEPLAN §4.4: crypto envelope and SFO scope](../../GAMEPLAN.md)
  Authoritative spec for the 4-datagram burst, 3x repeat, and telemetry-only rule.
- [Implementation: `firmware/proto/proto_sfo.c`](../../../firmware/proto/proto_sfo.c)
  The exact permutation table, CRC8, and majority decode used by the firmware.
- [Ticket 15: SFO burst channel](../../.scratch/phantom-link-v1/issues/15-sfo-burst-channel.md)
  Acceptance criteria for the primitive; host-tested with 11/11 ctest.
- [Ticket 16: SFO telemetry and degraded mode](../../.scratch/phantom-link-v1/issues/16-sfo-telemetry-and-degraded-mode.md)
  The layer being added now: counters, degraded state, read-only accessor.

## Wisdom (Communities)

- No community recommended yet. The user has not opted into forums; keep this
  section empty unless they ask for external practitioner feedback.

## Gaps

- No measured detection-rate data yet. Ticket 17 produces honest field numbers;
  do not claim stealth before then.
