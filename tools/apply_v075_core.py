from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def load(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def save(name: str, text: str) -> None:
    (ROOT / name).write_text(text, encoding="utf-8", newline="\n")


def replace(name: str, old: str, new: str) -> None:
    text = load(name)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {name}: {old[:120]!r}")
    save(name, text.replace(old, new, 1))


def sub(name: str, pattern: str, replacement: str) -> None:
    text = load(name)
    changed, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"pattern matched {count} times in {name}: {pattern[:100]!r}")
    save(name, changed)


replace("CMakeLists.txt", "project(Runner VERSION 0.7.4 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.5 LANGUAGES CXX)")
replace("src/ppo.hpp", "inline constexpr std::uint32_t training_semantics_version = 0x0007'0400u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'0500u;")

replace("src/simulation.hpp",
'''        case CourseStage::duck_press:
            return duck_seconds >= 0.50f && obstacles_passed >= 2u;''',
'''        case CourseStage::duck_press:
            return alternating_steps >= 4u && duck_seconds >= 2.0f
                && obstacles_passed >= 3u;''')
replace("src/simulation.hpp",
'''        case CourseStage::duck_press: return "2. PRESS DUCK / HOLD / RECOVER";''',
'''        case CourseStage::duck_press: return "2. CROUCH WALK / UNEVEN AVOID";''')
replace("src/simulation.hpp",
'''        robotic_torso_swing,
        press_penetration
    };''',
'''        robotic_torso_swing,
        press_penetration,
        duck_body_contact
    };''')
replace("src/simulation.hpp",
'''        case InvalidMotion::press_penetration: return "DUCK PRESS PENETRATION";''',
'''        case InvalidMotion::press_penetration: return "DUCK PRESS PENETRATION";
        case InvalidMotion::duck_body_contact: return "DUCK CONTACT - FEET ONLY";''')
replace("src/simulation.hpp",
'''    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept''',
'''    [[nodiscard]] inline bool duck_ground_contact_allowed(bool duck_active,
        bool non_foot_grounded) noexcept
    {
        return !duck_active || !non_foot_grounded;
    }

    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept''')
replace("src/simulation.hpp",
'''        [[nodiscard]] float course_progress() const noexcept { return elapsed_seconds_ * course_speed(); }''',
'''        [[nodiscard]] float course_progress() const noexcept
        {
            if (course_stage_ == CourseStage::duck_press && duck_press_completed_)
                return std::max(0.0f, elapsed_seconds_ - duck_walk_started_seconds_)
                    * course_speed();
            return elapsed_seconds_ * course_speed();
        }''')
replace("src/simulation.hpp",
'''        [[nodiscard]] float duck_seconds() const noexcept { return duck_seconds_; }
        [[nodiscard]] bool duck_active() const noexcept { return duck_active_; }''',
'''        [[nodiscard]] float duck_seconds() const noexcept { return duck_seconds_; }
        [[nodiscard]] float crouch_walk_seconds() const noexcept { return crouch_walk_seconds_; }
        [[nodiscard]] float crouch_walk_distance() const noexcept { return crouch_walk_distance_; }
        [[nodiscard]] bool duck_active() const noexcept { return duck_active_; }''')
replace("src/simulation.hpp",
'''        float duck_press_hold_seconds_{};
        float duck_press_max_penetration_{};
        float torso_swing_seconds_{};''',
'''        float duck_press_hold_seconds_{};
        float duck_press_max_penetration_{};
        float duck_walk_started_seconds_{};
        float crouch_walk_seconds_{};
        float crouch_walk_distance_{};
        float torso_swing_seconds_{};''')

replace("src/simulation.cpp",
'''        duck_press_hold_seconds_ = 0.0f;
        duck_press_max_penetration_ = 0.0f;
        torso_swing_seconds_ = 0.0f;''',
'''        duck_press_hold_seconds_ = 0.0f;
        duck_press_max_penetration_ = 0.0f;
        duck_walk_started_seconds_ = 0.0f;
        crouch_walk_seconds_ = 0.0f;
        crouch_walk_distance_ = 0.0f;
        torso_swing_seconds_ = 0.0f;''')

replace("src/simulation.cpp",
'''        if (course_stage_ != CourseStage::moving_hazards)
            return 0.0f;''',
'''        if (course_stage_ != CourseStage::moving_hazards
            && !(course_stage_ == CourseStage::duck_press && duck_press_completed_))
            return 0.0f;''')
replace("src/simulation.cpp",
'''        if (course_stage_ >= CourseStage::uneven && !course_zone_is_flat(course_x))
        {
            const float roughness = course_difficulty_ * 0.065f;
            height += std::sin(course_x * 0.83f) * roughness;
            height += std::sin(course_x * 2.17f + 0.7f) * roughness * 0.42f;
        }
        return std::max(-0.06f, height);''',
'''        if (course_stage_ == CourseStage::duck_press)
        {
            const float roughness = 0.045f + course_difficulty_ * 0.075f;
            height += std::sin(course_x * 0.91f) * roughness;
            height += std::sin(course_x * 2.43f + 0.7f) * roughness * 0.48f;
            height += std::sin(course_x * 4.10f + 1.2f) * roughness * 0.18f;
        }
        else if (course_stage_ >= CourseStage::uneven && !course_zone_is_flat(course_x))
        {
            const float roughness = course_difficulty_ * 0.065f;
            height += std::sin(course_x * 0.83f) * roughness;
            height += std::sin(course_x * 2.17f + 0.7f) * roughness * 0.42f;
        }
        return std::max(-0.08f, height);''')

sub("src/simulation.cpp",
    r'''            constexpr float cycle = 11\.0f;.*?            return;\n        \}\n        const int first_sequence''',
'''            const float runway = 6.5f;
            const float spacing = 4.8f;
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
        const int first_sequence''')

replace("src/simulation.cpp",
'''        duck_active_ = feet_supported && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        if (duck_active_)
            duck_seconds_ += dt;''',
'''        duck_active_ = feet_supported && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        if (!duck_ground_contact_allowed(duck_active_, non_foot_grounded_))
            invalidate(InvalidMotion::duck_body_contact);
        if (duck_active_ && !non_foot_grounded_)
            duck_seconds_ += dt;
        if (course_stage_ == CourseStage::duck_press && duck_press_completed_
            && duck_active_ && !non_foot_grounded_ && feet_supported)
        {
            crouch_walk_seconds_ += dt;
            crouch_walk_distance_ += std::max(0.0f, root_speed) * dt;
        }''')
replace("src/simulation.cpp",
'''            if (duck_press_contact_this_step_ && duck_active_
                && duck_clearance_margin_ >= -0.025f && body_integrity_valid())''',
'''            if (duck_press_contact_this_step_ && duck_active_ && !non_foot_grounded_
                && duck_clearance_margin_ >= -0.025f && body_integrity_valid())''')
replace("src/simulation.cpp",
'''                duck_press_completed_ = true;
                ++duck_recovery_count_;''',
'''                duck_press_completed_ = true;
                duck_walk_started_seconds_ = elapsed_seconds_;
                progress_window_start_x_ = root_x;
                progress_window_start_steps_ = alternating_steps_;
                ++duck_recovery_count_;''')

replace("src/simulation.cpp",
'''        if (stage_requires_forward_gait(course_stage_)
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))''',
'''        const bool locomotion_required = stage_requires_forward_gait(course_stage_)
            || (course_stage_ == CourseStage::duck_press && duck_press_completed_);
        if (locomotion_required
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))''')
replace("src/simulation.cpp",
'''            if (stage_requires_forward_gait(course_stage_)
                && (high_energy_stall || inefficient_vibration))''',
'''            if (locomotion_required && (high_energy_stall || inefficient_vibration))''')
replace("src/simulation.cpp",
'''            const bool idle_window = stage_requires_forward_gait(course_stage_)
                && elapsed_seconds_ > rolling_gate_warmup_end_seconds''',
'''            const bool idle_window = locomotion_required
                && elapsed_seconds_ > rolling_gate_warmup_end_seconds''')
replace("src/simulation.cpp",
'''        const float run_reward = stage_requires_forward_gait(course_stage_)
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;''',
'''        const float run_reward = locomotion_required
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;''')

replace("src/simulation.cpp",
'''        case CourseStage::duck_press:
            last_reward_ = std::max(0.0f, upright) * 0.016f
                + contact * 0.0015f + duck_reward + obstacle_duck_reward
                + press_contact_reward + pass_reward
                - std::abs(forward_speed_) * 0.0030f
                - action_energy * 0.0009f - torso_swing_penalty
                - premature_duck_penalty - body_contact_penalty;
            break;''',
'''        case CourseStage::duck_press:
            if (!duck_press_completed_)
            {
                last_reward_ = std::max(0.0f, upright) * 0.016f
                    + contact * 0.0015f + duck_reward + obstacle_duck_reward
                    + press_contact_reward + pass_reward
                    - std::abs(forward_speed_) * 0.0030f
                    - action_energy * 0.0009f - torso_swing_penalty
                    - premature_duck_penalty - body_contact_penalty;
            }
            else
            {
                const float maintained_crouch = duck_active_ && !non_foot_grounded_
                    ? 0.030f : -0.050f;
                last_reward_ = forward_gait_reward + maintained_crouch
                    + std::max(0.0f, upright) * 0.010f
                    + duck_reward * 1.25f + obstacle_duck_reward
                    + swing_reward + run_reward + real_step_reward
                    + obstacle_lift_reward + pass_reward
                    - backward_penalty - unearned_progress_penalty
                    - double_support_shuffle_penalty - action_energy * 0.0010f
                    - action_change_penalty - collision_penalty - knee_first_penalty
                    - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                    - body_contact_penalty * 2.0f - torso_swing_penalty;
            }
            break;''')
replace("src/simulation.cpp",
'''        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : course_stage_ == CourseStage::duck_press || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars ? 20.0f''',
'''        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : course_stage_ == CourseStage::duck_press ? 36.0f
            : course_stage_ == CourseStage::ramps || course_stage_ == CourseStage::duck_bars ? 20.0f''')

replace("src/ppo.hpp",
'''        const float pressure = environment.duck_obstacle_weight();
        action[0] = clamp(action[0] - 0.30f * pressure, -0.70f, 0.70f);
        action[1] = clamp(action[1] + 0.62f * pressure, -0.82f, 0.82f);
        action[2] = clamp(action[2] + 0.30f * pressure, -0.70f, 0.70f);
        action[3] = clamp(action[3] - 0.62f * pressure, -0.82f, 0.82f);''',
'''        const float pressure = environment.duck_press_completed()
            ? std::max(0.72f, environment.duck_obstacle_weight())
            : environment.duck_obstacle_weight();
        if (!environment.duck_press_completed())
        {
            action[0] = clamp(action[0] - 0.30f * pressure, -0.70f, 0.70f);
            action[1] = clamp(action[1] + 0.62f * pressure, -0.82f, 0.82f);
            action[2] = clamp(action[2] + 0.30f * pressure, -0.70f, 0.70f);
            action[3] = clamp(action[3] - 0.62f * pressure, -0.82f, 0.82f);
        }
        else
        {
            const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.05f;
            const float swing = std::sin(phase);
            action[0] = clamp(action[0] - 0.24f * pressure + 0.34f * swing, -0.82f, 0.82f);
            action[1] = clamp(action[1] + 0.50f * pressure + 0.34f * std::max(0.0f, swing), -0.90f, 0.90f);
            action[2] = clamp(action[2] + 0.24f * pressure - 0.34f * swing, -0.82f, 0.82f);
            action[3] = clamp(action[3] - 0.50f * pressure - 0.34f * std::max(0.0f, -swing), -0.90f, 0.90f);
        }''')

replace("src/ppo.hpp",
'''            if (environment.duck_recoveries() < 1u || environment.duck_seconds() < 0.50f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.obstacles_passed() < 2u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);''',
'''            if (environment.duck_recoveries() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.alternating_steps() < 4u
                || environment.crouch_walk_seconds() < 2.0f
                || environment.crouch_walk_distance() < 0.75f
                || environment.obstacles_passed() < 3u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);''')
replace("src/ppo.hpp",
'''        if (stage == sim::CourseStage::duck_press)
        {
            return environment.uprightness() >= 0.60f
                && (environment.duck_active()
                    || environment.stable_stance_seconds() >= 0.50f)
                && (environment.left_supported() || environment.right_supported());
        }''',
'''        if (stage == sim::CourseStage::duck_press)
        {
            return environment.duck_press_completed()
                && environment.duck_active()
                && !environment.non_foot_grounded()
                && environment.uprightness() >= 0.60f
                && environment.crouch_walk_seconds() >= 0.35f
                && environment.alternating_steps() >= 1u
                && (environment.left_supported() || environment.right_supported());
        }''')

replace("src/autonomy_curriculum.cpp",
'''        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 1.0f
                && metrics.evaluation_stable_stance >= 1.0f
                && metrics.evaluation_duck_seconds >= 1.0f
                && metrics.evaluation_obstacles_passed >= 1.0f
                && metrics.evaluation_survival >= 8.0f;''',
'''        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 1.0f
                && metrics.evaluation_stride_events >= 4.0f
                && metrics.evaluation_duck_seconds >= 2.0f
                && metrics.evaluation_distance >= 0.75f
                && metrics.evaluation_obstacles_passed >= 3.0f
                && metrics.evaluation_survival >= 12.0f;''')

replace("src/autonomy_persistence.cpp", 'output << "RUNAUTONOMY 7\\n";',
        'output << "RUNAUTONOMY 8\\n";')
replace("src/autonomy_persistence.cpp", 'magic != "RUNAUTONOMY" || version != 7',
        'magic != "RUNAUTONOMY" || version != 8')
replace("src/autonomy_commands.cpp", "NO V0.7.4 AUTOSAVE FOUND", "NO V0.7.5 AUTOSAVE FOUND")
replace("src/autonomy_commands.cpp", "V0.7.4 AUTOSAVE RESUMED", "V0.7.5 AUTOSAVE RESUMED")
replace("src/app.cpp", "runner-v074-autosave.eppo", "runner-v075-autosave.eppo")
replace("src/app.cpp", "runner-v074-evolved.rig", "runner-v075-evolved.rig")
replace("src/app.cpp", "runner-v074-autonomy.state", "runner-v075-autonomy.state")

Path(__file__).unlink()
print("materialized v0.7.5 foot-only crouch-walk core")
