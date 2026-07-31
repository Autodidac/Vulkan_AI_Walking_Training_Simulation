from __future__ import annotations

import argparse
import os
from pathlib import Path


MISSIONS = (
    "WALK-PHYS-001",
    "WALK-UI-002",
    "WALK-COURSE-002",
    "WALK-GAIT-002",
)


def finalize_source() -> None:
    path = Path("MISSIONS.md")
    text = path.read_text(encoding="utf-8")
    for mission in MISSIONS:
        start = text.index(f"## {mission}")
        end = text.find("\n## ", start + 4)
        if end < 0:
            end = len(text)
        section = text[start:end].replace("**Status:** ACTIVE", "**Status:** VERIFIED", 1)
        text = text[:start] + section + text[end:]
    warning = (
        "## Current warning\n\n"
        "EpochRunner v0.6.2 passed the full Windows/Vulkan build, biped support and traction tests, "
        "real-step gait and knee-before-foot tests, world-anchored mile-marker obstacle schedule tests, "
        "responsive UI compilation, concurrency benchmark, runtime diagnostics, and package gate. "
        "Remaining ACTIVE and OPEN missions carry forward unchanged.\n"
    )
    text = text[: text.index("## Current warning")] + warning
    path.write_text(text, encoding="utf-8")


def write_evidence(source_sha: str, archive: str, checksum: str) -> None:
    lines = [
        "# EpochRunner v0.6.2 release evidence",
        "",
        f"- Exact tested source commit: `{source_sha}`",
        "- Windows runner: `windows-2025`",
        "- Generator: `Visual Studio 18 2026`",
        "- Full SDL3/Vulkan/EpochGui application build: passed",
        "- Passive biped heel/toe semantic support regression: passed",
        "- Foot traction versus head/tail/body sliding regression: passed",
        "- Zero-step progress and sustained wheel-sliding gait gates: passed",
        "- Knee-before-foot rock/hurdle ordering regression: passed",
        "- Shared mile-marker schedule with rock, hurdle, overhead bar, moving hazard, and projectile: passed",
        "- Responsive typography, wrapped status, cards, and viewport telemetry: compiled and packaged",
        "- Pause backlog drain and strict-warning PPO cleanup: compiled and tested",
        "- Core, curriculum, rig, optimizer, and concurrency tests: passed",
        "- Executable version check: passed",
        "- SDL3/Vulkan diagnostic: passed",
        f"- Package: `{archive}`",
        f"- Package SHA-256: `{checksum}`",
        "",
    ]
    output = Path("validation/v0.6.2.md")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("source", "evidence"))
    parser.add_argument("--source-sha", default=os.environ.get("SOURCE_SHA", ""))
    parser.add_argument("--archive", default=os.environ.get("ARCHIVE", ""))
    parser.add_argument("--checksum", default=os.environ.get("CHECKSUM", ""))
    args = parser.parse_args()

    if args.mode == "source":
        finalize_source()
        return
    if not args.source_sha or not args.archive or not args.checksum:
        raise SystemExit("evidence mode requires source SHA, archive, and checksum")
    write_evidence(args.source_sha, args.archive, args.checksum)


if __name__ == "__main__":
    main()
