#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_between(text: str, start: str, end: str, replacement: str, label: str) -> str:
    first = text.find(start)
    if first < 0:
        raise RuntimeError(f"{label}: start marker not found")
    last = text.find(end, first)
    if last < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:first] + replacement + text[last:]


def patch_ppo_header() -> None:
    path = "src/ppo.hpp"
    text = read(path)
    text = replace_once(text, '#include "simulation.hpp"\n',
        '#include "simulation.hpp"\n#include "locomotion_strategy.hpp"\n',
        "ppo strategy include")
    text = replace_once(text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1801u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1901u;",
        "training semantics")

    helper = r'''    [[nodiscard]] inline locomotion::Signals locomotion_signals(
        const sim::Environment& environment) noexcept
    {
        locomotion::Signals signals{};
        const auto& rig = environment.blueprint();
        const auto particles = environment.particles();
        if (!rig.valid() || particles.empty() || rig.root_node >= particles.size()
            || rig.left_contact_node >= particles.size()
            || rig.right_contact_node >= particles.size())
            return signals;

        const sim::Vec2 root = particles[rig.root_node].position;
        const float ground = environment.ground_height_at(root.x);
        signals.uprightness = environment.uprightness();
        signals.root_x = root.x;
        signals.left_support_x = particles[rig.left_contact_node].position.x;
        signals.right_support_x = particles[rig.right_contact_node].position.x;
        signals.left_supported = environment.left_supported();
        signals.right_supported = environment.right_supported();
        signals.near_rise = environment.ground_height_at(root.x + 0.65f) - ground;
        signals.mid_rise = environment.ground_height_at(root.x + 1.50f) - ground;
        signals.far_rise = environment.ground_height_at(root.x + 3.00f) - ground;
        signals.slope = environment.terrain().slope_at(
            sim::terrain_sample_x(root.x, environment.course_progress()));
        signals.forward_speed = environment.forward_speed();
        signals.recovering = environment.recovering();
        signals.non_foot_grounded = environment.non_foot_grounded();
        signals.burial_depth = environment.burial_depth();
        signals.obstruction_mask = environment.obstruction_mask();
        signals.free_space_direction = environment.free_space_direction();
        signals.incoming_velocity_x = environment.incoming_material_velocity().x;
        signals.incoming_time_to_impact = environment.incoming_time_to_impact();
        signals.incoming_density = environment.incoming_material_density();
        signals.gait_cycles = environment.gait_cycles();

        for (const sim::CourseFeature& feature : environment.course_features())
        {
            if (feature.kind != sim::CourseFeatureKind::moving_hazard
                && feature.kind != sim::CourseFeatureKind::projectile)
                continue;
            const float dx = feature.center.x - root.x;
            const float relative_velocity = feature.velocity.x
                - environment.forward_speed();
            const float closing_speed = dx > 0.0f
                ? std::max(0.0f, -relative_velocity)
                : std::max(0.0f, relative_velocity);
            if (closing_speed <= 0.05f)
                continue;
            const float time = std::abs(dx) / closing_speed;
            if (time < signals.incoming_time_to_impact)
            {
                signals.incoming_time_to_impact = time;
                signals.incoming_velocity_x = relative_velocity;
                signals.incoming_density = feature.kind == sim::CourseFeatureKind::moving_hazard
                    ? 0.90f : 0.65f;
            }
        }
        return signals;
    }

    [[nodiscard]] inline locomotion::Plan current_locomotion_plan(
        const sim::Environment& environment) noexcept
    {
        return locomotion::plan(locomotion_signals(environment));
    }

'''
    marker = '    [[nodiscard]] inline std::array<float, sim::action_count> walking_teacher_action('
    text = replace_once(text, marker, helper + marker, "locomotion signal helper")

    walking = r'''    [[nodiscard]] inline std::array<float, sim::action_count> walking_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.paired_leg_chains())
            return action;

        const locomotion::Plan movement = current_locomotion_plan(environment);
        const float phase = environment.elapsed_seconds() * 2.0f * pi
            * movement.cadence_hz;
        const float swing = std::sin(phase);
        const float directed_swing = swing * movement.direction;
        const float left_lift = std::max(0.0f, swing);
        const float right_lift = std::max(0.0f, -swing);
        const float span_brake = clamp(
            (environment.primary_support_span_ratio() - 1.08f) * 0.52f,
            0.0f, 0.34f);

        if (movement.intent == locomotion::Intent::crawl)
        {
            action[0] = clamp(action[0] - 0.24f + 0.20f * directed_swing,
                -0.82f, 0.82f);
            action[1] = clamp(action[1] + 0.58f + 0.18f * left_lift,
                -0.90f, 0.90f);
            action[2] = clamp(action[2] + 0.24f - 0.20f * directed_swing,
                -0.82f, 0.82f);
            action[3] = clamp(action[3] - 0.58f - 0.18f * right_lift,
                -0.90f, 0.90f);
            if (rig.active_motor_count >= 8u)
            {
                action[4] = clamp(action[4] - 0.34f * directed_swing, -0.70f, 0.70f);
                action[5] = clamp(action[5] + 0.18f * left_lift, -0.55f, 0.55f);
                action[6] = clamp(action[6] + 0.34f * directed_swing, -0.70f, 0.70f);
                action[7] = clamp(action[7] - 0.18f * right_lift, -0.55f, 0.55f);
            }
            return bilateral_joint_synergy_action(environment, action,
                sim::CourseStage::moving_hazards);
        }

        const float hip_amplitude = 0.34f + movement.stride_scale * 0.34f;
        const float knee_amplitude = 0.24f + movement.swing_lift * 0.48f;
        const float stance_extension = movement.stance_extension * 0.18f;
        action[0] = clamp(action[0] + hip_amplitude * directed_swing - span_brake,
            -0.92f, 0.92f);
        action[1] = clamp(action[1] + knee_amplitude * left_lift
            - 0.16f * right_lift, -0.94f, 0.94f);
        action[2] = clamp(action[2] - hip_amplitude * directed_swing + span_brake,
            -0.92f, 0.92f);
        action[3] = clamp(action[3] - knee_amplitude * right_lift
            + 0.16f * left_lift, -0.94f, 0.94f);

        if (environment.left_supported() && !environment.right_supported())
            action[1] = clamp(action[1] - stance_extension, -0.94f, 0.94f);
        if (environment.right_supported() && !environment.left_supported())
            action[3] = clamp(action[3] + stance_extension, -0.94f, 0.94f);

        if (movement.brake)
        {
            action[0] *= 0.72f;
            action[2] *= 0.72f;
        }
        if (rig.active_motor_count >= 8u)
        {
            action[4] = clamp(action[4] - 0.10f * directed_swing, -0.55f, 0.55f);
            action[6] = clamp(action[6] + 0.10f * directed_swing, -0.55f, 0.55f);
        }
        return bilateral_joint_synergy_action(environment, action,
            environment.course_stage());
    }

'''
    text = replace_between(text,
        '    [[nodiscard]] inline std::array<float, sim::action_count> walking_teacher_action(',
        '    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(',
        walking,
        "terrain-aware walking teacher")

    old_uneven = r'''        else if (stage == sim::CourseStage::uneven)
        {
            const auto teacher = walking_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.34f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.42f);
        }
'''
    new_uneven = r'''        else if (stage == sim::CourseStage::uneven)
        {
            const auto teacher = walking_teacher_action(environment);
            const locomotion::Plan movement = current_locomotion_plan(environment);
            const float leg_assist = movement.intent == locomotion::Intent::recover
                ? 0.68f : movement.step_up ? 0.56f : 0.34f;
            const float body_assist = movement.intent == locomotion::Intent::recover
                ? 0.60f : 0.42f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], body_assist);
        }
'''
    text = replace_once(text, old_uneven, new_uneven, "uneven adaptive assist")

    old_ramps = r'''        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.26f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.88f);
        }
'''
    new_ramps = old_ramps + r'''        else if (stage == sim::CourseStage::hurdles
            || stage == sim::CourseStage::moving_hazards)
        {
            const auto teacher = walking_teacher_action(environment);
            const locomotion::Plan movement = current_locomotion_plan(environment);
            const float leg_assist = movement.intent == locomotion::Intent::crawl
                ? 0.78f : movement.intent == locomotion::Intent::flee
                    ? 0.52f : movement.intent == locomotion::Intent::recover
                        ? 0.62f : movement.step_up ? 0.48f : 0.24f;
            const float body_assist = movement.intent == locomotion::Intent::crawl
                ? 0.60f : movement.intent == locomotion::Intent::flee ? 0.34f : 0.20f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], body_assist);
        }
'''
    text = replace_once(text, old_ramps, new_ramps, "mixed terrain assist")

    text = replace_once(text,
        '        PolicyNetwork policy_{};\n        AdamState adam_{};',
        '        PolicyNetwork policy_{};\n        PolicyNetwork preview_policy_{};\n        AdamState adam_{};',
        "preview champion network")
    text = replace_once(text,
        '        std::uint64_t random_state_{ 0x12345678ABCDEFu };\n',
        '        std::uint64_t random_state_{ 0x12345678ABCDEFu };\n'
        '        std::uint64_t preview_reset_sequence_{};\n',
        "preview seed sequence")
    write(path, text)


def patch_ppo_trainer() -> None:
    path = "src/ppo_trainer.cpp"
    text = read(path)
    bootstrap = r'''        [[nodiscard]] std::array<float, sim::action_count> skill_bootstrap_action(
            const sim::Environment& environment, sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
                return balance_teacher_action(environment);
            if (stage == sim::CourseStage::duck_press)
                return duck_teacher_action(environment);
            if (stage == sim::CourseStage::crouch_walk)
                return crouch_walk_teacher_action(environment);
            if (sim::stage_requires_forward_gait(stage))
                return walking_teacher_action(environment);
            return balance_teacher_action(environment);
        }
'''
    text = replace_between(text,
        '        [[nodiscard]] std::array<float, sim::action_count> skill_bootstrap_action(',
        '    }\n\n    PpoTrainer::PpoTrainer',
        bootstrap + '    }\n\n    PpoTrainer::PpoTrainer',
        "strategy bootstrap")
    text = replace_once(text,
        ': blueprint_(blueprint), preview_(blueprint, 0xDEADBEEFu), policy_(0xC0FFEEu)',
        ': blueprint_(blueprint), preview_(blueprint, 0xDEADBEEFu), policy_(0xC0FFEEu),\n'
        '              preview_policy_(0xBEEFBEEFu)',
        "preview policy initialization")
    old_probe = r'''                const MotorDiscoveryProbe probe = motor_discovery_probe(
                    environment, environment_index, metrics_.update, step);
'''
    new_probe = r'''                const MotorDiscoveryProbe probe = course_stage_ == sim::CourseStage::balance
                    ? motor_discovery_probe(environment, environment_index, metrics_.update, step)
                    : MotorDiscoveryProbe{};
'''
    text = replace_once(text, old_probe, new_probe, "motor discovery stage isolation")
    text = replace_once(text,
        '        policy_ = PolicyNetwork(seed);\n        reset_training_state();',
        '        policy_ = PolicyNetwork(seed);\n'
        '        preview_policy_.parameters() = policy_.parameters();\n'
        '        reset_training_state();',
        "reset preview policy")
    text = replace_once(text,
        '        policy_.parameters() = best_parameters_;\n        adam_.first_moment.assign',
        '        policy_.parameters() = best_parameters_;\n'
        '        preview_policy_.parameters() = best_parameters_;\n'
        '        adam_.first_moment.assign',
        "restore champion preview")
    preview = r'''    void PpoTrainer::step_preview(float dt)
    {
        const PolicyNetwork& display_policy = best_parameters_.empty()
            ? policy_ : preview_policy_;
        const auto raw_action = display_policy.deterministic_action(preview_.observation());
        const auto action = effective_policy_action(preview_, raw_action, course_stage_);
        if (preview_.step(action, dt).terminated)
        {
            ++preview_reset_sequence_;
            preview_.reset(0xDEADBEEFu + metrics_.update
                + preview_reset_sequence_ * 7919u);
        }
    }

    void PpoTrainer::reset_preview(std::uint64_t seed) noexcept
    {
        preview_reset_sequence_ = 0u;
        preview_.reset(seed);
    }

'''
    text = replace_between(text,
        '    void PpoTrainer::step_preview(float dt)',
        '    void PpoTrainer::append_history(',
        preview,
        "champion preview playback")
    write(path, text)


def patch_ppo_parallel() -> None:
    path = "src/ppo_parallel.cpp"
    text = read(path)
    text = replace_once(text,
        '''            case sim::CourseStage::uneven:\n                metrics_.evaluation_score = metrics_.evaluation_reward\n                    + metrics_.evaluation_distance * 0.75f\n                    + metrics_.evaluation_stride_events * 0.04f\n                    + metrics_.evaluation_speed * 0.12f;''',
        '''            case sim::CourseStage::uneven:\n                metrics_.evaluation_score = metrics_.evaluation_reward\n                    + metrics_.evaluation_distance * 0.70f\n                    + metrics_.evaluation_stride_events * 0.06f\n                    + metrics_.evaluation_survival * 0.08f\n                    + metrics_.evaluation_speed * 0.04f;''',
        "walk evaluation control bias")
    text = replace_once(text,
        '            policy_.parameters() = best_parameters_;\n            adam_.first_moment.assign',
        '            policy_.parameters() = best_parameters_;\n'
        '            preview_policy_.parameters() = best_parameters_;\n'
        '            adam_.first_moment.assign',
        "regression preview sync")
    text = replace_once(text,
        '            best_parameters_ = policy_.parameters();\n            metrics_.best_evaluation_distance',
        '            best_parameters_ = policy_.parameters();\n'
        '            preview_policy_.parameters() = best_parameters_;\n'
        '            preview_reset_sequence_ = 0u;\n'
        '            preview_.reset(0xDEADBEEFu + metrics_.update);\n'
        '            metrics_.best_evaluation_distance',
        "best preview sync")
    write(path, text)


def patch_checkpoint() -> None:
    path = "src/training_checkpoint.cpp"
    text = read(path)
    text = replace_once(text,
        '        policy_.parameters() = std::move(data.parameters);\n        if (transfer_only)',
        '        policy_.parameters() = std::move(data.parameters);\n'
        '        preview_policy_.parameters() = policy_.parameters();\n'
        '        if (transfer_only)',
        "checkpoint preview policy")
    text = replace_once(text,
        '        best_parameters_ = std::move(data.best_parameters);\n        reward_history_',
        '        best_parameters_ = std::move(data.best_parameters);\n'
        '        preview_policy_.parameters() = best_parameters_.empty()\n'
        '            ? policy_.parameters() : best_parameters_;\n'
        '        reward_history_',
        "checkpoint best preview")
    write(path, text)


def patch_simulation() -> None:
    path = "src/simulation.cpp"
    text = read(path)
    text = replace_once(text, '#include "simulation.hpp"\n',
        '#include "simulation.hpp"\n#include "locomotion_strategy.hpp"\n',
        "simulation strategy include")

    material = r'''    void Environment::update_materials(float dt) noexcept
    {
        const bool mixed_hazards = course_stage_ == CourseStage::moving_hazards;
        const bool falling_sand_lesson = course_stage_ == CourseStage::uneven
            || course_stage_ == CourseStage::hurdles || mixed_hazards;
        if (!falling_sand_lesson)
        {
            material_particles_.clear();
            return;
        }
        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float interval = mixed_hazards
            ? std::lerp(4.20f, 2.20f, course_difficulty_)
            : std::lerp(8.00f, 5.20f, course_difficulty_);
        while (elapsed_seconds_ >= next_material_event_seconds_)
        {
            ++material_event_sequence_;
            if (material_particles_.size() > 72u)
                std::erase_if(material_particles_, [](const MaterialParticle& item) { return !item.active; });
            const float spawn_x = root_x + 3.2f + random_unit() * 3.0f
                + (random_unit() - 0.5f) * 1.4f;
            if (mixed_hazards && (material_event_sequence_ % 4u) == 0u)
            {
                const MaterialKind kind = (material_event_sequence_ % 8u) == 0u
                    ? MaterialKind::rock : MaterialKind::debris;
                material_particles_.push_back({ kind,
                    { spawn_x, 5.6f + random_unit() * 2.2f },
                    { -0.55f - course_difficulty_ * 1.1f, -0.35f - random_unit() * 0.60f },
                    kind == MaterialKind::rock ? 0.23f : 0.17f,
                    kind == MaterialKind::rock ? 0.92f : 0.70f, true });
            }
            else
            {
                const std::size_t burst_count = mixed_hazards ? 10u : 6u;
                for (std::size_t index = 0; index < burst_count; ++index)
                {
                    const float spread = (static_cast<float>(index)
                        - static_cast<float>(burst_count - 1u) * 0.5f) * 0.13f;
                    material_particles_.push_back({ MaterialKind::sand,
                        { spawn_x + spread, 5.2f + random_unit() * 1.8f },
                        { -0.25f - random_unit() * 0.45f, -0.20f - random_unit() * 0.35f },
                        0.055f + random_unit() * 0.025f, 0.42f, true });
                }
            }
            next_material_event_seconds_ += interval;
        }
        const float treadmill = course_speed();
        for (MaterialParticle& item : material_particles_)
        {
            if (!item.active)
                continue;
            item.velocity.y -= 13.0f * dt;
            item.position += item.velocity * dt;
            item.position.x -= treadmill * dt;
            const float ground = ground_height_at(item.position.x);
            if (item.position.y - item.radius > ground)
                continue;
            item.position.y = ground + item.radius;
            if (item.kind == MaterialKind::sand)
            {
                terrain_.deposit(terrain_sample_x(item.position.x, course_progress()),
                    std::clamp(item.radius * item.radius * 2.8f, 0.004f, 0.025f), 0.18f);
                item.active = false;
            }
            else
            {
                item.velocity.y = std::abs(item.velocity.y) * 0.16f;
                item.velocity.x *= 0.72f;
                if (std::abs(item.velocity.x) < 0.08f && std::abs(item.velocity.y) < 0.08f)
                {
                    terrain_.deposit(terrain_sample_x(item.position.x, course_progress()),
                        item.radius * 0.12f, item.density);
                    item.active = false;
                }
            }
        }
        std::erase_if(material_particles_, [root_x](const MaterialParticle& item)
        {
            return !item.active || item.position.x < root_x - 12.0f
                || item.position.y < -3.0f || item.position.y > 18.0f;
        });
    }

'''
    text = replace_between(text,
        '    void Environment::update_materials(float dt) noexcept',
        '    void Environment::append_material_features() noexcept',
        material,
        "falling sand material lesson")
    text = replace_once(text,
        '''        if (course_stage_ != CourseStage::duck_press\n            && course_stage_ != CourseStage::crouch_walk\n            && course_stage_ != CourseStage::hurdles\n            && course_stage_ != CourseStage::moving_hazards)\n            return;''',
        '''        if (course_stage_ != CourseStage::duck_press\n            && course_stage_ != CourseStage::uneven\n            && course_stage_ != CourseStage::crouch_walk\n            && course_stage_ != CourseStage::hurdles\n            && course_stage_ != CourseStage::moving_hazards)\n            return;''',
        "uneven course features")

    old_rolling = r'''        else if (body_rolling_seconds_ > body_rolling_limit(course_stage_, elapsed_seconds_)
            || head_contact_seconds_ > head_contact_limit(elapsed_seconds_))
        {
            invalidate(InvalidMotion::body_rolling);
        }
'''
    new_rolling = r'''        else
        {
            const bool crawl_escape_window = course_stage_ == CourseStage::moving_hazards
                && non_foot_grounded_
                && (burial_depth_ > 0.08f || obstruction_mask_ != 0u)
                && std::abs(free_space_direction_) >= 0.5f;
            if ((!crawl_escape_window
                    && body_rolling_seconds_ > body_rolling_limit(course_stage_, elapsed_seconds_))
                || head_contact_seconds_ > head_contact_limit(elapsed_seconds_))
                invalidate(InvalidMotion::body_rolling);
        }
'''
    text = replace_once(text, old_rolling, new_rolling, "crawl body-contact window")
    text = replace_once(text,
        '        if (hazard_stall_seconds_ > 1.35f)\n            invalidate(InvalidMotion::hazard_quiver);',
        '''        const bool terrain_step_recovery = ground_height_at(root_x + 0.70f)
            - ground_height_at(root_x) > 0.12f;
        if (hazard_stall_seconds_ > (terrain_step_recovery ? 2.40f : 1.35f))
            invalidate(InvalidMotion::hazard_quiver);''',
        "plateau hazard patience")
    text = replace_once(text,
        '            if (locomotion_required && (high_energy_stall || inefficient_vibration))\n                micro_motion_seconds_ += progress_window_seconds_;',
        '            if (locomotion_required && !recovery_active_ && !terrain_step_recovery\n'
        '                && (high_energy_stall || inefficient_vibration))\n'
        '                micro_motion_seconds_ += progress_window_seconds_;',
        "recovery micro-motion exemption")
    text = replace_once(text,
        '                    obstacle_lift_clearance_, recovery_active_);',
        '                    obstacle_lift_clearance_, recovery_active_ || terrain_step_recovery);',
        "plateau zero-progress exemption")

    strategy_insert = r'''        locomotion::Signals motion_signals{};
        motion_signals.uprightness = upright;
        motion_signals.root_x = pelvis_position.x;
        motion_signals.left_support_x = particles_[blueprint_.left_contact_node].position.x;
        motion_signals.right_support_x = particles_[blueprint_.right_contact_node].position.x;
        motion_signals.left_supported = contact_supported(blueprint_.left_contact_node);
        motion_signals.right_supported = contact_supported(blueprint_.right_contact_node);
        motion_signals.near_rise = ground_height_at(pelvis_position.x + 0.65f) - local_ground;
        motion_signals.mid_rise = ground_height_at(pelvis_position.x + 1.50f) - local_ground;
        motion_signals.far_rise = ground_height_at(pelvis_position.x + 3.00f) - local_ground;
        motion_signals.slope = terrain_.slope_at(
            terrain_sample_x(pelvis_position.x, course_progress()));
        motion_signals.forward_speed = forward_speed_;
        motion_signals.recovering = recovery_active_;
        motion_signals.non_foot_grounded = non_foot_grounded_;
        motion_signals.burial_depth = burial_depth_;
        motion_signals.obstruction_mask = obstruction_mask_;
        motion_signals.free_space_direction = free_space_direction_;
        motion_signals.incoming_velocity_x = incoming_material_velocity_.x;
        motion_signals.incoming_time_to_impact = incoming_time_to_impact_;
        motion_signals.incoming_density = incoming_material_density_;
        motion_signals.gait_cycles = gait_cycles();
        for (const CourseFeature& feature : course_features_)
        {
            if (feature.kind != CourseFeatureKind::moving_hazard
                && feature.kind != CourseFeatureKind::projectile)
                continue;
            const float dx = feature.center.x - pelvis_position.x;
            const float relative_velocity = feature.velocity.x - forward_speed_;
            const float closing_speed = dx > 0.0f
                ? std::max(0.0f, -relative_velocity)
                : std::max(0.0f, relative_velocity);
            if (closing_speed <= 0.05f)
                continue;
            const float impact_time = std::abs(dx) / closing_speed;
            if (impact_time < motion_signals.incoming_time_to_impact)
            {
                motion_signals.incoming_time_to_impact = impact_time;
                motion_signals.incoming_velocity_x = relative_velocity;
                motion_signals.incoming_density = feature.kind == CourseFeatureKind::moving_hazard
                    ? 0.90f : 0.65f;
            }
        }
        const locomotion::Plan motion_plan = locomotion::plan(motion_signals);
        const bool emergency_crawl = course_stage_ == CourseStage::moving_hazards
            && motion_plan.emergency_crawl;
'''
    anchor = '        fallen_ = geometric_fall;\n\n        float recovery_reward = 0.0f;'
    text = replace_once(text, anchor,
        '        fallen_ = geometric_fall;\n\n' + strategy_insert
        + '\n        float recovery_reward = 0.0f;',
        "runtime locomotion plan")
    text = replace_once(text,
        '            collided_this_step_, upright, geometric_fall, hard_fall))',
        '            collided_this_step_, upright, geometric_fall, hard_fall && !emergency_crawl))',
        "crawl recovery entry")
    text = replace_once(text,
        '''            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;''',
        '''            const float prior_recovery_best = recovery_best_upright_;
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            recovery_reward += std::max(0.0f,
                recovery_best_upright_ - prior_recovery_best) * 0.18f;
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;''',
        "incremental recovery reward")
    text = replace_once(text,
        '''                recovery_active_ = false;
                ++recovery_successes_;
            }
            else if (hard_fall || recovery_time > 3.0f)''',
        '''                recovery_active_ = false;
                ++recovery_successes_;
                recovery_reward += 0.12f;
            }
            else if ((hard_fall && !emergency_crawl)
                || recovery_time > (emergency_crawl ? 5.5f : 3.0f))''',
        "extended constrained recovery")
    text = replace_once(text,
        '''        const bool terminal_fall = recovery_terminal_fall(
            geometric_fall, hard_fall, recovery_active_);''',
        '''        const bool terminal_fall = recovery_terminal_fall(
            geometric_fall, hard_fall && !emergency_crawl,
            recovery_active_ || emergency_crawl);''',
        "emergency crawl terminal gate")
    text = replace_once(text,
        '        const float safe_progress = clamp(frame_progress, -0.015f, 0.065f);',
        '        const float directed_progress = frame_progress * motion_plan.direction;\n'
        '        const float safe_progress = clamp(directed_progress, -0.015f, 0.065f);',
        "signed progress reward")
    text = replace_once(text,
        '''        const float body_contact_penalty = non_foot_grounded_
            ? (head_ground_contact() ? 0.16f : 0.08f) : 0.0f;''',
        '''        const float body_contact_penalty = non_foot_grounded_
            ? emergency_crawl
                ? (head_ground_contact() ? 0.10f : 0.012f)
                : (head_ground_contact() ? 0.16f : 0.08f)
            : 0.0f;''',
        "crawl body penalty")
    old_speed = r'''        const float target_speed = 0.90f + course_difficulty_ * 1.30f;
        const bool reward_requires_locomotion = stage_requires_forward_gait(course_stage_);
        const float run_reward = reward_requires_locomotion
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;
'''
    new_speed = r'''        const float target_speed = std::max(0.10f, motion_plan.target_speed);
        const bool reward_requires_locomotion = stage_requires_forward_gait(course_stage_);
        const float signed_speed = forward_speed_ * motion_plan.direction;
        const float speed_quality = locomotion::target_speed_reward(motion_plan, signed_speed);
        const float run_reward = reward_requires_locomotion
            ? speed_quality * (0.0025f + gait * 0.0075f) : 0.0f;
        const float balance_reward = reward_requires_locomotion
            ? motion_plan.balance_reserve * 0.010f : 0.0f;
        const float step_up_reward = motion_plan.step_up
            ? obstacle_lift_ratio * (single_support ? 0.018f : 0.005f) : 0.0f;
        const float brake_reward = motion_plan.brake
            && std::abs(signed_speed) <= target_speed * 1.15f ? 0.008f : 0.0f;
        const float controlled_overspeed_penalty = std::max(0.0f,
            std::abs(signed_speed) - std::max(0.45f, target_speed * 1.45f)) * 0.018f;
        const float crawl_escape_reward = emergency_crawl
            ? std::max(0.0f, safe_progress) * 0.80f
                + std::max(0.0f, burial_change) * 0.35f
            : 0.0f;
'''
    text = replace_once(text, old_speed, new_speed, "target-speed reward")

    text = replace_once(text,
        '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f + swing_reward + run_reward + real_step_reward''',
        '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f + swing_reward + run_reward + real_step_reward
                + balance_reward + step_up_reward + brake_reward''',
        "uneven balance rewards")
    text = replace_once(text,
        '''                - action_change_penalty - stance_slip_penalty - wheel_penalty
                - body_contact_penalty;''',
        '''                - action_change_penalty - stance_slip_penalty - wheel_penalty
                - controlled_overspeed_penalty - body_contact_penalty;''',
        "uneven controlled overspeed")
    text = replace_once(text,
        '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.011f
                + swing_reward + run_reward + real_step_reward''',
        '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.011f
                + swing_reward + run_reward + real_step_reward
                + balance_reward + step_up_reward + brake_reward''',
        "hurdle balance rewards")
    text = replace_once(text,
        '''                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;''',
        '''                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - controlled_overspeed_penalty - body_contact_penalty;''',
        "hurdle controlled overspeed")
    text = replace_once(text,
        '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.010f
                + swing_reward + run_reward + real_step_reward''',
        '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.010f
                + swing_reward + run_reward + real_step_reward
                + balance_reward + step_up_reward + brake_reward + crawl_escape_reward''',
        "mixed survival rewards")
    text = replace_once(text,
        '''                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;''',
        '''                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - controlled_overspeed_penalty - body_contact_penalty;''',
        "mixed controlled overspeed")
    write(path, text)


def patch_terrain() -> None:
    path = "src/deformable_terrain.hpp"
    text = read(path)
    old = r'''            const std::size_t macro_x = static_cast<std::size_t>(
                std::floor(local / macro_tile_size));
            bool ledge = false;
            if ((macro_x >= 10u && macro_x <= 12u)
                || (macro_x >= 47u && macro_x <= 49u))
            {
                const float ledge_height = macro_x < 20u
                    ? macro_tile_size : macro_tile_size * 2.0f;
                height += ledge_height;
                ledge = true;
            }
'''
    new = r'''            const std::size_t macro_x = static_cast<std::size_t>(
                std::floor(local / macro_tile_size));
            const std::size_t first_ledge = 10u
                + static_cast<std::size_t>(seed_ % 3u);
            const std::size_t second_ledge = 46u
                + static_cast<std::size_t>((seed_ >> 8u) % 3u);
            const bool first_plateau = macro_x >= first_ledge
                && macro_x <= first_ledge + 2u;
            const bool second_plateau = macro_x >= second_ledge
                && macro_x <= second_ledge + 2u;
            bool ledge = first_plateau || second_plateau;
            if (ledge)
            {
                const float variation = unit_hash(seed_
                    ^ (static_cast<std::uint64_t>(macro_x) * 0x9e3779b97f4a7c15ULL));
                const float ledge_height = first_plateau
                    ? 0.42f + difficulty_ * 0.38f + variation * 0.14f
                    : 0.68f + difficulty_ * 0.45f + variation * 0.20f;
                height += ledge_height;
            }
'''
    text = replace_once(text, old, new, "reachable randomized plateaus")
    write(path, text)


def patch_generator() -> None:
    old_path = ROOT / "tools/generate_v0718_sources.py"
    new_path = ROOT / "tools/generate_v0719_sources.py"
    text = old_path.read_text(encoding="utf-8")
    text = text.replace("v0718", "v0719")
    text = text.replace("V0.7.18", "V0.7.19")
    text = text.replace("v0.7.18", "v0.7.19")
    text = text.replace("0x718000u", "0x719000u")
    text = text.replace('output << "RUNAUTONOMY 14\\n";',
        'output << "RUNAUTONOMY 15\\n";')
    text = text.replace("version != 14", "version != 15")
    new_path.write_text(text, encoding="utf-8", newline="\n")
    old_path.unlink()


def patch_cmake() -> None:
    path = "CMakeLists.txt"
    text = read(path)
    text = text.replace("project(Runner VERSION 0.7.18 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.19 LANGUAGES CXX)")
    text = text.replace("generated-v0718", "generated-v0719")
    text = text.replace("generate_v0718_sources.py", "generate_v0719_sources.py")
    text = text.replace("Runner v0.7.18 deterministic source generation failed",
        "Runner v0.7.19 deterministic source generation failed")
    text = replace_once(text,
        '    src/ui_font.hpp src/view_camera.hpp)',
        '    src/ui_font.hpp src/view_camera.hpp src/locomotion_strategy.hpp)',
        "app strategy header")
    text = replace_once(text,
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"''',
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"''',
        "package general locomotion doc")
    text = replace_once(text,
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
        DESTINATION docs)''',
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"
        DESTINATION docs)''',
        "install general locomotion doc")
    test_block = r'''    add_executable(RunnerV0719GeneralLocomotionTests tests/v0719_general_locomotion_tests.cpp)
    target_include_directories(RunnerV0719GeneralLocomotionTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0719GeneralLocomotionTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0719GeneralLocomotionTests)
    add_test(NAME Runner.V0719GeneralLocomotion COMMAND RunnerV0719GeneralLocomotionTests)
    set_tests_properties(Runner.V0719GeneralLocomotion PROPERTIES TIMEOUT 30)

'''
    text = replace_once(text,
        '    add_executable(RunnerCoreTests tests/core_tests.cpp)\n',
        test_block + '    add_executable(RunnerCoreTests tests/core_tests.cpp)\n',
        "general locomotion test target")
    write(path, text)


def patch_readme_and_changelog() -> None:
    path = "README.md"
    text = read(path)
    text = text.replace("Runner 0.7.18 is", "Runner 0.7.19 is", 1)
    text = text.replace("Python 3 for deterministic v0.7.18 source generation",
        "Python 3 for deterministic v0.7.19 source generation")
    text = replace_once(text,
        '- [`docs/RUNNER_V0718_RUNTIME_RECOVERY.md`](docs/RUNNER_V0718_RUNTIME_RECOVERY.md) documents the update-loop, marker, control, telemetry, skin, and walking recovery.\n',
        '- [`docs/RUNNER_V0718_RUNTIME_RECOVERY.md`](docs/RUNNER_V0718_RUNTIME_RECOVERY.md) documents the update-loop, marker, control, telemetry, skin, and walking recovery.\n'
        '- [`docs/RUNNER_V0719_GENERAL_LOCOMOTION.md`](docs/RUNNER_V0719_GENERAL_LOCOMOTION.md) documents balance reserve, terrain adaptation, running, reversal, flee behavior, and emergency recovery.\n',
        "README v0719 doc")
    section = '''## v0.7.19 general locomotion\n\n- Uses a reusable material-independent locomotion strategy shared by PPO bootstrap and reward targeting.\n- Values balance reserve and controlled support transfer before raw speed.\n- Slows, lifts, loads, and levers over reachable ledges and plateaus instead of repeatedly striking them at fixed cadence.\n- Gates running behind established walking, clear terrain, and adequate balance reserve; braking is rewarded before difficult terrain.\n- Trains signed-direction reversal and flee behavior for imminent moving or thrown threats.\n- Allows crawling only as an obstructed/buried emergency escape and never counts it as upright Walk/Run mastery.\n- Adds low-rate falling sand to general deformable-terrain lessons; deposited sand changes the terrain through the same live SandHybrid bridge.\n- The large preview follows the best validated champion when available and varies deterministic restart seeds instead of replaying one failing two-step episode forever.\n- Restricts motor-discovery probes to the Balance nursery so they no longer overwrite early Walk actions for hundreds of updates.\n\n'''
    text = replace_once(text, '## v0.7.18 runtime recovery\n',
        section + '## v0.7.18 runtime recovery\n', "README v0719 section")
    write(path, text)

    changelog = read("CHANGELOG.md")
    if not changelog.startswith("## 0.7.19"):
        prefix = '''## 0.7.19\n\n- Added balance-reserve-aware general locomotion planning for walk, run, recover, crawl, and flee behavior.\n- Added terrain-aware slowdown, swing lift, stance extension, and braking for reachable plateaus and step-ups.\n- Restricted anatomy motor-discovery probes to Balance so Walk training is no longer overwritten by discovery pulses.\n- Added signed-direction threat escape and emergency crawl-with-return-to-stand constraints.\n- Added falling/depositing sand to deformable Walk/Hurdle lessons while keeping the locomotion strategy material-independent.\n- Reduced and deterministically randomized authored structural plateau heights into reachable training ranges.\n- Changed the Live preview to display the retained champion when available and vary restart seeds after failures.\n- Bumped training/autonomy semantics and isolated v0.7.19 runtime state.\n\n'''
        write("CHANGELOG.md", prefix + changelog)


def patch_repository_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = read(path)
    text = text.replace("tools/generate_v0718_sources.py",
        "tools/generate_v0719_sources.py")
    text = replace_once(text,
        '        tests/v0718_runtime_recovery_tests.cpp\n',
        '        tests/v0718_runtime_recovery_tests.cpp\n'
        '        tests/v0719_general_locomotion_tests.cpp\n'
        '        docs/RUNNER_V0719_GENERAL_LOCOMOTION.md\n'
        '        src/locomotion_strategy.hpp\n',
        "audit v0719 files")
    text = text.replace('"project(Runner VERSION 0.7.18 LANGUAGES CXX)"',
        '"project(Runner VERSION 0.7.19 LANGUAGES CXX)"')
    text = text.replace('"generate_v0718_sources.py"', '"generate_v0719_sources.py"')
    text = text.replace('"RunnerV0718RuntimeRecoveryTests"',
        '"RunnerV0718RuntimeRecoveryTests"\n        "RunnerV0719GeneralLocomotionTests"')
    text = text.replace('"RUNNER_V0718_RUNTIME_RECOVERY.md")',
        '"RUNNER_V0718_RUNTIME_RECOVERY.md"\n        "RUNNER_V0719_GENERAL_LOCOMOTION.md")')
    text = replace_once(text,
        '        "Runner v0.7.19 equipment, carry, and target curriculum")',
        '        "WALK-BALANCE-RESERVE-233"\n'
        '        "WALK-GENERAL-TEST-249"\n'
        '        "WALK-RELEASE-252"\n'
        '        "Runner v0.7.20 equipment, carry, and target curriculum")',
        "audit mission contracts")
    text = text.replace('file(READ "${RUNNER_SOURCE_DIR}/tools/generate_v0718_sources.py" generator_text)',
        'file(READ "${RUNNER_SOURCE_DIR}/tools/generate_v0719_sources.py" generator_text)')
    text = text.replace('runner-v0718-runtime-autosave.eppo',
        'runner-v0719-general-autosave.eppo')
    text = text.replace('"Runner v0.7.18 repository hygiene passed"',
        '"Runner v0.7.19 repository hygiene passed"')
    text = text.replace('tools/v0718.trigger', 'tools/v0719.trigger')
    text = text.replace('tools/v0718.prtrigger', 'tools/v0719.prtrigger')
    text = text.replace('tools/v0718-executor-merge-trigger.txt', 'tools/v0719-executor-merge-trigger.txt')
    text = text.replace('tools/apply_v0718_runtime_recovery.py', 'tools/apply_v0719_general_locomotion.py')
    text = text.replace('tools/finalize_v0718_runtime_recovery.py', 'tools/finalize_v0719_general_locomotion.py')
    text = text.replace('.github/workflows/apply-v0718-runtime-recovery.yml',
        '.github/workflows/apply-v0719-general-locomotion.yml')
    # The executor removes itself before permanent validation; audit should reject leftovers.
    write(path, text)


def patch_pr_validation() -> None:
    path = ".github/workflows/runner-pr-validation.yml"
    text = read(path)
    text = text.replace("0.7.18", "0.7.19")
    text = text.replace("WALK-COURSE-FRAME-226", "WALK-BALANCE-RESERVE-233")
    text = text.replace("training_semantics_version = 0x0007'1801u",
        "training_semantics_version = 0x0007'1901u")
    text = replace_once(text,
        '          grep -F "WALK-BALANCE-RESERVE-233" missioncache.md\n',
        '          grep -F "WALK-BALANCE-RESERVE-233" missioncache.md\n'
        '          grep -F "WALK-RELEASE-252" missioncache.md\n'
        '          test -f src/locomotion_strategy.hpp\n'
        '          test -f tests/v0719_general_locomotion_tests.cpp\n',
        "PR v0719 audit")
    write(path, text)


def patch_release_workflow() -> None:
    old_path = ROOT / ".github/workflows/runner-v0717-release.yml"
    new_path = ROOT / ".github/workflows/runner-v0719-release.yml"
    text = old_path.read_text(encoding="utf-8")
    text = text.replace("Runner v0.7.18", "Runner v0.7.19")
    text = text.replace("0.7.18", "0.7.19")
    text = text.replace("v0.7.18", "v0.7.19")
    text = text.replace("v0718", "v0719")
    text = text.replace("V0718", "V0719")
    text = text.replace("WALK-RUNTIME-RESET-211", "WALK-BALANCE-RESERVE-233")
    text = text.replace("WALK-RELEASE-225", "WALK-RELEASE-252")
    text = text.replace("WALK-COURSE-FRAME-226", "WALK-PLATEAU-LEVER-234")
    text = text.replace("WALK-COORDINATE-TEST-232", "WALK-GENERAL-TEST-249")
    text = text.replace("Runner v0.7.19 equipment, carry, and target curriculum",
        "Runner v0.7.20 equipment, carry, and target curriculum")
    text = text.replace("range(211, 233)", "range(233, 253)")
    text = text.replace("tests/v0718_runtime_recovery_tests.cpp",
        "tests/v0719_general_locomotion_tests.cpp")
    text = text.replace("tools/generate_v0718_sources.py",
        "tools/generate_v0719_sources.py")
    text = text.replace("docs/RUNNER_V0718_RUNTIME_RECOVERY.md",
        "docs/RUNNER_V0719_GENERAL_LOCOMOTION.md")
    text = text.replace("runner-v0718-optional-backup", "runner-v0719-optional-backup")
    text = text.replace("runner-v0718-extracted", "runner-v0719-extracted")

    # Put new immutable evidence before the previous v0.7.18 record.
    evidence_pattern = re.compile(
        r"text = text\.replace\('## Runner v0\.7\.17\\n', '## Runner v0\.7\.19\\n\\n\*\*Status:\*\* PUBLISHED\.\\n' \+ evidence \+ '\\n## Runner v0\.7\.17\\n', 1\)")
    text, count = evidence_pattern.subn(
        "text = text.replace('## Runner v0.7.18\\n', '## Runner v0.7.19\\n\\n**Status:** PUBLISHED.\\n' + evidence + '\\n## Runner v0.7.18\\n', 1)",
        text)
    if count != 1:
        raise RuntimeError(f"release evidence insertion: expected one match, found {count}")

    notes_start = "          cat > release-notes.md <<'NOTES'\n"
    notes_end = "\n          NOTES\n"
    notes = """          cat > release-notes.md <<'NOTES'
          Runner v0.7.19 is the general-locomotion and game-AI transfer release.

          - Fixes the visible two-step preview loop by replaying the retained validated champion when one exists and varying deterministic restart seeds.
          - Restricts motor-discovery probes to Balance so early Walk actions are no longer overwritten for hundreds of updates.
          - Adds reusable balance-reserve and terrain-demand planning for hold, walk, run, recover, crawl, and flee behavior.
          - Teaches deliberate slowdown, swing lift, stance loading/extension, and braking for reachable ledges and plateaus.
          - Gates running behind established gait, clear terrain, and adequate balance reserve.
          - Adds signed-direction reversal and threat escape behavior for moving and thrown hazards.
          - Allows crawling only as an obstructed/buried emergency escape; crawl never qualifies upright Walk/Run mastery.
          - Adds low-rate falling/depositing sand to deformable Walk/Hurdle lessons while keeping the locomotion strategy material-independent.
          - Reduces and deterministically randomizes structural plateau heights into reachable training ranges.
          - Bumps v0.7.19 training/autonomy semantics and isolates runtime state.
          NOTES
"""
    text = replace_between(text, notes_start, notes_end,
        notes, "release notes")

    # Exact package files for the new focused document while retaining historical docs.
    text = text.replace(
        "'docs/RUNNER_V0716_CAMERA_BATCH.md','docs/RUNNER_V0717_EYE_TEST_CORRECTION.md',\n            'docs/RUNNER_V0719_GENERAL_LOCOMOTION.md'))",
        "'docs/RUNNER_V0716_CAMERA_BATCH.md','docs/RUNNER_V0717_EYE_TEST_CORRECTION.md',\n            'docs/RUNNER_V0718_RUNTIME_RECOVERY.md','docs/RUNNER_V0719_GENERAL_LOCOMOTION.md'))")

    # Clean stale v0.7.18 lanes and this completed branch after publication.
    cleanup_start = '          branches=(\n'
    cleanup_end = '          )\n          for branch in "${branches[@]}"; do\n'
    cleanup = '''          branches=(
            'agent/v0718-publisher-watch'
            'agent/v0718-release'
            'agent/v0718-release-trigger'
            'agent/v0718-release-gate-trigger'
            'agent/v0719-general-locomotion'
          )
'''
    text = replace_between(text, cleanup_start, cleanup_end,
        cleanup + '          for branch in "${branches[@]}"; do\n',
        "release branch cleanup")
    text = text.replace("for pr in 59 61 62 67; do", "for pr in 64 68; do")

    new_path.write_text(text, encoding="utf-8", newline="\n")
    old_path.unlink()


def patch_runtime_generator_state_paths() -> None:
    path = "tools/generate_v0719_sources.py"
    text = read(path)
    text = text.replace("runner-v0719-runtime-autosave.eppo",
        "runner-v0719-general-autosave.eppo")
    text = text.replace("runner-v0719-runtime-evolved.rig",
        "runner-v0719-general-evolved.rig")
    text = text.replace("runner-v0719-runtime-autonomy.state",
        "runner-v0719-general-autonomy.state")
    write(path, text)


def main() -> int:
    patch_ppo_header()
    patch_ppo_trainer()
    patch_ppo_parallel()
    patch_checkpoint()
    patch_simulation()
    patch_terrain()
    patch_generator()
    patch_runtime_generator_state_paths()
    patch_cmake()
    patch_readme_and_changelog()
    patch_repository_audit()
    patch_pr_validation()
    patch_release_workflow()
    print("Runner v0.7.19 general locomotion source refinement applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
