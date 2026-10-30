#!/usr/bin/env python3
"""Validate a complete, exact-source C5 installer release bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


class BundleError(ValueError):
    pass


def _validate_asset(bundle: Path, segment: dict) -> str:
    name = segment.get("fileName")
    path = bundle / str(name)
    if not name or not path.is_file():
        raise BundleError(f"missing referenced asset {name}")
    data = path.read_bytes()
    if len(data) != segment.get("size"):
        raise BundleError(f"size mismatch for {name}")
    if hashlib.sha256(data).hexdigest() != segment.get("sha256"):
        raise BundleError(f"SHA-256 mismatch for {name}")
    return name


def validate_bundle(bundle: Path, version: str, source_commit: str) -> tuple[int, int]:
    manifest_path = bundle / "firmware-manifest.json"
    if not manifest_path.is_file():
        raise BundleError("firmware-manifest.json is missing")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("version") != version:
        raise BundleError("manifest version does not match release tag")
    if manifest.get("sourceCommit") != source_commit:
        raise BundleError("manifest source commit does not match release commit")

    targets = manifest.get("targets")
    if not isinstance(targets, list) or len(targets) != 2:
        raise BundleError("bundle must contain exactly two hardware targets")
    expected_targets = {"jcmk-c5-wardriver", "lilygo-t-dongle-c5"}
    if {target.get("id") for target in targets} != expected_targets:
        raise BundleError("bundle target set is incomplete")

    referenced: set[str] = set()
    for target in targets:
        flash = target.get("flash", {})
        factory = flash.get("factory", {}).get("segments", [])
        update = flash.get("update", {}).get("segments", [])
        if len(factory) < 4 or len(update) != 1:
            raise BundleError(f"{target.get('id')} has incomplete factory/update segments")
        applications = [segment for segment in factory if segment.get("role") == "application"]
        if len(applications) != 1 or update[0] != applications[0]:
            raise BundleError(f"{target.get('id')} update image does not match its factory application segment")
        for segment in factory:
            referenced.add(_validate_asset(bundle, segment))
        _validate_asset(bundle, update[0])

    actual = {path.name for path in bundle.glob("*.bin")}
    if actual != referenced:
        raise BundleError("bundle contains missing or unreferenced binary assets")
    return len(targets), len(actual)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    try:
        targets, assets = validate_bundle(args.bundle, args.version, args.source_commit)
    except (BundleError, json.JSONDecodeError) as error:
        raise SystemExit(f"release bundle error: {error}") from error
    print(f"Validated {targets} targets and {assets} binary assets.")


if __name__ == "__main__":
    main()
