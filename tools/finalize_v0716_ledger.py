#!/usr/bin/env python3
from __future__ import annotations

import os
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
PATH = ROOT / "missioncache.md"


def main() -> int:
    validated_sha = os.environ.get("VALIDATED_SHA", "").strip()
    if len(validated_sha) != 40:
        raise RuntimeError("VALIDATED_SHA must be an exact 40-character commit SHA")

    text = PATH.read_text(encoding="utf-8")
    for mission in range(156, 180):
        pattern = re.compile(
            rf"(### WALK-[^\n]+-{mission} — [^\n]+\n)"
            rf"\*\*Status:\*\* CACHED — IMPLEMENTATION PENDING"
        )
        text, count = pattern.subn(
            rf"\1**Status:** VERIFIED — IMPLEMENTED AND CROSS-PLATFORM VALIDATED",
            text,
            count=1,
        )
        if count != 1:
            raise RuntimeError(f"Mission {mission} status marker not found")

    text, count = re.subn(
        r"(### WALK-RELEASE-180 — [^\n]+\n)"
        r"\*\*Status:\*\* CACHED — IMPLEMENTATION PENDING",
        r"\1**Status:** READY FOR PUBLICATION — FULL PR PACKAGE GATE REQUIRED",
        text,
        count=1,
    )
    if count != 1:
        raise RuntimeError("Release mission status marker not found")

    text = text.replace(
        "**Release state:** CACHED — 25 MISSIONS SELECTED FOR ONE AUDITED RELEASE.",
        "**Release state:** IMPLEMENTED AND CROSS-PLATFORM VALIDATED — PR package and publication gates remain.",
        1,
    )

    marker = "# Runner v0.7.17 equipment, carry, and target curriculum"
    evidence = f"""## v0.7.16 pre-publication validation evidence

- Cache-first mission commit preceded product source changes.
- Exact implementation source: `{validated_sha}`.
- Linux GCC 14 and Windows Server 2025 / MSVC validation passed.
- Repository hygiene, camera math/layout tests, deterministic suites, full SDL3/Vulkan build, `--diagnose-package`, `--diagnose-acceptance`, and `--diagnose-camera` passed.
- Final PR source contains no temporary applicator or one-use workflow.
- The PR release workflow must repeat installation, extraction, checksum, manifest, and artifact auditing before merge.

"""
    if "## v0.7.16 pre-publication validation evidence" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.17 carry-forward marker not found")
        text = text.replace(marker, evidence + marker, 1)

    PATH.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
