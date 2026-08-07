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
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def replace_exact_count(text: str, old: str, new: str, count: int, label: str) -> str:
    found = text.count(old)
    if found != count:
        raise RuntimeError(f"{label}: expected {count} matches, found {found}")
    return text.replace(old, new)


def cache() -> None:
    path = "missioncache.md"
    text = read(path)
    marker = "# Runner v0.7.18 treadmill-coordinate walking correction"
    if marker in text:
        return
    insertion = r'''# Runner v0.7.18 treadmill-coordinate walking correction

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

The overnight v0.7.17 eye test reaches Walk but reports only zero-to-two credited steps while the course itself moves at walking speed. Source audit found a coordinate-frame contradiction: moving lessons scroll terrain with `course_progress()`, but gait strike displacement, `distance_travelled_`, `forward_speed_`, and forward reward are measured only in fixed screen/world X. A correct treadmill gait can therefore walk in place relative to the camera yet receive zero travelled distance, fail the 5.5 cm step-displacement gate, fail the 6 m qualification gate, and never create a valid Walk champion. The existing qualification gate also conflates a safe incremental candidate with final stage mastery, so a two-step improvement is discarded instead of checkpointed.

### WALK-COURSE-FRAME-226 — Use terrain-relative locomotion coordinates
**Status:** OPEN — RELEASE BLOCKING

Moving-course locomotion distance and per-frame forward progress use the same transform as the scrolling terrain: world X plus `course_progress()`. Static Stand/Crouch/Jump lessons remain unchanged because their course speed is zero.

### WALK-STEP-FRAME-227 — Credit real alternating strikes on the treadmill
**Status:** OPEN — RELEASE BLOCKING

Alternating step displacement is measured in terrain-relative locomotion X while foot crossing, swing-air time, swing clearance, and contact transitions remain physical world-space evidence. A walker may stay camera-centered without losing legitimate step credit.

### WALK-SPEED-FRAME-228 — Report and train terrain-relative forward speed
**Status:** OPEN — RELEASE BLOCKING

Logical forward speed on moving lessons includes course speed plus physical root speed. PPO evaluation, speed mastery, reward shaping, telemetry, and overspeed use the resulting ground-relative speed; static lessons are numerically unchanged.

### WALK-INCREMENTAL-CHAMPION-229 — Separate safe candidate qualification from mastery
**Status:** OPEN — RELEASE BLOCKING

Walk may checkpoint a physically valid incremental sagittal candidate after two alternating steps, at least one genuine limb crossing, one metre of terrain-relative progress, and two seconds of survival. Final Walk mastery remains strict at the existing 18 m / 16 stride / speed / survival requirements, and crab walking, body contact, invalid motion, and structural failures remain rejected.

### WALK-IDLE-GATE-230 — Preserve anti-idle and anti-vibration behavior
**Status:** OPEN — RELEASE BLOCKING

The one-second zero-progress anti-idle window stays in camera/world space and still requires useful swing lift or a credited step. Merely standing still while terrain scrolls must not count as active gait.

### WALK-BOOTSTRAP-231 — Keep useful guidance long enough to establish gait
**Status:** OPEN — RELEASE BLOCKING

Early Walk bootstrap remains strongly sagittal through the first meaningful training window, then decays gradually so PPO takes control after a valid incremental walker exists.

### WALK-COORDINATE-TEST-232 — Deterministically lock the coordinate contract
**Status:** OPEN — RELEASE BLOCKING

Regression tests prove terrain-relative distance/frame progress, nonzero moving-course distance with a camera-centered rig, opposite-phase teacher drive, and the existing v0.7.18 reset/marker contracts. Full Linux and Windows release gates remain mandatory.

'''
    anchor = "# Carried open work\n"
    if anchor not in text:
        raise RuntimeError("mission cache insertion anchor missing")
    text = text.replace(anchor, insertion + anchor, 1)
    write(path, text)


def implement_simulation_header() -> None:
    path = "src/simulation.hpp"
    text = read(path)
    old = '''    [[nodiscard]] constexpr float terrain_world_x(float terrain_x,
        float course_progress) noexcept
    {
        return terrain_x - course_progress;
    }
'''
    new = old + '''
    [[nodiscard]] constexpr float terrain_relative_distance(float world_x,
        float initial_world_x, float course_progress) noexcept
    {
        return terrain_sample_x(world_x, course_progress) - initial_world_x;
    }

    [[nodiscard]] constexpr float terrain_relative_frame_progress(
        float current_world_x, float previous_world_x,
        float course_speed, float dt) noexcept
    {
        return (current_world_x - previous_world_x) + course_speed * dt;
    }
'''
    text = replace_once(text, old, new, "terrain-relative helpers")
    write(path, text)


def implement_simulation_cpp() -> None:
    path = "src/simulation.cpp"
    text = read(path)
    old_root = '''        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float left_clearance = contact_cluster_clearance(blueprint_.left_contact_node);
'''
    new_root = '''        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float locomotion_x = terrain_sample_x(root_x, course_progress());
        const float left_clearance = contact_cluster_clearance(blueprint_.left_contact_node);
'''
    text = replace_once(text, old_root, new_root, "gait locomotion coordinate")
    text = replace_once(text,
        '''                    elapsed_seconds_ - last_step_time_, root_x - last_step_x_,
''',
        '''                    elapsed_seconds_ - last_step_time_, locomotion_x - last_step_x_,
''', "crossing-step displacement")
    text = replace_exact_count(text, "last_step_x_ = root_x;", "last_step_x_ = locomotion_x;", 2,
        "strike coordinate publication")
    text = replace_once(text,
        "&& std::abs(root_x - last_single_leg_landing_x_) >= 0.040f)",
        "&& std::abs(locomotion_x - last_single_leg_landing_x_) >= 0.040f)",
        "single-leg displacement")
    text = replace_once(text,
        "last_single_leg_landing_x_ = root_x;",
        "last_single_leg_landing_x_ = locomotion_x;",
        "single-leg landing coordinate")

    old_motion = '''        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;
        const float raw_speed = (pelvis_position.x - previous_pelvis_.x) / dt;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        const float frame_progress = pelvis_position.x - previous_pelvis_.x;
        previous_pelvis_ = pelvis_position;
        distance_travelled_ = pelvis_position.x - blueprint_.nodes[blueprint_.root_node].x;
        maximum_speed_kmh_ = std::max(maximum_speed_kmh_, std::max(std::abs(raw_speed), std::abs(forward_speed_)) * 3.6f);
'''
    new_motion = '''        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;
        const float raw_world_speed = (pelvis_position.x - previous_pelvis_.x) / dt;
        const float moving_course_speed = course_speed();
        const float raw_speed = raw_world_speed + moving_course_speed;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        const float frame_progress = terrain_relative_frame_progress(
            pelvis_position.x, previous_pelvis_.x, moving_course_speed, dt);
        previous_pelvis_ = pelvis_position;
        distance_travelled_ = terrain_relative_distance(pelvis_position.x,
            blueprint_.nodes[blueprint_.root_node].x, course_progress());
        maximum_speed_kmh_ = std::max(maximum_speed_kmh_, std::max(std::abs(raw_speed), std::abs(forward_speed_)) * 3.6f);
'''
    text = replace_once(text, old_motion, new_motion, "ground-relative speed and distance")
    write(path, text)


def implement_ppo_header() -> None:
    path = "src/ppo.hpp"
    text = read(path)
    text = replace_once(text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1700u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1801u;",
        "training semantics")
    old = '''        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.blueprint().paired_leg_chains()
                && sim::crab_walking_motion(environment.alternating_steps(),
                    environment.limb_crossings(), environment.distance_travelled(),
                    environment.elapsed_seconds(),
                    environment.primary_support_span_ratio()))
                rejection |= evidence_bit(MotionEvidenceFailure::lateral_crab_gait);
            if (environment.blueprint().paired_leg_chains()
                ? !sim::sagittal_gait_evidence(environment.alternating_steps(),
                    environment.limb_crossings(), environment.distance_travelled(),
                    environment.elapsed_seconds(),
                    environment.primary_support_span_ratio())
                : environment.gait_cycles() < 10u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 6.0f
                || environment.elapsed_seconds() < 8.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
'''
    new = '''        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.blueprint().paired_leg_chains()
                && sim::crab_walking_motion(environment.alternating_steps(),
                    environment.limb_crossings(), environment.distance_travelled(),
                    environment.elapsed_seconds(),
                    environment.primary_support_span_ratio()))
                rejection |= evidence_bit(MotionEvidenceFailure::lateral_crab_gait);
            // Qualification is the safe incremental checkpoint gate, not final
            // Walk mastery. Preserve a real two-step sagittal improvement so PPO
            // can build on it instead of discarding every policy below mastery.
            if (environment.blueprint().paired_leg_chains()
                ? (environment.alternating_steps() < 2u
                    || environment.limb_crossings() < 1u)
                : environment.gait_cycles() < 2u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f
                || environment.elapsed_seconds() < 2.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
'''
    text = replace_once(text, old, new, "incremental walk qualification")
    write(path, text)


def implement_ppo_trainer() -> None:
    path = "src/ppo_trainer.cpp"
    text = read(path)
    old = '''            if (update < 800u)
                return 0.50f;
            if (update < 3000u)
                return lerp(0.50f, 0.18f,
                    static_cast<float>(update - 800u) / 2200.0f);
            if (update < 8000u)
                return lerp(0.18f, 0.04f,
                    static_cast<float>(update - 3000u) / 5000.0f);
            return 0.02f;
'''
    new = '''            if (update < 1200u)
                return 0.62f;
            if (update < 4000u)
                return lerp(0.62f, 0.24f,
                    static_cast<float>(update - 1200u) / 2800.0f);
            if (update < 9000u)
                return lerp(0.24f, 0.06f,
                    static_cast<float>(update - 4000u) / 5000.0f);
            return 0.03f;
'''
    text = replace_once(text, old, new, "walk bootstrap duration")
    write(path, text)


def implement_tests() -> None:
    path = "tests/v0718_runtime_recovery_tests.cpp"
    text = read(path)
    anchor = '''    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::imperial) - 15.24f) < 0.0001f,
        "imperial marker spacing is not 50 feet");

'''
    addition = anchor + '''    require(std::abs(sim::terrain_relative_distance(0.0f, 0.0f, 6.0f) - 6.0f)
            < 0.0001f,
        "moving-course distance ignores terrain progress");
    require(std::abs(sim::terrain_relative_frame_progress(
            1.0f, 1.0f, 1.25f, 0.02f) - 0.025f) < 0.0001f,
        "camera-centered treadmill motion has zero logical progress");

'''
    text = replace_once(text, anchor, addition, "coordinate helper tests")
    old_walk = '''    for (int frame = 0; frame < 12; ++frame)
        (void)walking.step(neutral);
    const auto teacher = rl::walking_teacher_action(walking);
'''
    new_walk = '''    for (int frame = 0; frame < 12; ++frame)
        (void)walking.step(neutral);
    require(walking.distance_travelled() > 0.10f,
        "moving Walk course still reports zero terrain-relative distance");
    require(walking.forward_speed() > 0.20f,
        "moving Walk course still reports zero ground-relative speed");
    const auto teacher = rl::walking_teacher_action(walking);
'''
    text = replace_once(text, old_walk, new_walk, "runtime coordinate integration test")
    write(path, text)


def implement_changelog() -> None:
    path = "CHANGELOG.md"
    text = read(path)
    if text.startswith("## 0.7.18\n"):
        return
    prefix = '''## 0.7.18

- Fixed the moving-course coordinate-frame bug that measured treadmill gait in fixed camera X, causing real alternating steps to receive zero distance and fail qualification.
- Made locomotion distance, logical speed, step displacement, PPO shaping, and evaluation use terrain-relative progress while preserving the world-space anti-idle gate.
- Split safe incremental Walk candidate qualification from final mastery so two-step sagittal improvements can be checkpointed and refined instead of discarded.
- Extended early sagittal teacher/bootstrap authority and isolated corrected training semantics from v0.7.17 state.
- Retained the v0.7.17 terrain model, restored near-course markers/controls/telemetry, and kept optional body art disabled by default.

'''
    write(path, prefix + text)


def implement_release_workflow() -> None:
    path = ".github/workflows/runner-v0717-release.yml"
    text = read(path)
    text = replace_once(text,
        "          grep -F 'WALK-RELEASE-225' missioncache.md\n",
        "          grep -F 'WALK-RELEASE-225' missioncache.md\n          grep -F 'WALK-COURSE-FRAME-226' missioncache.md\n          grep -F 'WALK-COORDINATE-TEST-232' missioncache.md\n",
        "release source audit missions")
    text = replace_once(text,
        "          for number in range(211, 225):",
        "          for number in range(211, 233):",
        "release evidence mission range")
    old_notes = "          - Strengthens early sagittal walking bootstrap without weakening crab-walk rejection.\n"
    new_notes = old_notes + "          - Fixes treadmill/world coordinate mismatch so real side-view steps accumulate terrain-relative distance and speed.\n          - Checkpoints safe incremental two-step sagittal walkers before final mastery instead of discarding them.\n"
    text = replace_once(text, old_notes, new_notes, "release notes coordinate fix")
    write(path, text)


def implement_docs() -> None:
    path = "docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
    text = read(path)
    marker = "## Treadmill-coordinate correction"
    if marker in text:
        return
    text += '''

## Treadmill-coordinate correction

The moving Walk/Crouch/Hurdle/Mixed lessons render and collide in a scrolling terrain frame. Locomotion evidence must therefore measure ground-relative travel as `world_x + course_progress`, not fixed camera/world X alone. v0.7.18 now uses that same transform for distance, per-frame progress, logical forward speed, and strike displacement. Physical foot crossing and contact remain world-space, while the anti-idle window intentionally remains camera/world-space so standing still on a moving course does not become a fake gait.

Walk qualification is now an incremental safe-checkpoint gate (two alternating steps, one sagittal crossing, one metre, two seconds) while strict stage mastery remains unchanged. This lets PPO retain a two-step improvement and evolve it into sustained walking instead of repeatedly throwing it away for not already being a mastered walker.
'''
    write(path, text)


def implement() -> None:
    implement_simulation_header()
    implement_simulation_cpp()
    implement_ppo_header()
    implement_ppo_trainer()
    implement_tests()
    implement_changelog()
    implement_release_workflow()
    implement_docs()


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        raise SystemExit("usage: apply_v0718_walk_coordinate_recovery.py cache|implement")
    if sys.argv[1] == "cache":
        cache()
    else:
        implement()
