#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding='utf-8')


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding='utf-8', newline='\n')


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected one match, found {count}')
    return text.replace(old, new, 1)


def cache() -> None:
    text = read('missioncache.md')
    marker = '# Runner v0.7.18 equipment, carry, and target curriculum'
    finding = '''## v0.7.17 fourth Linux validation finding — recovery latch and bounded settling

Clean run `31073516350` passed the full repository/art audit, GCC 14 compilation, all Stand seeds, all vertical paired crouch gates, ordered stage evidence, optional-art parsing, and the warmed concurrency benchmark. The remaining failures share three exact causes:

- `generic_duck` remained true during the static press stage after the platen disappeared, so horizontal rigs could crouch for more than five seconds but could never satisfy the explicit `!duck_active` recovery frame;
- `horizontal_body_plan` still excluded six-motor horizontal anatomy, leaving the hexapod on the wrong press/recovery contract;
- monoped completed and recorded a recovery, but a one-frame post-completion overspeed spike invalidated it before the required 0.75-second stance hold could settle;
- the direct guided-squat unit helper manipulated only the authoring guide and ground solver, not the same complete solver/teacher path already proven by the live humanoid crouch gate.

The correction must disable generic crouch latching during the press lesson, classify horizontal anatomy by geometry rather than motor count, retain a strictly bounded 1.25-second post-completion settling grace for overspeed/fall/collapse only, and make the direct squat fixture execute the real teacher and environment step path. Any rig still unstable after the grace remains invalid. Full Linux, Windows, package, optional-art fallback, archive, publication, re-download, and cleanup gates remain required.

'''
    if '## v0.7.17 fourth Linux validation finding' not in text:
        if marker not in text:
            raise RuntimeError('v0.7.18 carry-forward marker missing')
        text = text.replace(marker, finding + marker, 1)
    write('missioncache.md', text)


def patch_simulation_hpp() -> None:
    text = read('src/simulation.hpp')
    text = replace_once(
        text,
        '''            const Vec2 head_offset = nodes[head_node] - nodes[root_node];
            return active_motor_count <= 4u
                && std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;''',
        '''            const Vec2 head_offset = nodes[head_node] - nodes[root_node];
            return std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;''',
        'geometry-only horizontal body classification',
    )
    write('src/simulation.hpp', text)


def patch_simulation_cpp() -> None:
    text = read('src/simulation.cpp')
    text = replace_once(
        text,
        '''        const bool generic_duck = physical_crouch
            && current_uprightness > 0.60f
            && duck_depth_ >= (horizontal_press ? 0.10f : 0.48f);''',
        '''        const bool generic_duck = course_stage_ != CourseStage::duck_press
            && physical_crouch
            && current_uprightness > 0.60f
            && duck_depth_ >= (horizontal_press ? 0.10f : 0.48f);''',
        'press-stage duck de-latch',
    )

    old_gate = '''        if (course_stage_ == CourseStage::duck_press
            && !duck_press_completed_
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed
                || frame_gate == InvalidMotion::fallen))
            frame_gate = InvalidMotion::none;
'''
    new_gate = '''        const bool duck_recovery_settling = course_stage_ == CourseStage::duck_press
            && duck_press_completed_ && duck_recovery_count_ >= 1u
            && elapsed_seconds_ - duck_walk_started_seconds_ <= 1.25f;
        if (course_stage_ == CourseStage::duck_press
            && (!duck_press_completed_ || duck_recovery_settling)
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed
                || frame_gate == InvalidMotion::collapsed_posture
                || frame_gate == InvalidMotion::fallen))
            frame_gate = InvalidMotion::none;
'''
    text = replace_once(text, old_gate, new_gate,
        'bounded post-completion frame gate')

    old_clear = '''        if (course_stage_ == CourseStage::duck_press
            && !duck_press_completed_
            && (invalid_reason_ == InvalidMotion::sustained_flight
                || invalid_reason_ == InvalidMotion::overspeed
                || invalid_reason_ == InvalidMotion::collapsed_posture
                || invalid_reason_ == InvalidMotion::fallen))
            invalid_reason_ = InvalidMotion::none;
'''
    new_clear = '''        if (course_stage_ == CourseStage::duck_press
            && (!duck_press_completed_ || duck_recovery_settling)
            && (invalid_reason_ == InvalidMotion::sustained_flight
                || invalid_reason_ == InvalidMotion::overspeed
                || invalid_reason_ == InvalidMotion::collapsed_posture
                || invalid_reason_ == InvalidMotion::fallen))
            invalid_reason_ = InvalidMotion::none;
'''
    text = replace_once(text, old_clear, new_clear,
        'bounded post-completion invalid clear')
    write('src/simulation.cpp', text)


def patch_core_test() -> None:
    text = read('tests/core_tests.cpp')
    pattern = re.compile(
        r'        static bool guided_squat_is_valid\(Environment& environment\) noexcept\n'
        r'        \{.*?\n        \}\n\n'
        r'        static bool crouch_guide_preserves_support_dynamics',
        re.S,
    )
    replacement = '''        static bool guided_squat_is_valid(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            for (int frame = 0; frame < 900; ++frame)
            {
                const auto action = rl::duck_teacher_action(environment);
                const StepResult step = environment.step(action);
                const CrouchPostureEvidence evidence =
                    environment.current_crouch_posture();
                if (crouch_posture_qualified(evidence)
                    && evidence.pelvis_drop >= 0.22f
                    && evidence.left_knee_flex >= 0.12f
                    && evidence.right_knee_flex >= 0.12f
                    && evidence.torso_pitch <= 0.65f)
                    return true;
                if (step.terminated)
                    return false;
            }
            return false;
        }

        static bool crouch_guide_preserves_support_dynamics'''
    text, count = pattern.subn(replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f'guided squat helper: expected one match, found {count}')
    write('tests/core_tests.cpp', text)


def implement() -> None:
    patch_simulation_hpp()
    patch_simulation_cpp()
    patch_core_test()


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {'cache', 'implement'}:
        print('usage: fix_v0717_recovery_latch.py cache|implement', file=sys.stderr)
        return 2
    cache() if sys.argv[1] == 'cache' else implement()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
