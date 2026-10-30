from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path
from types import SimpleNamespace

from tools.installer_manifest import (
    ManifestError,
    generate,
    parse_flash_args,
    parse_target_builds,
    registry,
)

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "installer/targets.json"


class ManifestTests(unittest.TestCase):
    def make_build(
        self,
        root: Path,
        name: str,
        *,
        mode: str = "dio",
        frequency: str = "80m",
        app_offset: int = 0x10000,
        app_data: bytes = b"app",
    ) -> Path:
        build = root / name
        build.mkdir()
        (build / "bootloader.bin").write_bytes(b"boot-" + name.encode())
        (build / "partitions.bin").write_bytes(b"part-" + name.encode())
        (build / "src.ino.bin").write_bytes(app_data)
        (build / "flash_args").write_text(
            f"--flash_mode {mode} --flash_freq {frequency} --flash_size 8MB "
            f"0x0 bootloader.bin 0x8000 partitions.bin {app_offset:#x} src.ino.bin\n",
            encoding="utf-8",
        )
        return build

    def mappings(self, jcmk: Path, lilygo: Path) -> list[str]:
        return [
            f"jcmk-c5-wardriver=C5_WARDRIVER={jcmk}",
            f"lilygo-t-dongle-c5=T_DONGLE_C5={lilygo}",
        ]

    def args(self, root: Path, jcmk: Path, lilygo: Path, *, registry_path: Path = REGISTRY):
        return SimpleNamespace(
            registry=registry_path,
            target_builds=self.mappings(jcmk, lilygo),
            version="v2.3.0",
            release_date="20260802",
            source_commit="a" * 40,
            output_dir=root / "out",
        )

    def write_registry(self, root: Path, data: dict) -> Path:
        path = root / "targets.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def test_registry_contains_both_official_targets(self):
        targets = registry(REGISTRY)
        by_id = {target["id"]: target for target in targets}
        self.assertEqual(
            set(by_id), {"jcmk-c5-wardriver", "lilygo-t-dongle-c5"}
        )
        self.assertEqual(by_id["jcmk-c5-wardriver"]["buildFlag"], "C5_WARDRIVER")
        self.assertEqual(
            by_id["lilygo-t-dongle-c5"]["assetSuffix"],
            "lilygo_t_dongle_c5",
        )
        self.assertEqual(
            by_id["lilygo-t-dongle-c5"]["aliases"],
            ["T-Dongle C5", "TDongle-C5"],
        )

    def test_registry_rejects_duplicate_ids_flags_suffixes_and_aliases(self):
        source = json.loads(REGISTRY.read_text(encoding="utf-8"))
        cases = (
            ("id", "jcmk-c5-wardriver", "target ids"),
            ("buildFlag", "C5_WARDRIVER", "build flags"),
            ("assetSuffix", "ESP32C5DEVKITC1", "asset suffixes"),
        )
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for field, value, error in cases:
                data = deepcopy(source)
                data["targets"][1][field] = value
                with self.subTest(field=field), self.assertRaisesRegex(ManifestError, error):
                    registry(self.write_registry(root, data))
            data = deepcopy(source)
            data["targets"][1]["aliases"][0] = "c5 wardriver"
            with self.assertRaisesRegex(ManifestError, "display names and aliases"):
                registry(self.write_registry(root, data))

    def test_generates_all_targets_from_exact_build_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            jcmk = self.make_build(root, "jcmk", app_data=b"jcmk-app")
            lilygo = self.make_build(
                root,
                "lilygo",
                mode="qio",
                frequency="40m",
                app_offset=0x20000,
                app_data=b"lilygo-app",
            )
            manifest_path = generate(self.args(root, jcmk, lilygo))
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(
                manifest["$schema"],
                "https://raw.githubusercontent.com/justcallmekoko/"
                "ESP32DualBandWardriver/"
                + "a" * 40
                + "/installer/firmware-manifest.schema.json",
            )
            self.assertEqual(
                [target["id"] for target in manifest["targets"]],
                ["jcmk-c5-wardriver", "lilygo-t-dongle-c5"],
            )
            by_id = {target["id"]: target for target in manifest["targets"]}
            self.assertEqual(by_id["jcmk-c5-wardriver"]["flash"]["mode"], "dio")
            self.assertEqual(by_id["lilygo-t-dongle-c5"]["flash"]["mode"], "qio")
            self.assertEqual(
                by_id["lilygo-t-dongle-c5"]["flash"]["frequency"], "40m"
            )
            self.assertEqual(
                [
                    segment["offset"]
                    for segment in by_id["lilygo-t-dongle-c5"]["flash"]["factory"]["segments"]
                ],
                [0, 0x8000, 0x20000],
            )
            all_segments = [
                segment
                for target in manifest["targets"]
                for segment in target["flash"]["factory"]["segments"]
            ]
            names = [segment["fileName"] for segment in all_segments]
            self.assertEqual(len(names), len(set(names)))
            self.assertTrue(any("esp32c5devkitc1" in name for name in names))
            self.assertTrue(any("lilygo_t_dongle_c5" in name for name in names))
            for segment in all_segments:
                asset = manifest_path.parent / segment["fileName"]
                self.assertEqual(segment["sha256"], hashlib.sha256(asset.read_bytes()).hexdigest())
            self.assertEqual(len(list(manifest_path.parent.glob("*.bin"))), 6)

    def test_rejects_missing_target_mapping(self):
        targets = registry(REGISTRY)
        with self.assertRaisesRegex(ManifestError, "missing build mappings.*lilygo"):
            parse_target_builds(
                ["jcmk-c5-wardriver=C5_WARDRIVER=/tmp/jcmk"], targets
            )

    def test_rejects_extra_target_mapping(self):
        targets = registry(REGISTRY)
        specs = [
            "jcmk-c5-wardriver=C5_WARDRIVER=/tmp/jcmk",
            "lilygo-t-dongle-c5=T_DONGLE_C5=/tmp/lilygo",
            "unknown-c5=UNKNOWN_C5=/tmp/unknown",
        ]
        with self.assertRaisesRegex(ManifestError, "extra build mapping.*unknown-c5"):
            parse_target_builds(specs, targets)

    def test_rejects_duplicate_target_mapping(self):
        targets = registry(REGISTRY)
        specs = [
            "jcmk-c5-wardriver=C5_WARDRIVER=/tmp/jcmk-a",
            "jcmk-c5-wardriver=C5_WARDRIVER=/tmp/jcmk-b",
        ]
        with self.assertRaisesRegex(ManifestError, "duplicate build mapping"):
            parse_target_builds(specs, targets)

    def test_rejects_cross_mapped_build_flags(self):
        targets = registry(REGISTRY)
        specs = [
            "jcmk-c5-wardriver=T_DONGLE_C5=/tmp/jcmk",
            "lilygo-t-dongle-c5=C5_WARDRIVER=/tmp/lilygo",
        ]
        with self.assertRaisesRegex(ManifestError, "uses T_DONGLE_C5, expected C5_WARDRIVER"):
            parse_target_builds(specs, targets)

    def test_rejects_shared_build_directory(self):
        targets = registry(REGISTRY)
        specs = [
            "jcmk-c5-wardriver=C5_WARDRIVER=/tmp/shared",
            "lilygo-t-dongle-c5=T_DONGLE_C5=/tmp/shared",
        ]
        with self.assertRaisesRegex(ManifestError, "isolated build directories"):
            parse_target_builds(specs, targets)

    def test_refuses_missing_flash_args_for_any_target(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            jcmk = self.make_build(root, "jcmk")
            lilygo = root / "lilygo"
            lilygo.mkdir()
            with self.assertRaisesRegex(
                ManifestError, "missing Arduino build properties or flash_args"
            ):
                generate(self.args(root, jcmk, lilygo))
            self.assertFalse((root / "out").exists())

    def test_reads_expanded_arduino_upload_properties(self):
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp)
            (build / "bootloader.bin").write_bytes(b"boot")
            (build / "src.ino.bin").write_bytes(b"app")
            (build / "arduino-build-properties.txt").write_text(
                "build.flash_mode=dio\n"
                "build.flash_freq=80m\n"
                "build.flash_size=8MB\n"
                "tools.esptool_py.upload.pattern_args=--chip esp32c5 "
                "0x2000 bootloader.bin 0x10000 src.ino.bin\n",
                encoding="utf-8",
            )
            options, segments = parse_flash_args(build)
            self.assertEqual(options["--flash_size"], "8MB")
            self.assertEqual([offset for offset, _ in segments], [0x2000, 0x10000])

    def test_refuses_flash_args_without_required_options(self):
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp)
            (build / "src.ino.bin").write_bytes(b"app")
            (build / "flash_args").write_text("0x10000 src.ino.bin\n")
            with self.assertRaisesRegex(ManifestError, "flash_args lacks --flash_mode"):
                parse_flash_args(build)

    def test_refuses_duplicate_segment_source(self):
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp)
            (build / "src.ino.bin").write_bytes(b"app")
            (build / "flash_args").write_text(
                "--flash_mode dio --flash_freq 80m --flash_size 8MB "
                "0x10000 src.ino.bin 0x20000 src.ino.bin\n"
            )
            with self.assertRaisesRegex(ManifestError, "segment more than once"):
                parse_flash_args(build)

    def test_refuses_segment_outside_build_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            build.mkdir()
            (root / "src.ino.bin").write_bytes(b"app")
            (build / "flash_args").write_text(
                "--flash_mode dio --flash_freq 80m --flash_size 8MB "
                "0x10000 ../src.ino.bin\n"
            )
            with self.assertRaisesRegex(ManifestError, "escapes build directory"):
                parse_flash_args(build)

    def test_allows_arduino_core_ota_seed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            core_partitions = root / "core" / "tools" / "partitions"
            build.mkdir()
            core_partitions.mkdir(parents=True)
            ota_seed = core_partitions / "boot_app0.bin"
            ota_seed.write_bytes(b"ota")
            (build / "src.ino.bin").write_bytes(b"app")
            (build / "arduino-build-properties.txt").write_text(
                "build.flash_mode=dio\n"
                "build.flash_freq=80m\n"
                "build.flash_size=8MB\n"
                f"tools.esptool_py.upload.pattern_args=0xe000 {ota_seed} "
                "0x10000 src.ino.bin\n"
            )
            _, segments = parse_flash_args(build)
            self.assertEqual(
                [path.name for _, path in segments], ["boot_app0.bin", "src.ino.bin"]
            )

    def test_refuses_overlapping_segments(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            jcmk = self.make_build(root, "jcmk")
            lilygo = self.make_build(root, "lilygo")
            (jcmk / "bootloader.bin").write_bytes(b"b" * 32)
            (jcmk / "flash_args").write_text(
                "--flash_mode dio --flash_freq 80m --flash_size 8MB "
                "0x1000 bootloader.bin 0x1010 src.ino.bin\n"
            )
            with self.assertRaisesRegex(ManifestError, "segments overlap"):
                generate(self.args(root, jcmk, lilygo))

    def test_refuses_out_of_range_segment(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            jcmk = self.make_build(root, "jcmk")
            lilygo = self.make_build(root, "lilygo")
            (jcmk / "flash_args").write_text(
                "--flash_mode dio --flash_freq 80m --flash_size 8MB "
                "0x800000 src.ino.bin\n"
            )
            with self.assertRaisesRegex(ManifestError, "exceeds flash size"):
                generate(self.args(root, jcmk, lilygo))


if __name__ == "__main__":
    unittest.main()
