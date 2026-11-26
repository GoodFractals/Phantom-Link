# Ticket 17 — SFO field measurement plan

**Status:** template, ready to execute. No numbers yet — do not claim range or
decode rate until this table is filled in with real observations.

## What we are measuring

Phantom Link's "SFO" is an **application-layer burst channel**: four small UDP
DATA frames carry one nibble encoded by their permutation, repeated three times,
with a CRC8 per burst. We measure the *burst decode rate* — how many bursts the
receiver decodes correctly out of the bursts it attempts — at three distances.

This is **not** a covert-channel measurement. The CYW43 MAC/PHY is untouched;
type, length, timing and addresses remain visible (tickets 15, 16, 29; Lesson 001).

## What to record

For each distance, let the pair run for **at least 100 bursts**, then read the
serial `S` command on the receiving board:

```
sfo good=<good>/<total> reorder=<n> degraded=<n>
```

Compute `decode rate = (good₂ − good₁) / (total₂ − total₁)` from two samples
taken at least 30 s apart, so you are measuring a window, not a boot value.

## Procedure

1. Put both boards in the final enclosure/orientation you intend to demo.
2. Set distance **D1** (short, ~1 m). Power on, wait for `link=UP`.
3. Sample `S` at T+30 s and T+60 s. Record both lines verbatim.
4. Repeat for **D2** (mid, ~5 m) and **D3** (long, whatever the lab allows).
5. For each distance note: antenna orientation, people between boards, nearby
   Wi-Fi activity, and whether either board moved during the window.
6. Pick the demo distance from the **measured** results, not from hope.

## Results table (fill in)

| Distance | Good₁/Total₁ | Good₂/Total₂ | Bursts in window | Decode rate | Reorder | Degraded | Conditions |
|---|---|---|---|---|---|---|---|
| D1 = ___ m | | | | | | | |
| D2 = ___ m | | | | | | | |
| D3 = ___ m | | | | | | | |

## Reporting rules

- **Report the worst result, not the best.** If D3 falls apart, that number goes
  in the docs next to D1's success.
- Quote the distances as measured. If the lab only allowed 8 m, say "8 m", not
  "long range" and not an extrapolated figure.
- If any run had a link drop or reset, record it separately; do not quietly
  discard the run.
- The acceptance gate is three distances × ≥100 bursts each, a committed table,
  the worst result reported, a chosen demo distance, and the conditions noted.
  Until all five are true, ticket 17 stays open.

## Evidence path

Save the raw serial lines with the build hash in `logs/` next to this file, then
link them from ticket 17 and from the claims ledger. `logs/` is gitignored, so
the table in this file (or a docs page) is what survives in git.
