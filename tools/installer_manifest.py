#!/usr/bin/env python3
"""Build fail-closed C5 Wardriver installer metadata from Arduino flash_args."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shlex
import shutil
from pathlib import Path

SCHEMA_VERSION = 1
SOURCE_REPOSITORY = "justcallmekoko/ESP32DualBandWardriver"
TARGET_FIELDS = {"id", "displayName", "aliases", "buildFlag", "assetSuffix", "chipFamily", "esptoolChip"}
SHA = re.compile(r"^[0-9a-f]{40}$")
OFFSET = re.compile(r"^0x[0-9a-fA-F]+$")

class ManifestError(RuntimeError):
    pass

def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def registry(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ManifestError(f"invalid target registry: {error}") from error
    targets = data.get("targets")
    if data.get("schemaVersion") != SCHEMA_VERSION or not isinstance(targets, list) or len(targets) != 1:
        raise ManifestError("registry must contain exactly one schema-v1 target")
    target = targets[0]
    if not isinstance(target, dict) or set(target) != TARGET_FIELDS:
        raise ManifestError("registry target fields are invalid")
    if target["buildFlag"] != "C5_WARDRIVER" or target["chipFamily"] != "ESP32-C5" or target["esptoolChip"] != "esp32c5":
        raise ManifestError("registry is not a C5 Wardriver target")
    return target

def role(path: Path) -> str:
    name = path.name.lower()
    if "bootloader" in name: return "bootloader"
    if "partition" in name: return "partition-table"
    if "boot_app0" in name or "ota_data" in name: return "ota-data"
    if name.endswith(".ino.bin"): return "application"
    return "auxiliary"

def parse_flash_args(build_dir: Path) -> tuple[dict, list[tuple[int, Path]]]:
    flash_args = build_dir / "flash_args"
    if not flash_args.is_file():
        raise ManifestError("missing Arduino flash_args; refusing to guess flash geometry")
    tokens = shlex.split(flash_args.read_text(encoding="utf-8"))
    options = {}
    for key in ("--flash_mode", "--flash_freq", "--flash_size"):
        try: options[key] = tokens[tokens.index(key) + 1]
        except (ValueError, IndexError) as error: raise ManifestError(f"flash_args lacks {key}") from error
    segments = []
    for index, value in enumerate(tokens[:-1]):
        if OFFSET.fullmatch(value):
            path = (build_dir / tokens[index + 1]).resolve()
            if not path.is_file(): raise ManifestError(f"flash segment missing: {path}")
            segments.append((int(value, 16), path))
    if not segments or len({offset for offset, _ in segments}) != len(segments):
        raise ManifestError("flash_args has no segments or duplicate offsets")
    if sum(role(path) == "application" for _, path in segments) != 1:
        raise ManifestError("flash_args must declare exactly one application image")
    return options, sorted(segments)

def parse_size(value: str) -> int:
    match = re.fullmatch(r"(\d+)(MB|M)", value, re.I)
    if not match: raise ManifestError(f"unsupported flash size: {value}")
    return int(match.group(1)) * 1024 * 1024

def generate(args: argparse.Namespace) -> Path:
    target = registry(args.registry)
    if args.build_flag != target["buildFlag"]: raise ManifestError("build flag does not match registry")
    if not SHA.fullmatch(args.source_commit): raise ManifestError("source commit must be a lowercase full SHA")
    if not re.fullmatch(r"v[0-9A-Za-z._-]+", args.version): raise ManifestError("invalid version")
    if not re.fullmatch(r"\d{8}", args.release_date): raise ManifestError("invalid release date")
    options, sources = parse_flash_args(args.build_dir)
    flash_size = parse_size(options["--flash_size"])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    stem = f"esp32_c5_wardriver_installer_{args.version.replace('.', '_')}_{args.release_date}_{target['assetSuffix']}"
    segments = []
    used = set()
    for offset, source in sources:
        file_role = role(source)
        name = f"{stem}.bin" if file_role == "application" else f"{stem}.{file_role}.bin"
        if name in used: raise ManifestError("multiple flash segments map to one asset")
        used.add(name)
        output = args.output_dir / name
        shutil.copyfile(source, output)
        size = output.stat().st_size
        if not size or offset + size > flash_size: raise ManifestError("segment is empty or exceeds flash size")
        segments.append({"role": file_role, "offset": offset, "size": size, "sha256": sha256(output), "fileName": name})
    for current, following in zip(segments, segments[1:]):
        if current["offset"] + current["size"] > following["offset"]: raise ManifestError("flash segments overlap")
    application = [item for item in segments if item["role"] == "application"]
    data = {"$schema": f"https://raw.githubusercontent.com/{SOURCE_REPOSITORY}/{args.source_commit}/installer/firmware-manifest.schema.json", "schemaVersion": 1, "kind": "esp32-c5-wardriver-installer-release", "metadataStatus": "authoritative", "sourceRepository": SOURCE_REPOSITORY, "sourceCommit": args.source_commit, "channel": "stable", "version": args.version, "targets": [{**target, "flash": {"sizeBytes": flash_size, "mode": options["--flash_mode"], "frequency": options["--flash_freq"], "update": {"erase": False, "preservesUserData": True, "segments": application}, "factory": {"erase": True, "preservesUserData": False, "segments": segments}}}]}
    manifest = args.output_dir / "firmware-manifest.json"
    manifest.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return manifest

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--registry", type=Path, default=Path("installer/targets.json"))
    parser.add_argument("--validate-registry", action="store_true")
    parser.add_argument("--build-flag")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--release-date")
    parser.add_argument("--source-commit")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    if args.validate_registry:
        registry(args.registry); print("Validated C5 Wardriver installer target."); return
    if not all((args.build_flag, args.build_dir, args.version, args.release_date, args.source_commit, args.output_dir)):
        raise ManifestError("all build inputs are required")
    print(generate(args))

if __name__ == "__main__":
    try: main()
    except ManifestError as error: raise SystemExit(f"installer manifest error: {error}") from error
