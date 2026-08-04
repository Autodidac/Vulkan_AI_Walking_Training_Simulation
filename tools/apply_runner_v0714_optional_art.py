#!/usr/bin/env python3
from __future__ import annotations

import base64
import hashlib
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHUNK_DIR = ROOT / "tools" / "runner_v0714_optional_art"
ASSET_DIR = ROOT / "assets" / "optional" / "runner_armor_concepts"
ASSET_PATH = ASSET_DIR / "runner_armor_concepts.webp"
EXPECTED_SIZE = 14_422
EXPECTED_SHA256 = "2f99c87459bb1fbc28bc556e8df07b58a5b2d51650a300563bffcb8a38a57f19"


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def materialize_asset() -> None:
    chunks = sorted(CHUNK_DIR.glob("chunk_*.b64"))
    if len(chunks) != 4:
        raise SystemExit(f"expected four optional-art chunks, found {len(chunks)}")
    encoded = "".join(chunk.read_text(encoding="ascii").strip() for chunk in chunks)
    payload = base64.b64decode(encoded, validate=True)
    digest = hashlib.sha256(payload).hexdigest()
    if len(payload) != EXPECTED_SIZE or digest != EXPECTED_SHA256:
        raise SystemExit(
            f"optional art mismatch: size={len(payload)} sha256={digest}"
        )
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    ASSET_PATH.write_bytes(payload)

    write(
        "assets/optional/runner_armor_concepts/README.md",
        """# Optional Runner armor concept art

`runner_armor_concepts.webp` is a compact package reference assembled from the four modular sci-fi armor sheets supplied for Runner. It is deliberately optional and is not loaded by startup, diagnostics, simulation, training, or the current renderer.

- Dimensions: 200 x 141 RGBA WebP
- SHA-256: `2f99c87459bb1fbc28bc556e8df07b58a5b2d51650a300563bffcb8a38a57f19`
- Intended use: future rig skins, armor-part atlases, character previews, or replacement art derived from the supplied visual direction
- Allowed preparation: crop, repack, recolor, separate parts, or remake into a deterministic atlas as needed
- Required fallback: Runner must retain its existing rig rendering and operate normally when this directory is missing

The reference must not create new automatic panels, startup dependencies, or unrelated UI controls.
""",
    )


def update_mission_cache() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    mission = """
### WALK-ART-136 — Optional user armor concept assets
**Status:** IMPLEMENTED — VALIDATION REQUIRED

The four supplied modular sci-fi armor sheets are preserved as a compact optional contact sheet at `assets/optional/runner_armor_concepts/runner_armor_concepts.webp`, with provenance, hash, intended uses, and fallback rules documented beside it. The art may be cropped, repacked, recolored, separated into parts, or remade into a deterministic atlas before runtime use. Runtime adoption remains opt-in and the existing rig renderer remains the required fallback.

Acceptance requires the Windows package to include the optional reference and README, verify the recorded SHA-256, and still pass package and all-rig acceptance diagnostics after the entire `assets/optional` directory is removed. Missing or invalid optional art must never abort startup, alter training, or create automatic UI panels.

"""
    if "### WALK-ART-136" not in text:
        marker = "### WALK-CLIMB-134"
        if marker not in text:
            raise SystemExit("WALK-CLIMB-134 insertion marker missing")
        text = text.replace(marker, mission + marker, 1)
    path.write_text(text, encoding="utf-8", newline="\n")


def update_changelog() -> None:
    path = ROOT / "CHANGELOG.md"
    text = path.read_text(encoding="utf-8")
    bullet = "- Packaged the user-supplied modular armor direction as an optional, non-runtime concept contact sheet with a mandatory no-asset fallback."
    if bullet not in text:
        marker = "### Added\n"
        start = text.find("## [0.7.14]")
        if start < 0:
            raise SystemExit("v0.7.14 changelog section missing")
        pos = text.find(marker, start)
        if pos < 0:
            raise SystemExit("v0.7.14 Added section missing")
        pos += len(marker)
        text = text[:pos] + "\n" + bullet + "\n" + text[pos:]
    path.write_text(text, encoding="utf-8", newline="\n")


def update_readme() -> None:
    path = ROOT / "README.md"
    text = path.read_text(encoding="utf-8")
    section = """
## Optional concept art

The package includes `assets/optional/runner_armor_concepts/runner_armor_concepts.webp`, a compact reference assembled from Adam's modular sci-fi armor sheets. It is not a startup or rendering dependency. Future skin work may crop, repack, or remake it into a deterministic atlas, while the current rig renderer remains the fallback when optional art is absent.

"""
    if "## Optional concept art" not in text:
        marker = "## Controls"
        if marker not in text:
            raise SystemExit("README Controls marker missing")
        text = text.replace(marker, section + marker, 1)
    path.write_text(text, encoding="utf-8", newline="\n")


def cleanup_staging() -> None:
    shutil.rmtree(CHUNK_DIR)
    Path(__file__).unlink()


def main() -> None:
    materialize_asset()
    update_mission_cache()
    update_changelog()
    update_readme()
    cleanup_staging()


if __name__ == "__main__":
    main()
