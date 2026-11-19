# Two-party membership is physical possession

In a two-party link, membership is decided by holding a rig that has the key. There is no directory, no phone number, and no invite to mis-click. At three or more parties the cipher still works, but membership authentication does not: proving that a newly added device belongs to the person you meant requires an out-of-band channel that no in-band protocol can supply. The 2025 group-chat leak is the textbook case: the end-to-end encryption held, and a mis-added contact defeated it. Phantom Link v1 therefore stays strictly two-party.

## Consequences

- No group, mesh, or store-and-forward mode in v1. This is a security property, not a missing feature.
- If a third rig is ever added, the minimum bar is per-device identity keys, a short fingerprint displayed on both screens and confirmed by a person, explicit removal with key rotation, and a written rule that adding a member is a physical act.
- The UI must never imply membership beyond the two rigs in hand.
