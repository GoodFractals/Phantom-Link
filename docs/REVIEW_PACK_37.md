# Review pack — ticket 37 (for a real human review via PR)

## Honest status

A **machine-assisted read-only review** has been run against the source at the commit
below. It is an aid, not an independent human review. **No claim of "reviewed",
"audited", or "independently reviewed" may appear in the datasheet, README, release
notes or demo until a real person has completed this pack and findings are published.**

What you can say today: *"self-reviewed and machine-audited; external human review
pending — PR open."*

## 1. Source identity under review

| Item | Exact value |
|---|---|
| HEAD commit | `6317558b21d8bd4f74764190fbbda5215018b3a2` |
| HEAD subject | `docs: add per-ticket acceptance-gate section to claims ledger` |
| `git describe` | `poc/plaintext-v1-102-g6317558` |
| `firmware/main.c` SHA-256 | `F2B3958331AEB662C7C5840501232F6C6AA43C2C5587947353A559EBB18A758B` |
| Embedded banner ID | first 8 hex of the above = `f2b39583` |
| Tags | only `poc/plaintext-v1` (old plaintext PoC) — **no review tag yet** |
| Working tree | clean except untracked `docs/PHANTOM_LINK_DATASHEET.md` (user-owned) |

**Freeze caveat:** the boards are currently running a temporary `PHANTOM_DATA_PROBE=ON`
build for ticket 21's 24 h soak. The UF2s under `firmware/build/` are therefore **not**
the shipping artifacts:

| Current local artifact | SHA-256 (probe build) |
|---|---|
| `phantom_link_ap.uf2` | `8EBCE68AA8297A184C4461FA8D5CBD77C67AB3DDB76CF803BE4103C330589B89` |
| `phantom_link_sta.uf2` | `371D33465FFF84A121724860778ABF06D6D0AB6D51ACDB3DBFFA704E3BDAB300` |

After the soak finishes: rebuild with `PHANTOM_DATA_PROBE=OFF`, `PHANTOM_USE_AEAD=OFF`,
`PHANTOM_STATUS_SAVE=ON`, create annotated tag `security-review/37-v1` at the frozen
commit, hash both UF2s into `SHA256SUMS` outside git, and flash/capture both boot banners
to confirm the embedded ID matches. Until then, review the **source**, not these UF2s.

## 2. What to read

Start with `docs/teach/lessons/0003-the-claims-ledger.html` — it is the claim surface:
39 tickets, each with its acceptance gate, PROVEN evidence, DERIVED inference, and the
boundary that must not be crossed. Then:

| Topic | Files |
|---|---|
| Wire format | `firmware/proto/proto_spec.h`, `proto_frame.c/.h` |
| Primitives / KDF / CMAC / confirm | `firmware/proto/proto_crypto.c/.h` |
| Forward secrecy | `firmware/proto/proto_fs.c/.h`, `proto_x25519.c/.h`, `main.c` (`derive_session_keys`) |
| Ratchet | `firmware/proto/proto_ratchet.c/.h` |
| AEAD suite | `firmware/proto/proto_aead.c/.h`, `main.c` (`aead_*_adapter`) |
| Header masking | `main.c` (`mask_header_fields`, `unmask_header_fields`), `proto_hdr.c/.h` |
| Session/replay/reconnect | `firmware/proto/proto_session.c`, `proto_reconnect.c/.h` |
| LPI / SFO | `firmware/proto/proto_lpi.c`, `proto_sfo.c/.h`, ticket 29 |
| Threat model | ticket 38; `docs/teach/lessons/0002-the-moats.html`; `docs/adr/0001`, `0004`, `0005` |
| Acceptance gates | `.scratch/phantom-link-v1/issues/*.md` and section 1 of the claims ledger |

## 3. Questions worth your time

These are the eight from ticket 37, updated to the current design:

1. **KDF domain separation.** Are labels `0x01`/`0x02`/`0x03`/`0x05` genuinely
   separated given one CMAC primitive is reused for KDF, frame tag, confirmation
   and header mask? Is the two-half master sound? Is `salt_len > 62` fail-closed
   at every caller? (`proto_crypto.c`, `proto_fs.c`, `main.c`)
2. **Nonce reuse.** Retransmits reuse `(key, nonce)` with fresh random padding;
   `frame_nonce` has no session identifier; `HEADER_MASK_NONCE` is now bound to
   `session_id` for post-handshake frames. Is the two-time-pad leak on retransmit
   acceptable here, and is the cross-use between payload CTR and header mask
   fully separated? (`main.c`, `proto_lpi.c`)
3. **8-byte CMAC truncation / AEAD.** What is the forgery bound for ≤63 frames per
   message and a 24 h soak? Is verify-before-decrypt constant-time on RP2350 after
   compilation? Is the reserved suite stub fail-open? (`proto_crypto.c`, `proto_aead.c`)
4. **Header masking vs encryption.** The shipped design is ECDH-key CTR masking with
   a clear-prefix selector, **not** the separately-tagged `proto_hdr` envelope.
   Is that documented honestly? Does authenticating the masked header as AAD leave
   a semantic gap? (`main.c`, ticket 34, claims ledger section 6)
5. **Key confirmation binding.** Does confirmation prove only the long-term PSK, or
   the ephemeral ECDH secret too? Are direction, protocol version, peer identity and
   PSK identity bound? Can a reflected tag be replayed in the other direction?
   (`proto_crypto.c`, `main.c`, `proto_x25519.c`)
6. **BSSID lock.** BSSID is a spoofable MAC. Does the runtime check cover
   deauth/reassociation and the AP-reboot path? Is "reduces mis-association, no
   protection against a cloned BSSID" the honest claim? (ticket 39, `main.c`)
7. **Replay-window interaction with crypto failures.** Crypto failures return before
   `proto_session_accept`, so a rejected frame does not advance the window. Timing
   oracle? Window frozen by high-seq injection? Legitimate frame shadowing?
   (`main.c`, `proto_session.c`)
8. **PSK lifecycle.** PSK is compiled into flash and extractable by anyone with
   physical access. Do tickets 31/33/36 change that threat? Does zeroization clear
   RAM copies on BYE? Is the extraction caveat present wherever the link is called
   secure? (ticket 38, `proto_zeroize.c`, claims ledger section 9)

## 4. Highest-value feedback for a reviewer

- Anything that makes a **PROVEN** or **DERIVED** line in the claims ledger untrue.
- A concrete way to break confidentiality, authenticity or replay protection that
  the current tests miss — with a reproducer, not a hypothetical.
- A case where a "boundary" in the ledger is too generous.

Lower value: style, naming, UI polish, or suggested features. This is a review of
claims and security boundaries, not a redesign.

## 5. Before you merge feedback

- Publish findings in ticket 37, even the ones marked informational.
- Remediate anything critical/high, then bump the tag and hashes.
- Only then change the datasheet to say "reviewed" — and name the reviewer and date.
