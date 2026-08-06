#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "missioncache.md"
RUN_ID = "31097579829"
SOURCE_SHA = "a99e97bd852d0bbf06fbd8d59730db41e70e208d"

text = PATH.read_text(encoding="utf-8")
old_state = "**Release state:** IMPLEMENTED — CROSS-PLATFORM, PACKAGE, AND RELEASE VALIDATION PENDING. The equipment/carry/target curriculum remains intact for v0.7.18."
new_state = "**Release state:** VALIDATED — PR LINUX/WINDOWS/PACKAGE GATE PASSED; MERGE AND PUBLICATION PENDING. The equipment/carry/target curriculum remains intact for v0.7.18."
if old_state in text:
    text = text.replace(old_state, new_state, 1)
elif new_state not in text:
    raise RuntimeError("v0.7.17 release-state line not found")

for mission in range(181, 210):
    pattern = re.compile(
        rf"(### WALK-[^\n]+-{mission} — [^\n]+\n)\*\*Status:\*\* [^\n]+"
    )
    replacement = (
        rf"\1**Status:** VERIFIED — RUN {RUN_ID}; SOURCE {SOURCE_SHA}; "
        "LINUX, WINDOWS, PACKAGE, FALLBACK, ARCHIVE, EXTRACTION, AND ARTIFACT GATES PASSED"
    )
    text, count = pattern.subn(replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"mission {mission} status not found")

release_pattern = re.compile(
    r"(### WALK-RELEASE-210 — [^\n]+\n)\*\*Status:\*\* [^\n]+"
)
text, count = release_pattern.subn(
    rf"\1**Status:** VALIDATED — RUN {RUN_ID}; SOURCE {SOURCE_SHA}; MERGE, TAG, PUBLICATION, RE-DOWNLOAD, AND BRANCH CLEANUP PENDING",
    text,
    count=1,
)
if count != 1:
    raise RuntimeError("release mission 210 status not found")

evidence_heading = "## v0.7.17 authoritative PR validation evidence"
if evidence_heading not in text:
    marker = "**Authoritative rule:**"
    evidence = f'''{evidence_heading}

- **Workflow:** `Runner v0.7.17 validation and release`
- **Run:** `{RUN_ID}`
- **Validated source:** `{SOURCE_SHA}`
- **Linux:** GCC 14 warnings-as-errors, repository/art audit, and all deterministic suites passed.
- **Windows:** full SDL3/Vulkan configure/build and complete CTest matrix passed.
- **Runtime acceptance:** all eight six-seed Stand cases, all eight four-seed crouch/hold/recover cases, and the 24/24 live acceptance matrix passed.
- **Package:** build-tree diagnostics, installed diagnostics, optional-art removal fallback, `run.bat`, ZIP, SHA-256, per-file manifest, independent extraction, extracted diagnostics, and workflow artifact upload passed.
- **Remaining release work:** merge PR #56, publish annotated `v0.7.17`, re-download and byte-compare assets, then remove obsolete and completed v0.7.17 branches.

'''
    if marker not in text:
        raise RuntimeError("mission-cache authoritative-rule marker missing")
    text = text.replace(marker, evidence + marker, 1)

PATH.write_text(text, encoding="utf-8", newline="\n")
