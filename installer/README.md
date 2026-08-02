# C5 Wardriver installer manifest

`targets.json` is the C5 installer identity registry. `tools/installer_manifest.py` creates release assets only from Arduino's concrete `flash_args`; if the build does not provide those arguments, generation fails rather than guessing flash geometry.

Pull requests rebuild and upload the complete bundle as a short-lived review artifact, exercising the same build and manifest-generation path without touching a release. On publication of a non-prerelease release, the workflow rebuilds the tagged source, validates the firmware tag against `src/configs.h`, hashes the generated segments, and attaches an additive `firmware-manifest.json` plus uniquely named binary assets. Existing draft-release generation and its three existing binaries are unchanged.

No manifest is attached to old releases or drafts. The installer must therefore reject them until a release has completed this authoritative workflow.
