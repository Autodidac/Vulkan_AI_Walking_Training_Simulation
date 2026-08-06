#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / 'tools' / 'fix_v0717_crouch_recovery.py'


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected one match, found {count}')
    return text.replace(old, new, 1)


def main() -> int:
    text = HELPER.read_text(encoding='utf-8')
    start = text.index('def patch_tests() -> None:\n')
    end = text.index('\n\ndef implement() -> None:\n', start)
    replacement = '''def patch_tests() -> None:
    core = read('tests/core_tests.cpp')
    core = replace_once(
        core,
        ''' + '"""' + '''    require(!sim::CreatureBlueprint::chicken().horizontal_body_plan()
            && sim::CreatureBlueprint::quadruped().horizontal_body_plan()
            && sim::CreatureBlueprint::crawler4().horizontal_body_plan(),
        "horizontal press classification confuses bipeds with multi-support bodies");''' + '"""' + ''',
        ''' + '"""' + '''    require(sim::CreatureBlueprint::chicken().horizontal_body_plan()
            && !sim::CreatureBlueprint::chicken().horizontal_multi_support_plan()
            && sim::CreatureBlueprint::quadruped().horizontal_multi_support_plan()
            && sim::CreatureBlueprint::crawler4().horizontal_multi_support_plan(),
        "horizontal balance and multi-support press topology are not separated");''' + '"""' + ''',
        'core topology classification regression test',
    )
    core = replace_once(
        core,
        ''' + '"""' + '''                && rig.radii[rig.left_contact_node] <= 0.1121f
                && rig.radii[rig.right_contact_node] <= 0.1121f,
            "paired rig does not use one compact physical support stub per leg");''' + '"""' + ''',
        ''' + '"""' + '''                && rig.radii[rig.left_contact_node] >= 0.104f
                && rig.radii[rig.right_contact_node] >= 0.104f
                && rig.radii[rig.left_contact_node] <= 0.1121f
                && rig.radii[rig.right_contact_node] <= 0.1121f,
            "paired rig does not use one compact loaded support stub per leg");''' + '"""' + ''',
        'core loaded stub radius test',
    )
    write('tests/core_tests.cpp', core)

    focused = read('tests/v0717_eye_test_tests.cpp')
    focused = replace_once(
        focused,
        ''' + '"""' + '''    require(biped.is_support_seed(biped.left_contact_node)
            && biped.is_support_seed(biped.right_contact_node),
        "stub supports are not semantic contacts");''' + '"""' + ''',
        ''' + '"""' + '''    require(biped.is_support_seed(biped.left_contact_node)
            && biped.is_support_seed(biped.right_contact_node),
        "stub supports are not semantic contacts");
    require(biped.radii[biped.left_contact_node] >= 0.104f
            && biped.radii[biped.right_contact_node] >= 0.104f,
        "stub supports are too small to carry the authored stance");''' + '"""' + ''',
        'focused loaded stub radius test',
    )
    write('tests/v0717_eye_test_tests.cpp', focused)
'''
    text = text[:start] + replacement + text[end:]
    HELPER.write_text(text, encoding='utf-8', newline='\n')
    subprocess.run([sys.executable, str(HELPER), 'implement'],
        cwd=ROOT, check=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
