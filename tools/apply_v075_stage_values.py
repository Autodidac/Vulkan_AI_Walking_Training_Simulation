from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def save(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:180]!r}")
    save(path, text.replace(old, new, 1))


def sub(path: str, pattern: str, replacement: str) -> None:
    text = load(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"pattern matched {count} times in {path}: {pattern[:120]!r}")
    save(path, updated)


# Correct prerequisite order: stand -> static crouch -> walk/run -> crouch walk.
replace("src/simulation.hpp",
'''    enum class CourseStage : std::uint8_t
    {
        balance,
        uneven,
        duck_press,
        ramps,
        hurdles,
        duck_bars,
        moving_hazards
    };''',
'''    enum class CourseStage : std::uint8_t
    {
        balance,
        duck_press,
        uneven,
        crouch_walk,
        ramps,
        hurdles,
        duck_bars,
        moving_hazards
    };''')
replace("src/simulation.hpp", "inline constexpr std::size_t course_stage_count = 7;",
        "inline constexpr std::size_t course_stage_count = 8;")
replace("src/simulation.hpp",
'''        return stage == CourseStage::uneven
            || stage == CourseStage::hurdles
            || stage == CourseStage::moving_hazards;''',
'''        return stage == CourseStage::uneven
            || stage == CourseStage::crouch_walk
            || stage == CourseStage::hurdles
            || stage == CourseStage::moving_hazards;''')
replace("src/simulation.hpp",
'''        case CourseStage::duck_press:
            return alternating_steps >= 4u && duck_seconds >= 2.0f
                && obstacles_passed >= 3u;
        case CourseStage::ramps:
            return landed_jumps >= 1u;
        case CourseStage::uneven:
            return alternating_steps >= 2u;''',
'''        case CourseStage::duck_press:
            return duck_seconds >= 0.75f && obstacles_passed >= 1u;
        case CourseStage::uneven:
            return alternating_steps >= 4u;
        case CourseStage::crouch_walk:
            return alternating_steps >= 4u && duck_seconds >= 2.0f
                && obstacles_passed >= 3u;
        case CourseStage::ramps:
            return landed_jumps >= 1u;''')
replace("src/simulation.hpp",
'''        case CourseStage::balance: return "1. STAND";
        case CourseStage::uneven: return "2. WALK / RUN";
        case CourseStage::duck_press: return "3. CROUCH WALK / UNEVEN AVOID";
        case CourseStage::ramps: return "4. JUMP / LAND";
        case CourseStage::hurdles: return "5. MOVING LOW BAR / HURDLE";
        case CourseStage::duck_bars: return "6. CONTROLLED FLIPS";
        case CourseStage::moving_hazards: return "7. MIXED GOAL COURSE";''',
'''        case CourseStage::balance: return "1. STAND";
        case CourseStage::duck_press: return "2. STATIC CROUCH / HOLD / RECOVER";
        case CourseStage::uneven: return "3. WALK / RUN";
        case CourseStage::crouch_walk: return "4. CROUCH WALK / UNEVEN AVOID";
        case CourseStage::ramps: return "5. JUMP / LAND";
        case CourseStage::hurdles: return "6. MOVING LOW BAR / HURDLE";
        case CourseStage::duck_bars: return "7. CONTROLLED FLIPS";
        case CourseStage::moving_hazards: return "8. MIXED GOAL COURSE";''')

# Monoped gets real landing cycles instead of being forced through a biped
# left/right-step gate. Other rigs still require alternating support steps.
replace("src/simulation.hpp",
'''        [[nodiscard]] std::size_t support_seed_count() const noexcept
        {
            return 2u + additional_left_contact_nodes.size()
                + additional_right_contact_nodes.size();
        }
''',
'''        [[nodiscard]] std::size_t support_seed_count() const noexcept
        {
            return 2u + additional_left_contact_nodes.size()
                + additional_right_contact_nodes.size();
        }
        [[nodiscard]] bool monopedal_gait() const noexcept
        {
            return active_motor_count >= 4u
                && motors[2].enabled && motors[3].enabled
                && motors[2].a == motors[3].a
                && motors[2].pivot == motors[3].pivot;
        }
''')
replace("src/simulation.hpp",
'''        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] float duck_seconds() const noexcept { return duck_seconds_; }''',
'''        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] std::uint32_t gait_cycles() const noexcept
        {
            return blueprint_.monopedal_gait()
                ? std::max(alternating_steps_, single_leg_cycles_)
                : alternating_steps_;
        }
        [[nodiscard]] float duck_seconds() const noexcept { return duck_seconds_; }''')
replace("src/simulation.hpp",
'''        std::uint32_t alternating_steps_{};
        std::uint32_t progress_window_start_steps_{};''',
'''        std::uint32_t alternating_steps_{};
        std::uint32_t single_leg_cycles_{};
        float last_single_leg_landing_x_{};
        std::uint32_t progress_window_start_steps_{};''')

# Static crouch stays flat and stationary. Crouch walking gets the rough ground,
# low bars, and small rocks only after ordinary walking/running is mastered.
replace("src/simulation.hpp",
'''            if (course_stage_ == CourseStage::balance
                || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars)
                return 0.0f;
            if (course_stage_ == CourseStage::duck_press)
                return duck_press_completed_ ? 0.58f + course_difficulty_ * 0.12f : 0.0f;
            if (course_stage_ == CourseStage::uneven)''',
'''            if (course_stage_ == CourseStage::balance
                || course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars)
                return 0.0f;
            if (course_stage_ == CourseStage::crouch_walk)
                return 0.58f + course_difficulty_ * 0.18f;
            if (course_stage_ == CourseStage::uneven)''')
replace("src/simulation.hpp",
'''        [[nodiscard]] float course_progress() const noexcept
        {
            if (course_stage_ == CourseStage::duck_press && duck_press_completed_)
                return std::max(0.0f, elapsed_seconds_ - duck_walk_started_seconds_)
                    * course_speed();
            return elapsed_seconds_ * course_speed();
        }''',
'''        [[nodiscard]] float course_progress() const noexcept
        {
            return elapsed_seconds_ * course_speed();
        }''')
replace("src/simulation.cpp",
'''        if (course_stage_ != CourseStage::moving_hazards
            && !(course_stage_ == CourseStage::duck_press && duck_press_completed_))
            return 0.0f;''',
'''        if (course_stage_ != CourseStage::moving_hazards
            && course_stage_ != CourseStage::crouch_walk)
            return 0.0f;''')
replace("src/simulation.cpp",
'''        if (course_stage_ == CourseStage::duck_press)
        {
            const float roughness = 0.045f + course_difficulty_ * 0.075f;''',
'''        if (course_stage_ == CourseStage::crouch_walk)
        {
            const float roughness = 0.045f + course_difficulty_ * 0.075f;''')
replace("src/simulation.cpp",
'''        if (course_stage_ != CourseStage::duck_press
            && course_stage_ != CourseStage::hurdles
            && course_stage_ != CourseStage::moving_hazards)
            return;''',
'''        if (course_stage_ != CourseStage::duck_press
            && course_stage_ != CourseStage::crouch_walk
            && course_stage_ != CourseStage::hurdles
            && course_stage_ != CourseStage::moving_hazards)
            return;''')

sub("src/simulation.cpp",
    r'''        const float progress = course_progress\(\);\n        if \(course_stage_ == CourseStage::duck_press\)\n        \{.*?\n        \}\n        const int first_sequence = first_course_feature_sequence\(root_x, progress\);''',
'''        const float progress = course_progress();
        if (course_stage_ == CourseStage::duck_press)
        {
            const float rest_head_top = valid_node(blueprint_.head_node)
                ? blueprint_.nodes[blueprint_.head_node].y
                    + particles_[blueprint_.head_node].radius
                : 4.30f;
            if (!duck_press_completed_)
            {
                float minimum_x = blueprint_.nodes.empty() ? -0.5f : blueprint_.nodes.front().x;
                float maximum_x = minimum_x;
                for (const Vec2 node : blueprint_.nodes)
                {
                    minimum_x = std::min(minimum_x, node.x);
                    maximum_x = std::max(maximum_x, node.x);
                }
                const float half_width = clamp(
                    (maximum_x - minimum_x) * 0.42f + 0.45f, 0.82f, 1.20f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                constexpr float half_height = 0.14f;
                course_features_.push_back({
                    CourseFeatureKind::duck_press,
                    { root_x, profile.bottom_y + half_height },
                    { half_width, half_height }, 0.0f,
                    { 0.0f, profile.vertical_velocity }, -2
                });
            }
            return;
        }
        if (course_stage_ == CourseStage::crouch_walk)
        {
            const float rest_head_top = valid_node(blueprint_.head_node)
                ? blueprint_.nodes[blueprint_.head_node].y
                    + particles_[blueprint_.head_node].radius
                : 4.30f;
            constexpr float runway = 6.5f;
            constexpr float spacing = 4.8f;
            const int first_sequence = std::max(0, static_cast<int>(std::floor(
                (root_x + progress + runway) / spacing)));
            const float clearance = rest_head_top
                - (0.58f + course_difficulty_ * 0.10f);
            for (int offset = 0; offset < 7; ++offset)
            {
                const int sequence = first_sequence + offset;
                const float distance = static_cast<float>(sequence) * spacing + runway;
                const float x = distance - progress;
                if (x < root_x + 5.5f)
                    continue;
                const float ground = ground_height_at(x);
                if ((sequence % 3) == 1)
                {
                    const float radius = 0.12f + course_difficulty_ * 0.08f;
                    course_features_.push_back({
                        CourseFeatureKind::rock, { x, ground + radius }, {}, radius,
                        { -course_speed(), 0.0f }, 200 + sequence
                    });
                }
                else
                {
                    course_features_.push_back({
                        CourseFeatureKind::overhead_bar,
                        { x, ground + clearance + 0.11f }, { 0.92f, 0.11f }, 0.0f,
                        { -course_speed(), 0.0f }, 200 + sequence
                    });
                }
            }
            return;
        }
        const int first_sequence = first_course_feature_sequence(root_x, progress);''')

# Track a real single-leg landing cycle so the monoped can progress without
# pretending its split foot is a biped gait.
replace("src/simulation.cpp",
'''        alternating_steps_ = 0;
        progress_window_start_steps_ = 0;''',
'''        alternating_steps_ = 0;
        single_leg_cycles_ = 0;
        last_single_leg_landing_x_ = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        progress_window_start_steps_ = 0;''')
replace("src/simulation.cpp",
'''        else if (!was_supported)
        {
            if (powered_takeoff_)''',
'''        else if (!was_supported)
        {
            if (blueprint_.monopedal_gait()
                && (course_stage_ == CourseStage::uneven
                    || course_stage_ == CourseStage::crouch_walk)
                && std::abs(root_x - last_single_leg_landing_x_) >= 0.040f)
            {
                ++single_leg_cycles_;
                last_single_leg_landing_x_ = root_x;
            }
            if (powered_takeoff_)''')
replace("src/simulation.cpp",
'''        if (course_stage_ == CourseStage::duck_press && duck_press_completed_
            && duck_active_ && !non_foot_grounded_ && feet_supported)''',
'''        if (course_stage_ == CourseStage::crouch_walk
            && duck_active_ && !non_foot_grounded_ && feet_supported)''')
replace("src/simulation.cpp",
'''        if (course_stage_ == CourseStage::duck_press && duck_obstacle_weight_ > 0.10f
            && std::abs(torso_turn_speed_) > 0.85f)''',
'''        if ((course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            && duck_obstacle_weight_ > 0.10f
            && std::abs(torso_turn_speed_) > 0.85f)''')
replace("src/simulation.cpp",
'''        const bool locomotion_required = stage_requires_forward_gait(course_stage_)
            || (course_stage_ == CourseStage::duck_press && duck_press_completed_);''',
'''        const bool locomotion_required = stage_requires_forward_gait(course_stage_);''')

# Separate the static crouch teacher from the inherited moving-crouch teacher.
replace("src/ppo.hpp",
'''        const float leg_pair_strength = stage == sim::CourseStage::duck_press
            ? 0.12f : (stage == sim::CourseStage::balance''',
'''        const float leg_pair_strength = (stage == sim::CourseStage::duck_press
                || stage == sim::CourseStage::crouch_walk)
            ? 0.12f : (stage == sim::CourseStage::balance''')
replace("src/ppo.hpp",
'''        return bilateral_joint_synergy_action(environment, action, sim::CourseStage::duck_press);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action''',
'''        return bilateral_joint_synergy_action(environment, action, sim::CourseStage::duck_press);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const float pressure = std::max(0.72f, environment.duck_obstacle_weight());
        const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.05f;
        const float swing = std::sin(phase);
        action[0] = clamp(action[0] - 0.24f * pressure + 0.34f * swing, -0.82f, 0.82f);
        action[1] = clamp(action[1] + 0.50f * pressure
            + 0.34f * std::max(0.0f, swing), -0.90f, 0.90f);
        action[2] = clamp(action[2] + 0.24f * pressure - 0.34f * swing, -0.82f, 0.82f);
        action[3] = clamp(action[3] - 0.50f * pressure
            - 0.34f * std::max(0.0f, -swing), -0.90f, 0.90f);
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
            action[index] = 0.0f;
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::crouch_walk);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action''')
replace("src/ppo.hpp",
'''        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);''',
'''        else if (stage == sim::CourseStage::crouch_walk)
        {
            const auto teacher = crouch_walk_teacher_action(environment);
            const float leg_assist = 0.58f + environment.duck_obstacle_weight() * 0.24f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], 0.0f, 0.98f);
        }
        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);''')
replace("src/ppo_trainer.cpp",
'''            if (stage == sim::CourseStage::duck_press)
            {
                if (update < 600u)''',
'''            if (stage == sim::CourseStage::duck_press
                || stage == sim::CourseStage::crouch_walk)
            {
                if (update < 600u)''')
replace("src/ppo_trainer.cpp",
'''            if (stage == sim::CourseStage::duck_press)
                return duck_teacher_action(environment);''',
'''            if (stage == sim::CourseStage::duck_press)
                return duck_teacher_action(environment);
            if (stage == sim::CourseStage::crouch_walk)
                return crouch_walk_teacher_action(environment);''')
replace("src/ppo_trainer.cpp", "totals.alternating_steps += environment.alternating_steps();",
        "totals.alternating_steps += environment.gait_cycles();")
replace("src/ppo_parallel.cpp", "totals.strides += static_cast<float>(environment.alternating_steps());",
        "totals.strides += static_cast<float>(environment.gait_cycles());")

# Every stage now checks only the values that actually define that stage.
replace("src/ppo.hpp",
'''        case sim::CourseStage::duck_press:
            if (environment.longest_stable_stance_seconds() < 2.0f
                || environment.stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.duck_recoveries() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.alternating_steps() < 4u
                || environment.crouch_walk_seconds() < 2.0f
                || environment.crouch_walk_distance() < 0.75f
                || environment.obstacles_passed() < 3u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.maximum_joint_speed() > 12.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;
        case sim::CourseStage::ramps:''',
'''        case sim::CourseStage::duck_press:
            if (environment.longest_stable_stance_seconds() < 2.0f
                || environment.stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (!environment.duck_press_completed()
                || environment.duck_recoveries() < 1u
                || environment.duck_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.maximum_joint_speed() > 12.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;
        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.gait_cycles() < 4u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::crouch_walk:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.gait_cycles() < 4u
                || environment.crouch_walk_seconds() < 2.0f
                || environment.crouch_walk_distance() < 0.75f
                || environment.obstacles_passed() < 3u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::ramps:''')
# Remove the old duplicate uneven case following ramps.
sub("src/ppo.hpp",
    r'''        case sim::CourseStage::uneven:\n            if \(environment.longest_stable_stance_seconds\(\) < 1.25f\).*?            break;\n        case sim::CourseStage::hurdles:''',
    '''        case sim::CourseStage::hurdles:''')
replace("src/ppo.hpp",
'''        case sim::CourseStage::duck_press:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.duck_recoveries(), 65535u)),
                quality_bucket(environment.stable_stance_seconds()),
                quality_bucket(environment.duck_seconds()),
                quality_bucket(environment.elapsed_seconds()));
            break;
        case sim::CourseStage::ramps:''',
'''        case sim::CourseStage::duck_press:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.duck_recoveries(), 65535u)),
                quality_bucket(environment.duck_seconds()),
                quality_bucket(environment.stable_stance_seconds()),
                quality_bucket(environment.elapsed_seconds()));
            break;
        case sim::CourseStage::crouch_walk:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.gait_cycles(), 65535u)),
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.obstacles_passed(), 65535u)),
                quality_bucket(environment.crouch_walk_distance()),
                quality_bucket(environment.crouch_walk_seconds()));
            break;
        case sim::CourseStage::ramps:''')
replace("src/ppo.hpp", "environment.alternating_steps(), 65535u)),\n                static_cast<std::uint16_t>(std::min<std::uint32_t>(\n                    environment.obstacles_passed(), 65535u)),",
        "environment.gait_cycles(), 65535u)),\n                static_cast<std::uint16_t>(std::min<std::uint32_t>(\n                    environment.obstacles_passed(), 65535u)),")
replace("src/ppo.hpp",
'''        if (stage == sim::CourseStage::duck_press)
        {
            return environment.duck_press_completed()
                && environment.duck_active()
                && !environment.non_foot_grounded()
                && environment.uprightness() >= 0.60f
                && environment.crouch_walk_seconds() >= 0.35f
                && environment.alternating_steps() >= 1u
                && (environment.left_supported() || environment.right_supported());
        }''',
'''        if (stage == sim::CourseStage::duck_press)
        {
            return environment.duck_press_completed()
                && !environment.non_foot_grounded()
                && environment.duck_recoveries() >= 1u
                && environment.duck_seconds() >= 0.75f
                && environment.uprightness() >= 0.60f
                && (environment.left_supported() || environment.right_supported());
        }
        if (stage == sim::CourseStage::crouch_walk)
        {
            return environment.duck_active()
                && !environment.non_foot_grounded()
                && environment.uprightness() >= 0.60f
                && environment.crouch_walk_seconds() >= 0.35f
                && environment.gait_cycles() >= 1u
                && (environment.left_supported() || environment.right_supported());
        }''')

# Strict stage mastery values: no movement for stand/static crouch; ordinary gait
# must be established before moving crouch can ever unlock.
replace("src/autonomy_curriculum.cpp",
'''        case sim::CourseStage::balance:
            return metrics.evaluation_longest_stance >= 5.0f
                && metrics.evaluation_survival >= 6.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_speed >= 0.70f
                && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 2.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_duck_seconds >= 3.5f
                && metrics.evaluation_distance >= 1.50f
                && metrics.evaluation_obstacles_passed >= 4.0f
                && metrics.evaluation_collisions <= 1.0f
                && metrics.evaluation_survival >= 14.0f;
        case sim::CourseStage::ramps:''',
'''        case sim::CourseStage::balance:
            return metrics.evaluation_longest_stance >= 5.0f
                && metrics.evaluation_survival >= 6.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;
        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 2.0f
                && metrics.evaluation_duck_seconds >= 1.25f
                && metrics.evaluation_longest_stance >= 2.5f
                && metrics.evaluation_survival >= 9.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_speed >= 0.70f
                && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::crouch_walk:
            return metrics.evaluation_duck_recoveries >= 1.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_duck_seconds >= 3.5f
                && metrics.evaluation_distance >= 1.50f
                && metrics.evaluation_obstacles_passed >= 4.0f
                && metrics.evaluation_collisions <= 1.0f
                && metrics.evaluation_survival >= 14.0f;
        case sim::CourseStage::ramps:''')

# Make the PIP and aggregate telemetry use a valid gait cycle for every rig.
replace("src/app.cpp", "environment.alternating_steps(), environment.obstacles_passed())",
        "environment.gait_cycles(), environment.obstacles_passed())")
replace("src/autonomy_curriculum.cpp",
        "+ static_cast<float>(environment.alternating_steps()) * 0.03f",
        "+ static_cast<float>(environment.gait_cycles()) * 0.03f")

# Correct tests for the split stages and the monoped-specific gait evidence.
tests = load("tests/core_tests.cpp")
tests = tests.replace(
'''    require(sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
        && sim::course_stage_name(sim::CourseStage::uneven) == "2. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::duck_press) == "3. CROUCH WALK / UNEVEN AVOID"
        && sim::course_stage_name(sim::CourseStage::ramps) == "4. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::hurdles) == "5. MOVING LOW BAR / HURDLE"
        && sim::course_stage_name(sim::CourseStage::duck_bars) == "6. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards) == "7. MIXED GOAL COURSE"
        && static_cast<std::uint8_t>(sim::CourseStage::balance)
            < static_cast<std::uint8_t>(sim::CourseStage::uneven)
        && static_cast<std::uint8_t>(sim::CourseStage::uneven)
            < static_cast<std::uint8_t>(sim::CourseStage::duck_press),
        "walking and running do not precede crouch walking in the curriculum");''',
'''    require(sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
        && sim::course_stage_name(sim::CourseStage::duck_press)
            == "2. STATIC CROUCH / HOLD / RECOVER"
        && sim::course_stage_name(sim::CourseStage::uneven) == "3. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::crouch_walk)
            == "4. CROUCH WALK / UNEVEN AVOID"
        && sim::course_stage_name(sim::CourseStage::ramps) == "5. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::hurdles) == "6. MOVING LOW BAR / HURDLE"
        && sim::course_stage_name(sim::CourseStage::duck_bars) == "7. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards) == "8. MIXED GOAL COURSE"
        && static_cast<std::uint8_t>(sim::CourseStage::balance)
            < static_cast<std::uint8_t>(sim::CourseStage::duck_press)
        && static_cast<std::uint8_t>(sim::CourseStage::duck_press)
            < static_cast<std::uint8_t>(sim::CourseStage::uneven)
        && static_cast<std::uint8_t>(sim::CourseStage::uneven)
            < static_cast<std::uint8_t>(sim::CourseStage::crouch_walk),
        "stand, static crouch, walk/run, and crouch-walk prerequisites are misordered");''')
tests = tests.replace("sim::CourseStage::duck_press,\n            3u, 3.0f", "sim::CourseStage::crouch_walk,\n            3u, 3.0f")
tests = tests.replace("sim::CourseStage::duck_press,\n                5u, 1.5f", "sim::CourseStage::crouch_walk,\n                5u, 1.5f")
tests = tests.replace("sim::CourseStage::duck_press,\n                5u, 3.0f", "sim::CourseStage::crouch_walk,\n                5u, 3.0f")
tests = tests.replace("sim::CourseStage::duck_press, 5u, 3.0f", "sim::CourseStage::crouch_walk, 5u, 3.0f")
anchor = '''    require(!sim::duck_ground_contact_allowed(true, true)
            && sim::duck_ground_contact_allowed(true, false)
            && sim::duck_ground_contact_allowed(false, true),
        "foot-only duck contact rule is not strict");'''
addition = anchor + '''
    require(sim::stage_skill_evidence(sim::CourseStage::balance,
            0u, 0.0f, 0u, 0.0f, 0u, 0u),
        "standing incorrectly requires movement");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_press,
            0u, 0.80f, 0u, 0.0f, 0u, 1u),
        "static crouch incorrectly requires walking or running");
    require(!sim::stage_skill_evidence(sim::CourseStage::uneven,
            0u, 0.0f, 0u, 0.0f, 0u, 0u)
            && sim::stage_skill_evidence(sim::CourseStage::uneven,
                4u, 0.0f, 0u, 0.0f, 0u, 0u),
        "walking/running stage uses the wrong movement evidence");
    require(sim::CreatureBlueprint::monoped().monopedal_gait()
            && !sim::CreatureBlueprint::humanoid().monopedal_gait(),
        "monoped gait is not distinguished from alternating biped gait");'''
if addition not in tests:
    if anchor not in tests:
        raise RuntimeError("duck contact test anchor missing")
    tests = tests.replace(anchor, addition, 1)
save("tests/core_tests.cpp", tests)

# Reordered/split stage serialization invalidates every earlier v0.7.5 state.
replace("src/ppo.hpp", "inline constexpr std::uint32_t training_semantics_version = 0x0007'0501u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'0502u;")
replace("src/autonomy_persistence.cpp", 'output << "RUNAUTONOMY 9\\n";',
        'output << "RUNAUTONOMY 10\\n";')
replace("src/autonomy_persistence.cpp", 'magic != "RUNAUTONOMY" || version != 9',
        'magic != "RUNAUTONOMY" || version != 10')

mission = load("missioncache.md")
entry = '''

### WALK-STAGES-076 — Correct stage-specific qualification values
**Status:** IN PROGRESS

Standing passes on sustained stable foot support without distance. Static crouching passes on a foot-only compressed hold and controlled recovery without walking. Ordinary walking/running then requires real gait cycles, distance, and speed. Only after that controller is locked does crouch walking require inherited gait, sustained crouch, unstable-ground progress, and obstacle passes. Jumping, hurdles, flips, and mixed traversal retain their own separate evidence.

### WALK-MONOPED-077 — Restore single-leg gait progression
**Status:** IN PROGRESS

The monoped is no longer forced to fake alternating biped steps. A forward single-leg landing cycle counts as its gait cycle, while multi-leg rigs still require alternating support. The same stage thresholds remain strict about distance, speed, stability, and later crouch or obstacle evidence.
'''
if "### WALK-STAGES-076" not in mission:
    mission += entry
save("missioncache.md", mission)

notes = load("RELEASE_NOTES_v0.7.5.md")
lines = (
    "- Splits static crouching from crouch walking: stand and static crouch require no movement, walk/run must be mastered next, and moving crouch comes afterward.\n"
    "- Restores monoped progression by counting real forward single-leg landing cycles instead of demanding alternating biped footfalls.\n"
)
if "Splits static crouching" not in notes:
    notes = notes.replace("# Runner v0.7.5\n\n", "# Runner v0.7.5\n\n" + lines, 1)
save("RELEASE_NOTES_v0.7.5.md", notes)

Path(__file__).unlink()
print("split stage values and restored monoped-compatible gait evidence")
