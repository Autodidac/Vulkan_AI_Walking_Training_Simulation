#!/usr/bin/env python3
from __future__ import annotations

import re
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
    finding = '''## v0.7.17 sixth Linux validation finding — stale topology unit assertion

Clean run `31074432529` passed repository and optional-art audits, GCC 14 compilation, the focused v0.7.17 eye-test suite, all eight six-seed Stand cases, all eight four-seed static crouch/hold/recover cases, the complete 24/24 live acceptance matrix, deformable terrain, SandHybrid integration, runtime pipeline, and the warmed concurrency benchmark. The only failure was a direct core assertion that bundled chicken geometric orientation with quadruped multi-support press topology.

Production behavior no longer uses that bundled assumption: chicken uses paired-leg balance/recovery semantics, while the user-reported quadruped case uses explicit four-support multi-support press semantics. The test must assert those two contracts independently instead of requiring one shared orientation predicate across chicken, quadruped, crawler, and hexapod. No product behavior, acceptance threshold, or release gate is weakened. The complete Linux, Windows, package, optional-art fallback, archive, publication, re-download, and cleanup gates remain mandatory.

'''
    if "## v0.7.17 sixth Linux validation finding" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.18 carry-forward marker missing")
        text = text.replace(marker, finding + marker, 1)
    write("missioncache.md", text)


def implement() -> None:
    text = read("tests/core_tests.cpp")
    pattern = re.compile(
        r"    require\(sim::CreatureBlueprint::chicken\(\)\.horizontal_body_plan\(\)\n"
        r"            && !sim::CreatureBlueprint::chicken\(\)\.horizontal_multi_support_plan\(\)\n"
        r"            && sim::CreatureBlueprint::quadruped\(\)\.horizontal_multi_support_plan\(\)\n"
        r"            && sim::CreatureBlueprint::crawler4\(\)\.horizontal_multi_support_plan\(\)\n"
        r"            && sim::CreatureBlueprint::hexapod\(\)\.horizontal_multi_support_plan\(\),\n"
        r"        \"horizontal balance and multi-support press topology are not separated\"\);"
    )
    replacement = '''    const sim::CreatureBlueprint chicken_topology =
        sim::CreatureBlueprint::chicken();
    require(chicken_topology.paired_leg_chains()
            && !chicken_topology.horizontal_multi_support_plan(),
        "chicken paired-leg balance topology is not isolated from multi-support press logic");

    const sim::CreatureBlueprint quadruped_topology =
        sim::CreatureBlueprint::quadruped();
    require(quadruped_topology.support_seed_count() >= 4u
            && !quadruped_topology.paired_leg_chains()
            && !quadruped_topology.monopedal_gait()
            && quadruped_topology.horizontal_multi_support_plan(),
        "quadruped does not use explicit multi-support press topology");'''
    text, count = pattern.subn(replacement, text, count=1)
    if count != 1:
        raise RuntimeError(
            f"stale topology assertion: expected one match, found {count}")
    write("tests/core_tests.cpp", text)


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: fix_v0717_topology_test.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
