# Ticket 09 acceptance evidence — 2026-11-22

## Build identity

- `phantom-link v1 e4fadca7 AP` / `STA`
- `phantom_link_ap.uf2` — 681,472 B — SHA256 `4560CB1323BD5B396F1EF45214CE4C394A5435FC3AD511E8F3AA53666CAB0EB2`
- `phantom_link_sta.uf2` — 688,128 B — SHA256 `82F19A25288C75679D288746D2BE9934199A6DEADF209099D013B833D9EDCABA`
- Host: `ctest` 9/9, `HOST VALIDATION: PASS`

## Baseline results

- T2–T6 PASS (main UI, input, erase, addressing, cold boot)
- T7 PASS: AP power-cycle → `LINK LOST` within 3 s; STA recovered via the
  documented ticket 08 self-reboot workaround
- T8 PASS: `TX delivered msg=1 attempts=1 retransmits=0`, `RX msg=2 complete
  len=21`, `Secret Msg: HELLO WORLD ABCDEFGHI` in both directions
- T9 PASS: UI and input responsive during transmit
- T10 scoped: truthful HUD confirmed (no fabricated readings, AP signal cell
  empty); visual RSSI bars deferred to the UI rework as a modular addition
- T11 PASS: abort leaves no partial text on receiver, keeps sender draft
- T12 PASS: STA restart re-links cleanly from AP with no reset behavior

## Replay rejection

Session at time of test: `0xed05af2b`

```text
RX stale type=PING seq=100 id=0xed05af2b (accepted=485 duplicate=0 stale=1 wrong=2)
RX stale type=PING seq=100 id=0xed05af2b (accepted=485 duplicate=0 stale=2 wrong=2)
```

`accepted` did not increase across either replay: zero replays accepted.
First probe attempt used session `0xa8eaf23e`, which changed mid-run to
`0xed05af2b`; those datagrams were rejected as `wrong-session` with zero
accepted. Second probe used the settled session and produced the `stale`
lines above.

Probe transcripts:
- `logs/ap_replay_20261122_172829.log` (wrong-session run)
- `logs/ap_replay_20261122_173042.log` (clean stale run)

## OLED evidence

- `logs/t9evidence.jpg` and `logs/t9.2 evidence.jpg` — user confirms both tests
  passed and both pages are reachable.
- Clarification: the `LINK UP` indicator is drawn on the **numeric page** HUD,
  not the main text page. The main page shows the draft/SEND grid.
- This is a UI placement detail only; ticket 09 closes on the user's visual
  confirmation. UI layout is deferred to the parked UI tickets.

## Known limitations

- STA self-reboots after ~15 s without a link (ticket 08 workaround); the boot
  splash flashes during AP power-cycle recovery. Ticket 18 replaces this.
- Historical `wrong=2` and `gap=14` counters are from earlier session churn
  during T7/T12 and the first replay attempt; they are not new failures.
