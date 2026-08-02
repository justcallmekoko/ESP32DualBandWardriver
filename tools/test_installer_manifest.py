from __future__ import annotations
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from tools.installer_manifest import ManifestError, generate, registry

ROOT = Path(__file__).resolve().parents[1]

class ManifestTests(unittest.TestCase):
    def args(self, root: Path, build: Path):
        return SimpleNamespace(registry=ROOT / "installer/targets.json", build_flag="C5_WARDRIVER", build_dir=build, version="v2.3.0", release_date="20260802", source_commit="a" * 40, output_dir=root / "out")
    def test_registry_is_c5_only(self):
        self.assertEqual(registry(ROOT / "installer/targets.json")["esptoolChip"], "esp32c5")
    def test_generates_hashed_manifest_from_actual_flash_args(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); build = root / "build"; build.mkdir()
            for name, data in (("bootloader.bin", b"boot"), ("partitions.bin", b"part"), ("src.ino.bin", b"app")): (build / name).write_bytes(data)
            (build / "flash_args").write_text("--flash_mode dio --flash_freq 80m --flash_size 8MB 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 src.ino.bin\n")
            manifest = json.loads(generate(self.args(root, build)).read_text())
            self.assertEqual(
                manifest["$schema"],
                "https://raw.githubusercontent.com/justcallmekoko/ESP32DualBandWardriver/" + "a" * 40 + "/installer/firmware-manifest.schema.json",
            )
            flash = manifest["targets"][0]["flash"]
            self.assertEqual([s["offset"] for s in flash["factory"]["segments"]], [0, 0x8000, 0x10000])
            self.assertEqual(len(flash["update"]["segments"]), 1)
    def test_refuses_missing_flash_args(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); build = root / "build"; build.mkdir()
            with self.assertRaisesRegex(ManifestError, "missing Arduino flash_args"):
                generate(self.args(root, build))

if __name__ == "__main__": unittest.main()
