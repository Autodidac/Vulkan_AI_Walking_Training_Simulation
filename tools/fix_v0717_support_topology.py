#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def cache() -> None:
    text = read("missioncache.md")
    marker = "# Runner v0.7.18 equipment, carry, and target curriculum"
    finding = '''## v0.7.17 seventh Linux validation finding — semantic support topology

Clean run `31074718547` again passed repository and optional-art audits, GCC 14 compilation, the focused v0.7.17 eye-test suite, all eight six-seed Stand cases, all eight four-seed static crouch/hold/recover cases, the complete 24/24 live acceptance matrix, deformable terrain, SandHybrid integration, runtime pipeline, and the warmed concurrency benchmark. The only failing assertion showed that `horizontal_multi_support_plan()` still depended on the old `paired_leg_chains()` heuristic. The authored quadruped has four semantic support contacts but that heuristic also reports paired chains, so the helper rejected the exact user-reported multi-support body.

Press topology must be determined from semantic support count, not motor-pair grouping: any non-monoped rig with at least four semantic supports uses the multi-support press/recovery contract. Chicken and ordinary bipeds retain two-support paired behavior. The test and production helper must express the same semantic rule. Full Linux, Windows, package, optional-art fallback, archive, publication, re-download, and cleanup gates remain mandatory.

'''
    if "## v0.7.17 seventh Linux validation finding" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.18 carry-forward marker missing")
        text = text.replace(marker, finding + marker, 1)
    write("missioncache.md", text)


def implement() -> None:
    header = read("src/simulation.hpp")
    header = replace_once(
        header,
        '''        [[nodiscard]] bool horizontal_multi_support_plan() const noexcept
        {
            return !paired_leg_chains() && !monopedal_gait()
                && support_seed_count() >= 4u;
        }''',
        '''        [[nodiscard]] bool horizontal_multi_support_plan() const noexcept
        {
            return !monopedal_gait() && support_seed_count() >= 4u;
        }''',
        "semantic multi-support helper",
    )
    write("src/simulation.hpp", header)

    tests = read("tests/core_tests.cpp")
    tests = replace_once(
        tests,
        '''    require(quadruped_topology.support_seed_count() >= 4u
            && !quadruped_topology.paired_leg_chains()
            && !quadruped_topology.monopedal_gait()
            && quadruped_topology.horizontal_multi_support_plan(),
        "quadruped does not use explicit multi-support press topology");''',
        '''    require(quadruped_topology.support_seed_count() >= 4u
            && !quadruped_topology.monopedal_gait()
            && quadruped_topology.horizontal_multi_support_plan(),
        "quadruped semantic supports do not select multi-support press topology");''',
        "quadruped topology fixture",
    )
    write("tests/core_tests.cpp", tests)


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: fix_v0717_support_topology.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
