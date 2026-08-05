#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MISSION = ROOT / "missioncache.md"
CAMERA_DOC = ROOT / "docs/RUNNER_V0716_CAMERA_BATCH.md"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def main() -> int:
    mission = MISSION.read_text(encoding="utf-8")
    mission = replace_once(
        mission,
        "**Release state:** PR PACKAGE VALIDATED — merge, publication, re-download, and final cleanup remain.",
        "**Release state:** PUBLISHED — RELEASE ASSETS RE-DOWNLOADED AND VERIFIED; USER EYE TEST REMAINS AUTHORITATIVE.",
        "v0.7.16 release state",
    )
    mission = replace_once(
        mission,
        "### WALK-RELEASE-180 — Publish audited Runner v0.7.16\n"
        "**Status:** READY FOR MERGE — AUDITED PR PACKAGE GATE PASSED",
        "### WALK-RELEASE-180 — Publish audited Runner v0.7.16\n"
        "**Status:** PUBLISHED — RELEASE ASSETS, RE-DOWNLOAD, AND CLEANUP VERIFIED",
        "release mission status",
    )
    mission = replace_once(
        mission,
        "- Publication remains blocked only on merge, the main-branch publisher, published-asset re-download verification, and release-branch cleanup.",
        "- Merge commit: `1577706cade4a47cfde9c2834af22279e2cd793f`.\n"
        "- Published tag: `v0.7.16`, resolving to the merged Runner 0.7.16 source.\n"
        "- The main-branch publisher created the release, re-downloaded and byte-compared every published asset, verified the ZIP SHA-256, and then removed the completed release branch.\n"
        "- Repository cleanup after publication: only `main` remains.\n"
        "- Contradictory eye-test evidence reopens only the exact affected camera, PIP, layout, locomotion, or package mission.",
        "publication evidence",
    )
    MISSION.write_text(mission, encoding="utf-8", newline="\n")

    camera = CAMERA_DOC.read_text(encoding="utf-8")
    camera = replace_once(
        camera,
        "The permanent release workflow repeats these gates for the final source, records mission closure only after both platform jobs pass, and then requires merge, main-branch publication, published-asset re-download verification, and release-branch cleanup. The one-use ledger closure script is removed in the same evidence commit.",
        "The permanent release workflow repeated these gates for the final source and recorded mission closure only after both platform jobs passed. PR #55 merged as `1577706cade4a47cfde9c2834af22279e2cd793f`; tag `v0.7.16` now resolves to Runner 0.7.16. The publisher re-downloaded and byte-compared every release asset, verified the ZIP SHA-256, removed the completed release branch, and left only `main`. The one-use ledger scripts and workflows are absent. User eye testing remains the final authority for framing and readability.",
        "camera publication evidence",
    )
    CAMERA_DOC.write_text(camera, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
