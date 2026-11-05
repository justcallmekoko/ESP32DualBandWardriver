#!/usr/bin/env python3
"""Lock the C5 Core's minimal Pager integration contract."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "WiFiOps.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src" / "WiFiOps.h").read_text(encoding="utf-8")


def main() -> None:
    assert 'strcasecmp(command, "wardrive")' in CPP
    assert 'strcasecmp(command, "mode core")' in CPP
    assert 'settings.saveSetting<bool>("m", this->run_mode, true)' in CPP
    assert 'rec.type == "W6"' in CPP
    assert "wigle6GHzFrequencyMHz(rec.channel)" in CPP
    assert "setCurrent6gCount" in CPP
    assert "current_6g_count" in HEADER


if __name__ == "__main__":
    main()
