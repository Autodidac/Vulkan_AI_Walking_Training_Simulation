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
    finding = '''## v0.7.17 eighth Linux validation finding — monoped predicate omitted support count

Clean run `31074945350` again passed all runtime-facing Linux gates and failed only the quadruped topology unit. Source inspection found the exact cause: `monopedal_gait()` recognized a shared motor-parent/pivot pattern without requiring monoped support topology. The authored quadruped happens to share that motor pattern, so it was falsely classified as a monoped and excluded from `horizontal_multi_support_plan()` despite owning six semantic support contacts.

Monoped identity must require exactly two semantic support contacts in addition to the existing motor relationship. Quadruped, crawler, and hexapod then remain multi-support bodies; the true monoped retains its dedicated gait path. No accepted runtime threshold or behavior is weakened. Full Linux, Windows, package, optional-art fallback, archive, publication, re-download, and cleanup gates remain mandatory.

'''
    if "## v0.7.17 eighth Linux validation finding" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.18 carry-forward marker missing")
        text = text.replace(marker, finding + marker, 1)
    write("missioncache.md", text)


def implement() -> None:
    text = read("src/simulation.hpp")
    text = replace_once(
        text,
        '''        [[nodiscard]] bool monopedal_gait() const noexcept
        {
            return active_motor_count >= 4u
                && motors[2].enabled && motors[3].enabled
                && motors[2].a == motors[3].a
                && motors[2].pivot == motors[3].pivot;
        }''',
        '''        [[nodiscard]] bool monopedal_gait() const noexcept
        {
            return support_seed_count() == 2u
                && active_motor_count >= 4u
                && motors[2].enabled && motors[3].enabled
                && motors[2].a == motors[3].a
                && motors[2].pivot == motors[3].pivot;
        }''',
        "monoped semantic support predicate",
    )
    write("src/simulation.hpp", text)


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: fix_v0717_monoped_predicate.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
