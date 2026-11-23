# No battery-sense hardware

Each rig runs from a 2000 mAh USB power bank. A power bank's output is a regulated 5 V rail, so a voltage divider on it would read 5.0 V at full charge and 5.0 V at empty. It cannot report cell state. We add no divider and no ADC channel; the pack's own LEDs are the charge gauge.

## Consequences

- There is no software low-battery warning: a rig runs normally, then stops.
- The `BAT:` cell in the UI must be repurposed or left blank rather than showing a fake gauge.
- Phase 5 verifies that the pack does not auto-cutoff at idle current; that is the real power risk, not state of charge.
