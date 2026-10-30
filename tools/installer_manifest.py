#!/usr/bin/env python3
"""Build fail-closed multi-target C5 Wardriver installer metadata."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shlex
import shutil
import tempfile
from pathlib import Path

SCHEMA_VERSION = 1
SOURCE_REPOSITORY = "justcallmekoko/ESP32DualBandWardriver"
REGISTRY_FIELDS = {"schemaVersion", "sourceWorkflow", "targets"}
TARGET_FIELDS = {"id", "displayName", "aliases", "buildFlag", "assetSuffix", "chipFamily", "esptoolChip"}
SHA = re.compile(r"^[0-9a-f]{40}$")
OFFSET = re.compile(r"^0x[0-9a-fA-F]+$")
IDENTIFIER = re.compile(r"^[a-z0-9][a-z0-9-]*$")
BUILD_FLAG = re.compile(r"^[A-Z0-9_]+$")
ASSET_SUFFIX = re.compile(r"^[A-Za-z0-9_]+$")
FLASH_MODES = {"dio", "dout", "qio", "qout"}
FLASH_FREQUENCIES = {"20m", "26m", "40m", "80m"}


class ManifestError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _unique(values: list[str], description: str, *, casefold: bool = False) -> None:
    normalized = [value.casefold() if casefold else value for value in values]
    if len(set(normalized)) != len(normalized):
        raise ManifestError(f"registry {description} must be unique")


def registry(path: Path) -> list[dict]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ManifestError(f"invalid target registry: {error}") from error

    if not isinstance(data, dict) or set(data) != REGISTRY_FIELDS:
        raise ManifestError("registry fields are invalid")
    targets = data.get("targets")
    if data.get("schemaVersion") != SCHEMA_VERSION or not isinstance(targets, list) or not targets:
        raise ManifestError("registry must contain at least one schema-v1 target")
    if not isinstance(data.get("sourceWorkflow"), str) or not data["sourceWorkflow"].strip():
        raise ManifestError("registry source workflow is invalid")

    for target in targets:
        if not isinstance(target, dict) or set(target) != TARGET_FIELDS:
            raise ManifestError("registry target fields are invalid")
        if not isinstance(target["id"], str) or not IDENTIFIER.fullmatch(target["id"]):
            raise ManifestError("registry target id is invalid")
        if not isinstance(target["displayName"], str) or not target["displayName"].strip():
            raise ManifestError("registry display name is invalid")
        aliases = target["aliases"]
        if (
            not isinstance(aliases, list)
            or not aliases
            or any(not isinstance(alias, str) or not alias.strip() for alias in aliases)
            or len({alias.casefold() for alias in aliases}) != len(aliases)
        ):
            raise ManifestError("registry aliases are invalid")
        if not isinstance(target["buildFlag"], str) or not BUILD_FLAG.fullmatch(target["buildFlag"]):
            raise ManifestError("registry build flag is invalid")
        if not isinstance(target["assetSuffix"], str) or not ASSET_SUFFIX.fullmatch(target["assetSuffix"]):
            raise ManifestError("registry asset suffix is invalid")
        if target["chipFamily"] != "ESP32-C5" or target["esptoolChip"] != "esp32c5":
            raise ManifestError("registry target is not an ESP32-C5 Wardriver")

    _unique([target["id"] for target in targets], "target ids")
    _unique([target["buildFlag"] for target in targets], "build flags")
    _unique([target["assetSuffix"] for target in targets], "asset suffixes", casefold=True)
    identity_names = [
        name.strip()
        for target in targets
        for name in [target["displayName"], *target["aliases"]]
    ]
    _unique(identity_names, "display names and aliases", casefold=True)
    return targets


def parse_target_builds(specs: list[str] | None, targets: list[dict]) -> list[tuple[dict, Path]]:
    """Bind every registry target to its exact build flag and isolated build directory."""
    by_id = {target["id"]: target for target in targets}
    mappings: dict[str, tuple[str, Path]] = {}
    for spec in specs or []:
        parts = spec.split("=", 2)
        if len(parts) != 3 or not all(parts):
            raise ManifestError("target build must use TARGET_ID=BUILD_FLAG=BUILD_DIR")
        target_id, build_flag, raw_path = parts
        if target_id in mappings:
            raise ManifestError(f"duplicate build mapping for target: {target_id}")
        if target_id not in by_id:
            raise ManifestError(f"extra build mapping for unknown target: {target_id}")
        expected_flag = by_id[target_id]["buildFlag"]
        if build_flag != expected_flag:
            raise ManifestError(
                f"build mapping for {target_id} uses {build_flag}, expected {expected_flag}"
            )
        mappings[target_id] = (build_flag, Path(raw_path))

    missing = [target["id"] for target in targets if target["id"] not in mappings]
    if missing:
        raise ManifestError(f"missing build mappings for targets: {', '.join(missing)}")

    resolved = [path.resolve() for _, path in mappings.values()]
    if len(set(resolved)) != len(resolved):
        raise ManifestError("target builds must use isolated build directories")
    return [(target, mappings[target["id"]][1]) for target in targets]


def role(path: Path) -> str:
    name = path.name.lower()
    if "bootloader" in name:
        return "bootloader"
    if "partition" in name:
        return "partition-table"
    if "boot_app0" in name or "ota_data" in name:
        return "ota-data"
    if name.endswith(".ino.bin"):
        return "application"
    return "auxiliary"


def parse_flash_args(build_dir: Path) -> tuple[dict, list[tuple[int, Path]]]:
    build_root = build_dir.resolve()
    properties_path = build_dir / "arduino-build-properties.txt"
    if properties_path.is_file():
        properties = {}
        for line in properties_path.read_text(encoding="utf-8").splitlines():
            key, separator, value = line.partition("=")
            if separator:
                properties[key] = value
        try:
            tokens = shlex.split(properties["tools.esptool_py.upload.pattern_args"])
            options = {
                "--flash_mode": properties["build.flash_mode"].lower(),
                "--flash_freq": properties["build.flash_freq"].lower(),
                "--flash_size": properties["build.flash_size"],
            }
        except KeyError as error:
            raise ManifestError(f"Arduino build properties lack {error.args[0]}") from error
        source_description = "Arduino build properties"
    else:
        flash_args = next(
            (
                candidate
                for candidate in (build_dir / "flash_args", build_dir / "flash_args.txt")
                if candidate.is_file()
            ),
            None,
        )
        if flash_args is None:
            raise ManifestError(
                "missing Arduino build properties or flash_args; refusing to guess flash geometry"
            )
        tokens = shlex.split(flash_args.read_text(encoding="utf-8"))
        options = {}
        for normalized, spellings in (
            ("--flash_mode", ("--flash_mode", "--flash-mode")),
            ("--flash_freq", ("--flash_freq", "--flash-freq")),
            ("--flash_size", ("--flash_size", "--flash-size")),
        ):
            option = next((spelling for spelling in spellings if spelling in tokens), None)
            try:
                if option is None:
                    raise ValueError
                options[normalized] = tokens[tokens.index(option) + 1]
            except (ValueError, IndexError) as error:
                raise ManifestError(f"flash_args lacks {normalized}") from error
        options["--flash_mode"] = options["--flash_mode"].lower()
        options["--flash_freq"] = options["--flash_freq"].lower()
        source_description = "flash_args"

    if options["--flash_mode"] not in FLASH_MODES:
        raise ManifestError(f"unsupported flash mode: {options['--flash_mode']}")
    if options["--flash_freq"] not in FLASH_FREQUENCIES:
        raise ManifestError(f"unsupported flash frequency: {options['--flash_freq']}")

    segments = []
    for index, value in enumerate(tokens[:-1]):
        if OFFSET.fullmatch(value):
            path = (build_dir / tokens[index + 1]).resolve()
            is_core_ota_seed = (
                path.name == "boot_app0.bin"
                and path.parent.name == "partitions"
                and path.parent.parent.name == "tools"
            )
            if not path.is_relative_to(build_root) and not is_core_ota_seed:
                raise ManifestError(f"flash segment escapes build directory: {path}")
            if not path.is_file():
                raise ManifestError(f"flash segment missing: {path}")
            segments.append((int(value, 16), path))
    if not segments or len({offset for offset, _ in segments}) != len(segments):
        raise ManifestError(f"{source_description} has no segments or duplicate offsets")
    if len({path for _, path in segments}) != len(segments):
        raise ManifestError("flash_args references a segment more than once")
    if sum(role(path) == "application" for _, path in segments) != 1:
        raise ManifestError("flash_args must declare exactly one application image")
    return options, sorted(segments)


def parse_size(value: str) -> int:
    match = re.fullmatch(r"(\d+)(MB|M)", value, re.I)
    if not match:
        raise ManifestError(f"unsupported flash size: {value}")
    return int(match.group(1)) * 1024 * 1024


def _target_manifest(
    target: dict,
    build_dir: Path,
    destination: Path,
    version: str,
    release_date: str,
    asset_names: set[str],
) -> dict:
    options, sources = parse_flash_args(build_dir)
    flash_size = parse_size(options["--flash_size"])
    stem = (
        f"esp32_c5_wardriver_installer_{version.replace('.', '_')}_"
        f"{release_date}_{target['assetSuffix']}"
    )
    segments = []
    for offset, source in sources:
        file_role = role(source)
        name = f"{stem}.bin" if file_role == "application" else f"{stem}.{file_role}.bin"
        if name in asset_names:
            raise ManifestError("multiple flash segments map to one namespaced asset")
        asset_names.add(name)
        output = destination / name
        shutil.copyfile(source, output)
        size = output.stat().st_size
        if not size or offset + size > flash_size:
            raise ManifestError("segment is empty or exceeds flash size")
        segments.append(
            {
                "role": file_role,
                "offset": offset,
                "size": size,
                "sha256": sha256(output),
                "fileName": name,
            }
        )
    for current, following in zip(segments, segments[1:]):
        if current["offset"] + current["size"] > following["offset"]:
            raise ManifestError("flash segments overlap")
    application = [item for item in segments if item["role"] == "application"]
    return {
        **target,
        "flash": {
            "sizeBytes": flash_size,
            "mode": options["--flash_mode"],
            "frequency": options["--flash_freq"],
            "update": {
                "erase": False,
                "preservesUserData": True,
                "segments": application,
            },
            "factory": {
                "erase": True,
                "preservesUserData": False,
                "segments": segments,
            },
        },
    }


def generate(args: argparse.Namespace) -> Path:
    targets = registry(args.registry)
    builds = parse_target_builds(args.target_builds, targets)
    if not SHA.fullmatch(args.source_commit):
        raise ManifestError("source commit must be a lowercase full SHA")
    if not re.fullmatch(r"v[0-9A-Za-z._-]+", args.version):
        raise ManifestError("invalid version")
    if not re.fullmatch(r"\d{8}", args.release_date):
        raise ManifestError("invalid release date")

    output_dir = args.output_dir.resolve()
    if output_dir.exists() and any(output_dir.iterdir()):
        raise ManifestError("output directory must be empty")
    output_dir.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="installer-assets-", dir=output_dir.parent) as temporary:
        staging = Path(temporary)
        asset_names: set[str] = set()
        manifest_targets = [
            _target_manifest(
                target,
                build_dir,
                staging,
                args.version,
                args.release_date,
                asset_names,
            )
            for target, build_dir in builds
        ]
        data = {
            "$schema": (
                f"https://raw.githubusercontent.com/{SOURCE_REPOSITORY}/"
                f"{args.source_commit}/installer/firmware-manifest.schema.json"
            ),
            "schemaVersion": SCHEMA_VERSION,
            "kind": "esp32-c5-wardriver-installer-release",
            "metadataStatus": "authoritative",
            "sourceRepository": SOURCE_REPOSITORY,
            "sourceCommit": args.source_commit,
            "channel": "stable",
            "version": args.version,
            "targets": manifest_targets,
        }
        (staging / "firmware-manifest.json").write_text(
            json.dumps(data, indent=2) + "\n", encoding="utf-8"
        )
        if output_dir.exists():
            output_dir.rmdir()
        shutil.move(str(staging), output_dir)
    return output_dir / "firmware-manifest.json"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--registry", type=Path, default=Path("installer/targets.json"))
    parser.add_argument("--validate-registry", action="store_true")
    parser.add_argument(
        "--target-build",
        dest="target_builds",
        action="append",
        help="repeat for every registry target as TARGET_ID=BUILD_FLAG=BUILD_DIR",
    )
    parser.add_argument("--version")
    parser.add_argument("--release-date")
    parser.add_argument("--source-commit")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    if args.validate_registry:
        targets = registry(args.registry)
        print(f"Validated {len(targets)} C5 Wardriver installer targets.")
        return
    if not all(
        (
            args.target_builds,
            args.version,
            args.release_date,
            args.source_commit,
            args.output_dir,
        )
    ):
        raise ManifestError("all build inputs are required")
    print(generate(args))


if __name__ == "__main__":
    try:
        main()
    except ManifestError as error:
        raise SystemExit(f"installer manifest error: {error}") from error
