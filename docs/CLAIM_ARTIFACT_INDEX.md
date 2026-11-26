# Claim → artifact index (ticket 27 gate)

Every claim in `docs/teach/reference/claims-brag-sheet.html` maps to a durable
artifact. This is the "audit claims match the shipped artifacts" checklist.
If a row has no artifact, the claim does not go in release copy.

## Source identity of the current review candidate

| Item | Value |
|---|---|
| HEAD (local) | `1bcf303` at time of writing; see `git rev-parse HEAD` for the freeze commit |
| `firmware/main.c` SHA-256 | `43E8A9347249CAD6597626BD29DA2FB262D3B4CFECBB926E729D2E0BD16C00E2` |
| Embedded banner ID | `43e8a934` |
| AP UF2 (soak build) | `859F7E7F0594E9C4E1FAD38AF62246E3633B8F8661B062BCB44DA022B4972789` |
| STA UF2 (soak build) | `260EF94B46C439B654C12C64AD1DBC57E251A08FFD1F12E6A8A0CB5CB8AD6A2D` |
| Review tag | **none** — cut `security-review/37-v1` at the frozen commit |
| `docs/PHANTOM_LINK_DATASHEET.md` | untracked, user-owned — must be reconciled before freeze |

`logs/` is gitignored, so raw serial captures are local evidence only. The
ticket files and this index are what survive in git.

## Claim map

| # | Claim | Ticket | Durable artifact |
|---|---|---|---|
| 01 | Two boards, one source, full UI on both | 01–09 | `firmware/main.c`, `firmware/CMakeLists.txt`, `.scratch/.../issues/09-*.md` |
| 02 | No plaintext in a password-less RF capture | 14 | `logs/capture14/capture14_20261125_rf_evidence.md`, ticket 14 |
| 03 | Fresh X25519 per session; no old-traffic decryption | 31, 37 | `firmware/proto/proto_x25519.c`, `proto_fs.c`, ticket 37 findings |
| 04 | One key per frame | 33 | `firmware/proto/proto_ratchet.c`, `firmware/host/tests/test_ratchet.c`, ticket 33 |
| 05 | Header masked with ECDH-derived keys | 34 | `firmware/main.c` (`mask_header_fields`), `proto_hdr.c`, ticket 34 |
| 06 | Tamper rejected on hardware | 14, 32 | `firmware/main.c` (`X` probe), ticket 32 verification |
| 07 | Replay and wrong-PSK rejected | 13, 14 | `firmware/proto/proto_session.c`, ticket 14 verification |
| 08 | Forged frame cannot wedge the ratchet | 37 | `firmware/main.c` (RX snapshot), ticket 37 findings |
| 09 | AEAD suite verified as opt-in | 32 | `firmware/proto/proto_aead.c`, ticket 32 verification |
| 10 | 20/20 restarts recover | 18 | `firmware/proto/proto_reconnect.c`, ticket 18 verification |
| 11 | Watchdog recovers a forced hang | 19 | `firmware/main.c`, `logs/watchdog_flash_fix_19.md`, ticket 19 |
| 12 | Counters survive a power cycle | 20 | `firmware/main.c` (`status_save`), ticket 20 verification |
| 13 | Real firmware bugs found and fixed | 20, 37 | tickets 20 and 37 findings, `docs/REVIEW_PACK_37.md` |
| 14 | LPI-oriented traffic shaping | 15, 16, 29 | `firmware/proto/proto_lpi.c`, `proto_sfo.c`, ticket 29 |
| 15 | Reproducible, traceable builds | 01, 02 | `firmware/host/tests/`, `firmware/CMakeLists.txt`, ticket 02 |

## Gates still open — no claim allowed

| Ticket | Missing artifact |
|---|---|
| 17 | three-distance burst table with worst result reported |
| 21 | completed 24 h soak log + `py -3 tools/soak21_report.py` output |
| 22 | current table, idle hour, runtime within 20% of model |
| 23 | untethered boot/link run, repeated once |
| 27 | uncut demo video, clean tagged checkout builds |
| 29 | airtime cost (22); sniffer report done from existing ticket 14 capture |
| 37 | published findings from a real human reviewer via PR |

Ticket 35 was closed as research-only: its spare-board OTP acceptance gate was
reclassified out of v1 scope by ticket 38. No OTP was burned and no
"hardware hardened" claim is made.

## Release order

1. Finish ticket 21 (soak + report), 17, 22, 23, 29 as hardware allows.
2. User updates the untracked datasheet to match the shipped design.
3. Clean tree, rebuild from the frozen commit, cut annotated tag, hash UF2s into
   `SHA256SUMS` outside git, flash both and capture boot banners.
4. Human review via PR; publish findings in ticket 37, remediate critical/high.
5. Only then may the datasheet say "reviewed" and the demo go public.
