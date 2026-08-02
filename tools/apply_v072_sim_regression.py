from __future__ import annotations

from pathlib import Path
import re


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


def replace_regex(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"expected one {label} match, found {count}")
    return updated


def set_mission_status(text: str, mission_id: str, status: str) -> str:
    pattern = rf"(### {re.escape(mission_id)}[^\n]*\n\*\*Status:\*\*)[^\n]*"
    updated, count = re.subn(pattern, rf"\1 {status}", text, count=1)
    if count != 1:
        raise RuntimeError(f"mission {mission_id} not found")
    return updated


def patch_simulation_header() -> None:
    path = "src/simulation.hpp"
    text = read(path)
    text = replace_once(
        text,
        "        case CourseStage::walk:\n            return duck_seconds >= 0.50f;",
        "        case CourseStage::walk:\n            return duck_seconds >= 0.50f && obstacles_passed >= 1u;",
        "duck lesson evidence",
    )
    text = replace_once(
        text,
        '        case CourseStage::walk: return "2. DUCK / RECOVER";',
        '        case CourseStage::walk: return "2. LOW BAR DUCK / RECOVER";',
        "duck lesson label",
    )
    text = replace_once(
        text,
        "    [[nodiscard]] inline bool hazard_quiver_motion(float distance_ahead, float root_speed,",
        "    [[nodiscard]] inline float duck_obstacle_approach_weight(float distance_ahead) noexcept\n"
        "    {\n"
        "        if (distance_ahead <= -1.25f || distance_ahead >= 4.0f)\n"
        "            return 0.0f;\n"
        "        if (distance_ahead <= 1.0f)\n"
        "            return 1.0f;\n"
        "        return clamp((4.0f - distance_ahead) / 3.0f, 0.0f, 1.0f);\n"
        "    }\n\n"
        "    [[nodiscard]] inline bool hazard_quiver_motion(float distance_ahead, float root_speed,",
        "duck obstacle approach helper",
    )
    text = replace_once(
        text,
        "            if (course_stage_ == CourseStage::balance\n"
        "                || course_stage_ == CourseStage::walk\n"
        "                || course_stage_ == CourseStage::ramps\n"
        "                || course_stage_ == CourseStage::duck_bars)\n"
        "                return 0.0f;\n"
        "            if (course_stage_ == CourseStage::uneven)",
        "            if (course_stage_ == CourseStage::balance\n"
        "                || course_stage_ == CourseStage::ramps\n"
        "                || course_stage_ == CourseStage::duck_bars)\n"
        "                return 0.0f;\n"
        "            if (course_stage_ == CourseStage::walk)\n"
        "                return 0.52f + course_difficulty_ * 0.28f;\n"
        "            if (course_stage_ == CourseStage::uneven)",
        "duck obstacle course speed",
    )
    text = replace_once(
        text,
        "        [[nodiscard]] bool duck_active() const noexcept { return duck_active_; }\n",
        "        [[nodiscard]] bool duck_active() const noexcept { return duck_active_; }\n"
        "        [[nodiscard]] float duck_obstacle_weight() const noexcept\n"
        "        {\n"
        "            return duck_obstacle_weight_;\n"
        "        }\n"
        "        [[nodiscard]] float duck_clearance_margin() const noexcept\n"
        "        {\n"
        "            return duck_clearance_margin_;\n"
        "        }\n",
        "duck obstacle telemetry getters",
    )
    text = replace_once(
        text,
        "        float duck_depth_{};\n        float current_duck_hold_seconds_{};",
        "        float duck_depth_{};\n"
        "        float duck_obstacle_weight_{};\n"
        "        float duck_clearance_margin_{};\n"
        "        float current_duck_hold_seconds_{};",
        "duck obstacle state",
    )
    text = replace_once(
        text,
        "        if (stage == CourseStage::ramps || stage == CourseStage::uneven)\n"
        "            return CourseFeatureKind::rock;",
        "        if (stage == CourseStage::walk)\n"
        "            return CourseFeatureKind::overhead_bar;\n"
        "        if (stage == CourseStage::ramps || stage == CourseStage::uneven)\n"
        "            return CourseFeatureKind::rock;",
        "walk stage obstacle schedule",
    )
    write(path, text)


def patch_simulation_source() -> None:
    path = "src/simulation.cpp"
    text = read(path)
    feet = r'''        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            auto add_foot = [&](std::uint16_t ankle)
            {
                std::array<std::uint16_t, 3> result{};
                if (ankle >= rig.nodes.size() || rig.nodes.size() > 122)
                    return result;

                const Vec2 ankle_position = rig.nodes[ankle];
                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.68f, 0.095f, 0.135f) : 0.11f;
                const auto foot = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ ankle_position.x + 0.055f, ankle_position.y - 0.095f });
                rig.radii.push_back(radius);
                const Vec2 foot_position = rig.nodes[foot];
                const auto heel = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ foot_position.x - heel_reach, foot_position.y - 0.012f });
                rig.radii.push_back(radius);
                const auto toe = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ foot_position.x + toe_reach, foot_position.y - 0.018f });
                rig.radii.push_back(radius);

                // The ankle remains the final articulated leg joint.  A short
                // passive ankle-to-foot link feeds a separate contact plate;
                // the lower-leg endpoint itself is never a semantic foot.
                rig.bones.push_back({ ankle, foot, 0.0f, 0.98f });
                rig.bones.push_back({ foot, heel, 0.0f, 0.96f });
                rig.bones.push_back({ foot, toe, 0.0f, 0.96f });
                rig.bones.push_back({ heel, toe, 0.0f, 0.90f });
                result = { foot, heel, toe };
                return result;
            };

            const std::uint16_t left_ankle = rig.left_contact_node;
            const std::uint16_t right_ankle = rig.right_contact_node;
            const auto left = add_foot(left_ankle);
            const auto right = add_foot(right_ankle);
            if (left[0] != 0u && right[0] != 0u)
            {
                rig.left_contact_node = left[0];
                rig.right_contact_node = right[0];
                rig.additional_left_contact_nodes = { left[1], left[2] };
                rig.additional_right_contact_nodes = { right[1], right[2] };
            }
        }

'''
    text = replace_regex(
        text,
        r"        void add_passive_feet\(CreatureBlueprint& rig, float heel_reach = 0\.20f,\n"
        r"            float toe_reach = 0\.34f\) noexcept\n"
        r"        \{.*?\n        \}\n\n(?=        void calibrate_grounded_defaults)",
        feet,
        "passive foot topology",
    )
    text = replace_once(
        text,
        "        duck_depth_ = 0.0f;\n        current_duck_hold_seconds_ = 0.0f;",
        "        duck_depth_ = 0.0f;\n"
        "        duck_obstacle_weight_ = 0.0f;\n"
        "        duck_clearance_margin_ = 0.0f;\n"
        "        current_duck_hold_seconds_ = 0.0f;",
        "duck state reset",
    )
    contact = r'''    bool Environment::contact_cluster_contains(std::uint16_t contact_node,
        std::size_t particle_index) const noexcept
    {
        if (!valid_node(contact_node) || particle_index >= particles_.size())
            return false;
        if (particle_index == contact_node)
            return true;
        const std::uint16_t candidate = static_cast<std::uint16_t>(particle_index);
        if (contact_node == blueprint_.left_contact_node)
        {
            return std::ranges::find(blueprint_.additional_left_contact_nodes,
                candidate) != blueprint_.additional_left_contact_nodes.end();
        }
        if (contact_node == blueprint_.right_contact_node)
        {
            return std::ranges::find(blueprint_.additional_right_contact_nodes,
                candidate) != blueprint_.additional_right_contact_nodes.end();
        }
        return false;
    }

'''
    text = replace_regex(
        text,
        r"    bool Environment::contact_cluster_contains\(std::uint16_t contact_node,\n"
        r"        std::size_t particle_index\) const noexcept\n"
        r"    \{.*?\n    \}\n\n(?=    bool Environment::contact_supported)",
        contact,
        "explicit semantic foot contact",
    )
    text = replace_once(
        text,
        "        if (course_stage_ != CourseStage::hurdles\n"
        "            && course_stage_ != CourseStage::moving_hazards)\n"
        "            return;",
        "        if (course_stage_ != CourseStage::walk\n"
        "            && course_stage_ != CourseStage::hurdles\n"
        "            && course_stage_ != CourseStage::moving_hazards)\n"
        "            return;",
        "walk course feature enable",
    )
    text = replace_once(
        text,
        "        const float progress = course_progress();\n"
        "        const int first_sequence = first_course_feature_sequence(root_x, progress);",
        "        const float progress = course_progress();\n"
        "        if (course_stage_ == CourseStage::walk)\n"
        "        {\n"
        "            constexpr float cycle = 7.0f;\n"
        "            float local = std::fmod(progress, cycle);\n"
        "            if (local < 0.0f)\n"
        "                local += cycle;\n"
        "            float x = root_x + 3.0f - local;\n"
        "            int sequence = course_safe_runway_markers\n"
        "                + static_cast<int>(std::floor(progress / cycle));\n"
        "            if (x < root_x - 1.40f)\n"
        "            {\n"
        "                x += cycle;\n"
        "                ++sequence;\n"
        "            }\n"
        "            const float rest_head = valid_node(blueprint_.head_node)\n"
        "                ? blueprint_.nodes[blueprint_.head_node].y : 4.0f;\n"
        "            const float clearance = std::max(1.45f,\n"
        "                rest_head - (0.82f + course_difficulty_ * 0.18f));\n"
        "            course_features_.push_back({\n"
        "                CourseFeatureKind::overhead_bar,\n"
        "                { x, clearance + 0.12f }, { 1.05f, 0.12f }, 0.0f,\n"
        "                { -course_speed(), 0.0f }, sequence\n"
        "            });\n"
        "            return;\n"
        "        }\n"
        "        const int first_sequence = first_course_feature_sequence(root_x, progress);",
        "dedicated low-bar lesson",
    )
    text = replace_once(
        text,
        "        if (duck_active_)\n            duck_seconds_ += dt;\n\n"
        "        float current_joint_speed = 0.0f;",
        "        if (duck_active_)\n"
        "            duck_seconds_ += dt;\n\n"
        "        duck_obstacle_weight_ = 0.0f;\n"
        "        duck_clearance_margin_ = 0.0f;\n"
        "        for (const CourseFeature& feature : course_features_)\n"
        "        {\n"
        "            if (feature.kind != CourseFeatureKind::overhead_bar)\n"
        "                continue;\n"
        "            const float dx = feature.center.x - root_x;\n"
        "            const float weight = duck_obstacle_approach_weight(dx);\n"
        "            if (weight <= duck_obstacle_weight_)\n"
        "                continue;\n"
        "            duck_obstacle_weight_ = weight;\n"
        "            const float bar_bottom = feature.center.y - feature.half_extent.y;\n"
        "            const float head_top = valid_node(blueprint_.head_node)\n"
        "                ? particles_[blueprint_.head_node].position.y\n"
        "                    + particles_[blueprint_.head_node].radius\n"
        "                : bar_bottom;\n"
        "            duck_clearance_margin_ = bar_bottom - head_top;\n"
        "        }\n\n"
        "        float current_joint_speed = 0.0f;",
        "duck obstacle telemetry",
    )
    text = replace_once(
        text,
        "        const float duck_reward = duck_active_\n"
        "            ? 0.018f + clamp(duck_depth_ - 0.48f, 0.0f, 0.80f) * 0.012f : 0.0f;",
        "        const float duck_reward = duck_active_\n"
        "            ? 0.018f + clamp(duck_depth_ - 0.48f, 0.0f, 0.80f) * 0.012f : 0.0f;\n"
        "        const float obstacle_duck_reward = duck_obstacle_weight_\n"
        "            * (duck_active_ ? 0.055f\n"
        "                + clamp(duck_clearance_margin_, -0.30f, 0.20f) * 0.035f\n"
        "                : -0.020f);\n"
        "        const float premature_duck_penalty = (1.0f - duck_obstacle_weight_)\n"
        "            * (duck_active_ ? 0.018f : 0.0f);",
        "obstacle-conditioned duck reward",
    )
    text = replace_once(
        text,
        "        case CourseStage::walk:\n"
        "            last_reward_ = std::max(0.0f, upright) * 0.016f\n"
        "                + contact * 0.0015f + duck_reward\n"
        "                - std::abs(forward_speed_) * 0.0030f\n"
        "                - action_energy * 0.0009f - body_contact_penalty;",
        "        case CourseStage::walk:\n"
        "            last_reward_ = std::max(0.0f, upright) * 0.016f\n"
        "                + contact * 0.0015f + duck_reward + obstacle_duck_reward\n"
        "                + pass_reward - std::abs(forward_speed_) * 0.0030f\n"
        "                - action_energy * 0.0009f - collision_penalty\n"
        "                - premature_duck_penalty - body_contact_penalty;",
        "duck lesson reward",
    )
    write(path, text)


def patch_policy_header() -> None:
    path = "src/ppo.hpp"
    text = read(path)
    text = replace_once(
        text,
        "    inline constexpr std::uint32_t training_semantics_version = 0x0007'0103u;",
        "    inline constexpr std::uint32_t training_semantics_version = 0x0007'0200u;",
        "training semantics version",
    )
    old = r'''    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        sim::CourseStage stage) noexcept
    {
        if (stage != sim::CourseStage::balance)
            return policy_action;
        const auto teacher = balance_teacher_action(environment);
        constexpr float assist = 1.00f;
        for (std::size_t index = 0; index < policy_action.size(); ++index)
            policy_action[index] = lerp(policy_action[index], teacher[index], assist);
        return policy_action;
    }
'''
    new = r'''    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> action,
        sim::CourseStage stage) noexcept
    {
        if (environment.blueprint().support_seed_count() > 2u)
            return action;

        const float pair_strength = stage == sim::CourseStage::balance
            || stage == sim::CourseStage::walk || stage == sim::CourseStage::ramps
            ? 0.72f : 0.42f;
        auto mirror_pair = [&](std::size_t left, std::size_t right, float strength)
        {
            const float mirrored = 0.5f * (action[left] - action[right]);
            action[left] = lerp(action[left], mirrored, strength);
            action[right] = lerp(action[right], -mirrored, strength);
        };
        mirror_pair(0, 2, pair_strength);
        mirror_pair(1, 3, pair_strength);
        mirror_pair(4, 6, pair_strength * 0.75f);
        mirror_pair(5, 7, pair_strength * 0.75f);

        // Preserve residual freedom, but bias each leg toward a useful hip/knee
        // chain instead of asking PPO to discover eight unrelated actuators.
        constexpr float chain_strength = 0.34f;
        const float left_chain = 0.5f * (-action[0] + action[1]);
        const float right_chain = 0.5f * (action[2] - action[3]);
        action[0] = lerp(action[0], -left_chain, chain_strength);
        action[1] = lerp(action[1], left_chain, chain_strength);
        action[2] = lerp(action[2], right_chain, chain_strength);
        action[3] = lerp(action[3], -right_chain, chain_strength);
        for (float& value : action)
            value = clamp(value, -1.0f, 1.0f);
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const auto observation = environment.observation();
        const bool overhead_bar = std::abs(observation[30]) < 0.05f
            && observation[32] > 0.01f;
        const float distance = observation[29] * 6.0f;
        const float approach = overhead_bar
            ? sim::duck_obstacle_approach_weight(distance) : 0.0f;
        action[0] = clamp(action[0] - 0.28f * approach, -0.70f, 0.70f);
        action[1] = clamp(action[1] + 0.58f * approach, -0.80f, 0.80f);
        action[2] = clamp(action[2] + 0.28f * approach, -0.70f, 0.70f);
        action[3] = clamp(action[3] - 0.58f * approach, -0.80f, 0.80f);
        action[4] = clamp(action[4] + 0.16f * approach, -0.60f, 0.60f);
        action[5] = clamp(action[5] - 0.10f * approach, -0.60f, 0.60f);
        action[6] = clamp(action[6] - 0.16f * approach, -0.60f, 0.60f);
        action[7] = clamp(action[7] + 0.10f * approach, -0.60f, 0.60f);
        return bilateral_joint_synergy_action(environment, action, sim::CourseStage::walk);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        sim::CourseStage stage) noexcept
    {
        policy_action = bilateral_joint_synergy_action(environment, policy_action, stage);
        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            constexpr float assist = 0.78f;
            for (std::size_t index = 0; index < policy_action.size(); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], assist);
        }
        else if (stage == sim::CourseStage::walk)
        {
            const auto teacher = duck_teacher_action(environment);
            const float assist = 0.48f + environment.duck_obstacle_weight() * 0.38f;
            for (std::size_t index = 0; index < policy_action.size(); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], assist);
        }
        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);
            for (std::size_t index = 0; index < policy_action.size(); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.20f);
        }
        return bilateral_joint_synergy_action(environment, policy_action, stage);
    }
'''
    text = replace_once(text, old, new, "coordinated effective controller")
    text = replace_once(
        text,
        "        case sim::CourseStage::walk:\n"
        "            if (environment.longest_stable_stance_seconds() < 2.0f\n"
        "                || environment.stable_stance_seconds() < 0.75f)\n"
        "                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);\n"
        "            if (environment.duck_recoveries() < 1u || environment.duck_seconds() < 0.50f)\n"
        "                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);",
        "        case sim::CourseStage::walk:\n"
        "            if (environment.longest_stable_stance_seconds() < 2.0f\n"
        "                || environment.stable_stance_seconds() < 0.75f)\n"
        "                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);\n"
        "            if (environment.duck_recoveries() < 1u || environment.duck_seconds() < 0.50f)\n"
        "                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);\n"
        "            if (environment.obstacles_passed() < 1u)\n"
        "                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);",
        "walk obstacle qualification",
    )
    display_helper = r'''
    [[nodiscard]] inline bool stage_display_sample_eligible(sim::CourseStage stage,
        const sim::Environment& environment) noexcept
    {
        const StageMotionQualification qualification =
            stage_motion_qualification(stage, environment);
        if (!qualification.valid || environment.non_foot_grounded())
            return false;
        if (stage == sim::CourseStage::balance)
        {
            return environment.stable_stance_seconds() >= 1.0f
                && environment.uprightness() >= 0.82f
                && (environment.left_supported() || environment.right_supported());
        }
        if (stage == sim::CourseStage::walk)
        {
            return environment.uprightness() >= 0.60f
                && (environment.duck_active()
                    || environment.stable_stance_seconds() >= 0.50f)
                && (environment.left_supported() || environment.right_supported());
        }
        return environment.valid_motion() && environment.uprightness() >= 0.45f;
    }

'''
    text = replace_once(
        text,
        "    [[nodiscard]] inline bool policy_candidate_better(std::uint64_t quality,",
        display_helper + "    [[nodiscard]] inline bool policy_candidate_better(std::uint64_t quality,",
        "display sample qualification",
    )
    write(path, text)


def patch_trainer_source() -> None:
    path = "src/ppo_trainer.cpp"
    text = read(path)
    text = replace_once(
        text,
        "            if (!sim::stage_requires_forward_gait(stage))\n"
        "                return 0.0f;",
        "            if (stage == sim::CourseStage::walk)\n"
        "            {\n"
        "                if (update < 600u)\n"
        "                    return 0.88f;\n"
        "                if (update < 3000u)\n"
        "                    return lerp(0.88f, 0.32f,\n"
        "                        static_cast<float>(update - 600u) / 2400.0f);\n"
        "                return 0.18f;\n"
        "            }\n"
        "            if (stage == sim::CourseStage::ramps\n"
        "                || stage == sim::CourseStage::duck_bars)\n"
        "                return update < 1200u ? 0.42f : 0.12f;\n"
        "            if (!sim::stage_requires_forward_gait(stage))\n"
        "                return 0.0f;",
        "skill bootstrap schedule",
    )
    text = replace_once(
        text,
        "            if (stage == sim::CourseStage::balance)\n"
        "                return balance_teacher_action(environment);\n"
        "            const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.25f;",
        "            if (stage == sim::CourseStage::balance)\n"
        "                return balance_teacher_action(environment);\n"
        "            if (stage == sim::CourseStage::walk)\n"
        "                return duck_teacher_action(environment);\n"
        "            const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.25f;",
        "duck skill bootstrap",
    )
    text = replace_once(
        text,
        "                transition.log_probability = policy_.log_probability(transition.action, evaluation);\n"
        "                const sim::StepResult result = environment.step(transition.action);",
        "                transition.action = effective_policy_action(\n"
        "                    environment, transition.action, course_stage_);\n"
        "                transition.log_probability = policy_.log_probability(transition.action, evaluation);\n"
        "                const sim::StepResult result = environment.step(transition.action);",
        "rollout coordinated controller",
    )
    write(path, text)


def patch_autonomy() -> None:
    path = "src/autonomy_persistence.cpp"
    text = read(path)
    text = text.replace('output << "EPOCHAUTONOMY 4\\n";', 'output << "EPOCHAUTONOMY 5\\n";')
    text = text.replace('version != 4', 'version != 5')
    text = replace_once(
        text,
        "            if (!qualification.valid)\n                continue;",
        "            if (!qualification.valid\n"
        "                || !stage_display_sample_eligible(stage_, environment))\n"
        "                continue;",
        "current-frame preview gate",
    )
    write(path, text)

    path = "src/autonomy_curriculum.cpp"
    text = read(path)
    text = replace_once(
        text,
        "                    const auto action = worker_.policy().deterministic_action(environment.observation());\n"
        "                    const sim::StepResult result = environment.step(action);",
        "                    const auto raw_action = worker_.policy().deterministic_action(\n"
        "                        environment.observation());\n"
        "                    const auto action = effective_policy_action(\n"
        "                        environment, raw_action, stage);\n"
        "                    const sim::StepResult result = environment.step(action);",
        "rig evaluation coordinated controller",
    )
    text = replace_once(
        text,
        "            return metrics.evaluation_duck_recoveries >= 1.0f\n"
        "                && metrics.evaluation_stable_stance >= 1.0f\n"
        "                && metrics.evaluation_duck_seconds >= 2.0f\n"
        "                && metrics.evaluation_survival >= 8.0f;",
        "            return metrics.evaluation_duck_recoveries >= 1.0f\n"
        "                && metrics.evaluation_stable_stance >= 1.0f\n"
        "                && metrics.evaluation_duck_seconds >= 1.0f\n"
        "                && metrics.evaluation_obstacles_passed >= 1.0f\n"
        "                && metrics.evaluation_survival >= 8.0f;",
        "duck mastery obstacle requirement",
    )
    write(path, text)

    path = "src/autonomy_commands.cpp"
    text = read(path).replace("V0.7.1", "V0.7.2")
    write(path, text)


def patch_app_and_launch() -> None:
    path = "src/app.cpp"
    text = read(path)
    text = text.replace("epochrunner-v071-autosave.eppo", "epochrunner-v072-autosave.eppo")
    text = text.replace("epochrunner-v071-evolved.epochrig", "epochrunner-v072-evolved.epochrig")
    text = text.replace("epochrunner-v071-autonomy.state", "epochrunner-v072-autonomy.state")
    text = text.replace('"STAGE-VALID TRAINING SAMPLE"', '"CURRENT-STAGE VERIFIED SAMPLE"')
    write(path, text)

    run_bat = r'''@echo off
setlocal EnableExtensions

rem Always operate relative to this script, not the caller's working directory.
cd /d "%~dp0"

rem In a source checkout, never let a stale root-level executable override the
rem current build.  In an extracted release, CMakeLists.txt is absent and the
rem packaged executable beside this launcher is authoritative.
if exist "%~dp0CMakeLists.txt" goto source_tree

set "EPOCHRUNNER_EXE=%~dp0EpochRunner.exe"
if exist "%EPOCHRUNNER_EXE%" goto launch

echo ERROR: The packaged EpochRunner.exe is missing beside run.bat.
goto failed

:source_tree
set "EPOCHRUNNER_EXE=%~dp0build\windows-release\Release\EpochRunner.exe"
if exist "%EPOCHRUNNER_EXE%" goto launch

echo EpochRunner has not been built yet. Building the Windows Release target...
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake is not available on PATH.
    goto failed
)

if not defined VCPKG_ROOT if exist "%USERPROFILE%\source\repos\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=%USERPROFILE%\source\repos\vcpkg"
)
if not defined VCPKG_ROOT if exist "%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
)

cmake --preset windows-release
if errorlevel 1 goto build_failed
cmake --build --preset windows-release --parallel
if errorlevel 1 goto build_failed
if not exist "%EPOCHRUNNER_EXE%" (
    echo ERROR: The build completed without producing:
    echo        %EPOCHRUNNER_EXE%
    goto failed
)

:launch
for %%I in ("%EPOCHRUNNER_EXE%") do set "EPOCHRUNNER_DIR=%%~dpI"
pushd "%EPOCHRUNNER_DIR%"
"%EPOCHRUNNER_EXE%" %*
set "EPOCHRUNNER_RESULT=%ERRORLEVEL%"
popd
if not "%EPOCHRUNNER_RESULT%"=="0" (
    echo.
    echo EpochRunner exited with code %EPOCHRUNNER_RESULT%.
    pause
)
exit /b %EPOCHRUNNER_RESULT%

:build_failed
echo.
echo ERROR: EpochRunner failed to configure or build.

:failed
echo.
pause
exit /b 1
'''
    write("run.bat", run_bat)


def patch_tests() -> None:
    path = "tests/core_tests.cpp"
    text = read(path)
    text = text.replace(
        '&& sim::course_stage_name(sim::CourseStage::walk) == "2. DUCK / RECOVER"',
        '&& sim::course_stage_name(sim::CourseStage::walk) == "2. LOW BAR DUCK / RECOVER"',
    )
    text = replace_once(
        text,
        "    require(sim::stage_skill_evidence(sim::CourseStage::walk, 0u, 0.6f, 0u, 0.0f, 0u, 0u),\n"
        "        \"duck evidence cannot complete the duck lesson\");",
        "    require(!sim::stage_skill_evidence(sim::CourseStage::walk, 0u, 0.6f, 0u, 0.0f, 0u, 0u),\n"
        "        \"duck lesson completes without clearing its low bar\");\n"
        "    require(sim::stage_skill_evidence(sim::CourseStage::walk, 0u, 0.6f, 0u, 0.0f, 0u, 1u),\n"
        "        \"duck-and-clear evidence cannot complete the duck lesson\");",
        "duck evidence tests",
    )
    text = text.replace("require(humanoid.nodes.size() >= 17,", "require(humanoid.nodes.size() >= 19,")
    text = text.replace("require(humanoid.bones.size() >= 17,", "require(humanoid.bones.size() >= 21,")
    insert = r'''    require(humanoid.left_contact_node != humanoid.motors[1].c
            && humanoid.right_contact_node != humanoid.motors[3].c,
        "semantic feet are still the lower-leg motor endpoints");
    require(humanoid.additional_left_contact_nodes.size() == 2u
            && humanoid.additional_right_contact_nodes.size() == 2u,
        "dedicated foot plates do not include heel and toe contacts");
    require(std::ranges::none_of(humanoid.motors,
            [&humanoid](const sim::MotorConstraint& motor)
            {
                return motor.c == humanoid.left_contact_node
                    || motor.c == humanoid.right_contact_node;
            }),
        "a policy motor still terminates directly on a semantic foot contact");
'''
    text = replace_once(
        text,
        "    for (std::size_t motor_index = 0; motor_index < humanoid.active_motor_count; ++motor_index)\n",
        insert + "    for (std::size_t motor_index = 0; motor_index < humanoid.active_motor_count; ++motor_index)\n",
        "dedicated foot tests",
    )
    synergy = r'''
    {
        sim::Environment duck_lesson{ humanoid, 0xD0C7u };
        duck_lesson.set_course(sim::CourseStage::walk, 0.35f);
        require(std::ranges::any_of(duck_lesson.course_features(),
                [](const sim::CourseFeature& feature)
                {
                    return feature.kind == sim::CourseFeatureKind::overhead_bar;
                }),
            "duck lesson has no explicit low-bar obstacle");
        std::array<float, sim::action_count> unrelated{
            0.9f, -0.8f, 0.2f, 0.7f, -0.9f, 0.6f, 0.1f, -0.5f
        };
        const auto coordinated = rl::bilateral_joint_synergy_action(
            duck_lesson, unrelated, sim::CourseStage::walk);
        require(std::abs(coordinated[0] + coordinated[2])
                < std::abs(unrelated[0] + unrelated[2])
            && std::abs(coordinated[1] + coordinated[3])
                < std::abs(unrelated[1] + unrelated[3]),
            "AI outputs are still eight unrelated joint commands");
        const std::array<float, sim::action_count> neutral{};
        const auto duck = rl::effective_policy_action(
            duck_lesson, neutral, sim::CourseStage::walk);
        require(duck[0] < -0.05f && duck[1] > 0.10f
                && duck[2] > 0.05f && duck[3] < -0.10f,
            "low-bar obstacle does not trigger a coordinated duck primitive");
    }

'''
    text = replace_once(
        text,
        "\n    {\n        sim::Environment observation_environment{ humanoid, 0x0B5E7u };",
        synergy + "    {\n        sim::Environment observation_environment{ humanoid, 0x0B5E7u };",
        "synergy and duck tests",
    )
    text = replace_once(
        text,
        "        require(qualification.valid,\n"
        "            \"shared balance controller cannot sustain a stage-valid physics stance\");",
        "        require(qualification.valid,\n"
        "            \"shared balance controller cannot sustain a stage-valid physics stance\");\n"
        "        require(rl::stage_display_sample_eligible(\n"
        "                sim::CourseStage::balance, assisted_stance),\n"
        "            \"valid current stance is hidden from the training sample\");\n"
        "        sim::EnvironmentTestAccess::collapse_upper_body(assisted_stance);\n"
        "        require(!rl::stage_display_sample_eligible(\n"
        "                sim::CourseStage::balance, assisted_stance),\n"
        "            \"collapsed current frame is still published as a valid sample\");",
        "training sample current-frame regression test",
    )
    write(path, text)


def patch_versions_and_ledger() -> None:
    path = "CMakeLists.txt"
    text = read(path).replace(
        "project(EpochRunner VERSION 0.7.1 LANGUAGES CXX)",
        "project(EpochRunner VERSION 0.7.2 LANGUAGES CXX)",
    )
    write(path, text)

    path = "vcpkg.json"
    text = read(path).replace('"version-semver": "0.7.1"', '"version-semver": "0.7.2"')
    write(path, text)

    notes = "# EpochRunner v0.7.2\n\n"
    notes += "- Reopens the simulation-quality missions after packaged runtime screenshots contradicted v0.7.1 validation.\n"
    notes += "- Adds coordinated bilateral joint synergies while preserving learned residual control.\n"
    notes += "- Replaces lower-leg endpoint feet with dedicated passive foot plates, heel contacts, and toe contacts.\n"
    notes += "- Restricts traction and foot support to explicit semantic foot nodes.\n"
    notes += "- Adds an obstacle-conditioned low-bar duck, clearance, pass, and return-to-stance lesson.\n"
    notes += "- Applies the same coordinated controller in PPO rollout, deterministic evaluation, rig evaluation, preview, and live execution.\n"
    notes += "- Rejects a previously qualified rollout when its current displayed frame is collapsed or unsupported.\n"
    notes += "- Bumps checkpoint and autonomy semantics so v0.7.1 behavior cannot silently resume.\n"
    notes += "- Makes source-tree run.bat prefer the current Release build instead of a stale root executable.\n"
    write("RELEASE_NOTES_v0.7.2.md", notes)

    path = "missioncache.md"
    text = read(path)
    text = text.replace("**Target:** EpochRunner v0.7.1", "**Target:** EpochRunner v0.7.2")
    text = text.replace(
        "**Release state:** VERIFIED — EpochRunner v0.7.1 published",
        "**Release state:** IMPLEMENTED — cross-platform and packaged-runtime verification pending",
    )
    active = """
## v0.7.2 packaged-runtime regression correction

Adam's August 1, 2026 screenshots contradict the v0.7.1 runtime conclusion: the displayed training sample can be collapsed while labeled valid, foot semantics are entangled with lower-leg joints, independent motor outputs obscure useful movement structure, and ducking is not learned as obstacle avoidance. Contradictory evidence reopens the affected mission IDs; no v0.7.1 success claim is used as substitute evidence.

### WALK-REG-022 — Whole-simulation regression correction
**Status:** IMPLEMENTED — REVALIDATION PENDING

Revalidate every carried-forward mission after the rig, controller, curriculum, persistence, UI sample, launch, and package corrections. Build success alone is insufficient.

### WALK-SYNERGY-023 — Coordinated joint groups with learned residuals
**Status:** IMPLEMENTED — REVALIDATION PENDING

Bilateral hips, knees, shoulders, and elbows share stage-aware movement synergies. PPO retains residual control, but rollout, evaluation, rig evaluation, preview, and live execution no longer treat eight motors as unrelated gates.

### WALK-FOOT-024 — Dedicated semantic feet below the articulated ankles
**Status:** IMPLEMENTED — REVALIDATION PENDING

Lower-leg motor endpoints are ankles, not feet. Each biped foot has a separate passive plate, heel, and toe, and only explicit semantic foot nodes receive traction or foot-support classification.

### WALK-DUCK-025 — Obstacle-conditioned duck, clear, and recover lesson
**Status:** IMPLEMENTED — REVALIDATION PENDING

The second lesson presents a moving low bar. Qualification requires lowering the head with planted semantic feet, clearing the bar, and returning to a stable stance; permanent crouching and unrelated joint motion do not qualify.

### WALK-SAMPLE-026 — Current-frame training-sample integrity
**Status:** IMPLEMENTED — REVALIDATION PENDING

A rollout that was valid earlier cannot be displayed while its current frame is collapsed, body-supported, or otherwise incompatible with the active lesson. The PIP is empty until a currently displayable stage-qualified frame exists.

### WALK-LAUNCH-027 — Reject stale source-tree executables and v0.7.1 state
**Status:** IMPLEMENTED — REVALIDATION PENDING

Source-tree `run.bat` prefers the current Release build and cannot silently launch an old root executable. v0.7.2 uses new checkpoint, autosave, rig, and autonomy-state semantics and paths.

"""
    text = replace_once(text, "## Training-quality correction\n", active + "## Training-quality correction\n", "v0.7.2 mission section")
    for mission in (
        "WALK-TRAIN-013", "WALK-CURR-014", "WALK-BEST-015", "WALK-CTRL-020",
        "WALK-STATE-016", "WALK-RUNTIME-017", "WALK-LAUNCH-021", "WALK-SKILL-008",
        "WALK-LEARN-010", "WALK-OBS-001", "WALK-PHYS-001", "WALK-ROLL-003",
        "WALK-GUIDE-006", "WALK-PIP-007",
    ):
        text = set_mission_status(text, mission, "IMPLEMENTED — REVALIDATION PENDING")
    write(path, text)


def main() -> None:
    if "VERSION 0.7.2" in read("CMakeLists.txt"):
        raise RuntimeError("v0.7.2 patch already applied")
    patch_simulation_header()
    patch_simulation_source()
    patch_policy_header()
    patch_trainer_source()
    patch_autonomy()
    patch_app_and_launch()
    patch_tests()
    patch_versions_and_ledger()
    print("EpochRunner v0.7.2 simulation regression patch applied")


if __name__ == "__main__":
    main()
