## Title

Security review request — Phantom Link v1 crypto stack (ticket 37)

## Summary

Two-board Pico 2 W proof of concept, one `main.c`, full UI on both boards.
This PR asks for a human security review of the shipped crypto design before
any "reviewed" claim is made. A machine-assisted read-only pass already ran;
its findings and remediations are published in
`.scratch/phantom-link-v1/issues/37-independent-review.md`. **That pass is not
independent human review.** No "reviewed"/"audited" wording is used anywhere yet.

## What to review

`docs/REVIEW_PACK_37.md` has the exact source identity, file map, eight targeted
questions and the freeze caveats. Short version:

1. KDF domain separation (`proto_crypto.c`, `proto_fs.c`) — labels are not
   globally unique; are they scoped correctly by master/salt?
2. Nonce reuse (`main.c`) — retransmits get a fresh frame seq; header-mask
   keystream repeats within a session (accepted residual).
3. 8-byte CMAC/Poly1305 truncation — `2^-64` forgery bound; constant-time
   verify-before-decrypt; AEAD stub not fail-open.
4. Header masking — ECDH-derived keys, clear-prefix selector; the separately
   tagged `proto_hdr` envelope is **not** deployed.
5. Key confirmation — MACs 8 of 32 public-key bytes; direction and session are
   bound, version/peer identity are not.
6. BSSID lock — a spoofable MAC; reduces mis-association only.
7. Replay window vs crypto failures — RX restores ratchet state on bad tag.
8. PSK lifecycle — compiled into flash, extractable; BYE wipes RAM including
   mbedTLS heap limbs via `x25519_clear()`.

## High issues found and fixed during the machine pass

- ECDH scalar was reused across sessions; now a fresh keypair on every session,
  with a follow-on fix for the HELLO_ACK public-key mismatch and for PSK
  re-derivation after BYE. Hardware re-verified: fresh session, `wrong=0`.
- RX advanced the ratchet before tag verification (forged-frame DoS); now
  snapshot/restore. Hardware re-verified with valid and tampered probes.

## Evidence you can check

- Claims ledger: `docs/teach/lessons/0003-the-claims-ledger.html` (39 tickets,
  acceptance gate, PROVEN / INFERRED / BOUNDARY per row).
- Plain-English proof sheet: `docs/teach/reference/claims-brag-sheet.html`.
- Claim → artifact index: `docs/CLAIM_ARTIFACT_INDEX.md`.
- RF evidence: `logs/capture14/capture14_20261125_rf_evidence.md` (73 DATA
  frames, zero plaintext), sniffer analysis in
  `.scratch/phantom-link-v1/evidence/29-sniffer-detection-report.md`.
- Clean-checkout build: `.scratch/phantom-link-v1/evidence/27-clean-build.md`.
- Host suite: `ctest` 18/18, both firmware targets build `-Wall -Wextra -Werror`.

## What we deliberately do not claim

Untethered/battery runtime, 24 h soak (running), MAC-layer SFO or covertness,
independent review, secure boot/OTP, "military-grade" crypto, or 128-bit
authentication. The PSK is extractable by anyone with physical flash access.

## Feedback most useful

Anything that makes a PROVEN or INFERRED line in the claims ledger untrue; a
concrete reproducer that breaks confidentiality, authenticity or replay
protection; or a boundary that is too generous. Style and feature suggestions
are lower priority.

## Before merge

Publish findings here (including informational ones), remediate critical/high,
then cut the annotated tag and hashes listed in the freeze procedure. Only then
may the datasheet say "reviewed", with reviewer name and date.
