# Phantom Link Security Resources

## Knowledge

- [ADR 0001: Speck128/128 CTR + CMAC](../../adr/0001-speck128-128-ctr-cmac.md)
  Authoritative cipher choice and cipher-agnostic AEAD slot decision.
- [NSA SIMON & SPECK Implementation Guide 1.1](https://nsacyber.github.io/simon-speck/implementations/ImplementationGuide1.1.pdf)
  Primary source for Speck128/128 round function, key schedule, and KAT vectors.
- [RFC 4493: AES-CMAC](https://www.rfc-editor.org/rfc/rfc4493.txt)
  Primary source for the CMAC construction (subkeys, padding, truncation) our
  generic wrapper is validated against.
- [Research notes: Speck KATs and interface sketch](../../.scratch/phantom-link-v1/speck-research.md)
  Local record of vectors, byte-order correction, and gotchas.
- [Implementation: proto_crypto.c](../../../firmware/proto/proto_crypto.c)
  The exact primitive, KDF, CMAC, CTR, suite, and confirmation code.
- [Ticket 11: key confirmation and PSK](../../.scratch/phantom-link-v1/issues/11-key-confirmation-and-psk.md)
  Includes the uninitialized-byte bug and its fix.
- [Ticket 12: encrypted DATA](../../.scratch/phantom-link-v1/issues/12-encrypted-data.md)
  Encrypt-then-MAC, verify-before-decrypt, nonce construction.
- [ADR 0004: two-party membership is physical possession](../../adr/0004-two-party-membership-is-physical-possession.md)
  Why the PSK is a pairing boundary, not a secret against physical theft.

## Wisdom (Communities)

- None recommended yet. Ask me if you want practitioner review before the demo.

## Gaps

- No independent cryptographic audit. Do not call this "audited" or "covert".
