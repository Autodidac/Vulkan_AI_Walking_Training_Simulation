#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def cache() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    marker = "# Runner v0.7.18 equipment, carry, and target curriculum"
    finding = '''## v0.7.17 twelfth validation finding — Windows optional-art byte preservation

PR gate run `31096112530` built the complete Windows SDL3/Vulkan application successfully and passed Runner.Core, Runner.V0717EyeTest, all eight six-seed Stand cases, all eight four-seed crouch/hold/recover cases, the 24/24 live acceptance matrix, package diagnostics, camera diagnostics, deformable terrain, SandHybrid integration, runtime pipeline, and the warmed concurrency benchmark. The sole Windows failure was repository hygiene: Git checkout converted the text-form P3 PPM assets from LF to CRLF, so their raw SHA-256 bytes differed from the provenance contract even though parsing and runtime art tests passed.

The optional PPM source and runtime sprites must be marked `-text` in `.gitattributes` so Git preserves their committed bytes identically on Linux and Windows. The existing hashes remain authoritative; no art, physics, training, or acceptance behavior changes. Full Windows package installation, fallback, archive, extraction, upload, publication, re-download, and branch cleanup gates remain mandatory.

'''
    if "## v0.7.17 twelfth validation finding" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.18 carry-forward marker missing")
        text = text.replace(marker, finding + marker, 1)
    path.write_text(text, encoding="utf-8", newline="\n")


def implement() -> None:
    attributes = ROOT / ".gitattributes"
    attributes.write_text(
        "# Preserve byte-audited optional P3 artwork on every platform.\n"
        "assets/optional/runner_armor_concepts/**/*.ppm -text\n",
        encoding="utf-8",
        newline="\n",
    )


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: fix_v0717_ppm_line_endings.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
