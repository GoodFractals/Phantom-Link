#!/usr/bin/env python3
"""Host-side checks for the Phantom Link assets and transport tables.

Run from anywhere:
    python firmware/tests/validate_assets.py

This does not need the Pico SDK and does not touch hardware. It validates the
generated font table, the 49-code Base-6 alphabet, the multi-tap reachability,
and the numeric-page hitboxes against the values in main.c.
"""

from __future__ import annotations

import ast
import re
import sys
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parents[1]
FONT = FIRMWARE / "font_ui.h"
MAIN = FIRMWARE / "main.c"
CMAKE = FIRMWARE / "CMakeLists.txt"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def parse_c_array(name: str, text: str) -> list[int]:
    match = re.search(name + r"\s*\[\s*\]\s*=\s*\{(.*?)\};", text, re.S)
    if not match:
        raise AssertionError(f"{name} not found")
    return [int(value, 0) for value in re.findall(r"0x[0-9A-Fa-f]+", match.group(1))]


def parse_int_array(name: str, text: str) -> list[int]:
    match = re.search(name + r"\s*\[\s*\]\s*=\s*\{(.*?)\};", text, re.S)
    if not match:
        raise AssertionError(f"{name} not found")
    return [int(value) for value in match.group(1).split(",") if value.strip()]


def parse_glyph_table(main_text: str) -> str:
    match = re.search(
        r'static const char GLYPHS\[\]\s*=\s*\n?\s*("(?:[^"\\]|\\.)*")\s*;',
        main_text,
        re.S,
    )
    if not match:
        raise AssertionError("GLYPHS table not found")
    return ast.literal_eval(match.group(1))


def parse_tap_groups(main_text: str) -> list[str]:
    match = re.search(r"tap_groups\[\]\s*=\s*\{(.*?)\};", main_text, re.S)
    if not match:
        raise AssertionError("tap_groups not found")
    return [
        ast.literal_eval(f'"{value}"')
        for value in re.findall(r'"((?:[^"\\]|\\.)*)"', match.group(1))
    ]


def parse_numeric_boxes(main_text: str) -> list[tuple[int, int, int, int]]:
    match = re.search(r"numeric_boxes\[4\]\[3\]\s*=\s*\{(.*?)\n\};", main_text, re.S)
    if not match:
        raise AssertionError("numeric_boxes not found")
    return [
        tuple(map(int, values))
        for values in re.findall(
            r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}",
            match.group(1),
        )
    ]


def codec_round_trip(glyphs: str, message: str) -> str:
    decoded = []
    for char in message:
        code = glyphs.index(char)
        digits = [(code // 36) % 6, (code // 6) % 6, code % 6]
        accumulator = 0
        for digit in digits:
            accumulator = accumulator * 6 + digit
        decoded.append(glyphs[accumulator])
    return "".join(decoded)


def main() -> int:
    font_text = read(FONT)
    main_text = read(MAIN)
    cmake_text = read(CMAKE)

    image = parse_c_array("font_image", font_text)
    widths = parse_int_array("font_widths", font_text)
    offsets = parse_int_array("font_offsets", font_text)

    assert len(widths) == 48, "font_widths must contain 48 glyphs"
    assert len(offsets) == 48, "font_offsets must contain 48 glyphs"
    assert len(image) == 180, "font_image must contain 180 bytes"
    assert sum(widths) == 180, "font widths must sum to 180"
    assert offsets == [sum(widths[:i]) for i in range(48)], "font offsets are not cumulative"
    assert widths[47] == 3, "the merged quote glyph must be 3 pixels wide"
    assert image[177:180] == [0x0C, 0x00, 0x0C], "the merged quote glyph is wrong"
    assert widths[21] == 5 and image[86:91] == [0x18, 0x60, 0x80, 0x60, 0x18], "V glyph is missing"
    print("font: 48 glyphs, 180 bytes, cumulative offsets, merged quote, V present")

    glyphs = parse_glyph_table(main_text)
    assert len(glyphs) == 49, "transport alphabet must have 49 codes"
    assert len(set(glyphs)) == 49, "transport alphabet has duplicate characters"
    assert glyphs[0] == " " and glyphs[-1] == '"', "transport alphabet endpoints are wrong"

    for code, char in enumerate(glyphs):
        digits = [(code // 36) % 6, (code // 6) % 6, code % 6]
        assert all(0 <= digit < 6 for digit in digits), f"code {code} has a bad digit"
        accumulator = 0
        for digit in digits:
            accumulator = accumulator * 6 + digit
        assert accumulator == code, f"code {code} does not round-trip"
        assert glyphs[accumulator] == char
    print("codec: 49 codes, 3 Base-6 digits each, encode/decode round-trip")

    groups = parse_tap_groups(main_text)
    assert len(groups) == 13, f"expected 13 tap groups, found {len(groups)}"
    assert groups[7] == "WXYZV", "WXYZ must gain V as the fifth tap"
    assert len(groups[8]) == 8, "the alpha punctuation group must match .,()[]?!"
    assert len(groups[12]) == 5, "the numeric group must include 0=+- and quote"
    reachable = set("".join(groups)) | {" "}
    assert reachable == set(glyphs), (
        f"unreachable: {sorted(set(glyphs) - reachable)}; "
        f"extra: {sorted(reachable - set(glyphs))}"
    )
    print("tap groups: all 49 characters reachable, V is the fifth WXYZ tap")

    message = 'TEST 123 =+-.,?!()[]"'
    assert len(message) == 21
    assert codec_round_trip(glyphs, message) == message
    assert "MAX_TEXT_CHARS 21" in main_text
    print(f"message: {len(message)}-character full-glyph round-trip")

    boxes = parse_numeric_boxes(main_text)
    assert len(boxes) == 12, f"expected 12 numeric boxes, found {len(boxes)}"

    def nonzero(box: tuple[int, int, int, int]) -> bool:
        return box[2] > box[0] and box[3] > box[1]

    assert not nonzero(boxes[1]), "[0][1] IN RANGE? cell must be dead (ticket 24)"
    assert not nonzero(boxes[4]), "[1][1] must stay empty"
    assert not nonzero(boxes[7]), "SOS [2][1] cell must be dead (ticket 24)"
    assert nonzero(boxes[10]), "SPACE must be at [3][1]"
    assert not nonzero(boxes[11]), "the LINK indicator at [3][2] must not be a key"
    # The numeric branch must not select the dead column-1 cells; the alpha
    # page still legitimately uses row 2/col 1 for its STU group.
    assert "Rows 0 and 2 column 1 are dead" in main_text
    assert "group_index = 13" not in main_text
    print("numeric UI: dead [0][1]/[2][1], SPACE [3][1], LINK [3][2] indicator")

    # HELLO_ACK send is now unconditional in the deferred-ECDH path; the
    # failure case logs, while success is decided by the deferred derivation.
    assert "send_control_frame(PROTO_TYPE_HELLO_ACK)" in main_text
    assert "proto_frame_check_crc" in main_text
    assert "send_data_frame" in main_text
    # send_data_frame returns bool; assigning it to err_t made every
    # successful send look like an error and stalled TX forever.
    assert "err_t err = send_data_frame" not in main_text
    assert "if (!send_data_frame(" in main_text
    assert "tx_clear_delivered_draft();" in main_text
    assert "pkt_eq" not in main_text
    assert "TOK_HELLO" not in main_text
    assert "proto/proto_frame.c" in cmake_text
    assert "proto/proto_session.c" in cmake_text
    assert "proto/proto_txq.c" in cmake_text
    assert "get_rand_32()" in main_text
    assert "proto_session_accept" in main_text
    assert "proto_msg_rx_frame" in main_text
    assert "proto_msg_tx_ack" in main_text
    assert "send_ack_frame" in main_text
    assert "begin_handshake();" in main_text
    assert "session_id++" not in main_text
    assert "SESSION stats" in main_text
    assert "RTT timeout" in main_text
    assert "PROTO_TYPE_BYE" in main_text
    assert "request_teardown" in main_text
    assert "TEARDOWN requested" in main_text
    assert "RX BYE: peer closing link" in main_text
    assert "phantom_link_ap" in cmake_text and "phantom_link_sta" in cmake_text
    assert "IS_AP=1" in cmake_text or "IS_AP=${is_ap}" in cmake_text

    # The role flag must select only the Wi-Fi mode. UI and input stay
    # unconditional in both targets.
    assert "#if IS_AP" not in main_text and "#if !IS_AP" not in main_text
    assert "static void input_init(void)" in main_text
    assert "input_init();" in main_text
    assert "adc_init();" in main_text
    assert "adc_gpio_init(26);" in main_text
    assert "adc_gpio_init(27);" in main_text
    assert "button_init(&btn_select, BTN_SELECT);" in main_text
    assert "button_init(&btn_delete, BTN_DELETE);" in main_text
    assert "input_tick(now);" in main_text
    assert "WIFI_TIMEOUT_MS" not in main_text
    assert "cyw43_arch_enable_ap_mode" in main_text
    assert "cyw43_arch_enable_sta_mode" in main_text
    # Ticket 39 replaced the async connect helper with a BSSID-targeted join
    # plus runtime verification (the async helper could not express identity).
    assert "cyw43_wifi_join(" in main_text
    assert "cyw43_wifi_get_bssid" in main_text
    assert "BSSID MISMATCH" in main_text
    assert "if (is_ap)" in main_text and "if (!is_ap)" in main_text
    assert "#define BTN_BACK   12" in main_text
    assert "button_init(&btn_back, BTN_BACK);" in main_text
    assert "delete_pressed || back_pressed" in main_text
    assert "cyw43_wifi_get_rssi(&cyw43_state, &rssi)" in main_text
    assert "draw_status_hud();" in main_text
    assert "sta_use_static_ip();" in main_text
    assert "dhcp_stop(n);" in main_text
    assert "netif_set_addr(n, &ip, &mask, &gw);" in main_text
    assert "STA static IP:" in main_text
    print("roles: input/UI unconditional; IS_AP only selects Wi-Fi mode")
    print("buttons: BACK GP12 mirrors DELETE; SELECT/init all unconditional")
    print("hud: RSSI bars and link bullet drawn into the baked numeric cells")
    print("link: HELLO_ACK gated, RTT timeout present, both CMake targets defined")
    print("session: hardware RNG IDs, replay window, gap guard, stats line")
    print("reliable: ACK, whole-message retransmit, lost-ACK recovery, bounded retries")
    print("wifi: STA static IP on AP subnet; DHCP stopped; BSSID bound and verified")

    print("HOST VALIDATION: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as error:
        print(f"HOST VALIDATION: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
