#ifndef PHANTOM_PSK_H
#define PHANTOM_PSK_H

/* Phantom Link v1 pre-shared key.
 *
 * OVERRIDE: replace the 32 bytes below with your own value before flashing.
 * Each open-source builder should choose a different key. Anyone with physical
 * access to a board can read its flash, so this key is a pairing boundary,
 * not a secret against someone holding the hardware.
 *
 * Generate a random value with: tools/gen_psk.ps1
 */

#define PHANTOM_PSK_BYTES {                                                   \
    0x54, 0x68, 0x69, 0x73, 0x49, 0x73, 0x41, 0x50,                          \
    0x6c, 0x61, 0x63, 0x65, 0x68, 0x6f, 0x6c, 0x64,                          \
    0x42, 0x75, 0x69, 0x6c, 0x64, 0x42, 0x79, 0x20,                          \
    0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x21, 0x21                           \
}

#endif
