#!/usr/bin/env python3
"""Lock Node-mode Wi-Fi display accounting before ESP-NOW forwarding."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "WiFiOps.cpp").read_text(encoding="utf-8")


def main() -> None:
    process = CPP.index("uint16_t WiFiOps::processWardrive")
    node_start = CPP.index("else if (this->run_mode == NODE_MODE) {", process)
    forward = CPP.index("String enow_line =", node_start)
    node_prefix = CPP[node_start:forward]

    assert "setCurrentNetCount" in node_prefix
    assert "setTotalNetCount" in node_prefix
    assert "setCurrent5gCount" in node_prefix
    assert "setCurrent2g4Count" in node_prefix
    assert node_prefix.index("Node skipping excluded SSID") < node_prefix.index("setCurrentNetCount")


if __name__ == "__main__":
    main()
