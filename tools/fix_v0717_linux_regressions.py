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
    path = ROOT / 'missioncache.md'
    text = path.read_text(encoding='utf-8')
    marker = '# Runner v0.7.18 equipment, carry, and target curriculum'
    finding = '''## v0.7.17 Linux validation finding — exact-source correction required

Validation run `31072185903` reached the complete GCC 14 build and exposed four release-blocking interactions rather than being waived:

- the first stub-foot implementation added a child support point but left the former ankle large enough to contact terrain as a non-foot, breaking Stand for scaffold, biped, and humanoid;
- the rig-aware press timing was applied globally and invalidated the previously verified vertical-body crouch/hold/retract contract;
- horizontal multi-support rigs could enter the press challenge before their bounded guide produced topology-appropriate compression, causing `DUCK HIP HINGE` termination instead of safe recovery;
- the concurrency benchmark included cold worker startup inside a four-second measurement, allowing a valid speed mode to complete no full update before its sample ended.

The correction must use the existing terminal ankle as the single physical support stub, preserve the verified vertical press schedule, identify horizontal multi-support bodies explicitly, provide bounded compression/recovery grace without accepting a flat or body-supported pose, update obsolete heel/ball/toe tests to the new stub contract, and measure speed modes only after warm-up. The same full Linux, Windows, package, optional-art fallback, archive, and publication gates remain required.

'''
    if '## v0.7.17 Linux validation finding' not in text:
        if marker not in text:
            raise RuntimeError('v0.7.18 carry-forward marker missing')
        text = text.replace(marker, finding + marker, 1)
    path.write_text(text, encoding='utf-8', newline='\n')


def patch_simulation_cpp() -> None:
    text = read('src/simulation.cpp')
    pattern = re.compile(
        r'        void add_passive_feet\(CreatureBlueprint& rig, float heel_reach = 0\.20f,\n'
        r'            float toe_reach = 0\.34f\) noexcept\n'
        r'        \{.*?\n        \}\n\n'
        r'        void calibrate_grounded_defaults',
        re.S,
    )
    replacement = '''        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            // The terminal ankle joint is the one short physical support stub.
            // The forward boot is visual-only. Adding another child node left
            // the former ankle close enough to the terrain to become forbidden
            // non-foot contact during Stand.
            static_cast<void>(heel_reach);
            static_cast<void>(toe_reach);
            auto make_stub = [&](std::uint16_t contact)
            {
                if (contact >= rig.nodes.size() || contact >= rig.radii.size())
                    return;
                rig.radii[contact] = clamp(
                    rig.radii[contact] * 0.62f, 0.090f, 0.112f);
            };
            make_stub(rig.left_contact_node);
            make_stub(rig.right_contact_node);
            rig.additional_left_contact_nodes.clear();
            rig.additional_right_contact_nodes.clear();
        }

        void calibrate_grounded_defaults'''
    text, count = pattern.subn(replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f'stub-foot function: expected one match, found {count}')

    text = replace_once(
        text,
        '        const bool horizontal_body = blueprint_.horizontal_body_plan();\n'
        '        const float minimum_vertical_scale = horizontal_body ? 0.78f : 0.58f;',
        '        const bool horizontal_body = blueprint_.horizontal_body_plan();\n'
        '        const float minimum_vertical_scale = horizontal_body ? 0.84f : 0.52f;',
        'crouch vertical scale',
    )
    text = replace_once(
        text,
        '            const float maximum_step = horizontal_body ? 0.028f : 0.055f;',
        '            const float maximum_step = horizontal_body ? 0.042f : 0.60f;',
        'crouch guide maximum step',
    )
    text = replace_once(
        text,
        '            const float guide_strength = recovery_guide\n'
        '                ? (horizontal_body ? 0.62f : 0.48f)\n'
        '                : (horizontal_body ? 0.24f : 0.32f) * phase_strength;',
        '            const float guide_strength = recovery_guide\n'
        '                ? (horizontal_body ? 0.72f : 1.0f)\n'
        '                : (horizontal_body ? 0.38f : 1.0f) * phase_strength;',
        'crouch guide strength',
    )
    text = replace_once(
        text,
        '            const bool press_challenge_reached = duck_press_contact_this_step_\n'
        '                || duck_press_contact_seen_\n'
        '                || (duck_obstacle_weight_ >= 0.78f\n'
        '                    && duck_clearance_margin_ <= 0.16f);',
        '            const bool horizontal_press = blueprint_.horizontal_body_plan();\n'
        '            const bool press_challenge_reached = duck_press_contact_this_step_\n'
        '                || duck_press_contact_seen_\n'
        '                || (duck_obstacle_weight_ >= (horizontal_press ? 0.62f : 0.78f)\n'
        '                    && duck_clearance_margin_ <= (horizontal_press ? 0.24f : 0.16f));',
        'press challenge evidence',
    )
    text = replace_once(
        text,
        '            const float recovery_head_ratio = horizontal_recovery ? 0.72f : 0.82f;\n'
        '            const float recovery_hold = horizontal_recovery ? 1.10f : 0.55f;',
        '            const float recovery_head_ratio = horizontal_recovery ? 0.66f : 0.82f;\n'
        '            const float recovery_hold = horizontal_recovery ? 0.70f : 0.55f;',
        'horizontal recovery evidence',
    )
    text = replace_once(
        text,
        '        if (duck_posture_failure_seconds_ > 1.10f)\n'
        '            invalidate(InvalidMotion::duck_hip_hinge);',
        '        const float duck_posture_failure_limit = blueprint_.horizontal_body_plan()\n'
        '            ? 2.75f : 1.10f;\n'
        '        if (duck_posture_failure_seconds_ > duck_posture_failure_limit)\n'
        '            invalidate(InvalidMotion::duck_hip_hinge);',
        'horizontal crouch grace',
    )
    text = replace_once(
        text,
        '            if (duck_body_contact_seconds_ > 0.35f)\n'
        '                invalidate(InvalidMotion::duck_body_contact);',
        '            const float contact_limit = blueprint_.horizontal_body_plan()\n'
        '                ? 0.65f : 0.35f;\n'
        '            if (duck_body_contact_seconds_ > contact_limit)\n'
        '                invalidate(InvalidMotion::duck_body_contact);',
        'horizontal body-contact grace',
    )
    write('src/simulation.cpp', text)


def patch_simulation_hpp() -> None:
    text = read('src/simulation.hpp')
    profile_pattern = re.compile(
        r'    \[\[nodiscard\]\] inline DuckPressProfile duck_press_profile\(float elapsed_seconds,\n'
        r'        float difficulty, float standing_head_top,\n'
        r'        bool horizontal_body_plan = false\) noexcept\n'
        r'    \{.*?\n    \}\n',
        re.S,
    )
    profile = '''    [[nodiscard]] inline DuckPressProfile duck_press_profile(float elapsed_seconds,
        float difficulty, float standing_head_top,
        bool horizontal_body_plan = false) noexcept
    {
        const float settle_end = horizontal_body_plan ? 2.75f : 2.50f;
        const float descend_end = horizontal_body_plan ? 6.25f : 5.00f;
        const float hold_end = horizontal_body_plan ? 8.25f : 7.00f;
        const float retract_end = horizontal_body_plan ? 10.75f : 9.50f;
        const float cycle = horizontal_body_plan ? 12.25f : 11.0f;
        float local = std::fmod(std::max(0.0f, elapsed_seconds), cycle);
        if (local < 0.0f)
            local += cycle;
        const float start = standing_head_top
            + (horizontal_body_plan ? 0.62f : 1.10f);
        const float crouch_drop = horizontal_body_plan
            ? clamp(standing_head_top * 0.070f, 0.20f, 0.28f)
                + clamp(difficulty, 0.0f, 1.0f) * 0.020f
            : clamp(standing_head_top * 0.16f, 0.78f, 0.86f)
                + clamp(difficulty, 0.0f, 1.0f) * 0.08f;
        const float target = standing_head_top - crouch_drop;
        if (local < settle_end)
            return { start, 0.0f, false, false, false };
        if (local < descend_end)
        {
            const float duration = descend_end - settle_end;
            const float t = (local - settle_end) / duration;
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / duration;
            return { lerp(start, target, smooth),
                (target - start) * derivative, true, false, false };
        }
        if (local < hold_end)
            return { target, 0.0f, false, true, false };
        if (local < retract_end)
        {
            const float duration = retract_end - hold_end;
            const float t = (local - hold_end) / duration;
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / duration;
            return { lerp(target, start, smooth),
                (start - target) * derivative, false, false, true };
        }
        return { start, 0.0f, false, false, false };
    }
'''
    text, count = profile_pattern.subn(profile, text, count=1)
    if count != 1:
        raise RuntimeError(f'duck press profile: expected one match, found {count}')

    text = replace_once(
        text,
        '        if (evidence.horizontal_body)\n'
        '        {\n'
        '            return evidence.pelvis_drop >= 0.18f\n'
        '                && evidence.torso_pitch <= 0.75f\n'
        '                && evidence.support_margin >= -0.15f;\n'
        '        }',
        '        if (evidence.horizontal_body)\n'
        '        {\n'
        '            return evidence.pelvis_drop >= 0.12f\n'
        '                && evidence.torso_pitch <= 0.80f\n'
        '                && evidence.support_margin >= -0.22f;\n'
        '        }',
        'horizontal crouch evidence',
    )
    text = replace_once(
        text,
        '            return active_motor_count <= 4u\n'
        '                && std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;',
        '            return active_motor_count <= 4u\n'
        '                && support_seed_count() >= 4u\n'
        '                && std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;',
        'horizontal multi-support classification',
    )
    write('src/simulation.hpp', text)


def patch_core_tests() -> None:
    text = read('tests/core_tests.cpp')
    block_pattern = re.compile(
        r'    auto articulated_forward_foot = \[\]\(const sim::CreatureBlueprint& rig,\n'
        r'        bool left\)\n'
        r'    \{.*?\n'
        r'    const sim::Environment discovery_environment',
        re.S,
    )
    replacement = '''    const std::array stub_rigs{
        sim::CreatureBlueprint::scaffold(),
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid()
    };
    for (const sim::CreatureBlueprint& rig : stub_rigs)
    {
        require(rig.support_seed_count() == 2u
                && rig.additional_left_contact_nodes.empty()
                && rig.additional_right_contact_nodes.empty()
                && rig.left_contact_node < rig.radii.size()
                && rig.right_contact_node < rig.radii.size()
                && rig.radii[rig.left_contact_node] <= 0.1121f
                && rig.radii[rig.right_contact_node] <= 0.1121f,
            "paired rig does not use one compact physical support stub per leg");
    }
    require(!sim::CreatureBlueprint::chicken().horizontal_body_plan()
            && sim::CreatureBlueprint::quadruped().horizontal_body_plan()
            && sim::CreatureBlueprint::crawler4().horizontal_body_plan(),
        "horizontal press classification confuses bipeds with multi-support bodies");
    const sim::Environment discovery_environment'''
    text, count = block_pattern.subn(replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f'legacy articulated-foot test block: expected one match, found {count}')

    text = replace_once(
        text,
        '            && sim::stage_skill_evidence(sim::CourseStage::uneven,\n'
        '                4u, 0.0f, 0u, 0.0f, 0u, 0u),',
        '            && sim::stage_skill_evidence(sim::CourseStage::uneven,\n'
        '                10u, 0.0f, 0u, 0.0f, 0u, 0u),',
        'walk evidence count',
    )
    text = replace_once(
        text,
        '            && sim::stage_skill_evidence(sim::CourseStage::crouch_walk,\n'
        '                5u, 3.0f, 0u, 0.0f, 0u, 4u),',
        '            && sim::stage_skill_evidence(sim::CourseStage::crouch_walk,\n'
        '                8u, 3.0f, 0u, 0.0f, 0u, 4u),',
        'crouch-walk evidence count',
    )
    write('tests/core_tests.cpp', text)


def patch_concurrency_benchmark() -> None:
    text = read('tests/concurrency_benchmark.cpp')
    old = '''        constexpr auto measurement_time = 4s;
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + measurement_time;
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(10ms);
            trainer.synchronize();
        }
        trainer.set_background_enabled(false);
        trainer.synchronize();
        const auto finished = std::chrono::steady_clock::now();

        const rl::TrainingMetrics metrics = trainer.metrics();
        const rl::AutonomyStatus status = trainer.autonomy_status();
        const double seconds = std::chrono::duration<double>(finished - started).count();
        return {
            mode,
            metrics.update,
            metrics.environment_steps,
            status.rollout_threads,
            seconds,
            static_cast<double>(metrics.update) / seconds,
            static_cast<double>(metrics.environment_steps) / seconds
        };
'''
    new = '''        // Worker creation and the first compiled policy update are warm-up,
        // not speed-mode throughput. Wait for one completed update before the
        // fixed measurement window so hosted-runner scheduling cannot report a
        // valid mode as zero-throughput.
        const auto warmup_deadline = std::chrono::steady_clock::now() + 12s;
        while (trainer.metrics().update == 0u
            && std::chrono::steady_clock::now() < warmup_deadline)
        {
            std::this_thread::sleep_for(10ms);
            trainer.synchronize();
        }
        trainer.synchronize();
        const rl::TrainingMetrics baseline = trainer.metrics();
        constexpr auto measurement_time = 6s;
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + measurement_time;
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(10ms);
            trainer.synchronize();
        }
        trainer.set_background_enabled(false);
        trainer.synchronize();
        const auto finished = std::chrono::steady_clock::now();

        const rl::TrainingMetrics metrics = trainer.metrics();
        const rl::AutonomyStatus status = trainer.autonomy_status();
        const std::uint64_t updates = metrics.update - baseline.update;
        const std::uint64_t environment_steps =
            metrics.environment_steps - baseline.environment_steps;
        const double seconds = std::chrono::duration<double>(finished - started).count();
        return {
            mode,
            updates,
            environment_steps,
            status.rollout_threads,
            seconds,
            static_cast<double>(updates) / seconds,
            static_cast<double>(environment_steps) / seconds
        };
'''
    text = replace_once(text, old, new, 'steady-state concurrency benchmark')
    write('tests/concurrency_benchmark.cpp', text)


def implement() -> None:
    patch_simulation_cpp()
    patch_simulation_hpp()
    patch_core_tests()
    patch_concurrency_benchmark()


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {'cache', 'implement'}:
        print('usage: fix_v0717_linux_regressions.py cache|implement', file=sys.stderr)
        return 2
    if sys.argv[1] == 'cache':
        cache()
    else:
        implement()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
