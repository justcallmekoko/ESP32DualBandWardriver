#!/usr/bin/env python3
"""Lock external 6 GHz nodes out of normal 2.4/5 GHz allocation."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "WiFiOps.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src" / "WiFiOps.h").read_text(encoding="utf-8")
UI = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")


def ranges(external_flags, channel_count):
    assignable = [index for index, external in enumerate(external_flags) if not external]
    result = {}
    for node_number, slot in enumerate(assignable):
        start = (node_number * channel_count) // len(assignable)
        end = (((node_number + 1) * channel_count) // len(assignable)) - 1
        result[slot] = (start, end)
    return result


def main() -> None:
    channels_block = re.search(r"static const uint8_t scan_channels\[\] = \{(.*?)\};", CPP, re.S)
    assert channels_block
    channel_count = len(re.findall(r"\b\d+\b", re.sub(r"//.*", "", channels_block.group(1))))
    assert channel_count > 0

    # Pager-only: no 2.4/5 GHz range is assigned.
    assert ranges([True], channel_count) == {}

    # One normal node retains the full range when a Pager is present.
    assert ranges([False, True], channel_count) == {0: (0, channel_count - 1)}

    # Adding/removing/rejoining a Pager cannot change standard-node partitions.
    standard_only = ranges([False, False, False], channel_count)
    with_pager = ranges([False, True, False, False], channel_count)
    assert list(standard_only.values()) == list(with_pager.values())
    assert ranges([False, False, False, True], channel_count) == standard_only

    # Tie the behavioral model to the production capability and allocation paths.
    assert '#define NODE_FLAG_EXTERNAL_6G  0x08' in HEADER
    assert '#define NODE_CAPABILITY_EXTERNAL_6G "C5P1;ROLE=EXTERNAL;BANDS=6"' in HEADER
    assert "registerExternal6GCapabilities(slot, msg, srcMacStr)" in CPP
    assert CPP.count("registerExternal6GCapabilities(slot, msg, srcMacStr)") == 2
    assert "!(node_table[i].flags & NODE_FLAG_EXTERNAL_6G)" in CPP
    assert "wifi_ops.getAssignableNodeCount()" in CPP
    assert "(node_table[slot].flags & NODE_FLAG_EXTERNAL_6G)) return false;" in CPP
    assert 'row += "6G EXT ";' in UI


if __name__ == "__main__":
    main()
