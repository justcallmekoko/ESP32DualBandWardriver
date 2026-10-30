#!/usr/bin/env python3
"""Lock the tag-derived, pre-publication release workflow contract."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARALLEL = (ROOT / ".github/workflows/wardriver_build_parallel.yml").read_text()
MANIFEST = (ROOT / ".github/workflows/build_installer_manifest.yml").read_text()


def main() -> None:
    assert "workflow_call:" in MANIFEST
    assert "types: [published]" not in MANIFEST
    assert "Attach installer assets to published release" not in MANIFEST
    assert "gh release upload" not in MANIFEST

    assert "startsWith(github.ref, 'refs/tags/v')" in PARALLEL
    assert '[[ "$VERSION" =~ ^v(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$ ]]' in PARALLEL
    assert 'test "$VERSION" = "$GITHUB_REF_NAME"' in PARALLEL
    assert "actions/checkout@v" not in PARALLEL
    assert "actions/upload-artifact@v" not in PARALLEL
    assert "ArminJo/arduino-test-compile@v" not in PARALLEL
    assert "uses: ./.github/workflows/build_installer_manifest.yml" in PARALLEL
    assert "name: c5-installer-release-bundle" in PARALLEL
    assert "python3 tools/validate_release_bundle.py" in PARALLEL
    assert "Refusing to modify published release" in PARALLEL
    assert "softprops/action-gh-release@3bb12739c298aeb8a4eeaf626c5b8d85266b0e65" in PARALLEL
    assert "tools/validate_release_bundle.py" in MANIFEST
    assert "tools/test_release_bundle.py" in MANIFEST
    assert "release-assets/*.bin" in PARALLEL
    assert "release-assets/firmware-manifest.json" in PARALLEL
    assert "src.*.bin" not in PARALLEL.split("post_compile_steps:", 1)[1]


if __name__ == "__main__":
    main()
