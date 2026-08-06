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
    finding = '''## v0.7.17 second Linux validation finding — recovery and topology separation

Clean-source run `31072665828` proved the repository, supplied-art hashes, GCC 14 build, camera tests, terrain tests, runtime pipeline, and warmed speed-mode benchmark, but exposed four remaining release blockers:

- excluding the chicken from horizontal-body handling restored press classification but regressed its verified six-seed Stand behavior;
- vertical paired rigs physically crouched for the complete press cycle but did not establish a clean un-crouched recovery before the next cycle, so scaffold, biped, humanoid, and monoped remained incomplete;
- quadruped and crawler compression still entered the biped hip-hinge invalidation path before their multi-support body drop reached the required evidence;
- the live ordered-stage fixture still used the old four-step and five-step thresholds after sustained gait requirements were raised.

The correction must separate horizontal balance posture from horizontal multi-support press topology, retain a compact but sufficiently loaded chicken support radius, use actual multi-support body/head compression as crouch evidence, avoid terminating a finite supported horizontal rig merely for failing a biped-shaped crouch test, require a continuous un-crouched post-retraction recovery hold, restore bounded vertical recovery authority, and update the deterministic stage fixture to the current gait thresholds. Full Linux, Windows, package, optional-art fallback, archive, and publication validation remains mandatory.

'''
    if '## v0.7.17 second Linux validation finding' not in text:
        if marker not in text:
            raise RuntimeError('v0.7.18 marker missing')
        text = text.replace(marker, finding + marker, 1)
    write('missioncache.md', text)


def patch_simulation_hpp() -> None:
    text = read('src/simulation.hpp')
    text = replace_once(
        text,
        '''        [[nodiscard]] bool horizontal_body_plan() const noexcept
        {
            if (root_node >= nodes.size() || head_node >= nodes.size())
                return false;
            const Vec2 head_offset = nodes[head_node] - nodes[root_node];
            return active_motor_count <= 4u
                && support_seed_count() >= 4u
                && std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;
        }
''',
        '''        [[nodiscard]] bool horizontal_body_plan() const noexcept
        {
            if (root_node >= nodes.size() || head_node >= nodes.size())
                return false;
            const Vec2 head_offset = nodes[head_node] - nodes[root_node];
            return active_motor_count <= 4u
                && std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;
        }
        [[nodiscard]] bool horizontal_multi_support_plan() const noexcept
        {
            return horizontal_body_plan() && !paired_leg_chains()
                && support_seed_count() >= 4u;
        }
''',
        'horizontal topology separation',
    )
    write('src/simulation.hpp', text)


def patch_simulation_cpp() -> None:
    text = read('src/simulation.cpp')
    text = replace_once(
        text,
        '''                rig.radii[contact] = clamp(
                    rig.radii[contact] * 0.62f, 0.090f, 0.112f);''',
        '''                rig.radii[contact] = clamp(
                    rig.radii[contact] * 0.62f, 0.104f, 0.112f);''',
        'loaded support-stub radius',
    )
    count = text.count('blueprint_.horizontal_body_plan());')
    if count != 2:
        raise RuntimeError(f'press profile topology calls: expected 2, found {count}')
    text = text.replace('blueprint_.horizontal_body_plan());',
        'blueprint_.horizontal_multi_support_plan());')

    text = replace_once(
        text,
        '        const bool horizontal_body = blueprint_.horizontal_body_plan();\n'
        '        const float minimum_vertical_scale = horizontal_body ? 0.84f : 0.52f;',
        '        const bool horizontal_body = blueprint_.horizontal_multi_support_plan();\n'
        '        const float minimum_vertical_scale = horizontal_body ? 0.82f : 0.58f;',
        'multi-support crouch scale',
    )
    text = replace_once(
        text,
        '            const float maximum_step = horizontal_body ? 0.042f : 0.60f;',
        '            const float maximum_step = horizontal_body ? 0.080f : 0.055f;',
        'crouch guide step bounds',
    )
    text = replace_once(
        text,
        '''            const float guide_strength = recovery_guide
                ? (horizontal_body ? 0.72f : 1.0f)
                : (horizontal_body ? 0.38f : 1.0f) * phase_strength;''',
        '''            const float guide_strength = recovery_guide
                ? (horizontal_body ? 0.82f : 0.48f)
                : (horizontal_body ? 0.55f : 0.32f) * phase_strength;''',
        'crouch guide strengths',
    )
    text = replace_once(
        text,
        '            const float guide_strength = 0.28f + phase_strength * 0.36f;',
        '            const float guide_strength = recovery_guide\n'
        '                ? 0.88f : 0.28f + phase_strength * 0.36f;',
        'paired recovery strength',
    )
    text = replace_once(
        text,
        '                constexpr float maximum_step = 0.22f;\n'
        '                if (magnitude > maximum_step && magnitude > 1.0e-6f)',
        '                const float maximum_step = recovery_guide ? 0.32f : 0.22f;\n'
        '                if (magnitude > maximum_step && magnitude > 1.0e-6f)',
        'paired recovery step',
    )
    text = replace_once(
        text,
        '        evidence.horizontal_body = blueprint_.horizontal_body_plan();',
        '        evidence.horizontal_body = blueprint_.horizontal_multi_support_plan();',
        'crouch evidence topology',
    )

    old_crouch = '''        const CrouchPostureEvidence crouch_posture = current_crouch_posture();
        const bool physical_crouch = crouch_posture_qualified(crouch_posture);
        const bool generic_duck = physical_crouch
            && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        const bool press_duck = course_stage_ == CourseStage::duck_press
            && physical_crouch
            && duck_obstacle_weight_ >= 0.64f
            && duck_clearance_margin_ >= -0.10f
            && current_uprightness > 0.45f;
        duck_active_ = generic_duck || press_duck;

        const bool crouch_challenge = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            && duck_obstacle_weight_ >= 0.72f
            && duck_depth_ >= 0.30f
            && feet_supported && !non_foot_grounded_;
'''
    new_crouch = '''        const CrouchPostureEvidence crouch_posture = current_crouch_posture();
        const bool horizontal_press = blueprint_.horizontal_multi_support_plan();
        const bool horizontal_compression = horizontal_press
            && feet_supported && !non_foot_grounded_
            && current_uprightness > 0.52f
            && duck_depth_ >= 0.08f
            && crouch_posture.pelvis_drop >= 0.08f
            && crouch_posture.support_margin >= -0.24f;
        const bool physical_crouch = crouch_posture_qualified(crouch_posture)
            || horizontal_compression;
        const bool generic_duck = physical_crouch
            && current_uprightness > 0.60f
            && duck_depth_ >= (horizontal_press ? 0.10f : 0.48f);
        const bool press_duck = course_stage_ == CourseStage::duck_press
            && physical_crouch
            && duck_obstacle_weight_ >= (horizontal_press ? 0.48f : 0.64f)
            && duck_clearance_margin_ >= -0.10f
            && current_uprightness > 0.45f;
        duck_active_ = generic_duck || press_duck;

        const bool crouch_challenge = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            && duck_obstacle_weight_ >= (horizontal_press ? 0.48f : 0.72f)
            && duck_depth_ >= (horizontal_press ? 0.08f : 0.30f)
            && feet_supported && !non_foot_grounded_;
'''
    text = replace_once(text, old_crouch, new_crouch,
        'multi-support physical crouch evidence')

    text = replace_once(
        text,
        '''        const float duck_posture_failure_limit = blueprint_.horizontal_body_plan()
            ? 2.75f : 1.10f;
        if (duck_posture_failure_seconds_ > duck_posture_failure_limit)
            invalidate(InvalidMotion::duck_hip_hinge);''',
        '''        const bool horizontal_crouch_plan =
            blueprint_.horizontal_multi_support_plan();
        if (!horizontal_crouch_plan && duck_posture_failure_seconds_ > 1.10f)
            invalidate(InvalidMotion::duck_hip_hinge);''',
        'horizontal nonterminal crouch learning',
    )
    text = replace_once(
        text,
        '''            const float contact_limit = blueprint_.horizontal_body_plan()
                ? 0.65f : 0.35f;''',
        '''            const float contact_limit = blueprint_.horizontal_multi_support_plan()
                ? 0.90f : 0.35f;''',
        'horizontal contact grace',
    )
    text = replace_once(
        text,
        '            const bool horizontal_press = blueprint_.horizontal_body_plan();',
        '            const bool horizontal_press =\n'
        '                blueprint_.horizontal_multi_support_plan();',
        'press challenge topology',
    )
    text = replace_once(
        text,
        '            const bool horizontal_recovery = blueprint_.horizontal_body_plan();',
        '            const bool horizontal_recovery =\n'
        '                blueprint_.horizontal_multi_support_plan();',
        'press recovery topology',
    )

    old_recovery = '''            if (duck_press_hold_qualified_ && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f
                && feet_supported && !non_foot_grounded_
                && body_integrity_valid()
                && current_uprightness >= recovery_uprightness
                && head_height_ratio >= recovery_head_ratio
                && std::abs(torso_angle) <= 0.40f
                && stance_slip_speed_ <= 0.16f
                && stable_stance_seconds_ >= recovery_hold
                && !duck_press_completed_)
            {
                duck_press_completed_ = true;
                duck_walk_started_seconds_ = elapsed_seconds_;
                progress_window_start_x_ = root_x;
                progress_window_start_steps_ = alternating_steps_;
                ++duck_recovery_count_;
                ++obstacles_passed_;
                passed_obstacle_this_step_ = true;
            }
'''
    new_recovery = '''            const bool recovered_frame = duck_press_hold_qualified_
                && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f
                && feet_supported && !non_foot_grounded_
                && body_integrity_valid() && !duck_active_
                && current_uprightness >= recovery_uprightness
                && head_height_ratio >= recovery_head_ratio
                && std::abs(torso_angle) <= 0.40f
                && stance_slip_speed_ <= 0.16f;
            current_duck_hold_seconds_ = recovered_frame
                ? current_duck_hold_seconds_ + dt
                : std::max(0.0f, current_duck_hold_seconds_ - dt * 2.0f);
            if (current_duck_hold_seconds_ >= recovery_hold
                && !duck_press_completed_)
            {
                duck_press_completed_ = true;
                duck_walk_started_seconds_ = elapsed_seconds_;
                progress_window_start_x_ = root_x;
                progress_window_start_steps_ = alternating_steps_;
                ++duck_recovery_count_;
                ++obstacles_passed_;
                passed_obstacle_this_step_ = true;
            }
'''
    text = replace_once(text, old_recovery, new_recovery,
        'continuous un-crouched recovery evidence')
    write('src/simulation.cpp', text)


def patch_acceptance() -> None:
    text = read('src/acceptance.cpp')
    text = replace_once(
        text,
        '''            && sim::stage_skill_evidence(sim::CourseStage::uneven,
                4u, 0.0f, 0u, 0.0f, 0u, 0u)
            && sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
                5u, 3.0f, 0u, 0.0f, 0u, 4u);''',
        '''            && sim::stage_skill_evidence(sim::CourseStage::uneven,
                10u, 0.0f, 0u, 0.0f, 0u, 0u)
            && sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
                8u, 3.0f, 0u, 0.0f, 0u, 4u);''',
        'ordered stage evidence thresholds',
    )
    write('src/acceptance.cpp', text)


def patch_tests() -> None:
    text = read('tests/v0717_eye_test_tests.cpp')
    text = replace_once(
        text,
        '''    require(!sim::CreatureBlueprint::chicken().horizontal_body_plan()
            && sim::CreatureBlueprint::quadruped().horizontal_body_plan()
            && sim::CreatureBlueprint::crawler4().horizontal_body_plan(),
        "horizontal press classification confuses bipeds with multi-support bodies");''',
        '''    require(sim::CreatureBlueprint::chicken().horizontal_body_plan()
            && !sim::CreatureBlueprint::chicken().horizontal_multi_support_plan()
            && sim::CreatureBlueprint::quadruped().horizontal_multi_support_plan()
            && sim::CreatureBlueprint::crawler4().horizontal_multi_support_plan(),
        "horizontal balance and multi-support press topology are not separated");''',
        'topology classification regression test',
    )
    text = replace_once(
        text,
        '        require(rig.radii[rig.left_contact_node] <= 0.12f\n'
        '                && rig.radii[rig.right_contact_node] <= 0.12f,',
        '        require(rig.radii[rig.left_contact_node] >= 0.104f\n'
        '                && rig.radii[rig.right_contact_node] >= 0.104f\n'
        '                && rig.radii[rig.left_contact_node] <= 0.12f\n'
        '                && rig.radii[rig.right_contact_node] <= 0.12f,',
        'loaded stub radius test',
    )
    write('tests/v0717_eye_test_tests.cpp', text)


def implement() -> None:
    patch_simulation_hpp()
    patch_simulation_cpp()
    patch_acceptance()
    patch_tests()


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {'cache', 'implement'}:
        print('usage: fix_v0717_crouch_recovery.py cache|implement', file=sys.stderr)
        return 2
    if sys.argv[1] == 'cache':
        cache()
    else:
        implement()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
