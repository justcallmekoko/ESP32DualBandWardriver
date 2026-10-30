# C5 Wardriver installer manifest

`targets.json` is the authoritative installer identity registry. It currently covers:

- `jcmk-c5-wardriver` — JCMK C5 Wardriver (`C5_WARDRIVER`, asset suffix `esp32c5devkitc1`)
- `lilygo-t-dongle-c5` — LilyGo T-Dongle C5 (`T_DONGLE_C5`, asset suffix `lilygo_t_dongle_c5`; aliases `T-Dongle C5` and `TDongle-C5`)

Target IDs, build flags, asset suffixes, display names, and aliases must remain unique. Both products use ESP32-C5, so chip detection cannot distinguish the board variant: installers must require the user to select the exact hardware target.

## Authoritative bundle generation

`tools/installer_manifest.py` requires one explicit `TARGET_ID=BUILD_FLAG=BUILD_DIR` mapping for every registry target. It rejects missing, extra, duplicate, cross-mapped, and shared-directory inputs. Each target must be compiled into its own build directory with its registry build flag. The workflow discovers all registry entries, performs those isolated builds, and passes the complete mapping set to one manifest-generation invocation.

Flash mode, frequency, size, segment offsets, and segment files are derived independently from each target's expanded Arduino esptool upload recipe or concrete `flash_args`. If either build lacks authoritative metadata, generation fails rather than guessing. Segment sources must remain inside that target's build directory, except for Arduino ESP32 core's exact `tools/partitions/boot_app0.bin` OTA seed.

Every copied binary is hashed after staging and named with the target's unique asset suffix. The single `firmware-manifest.json` lists all registry targets in registry order. Update plans include only the exact application image; factory plans include the complete exact flash layout. Output is staged and only installed as a bundle after every target succeeds, preventing a partial target set from being treated as authoritative.

## Workflow behavior

Pull requests rebuild and upload the complete multi-target bundle as a short-lived review artifact, exercising the same build and manifest-generation path without touching a release. On publication of a non-prerelease release, the workflow rebuilds the tagged source, validates the firmware tag against `src/configs.h`, hashes every generated segment, and attaches an additive `firmware-manifest.json` plus namespaced binary assets. Existing draft-release generation and its existing binaries are unchanged.

No manifest is attached to old releases or drafts. The installer must therefore reject them until a release has completed this authoritative workflow.
