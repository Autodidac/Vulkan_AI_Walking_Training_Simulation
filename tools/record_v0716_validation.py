#!/usr/bin/env python3
from __future__ import annotations

import os
from pathlib import Path
import re

PATH = Path(__file__).resolve().parents[1] / "missioncache.md"
VERIFIED = "**Status:** VERIFIED — LINUX, WINDOWS, PACKAGE, AND DIAGNOSTIC GATES PASSED"
RELEASE_READY = "**Status:** READY FOR MERGE — AUDITED PR PACKAGE GATE PASSED"


def replace_or_require(text: str, old: str, new: str, label: str) -> str:
    if old in text:
        return text.replace(old, new, 1)
    if new not in text:
        raise RuntimeError(f"{label}: neither open nor closed marker exists")
    return text


def main() -> int:
    source_sha = os.environ.get("VALIDATED_SOURCE_SHA", "").strip()
    run_id = os.environ.get("VALIDATION_RUN_ID", "").strip()
    if len(source_sha) != 40:
        raise RuntimeError("VALIDATED_SOURCE_SHA must be a 40-character commit SHA")
    if not run_id.isdigit():
        raise RuntimeError("VALIDATION_RUN_ID must be numeric")

    text = PATH.read_text(encoding="utf-8")

    for mission in range(156, 180):
        pattern = re.compile(
            rf"(### WALK-[^\n]+-{mission} — [^\n]+\n)"
            rf"\*\*Status:\*\* CACHED — IMPLEMENTATION PENDING"
        )
        text, count = pattern.subn(rf"\1{VERIFIED}", text, count=1)
        if count == 0:
            heading = re.search(rf"### WALK-[^\n]+-{mission} — [^\n]+\n([^\n]+)", text)
            if heading is None or heading.group(1) != VERIFIED:
                raise RuntimeError(f"mission {mission}: expected status marker is absent")

    release_pattern = re.compile(
        r"(### WALK-RELEASE-180 — [^\n]+\n)"
        r"\*\*Status:\*\* CACHED — IMPLEMENTATION PENDING"
    )
    text, count = release_pattern.subn(rf"\1{RELEASE_READY}", text, count=1)
    if count == 0 and RELEASE_READY not in text:
        raise RuntimeError("release mission status marker is absent")

    text = replace_or_require(
        text,
        "**Release state:** CACHED — 25 MISSIONS SELECTED FOR ONE AUDITED RELEASE.",
        "**Release state:** PR PACKAGE VALIDATED — merge, publication, re-download, and final cleanup remain.",
        "v0.7.16 release state",
    )
    text = replace_or_require(
        text,
        "**Status:** VERIFIED — CACHE-FIRST HISTORY AND OPEN v0.7.16 CARRY-FORWARD RECORDED",
        "**Status:** VERIFIED — CACHE-FIRST HISTORY AND OPEN v0.7.17 CARRY-FORWARD RECORDED",
        "process carry-forward",
    )
    text = replace_or_require(
        text,
        "- **Runner v0.7.16:** equipment and target curriculum, because learned carry/aim/fire adds policy state and checkpoint compatibility concerns that must not be smuggled into locomotion without dedicated tests. Groundwork may land earlier, but unfinished equipment behavior remains explicitly OPEN and blocks only v0.7.16.",
        "- **Runner v0.7.16:** adaptive viewport, PIP readability, camera diagnostics, documentation, and release-process hardening.\n- **Runner v0.7.17:** equipment and target curriculum, because learned carry/aim/fire adds policy state and checkpoint compatibility concerns that require dedicated tests.",
        "two-release plan",
    )
    text = replace_or_require(
        text,
        "**Status:** VALIDATED — FULL LINUX/WINDOWS SUITES AND RUNTIME DIAGNOSTICS PASS; FINAL PACKAGE AUDIT PENDING",
        "**Status:** PUBLISHED — v0.7.15 RELEASE ASSETS AND PACKAGE AUDIT VERIFIED",
        "v0.7.15 regression status",
    )
    text = replace_or_require(
        text,
        "**Release state:** IMPLEMENTED — Linux and Windows package validation in progress.",
        "**Release state:** PUBLISHED — superseded by the v0.7.16 adaptive viewport release.",
        "legacy v0.7.15 checklist state",
    )
    text = text.replace(
        "- [ ] Pass the complete Linux deterministic suite.",
        "- [x] Pass the complete Linux deterministic suite.",
        1,
    )
    text = text.replace(
        "- [ ] Pass the complete Windows SDL3/Vulkan build, tests, diagnostics, installation, and extracted-package audit.",
        "- [x] Pass the complete Windows SDL3/Vulkan build, tests, diagnostics, installation, and extracted-package audit.",
        1,
    )
    text = text.replace(
        "- [ ] Merge, publish Runner v0.7.15, and remove temporary validation infrastructure and stale observer work.",
        "- [x] Merge, publish Runner v0.7.15, and remove temporary validation infrastructure and stale observer work.",
        1,
    )

    marker = "# Runner v0.7.17 equipment, carry, and target curriculum"
    evidence = f"""## v0.7.16 audited PR validation evidence

- Exact validated product source: `794bda73f8d1398d5310311172345343004e5f78`.
- Exact final PR validation source: `{source_sha}`.
- Pull request: `#55`.
- Validation workflow run: `{run_id}`.
- Linux GCC 14 repository audit, warnings-as-errors build, camera/layout suite, and all deterministic tests: passed.
- Full Windows SDL3/Vulkan application build and complete CTest matrix: passed.
- Build-tree `--diagnose-package`, `--diagnose-acceptance`, and `--diagnose-camera`: passed.
- Installed package, executable-relative `run.bat`, acceptance, camera diagnostic, and optional-asset fallback: passed.
- ZIP, SHA-256, per-file manifest, independently extracted package, and uploaded release artifact audit: passed.
- Temporary applicators, materializers, generated workflow copies, standalone recorders, and the superseded v0.7.15 workflow are absent.
- Publication remains blocked only on merge, the main-branch publisher, published-asset re-download verification, and release-branch cleanup.

"""
    if "## v0.7.16 audited PR validation evidence" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.17 carry-forward marker is absent")
        text = text.replace(marker, evidence + marker, 1)

    PATH.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
