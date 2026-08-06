#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def cache() -> None:
    text = read("missioncache.md")
    marker = "# Runner v0.7.18 equipment, carry, and target curriculum"
    finding = '''## v0.7.17 tenth Linux validation finding — stale uneven elite fixture

Clean run `31075475969` passed repository and optional-art audits, GCC 14 compilation, the focused v0.7.17 eye-test suite, all eight six-seed Stand cases, all eight four-seed static crouch/hold/recover cases, the complete 24/24 live acceptance matrix, deformable terrain, SandHybrid integration, runtime pipeline, and the warmed concurrency benchmark. The only remaining failure was a historical self-imitation unit fixture that still supplied four uneven-stage gait cycles after production qualification was raised to ten sustained cycles.

The fixture must supply ten cycles so it tests a genuinely qualified stepped result. No production behavior or threshold changes. The obsolete `agent/v0717-eye-test-batch` branch contains only a superseded cache script/workflow and no unique product source; it must be deleted after the authoritative branch is merged and the release assets are re-downloaded. Full Linux, Windows, package, optional-art fallback, archive, publication, re-download, and branch cleanup gates remain mandatory.

'''
    if "## v0.7.17 tenth Linux validation finding" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.18 carry-forward marker missing")
        text = text.replace(marker, finding + marker, 1)
    write("missioncache.md", text)


def implement() -> None:
    text = read("tests/core_tests.cpp")
    old = '''    require(rl::elite_motion_eligible(sim::CourseStage::uneven, true, 4, 1.2f, 4.0f),
        "valid stepped best result cannot seed self-imitation");'''
    new = '''    require(rl::elite_motion_eligible(sim::CourseStage::uneven, true, 10, 1.2f, 4.0f),
        "valid sustained stepped best result cannot seed self-imitation");'''
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"legacy uneven elite fixture: expected one match, found {count}")
    write("tests/core_tests.cpp", text.replace(old, new, 1))


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: fix_v0717_elite_fixture.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
