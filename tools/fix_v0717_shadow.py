#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def cache() -> None:
    path = ROOT / 'missioncache.md'
    text = path.read_text(encoding='utf-8')
    marker = '# Runner v0.7.18 equipment, carry, and target curriculum'
    finding = '''## v0.7.17 third Linux validation finding — warnings-as-errors shadow

Clean run `31073332261` passed repository, mission, optional-art, semantics, and CMake audits, then GCC 14 stopped compilation because `update_gait_metrics` redeclared `horizontal_press` inside the press-evidence block after the same topology decision had already been computed earlier in the function. The correction is source-neutral: remove the redundant inner declaration and reuse the existing value. The complete runtime and package gates still apply.

'''
    if '## v0.7.17 third Linux validation finding' not in text:
        if marker not in text:
            raise RuntimeError('v0.7.18 marker missing')
        text = text.replace(marker, finding + marker, 1)
    path.write_text(text, encoding='utf-8', newline='\n')


def implement() -> None:
    path = ROOT / 'src' / 'simulation.cpp'
    text = path.read_text(encoding='utf-8')
    old = '''        if (course_stage_ == CourseStage::duck_press)
        {
            const bool horizontal_press =
                blueprint_.horizontal_multi_support_plan();
            const bool press_challenge_reached = duck_press_contact_this_step_
'''
    new = '''        if (course_stage_ == CourseStage::duck_press)
        {
            const bool press_challenge_reached = duck_press_contact_this_step_
'''
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'horizontal press shadow: expected one match, found {count}')
    path.write_text(text.replace(old, new, 1), encoding='utf-8', newline='\n')


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {'cache', 'implement'}:
        return 2
    cache() if sys.argv[1] == 'cache' else implement()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
