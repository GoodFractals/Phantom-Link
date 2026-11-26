# 27: Refresh audit, protocol note, demo video, release

**What to build:** A refreshed audit against the shipped build, a two-page protocol note, an uncut demo video, and tagged releases where both the plaintext fallback and v1.0 build.

**Blocked by:** 26

**Status:** ready-for-human

- [ ] Audit claims match the shipped artifacts and hashes.
- [x] The protocol note covers the frame, key schedule, and the SFO caveat
  (`docs/PROTOCOL_NOTE.md`); audit/hash cross-check still requires the final tag.
- [ ] The demo shows boot, link, encrypted send both ways, tamper rejection, and SFO stats, with no cuts hiding failures.
- [ ] `poc/plaintext-v1` and `v1.0` both build from a clean checkout.
      Partial: the current tree builds **both targets from a fresh worktree with
      zero warnings** (`.scratch/phantom-link-v1/evidence/27-clean-build.md`).
      That gate found a real defect — `PICO_BOARD` was set after the SDK import,
      so a clean checkout configured for rp2040 and failed; fixed in `2f1f363`.
      `v1.0` does not exist yet, so the checkbox stays open.
- [ ] The next-context prompt points at the gameplan and the backlog.
