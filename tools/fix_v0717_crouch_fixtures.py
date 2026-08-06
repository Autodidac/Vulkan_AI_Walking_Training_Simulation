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
    finding = '''## v0.7.17 ninth Linux validation finding — duplicated legacy crouch-walk thresholds

Clean run `31075191957` passed all production-facing Linux contracts and failed one later core assertion. `core_tests.cpp` already contained the corrected eight-cycle crouch-walk evidence test, but a duplicated historical section farther down still expected five cycles to complete crouch-walk and to enter elite self-imitation. Production `stage_skill_evidence` correctly rejects both five-cycle fixtures.

Both duplicated fixtures must use the current eight-cycle sustained crouch-walk threshold. No production code, acceptance threshold, or runtime behavior changes. Full Linux, Windows, package, optional-art fallback, archive, publication, re-download, and cleanup gates remain mandatory.

'''
    if "## v0.7.17 ninth Linux validation finding" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.18 carry-forward marker missing")
        text = text.replace(marker, finding + marker, 1)
    write("missioncache.md", text)


def implement() -> None:
    text = read("tests/core_tests.cpp")
    old_stage = '''    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk, 5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "foot-only crouch walk and obstacle evidence cannot complete the duck lesson");'''
    new_stage = '''    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk, 8u, 3.0f, 0u, 0.0f, 0u, 4u),
        "foot-only sustained crouch walk and obstacle evidence cannot complete the duck lesson");'''
    if text.count(old_stage) != 1:
        raise RuntimeError("legacy crouch-walk stage fixture not found exactly once")
    text = text.replace(old_stage, new_stage, 1)

    old_elite = '''    require(rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 5, 1.2f, 12.0f,
            3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk result cannot seed self-imitation");'''
    new_elite = '''    require(rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 8, 1.2f, 12.0f,
            3.0f, 0u, 0.0f, 0u, 4u),
        "valid sustained foot-only crouch-walk result cannot seed self-imitation");'''
    if text.count(old_elite) != 1:
        raise RuntimeError("legacy crouch-walk elite fixture not found exactly once")
    text = text.replace(old_elite, new_elite, 1)
    write("tests/core_tests.cpp", text)


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: fix_v0717_crouch_fixtures.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
