#!/usr/bin/env python3
"""Analyze an ESP32 promiscuous capture dump for LPI properties.

Reads lines of the form `FRAME <hex>` from a no-PSK sniffer log and reports:
frame counts, the length distribution of DATA frames from the enrolled AP, and
the entropy of their application payload region. This does NOT measure
detection probability — that needs a labeled detection test (ticket 17).
"""
import math
import re
import sys
from collections import Counter

BSSID = bytes.fromhex('88a29e5bff69')


def frames(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            if not line.startswith('FRAME '):
                continue
            try:
                yield bytes.fromhex(line.split(None, 1)[1].strip())
            except ValueError:
                continue


def entropy(data):
    if not data:
        return 0.0
    counts = Counter(data)
    n = len(data)
    return -sum((c / n) * math.log2(c / n) for c in counts.values())


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else \
        'logs/capture14/esp32_capture_20261125_145804.txt'
    all_frames = list(frames(path))
    data = []
    for fr in all_frames:
        if len(fr) < 24:
            continue
        if (fr[4:10] != BSSID and fr[10:16] != BSSID and fr[16:22] != BSSID):
            continue
        if fr[0] & 0x0C != 0x08:            # 802.11 management/control/data type
            continue
        data.append(fr)

    lengths = Counter(len(fr) for fr in data)
    payloads = [fr[24:] for fr in data]
    payload_lengths = Counter(len(p) for p in payloads)
    joined = b''.join(payloads)

    print('Capture: %s' % path)
    print('Total FRAME lines: %d' % len(all_frames))
    print('DATA frames from enrolled AP BSSID: %d' % len(data))
    print('Full-frame length distribution: %s' % dict(sorted(lengths.items())))
    print('Payload (after 24-byte MAC header) length distribution: %s'
          % dict(sorted(payload_lengths.items())))
    print('Distinct payload lengths: %d' % len(payload_lengths))
    print('Payload bytes analyzed: %d' % len(joined))
    print('Shannon entropy of payload bytes: %.3f bits/byte (max 8.000)' % entropy(joined))
    print('Readable ASCII runs >= 4 chars in payload: %d'
          % len(re.findall(rb'[ -~]{4,}', joined)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
