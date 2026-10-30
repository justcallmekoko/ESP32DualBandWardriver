from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path

from tools.validate_release_bundle import BundleError, validate_bundle

VERSION = "v2.4.0"
SOURCE = "a" * 40
TARGETS = ("jcmk-c5-wardriver", "lilygo-t-dongle-c5")


class ReleaseBundleTests(unittest.TestCase):
    def make_bundle(self, root: Path) -> tuple[Path, dict]:
        bundle = root / "bundle"
        bundle.mkdir()
        targets = []
        for index, target_id in enumerate(TARGETS):
            segments = []
            for slot, offset in enumerate((0x2000, 0x8000, 0xE000, 0x10000)):
                name = f"target-{index}-segment-{slot}.bin"
                data = f"{target_id}-{slot}".encode()
                (bundle / name).write_bytes(data)
                segments.append(
                    {
                        "role": ("bootloader", "partition-table", "ota-data", "application")[slot],
                        "offset": offset,
                        "fileName": name,
                        "size": len(data),
                        "sha256": hashlib.sha256(data).hexdigest(),
                    }
                )
            targets.append(
                {
                    "id": target_id,
                    "flash": {
                        "factory": {"segments": segments},
                        "update": {"segments": [deepcopy(segments[-1])]},
                    },
                }
            )
        manifest = {"version": VERSION, "sourceCommit": SOURCE, "targets": targets}
        (bundle / "firmware-manifest.json").write_text(json.dumps(manifest))
        return bundle, manifest

    def test_accepts_complete_exact_source_bundle(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle, _ = self.make_bundle(Path(temp))
            self.assertEqual(validate_bundle(bundle, VERSION, SOURCE), (2, 8))

    def test_rejects_update_segment_not_in_factory_set(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle, manifest = self.make_bundle(Path(temp))
            manifest["targets"][0]["flash"]["update"]["segments"][0]["sha256"] = "0" * 64
            (bundle / "firmware-manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(BundleError, "does not match"):
                validate_bundle(bundle, VERSION, SOURCE)

    def test_rejects_bootloader_as_update_image(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle, manifest = self.make_bundle(Path(temp))
            factory = manifest["targets"][0]["flash"]["factory"]["segments"]
            manifest["targets"][0]["flash"]["update"]["segments"] = [deepcopy(factory[0])]
            (bundle / "firmware-manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(BundleError, "factory application"):
                validate_bundle(bundle, VERSION, SOURCE)

    def test_rejects_published_asset_tampering(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle, manifest = self.make_bundle(Path(temp))
            name = manifest["targets"][1]["flash"]["factory"]["segments"][3]["fileName"]
            (bundle / name).write_bytes(b"tampered")
            with self.assertRaisesRegex(BundleError, "size mismatch"):
                validate_bundle(bundle, VERSION, SOURCE)

    def test_rejects_wrong_version_or_source(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle, _ = self.make_bundle(Path(temp))
            with self.assertRaisesRegex(BundleError, "version"):
                validate_bundle(bundle, "v9.9.9", SOURCE)
            with self.assertRaisesRegex(BundleError, "source"):
                validate_bundle(bundle, VERSION, "b" * 40)


if __name__ == "__main__":
    unittest.main()
