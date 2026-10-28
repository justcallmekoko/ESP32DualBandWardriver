#!/usr/bin/env python3
"""Lock the ESP32-C5 Dock Mode BLE teardown safety contract."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "WiFiOps.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src" / "WiFiOps.h").read_text(encoding="utf-8")
STABLE = (ROOT / ".github" / "workflows" / "wardriver_build_parallel.yml").read_text(encoding="utf-8")
INSTALLER = (ROOT / ".github" / "workflows" / "build_installer_manifest.yml").read_text(encoding="utf-8")


def main() -> None:
    assert 'nimble_ver: "2.3.8"' in STABLE
    assert 'ref: 2.3.8' in INSTALLER
    assert 'NimBLEDevice.h> // 2.3.8' in HEADER

    assert 'static scanCallbacks ble_scan_callbacks;' in CPP
    assert 'setScanCallbacks(&ble_scan_callbacks, false);' in CPP
    assert 'new scanCallbacks()' not in CPP

    assert 'if (!ble_initialized)' in CPP
    assert 'if (ble_initialized)' in CPP
    assert 'NimBLEDevice::deinit(false)' in CPP
    assert 'pBLEScan = nullptr;' in CPP
    assert CPP.index('pBLEScan = nullptr;') < CPP.index('ble_initialized = false;')


if __name__ == "__main__":
    main()
