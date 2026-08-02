from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def write(relative: str, content: str) -> None:
    (ROOT / relative).write_text(content, encoding="utf-8", newline="\n")


def replace_text(relative: str, old: str, new: str, already: str | None = None) -> None:
    text = read(relative)
    if already is not None and already in text:
        return
    if old not in text:
        raise RuntimeError(f"Expected text not found in {relative}: {old[:100]!r}")
    write(relative, text.replace(old, new, 1))


def replace_regex(relative: str, pattern: str, replacement: str, already: str | None = None) -> None:
    text = read(relative)
    if already is not None and already in text:
        return
    compiled = re.compile(pattern, re.DOTALL)
    updated, count = compiled.subn(lambda _: replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"Expected regex matched {count} times in {relative}: {pattern[:100]!r}")
    write(relative, updated)


def insert_before(relative: str, needle: str, insertion: str, marker: str) -> None:
    text = read(relative)
    if marker in text:
        return
    if needle not in text:
        raise RuntimeError(f"Insertion point missing in {relative}: {needle!r}")
    write(relative, text.replace(needle, insertion + needle, 1))


if (ROOT / "RELEASE_NOTES_v0.7.3.md").exists() and "WALK-CONTROL-031" in read("missioncache.md"):
    print("v0.7.3 runtime correction already materialized")
    raise SystemExit(0)

replace_text("CMakeLists.txt", "project(EpochRunner VERSION 0.7.2", "project(EpochRunner VERSION 0.7.3", "VERSION 0.7.3")
replace_text("vcpkg.json", '"version-semver": "0.7.2"', '"version-semver": "0.7.3"', '"version-semver": "0.7.3"')
replace_text("src/ppo.hpp", "0x0007'0200u", "0x0007'0300u", "0x0007'0300u")
replace_text("src/app.cpp", "epochrunner-v072-autosave.eppo", "epochrunner-v073-autosave.eppo", "epochrunner-v073-autosave.eppo")
replace_text("src/app.cpp", "epochrunner-v072-evolved.epochrig", "epochrunner-v073-evolved.epochrig", "epochrunner-v073-evolved.epochrig")
replace_text("src/app.cpp", "epochrunner-v072-autonomy.state", "epochrunner-v073-autonomy.state", "epochrunner-v073-autonomy.state")
replace_text("src/autonomy_commands.cpp", "NO V0.7.2 AUTOSAVE FOUND", "NO V0.7.3 AUTOSAVE FOUND", "NO V0.7.3 AUTOSAVE FOUND")
replace_text("src/autonomy_commands.cpp", "V0.7.2 AUTOSAVE RESUMED ASYNCHRONOUSLY", "V0.7.3 AUTOSAVE RESUMED ASYNCHRONOUSLY", "V0.7.3 AUTOSAVE RESUMED ASYNCHRONOUSLY")
replace_text("src/autonomy_persistence.cpp", 'output << "EPOCHAUTONOMY 5\\n";', 'output << "EPOCHAUTONOMY 6\\n";', "EPOCHAUTONOMY 6")
replace_text("src/autonomy_persistence.cpp", "version != 5", "version != 6", "version != 6")

replace_regex(
    "src/ppo.hpp",
    r"    \[\[nodiscard\]\] inline std::array<float, sim::action_count> balance_teacher_action\(.*?\n    }\n\n    \[\[nodiscard\]\] inline std::array<float, sim::action_count> bilateral_joint_synergy_action",
    '''    [[nodiscard]] inline std::array<float, sim::action_count> balance_teacher_action(
        const sim::Environment& environment) noexcept
    {
        constexpr std::size_t joint_angle_begin = 4;
        constexpr std::size_t joint_velocity_begin = joint_angle_begin + sim::action_count;
        static_assert(sim::observation_count == 40);
        const auto observation = environment.observation();
        std::array<float, sim::action_count> action{};

        const std::size_t leg_count = std::min<std::size_t>(4u,
            environment.blueprint().active_motor_count);
        for (std::size_t index = 0; index < leg_count; ++index)
        {
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.12f * joint_speed, -0.30f, 0.30f);
        }
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
        {
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.035f * joint_speed, -0.08f, 0.08f);
        }

        // Plant and load the feet through the leg chains before granting the
        // arms meaningful authority. The support correction is intentionally
        // asymmetric so a missing foot is recovered instead of mirrored.
        action[0] = clamp(action[0] - 0.035f, -0.46f, 0.46f);
        action[1] = clamp(action[1] + 0.045f, -0.46f, 0.46f);
        action[2] = clamp(action[2] + 0.035f, -0.46f, 0.46f);
        action[3] = clamp(action[3] - 0.045f, -0.46f, 0.46f);
        if (!environment.left_supported())
        {
            action[0] = clamp(action[0] - 0.09f, -0.52f, 0.52f);
            action[1] = clamp(action[1] + 0.14f, -0.56f, 0.56f);
        }
        if (!environment.right_supported())
        {
            action[2] = clamp(action[2] + 0.09f, -0.52f, 0.52f);
            action[3] = clamp(action[3] - 0.14f, -0.56f, 0.56f);
        }

        const float correction = clamp(observation[0] * 0.40f
            + observation[2] * 0.07f, -0.20f, 0.20f);
        action[0] = clamp(action[0] - correction, -0.52f, 0.52f);
        action[1] = clamp(action[1] + correction * 0.20f, -0.52f, 0.52f);
        action[2] = clamp(action[2] - correction, -0.52f, 0.52f);
        action[3] = clamp(action[3] - correction * 0.20f, -0.52f, 0.52f);

        const bool feet_loaded = environment.left_supported() && environment.right_supported();
        const float arm_authority = feet_loaded
            && environment.stable_stance_seconds() >= 1.0f ? 0.20f : 0.03f;
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
            action[index] *= arm_authority;
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action''',
    "Plant and load the feet through the leg chains"
)

replace_regex(
    "src/ppo.hpp",
    r"    \[\[nodiscard\]\] inline std::array<float, sim::action_count> bilateral_joint_synergy_action\(.*?\n    }\n\n    \[\[nodiscard\]\] inline std::array<float, sim::action_count> duck_teacher_action",
    '''    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> action,
        sim::CourseStage stage) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const bool paired_leg_chains = rig.active_motor_count >= 4u
            && rig.motors[0].enabled && rig.motors[1].enabled
            && rig.motors[2].enabled && rig.motors[3].enabled
            && rig.motors[0].pivot == rig.motors[2].pivot
            && rig.motors[1].a == rig.motors[0].pivot
            && rig.motors[3].a == rig.motors[2].pivot;
        if (!paired_leg_chains)
            return action;

        const float leg_pair_strength = stage == sim::CourseStage::walk
            ? 0.34f : (stage == sim::CourseStage::balance
                || stage == sim::CourseStage::ramps ? 0.28f : 0.20f);
        auto mirror_pair = [&](std::size_t left, std::size_t right, float strength)
        {
            const float mirrored = 0.5f * (action[left] - action[right]);
            action[left] = lerp(action[left], mirrored, strength);
            action[right] = lerp(action[right], -mirrored, strength);
        };
        mirror_pair(0, 2, leg_pair_strength);
        mirror_pair(1, 3, leg_pair_strength);

        if (rig.active_motor_count >= 8u)
        {
            const float arm_pair_strength = sim::stage_allows_controlled_flips(stage)
                ? 0.24f : 0.06f;
            mirror_pair(4, 6, arm_pair_strength);
            mirror_pair(5, 7, arm_pair_strength);
        }

        // Keep a light hip/knee chain prior without forcing both legs into the
        // same folded pose. PPO retains most of the residual leg authority.
        constexpr float chain_strength = 0.14f;
        const float left_chain = 0.5f * (-action[0] + action[1]);
        const float right_chain = 0.5f * (action[2] - action[3]);
        action[0] = lerp(action[0], -left_chain, chain_strength);
        action[1] = lerp(action[1], left_chain, chain_strength);
        action[2] = lerp(action[2], right_chain, chain_strength);
        action[3] = lerp(action[3], -right_chain, chain_strength);

        if (rig.active_motor_count >= 8u
            && environment.longest_stable_stance_seconds() < 1.0f
            && !sim::stage_allows_controlled_flips(stage))
        {
            for (std::size_t index = 4; index < 8; ++index)
                action[index] *= 0.08f;
        }
        for (float& value : action)
            value = clamp(value, -1.0f, 1.0f);
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action''',
    "light hip/knee chain prior"
)

replace_regex(
    "src/ppo.hpp",
    r"    \[\[nodiscard\]\] inline std::array<float, sim::action_count> effective_policy_action\(.*?\n    }\n\n    struct TrainingMetrics",
    '''    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        sim::CourseStage stage) noexcept
    {
        const std::size_t active = environment.blueprint().active_motor_count;
        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.90f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.97f);
        }
        else if (stage == sim::CourseStage::walk)
        {
            const auto teacher = duck_teacher_action(environment);
            const auto observation = environment.observation();
            const bool overhead_bar = std::abs(observation[30]) < 0.05f
                && observation[32] > 0.01f;
            const float observed_weight = overhead_bar
                ? sim::duck_obstacle_approach_weight(observation[29] * 6.0f) : 0.0f;
            const float obstacle_weight = std::max(
                environment.duck_obstacle_weight(), observed_weight);
            const float leg_assist = 0.60f + obstacle_weight * 0.26f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.94f);
        }
        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.26f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.88f);
        }

        if (active >= 8u
            && environment.longest_stable_stance_seconds() < 1.0f
            && !sim::stage_allows_controlled_flips(stage))
        {
            for (std::size_t index = 4; index < 8; ++index)
                policy_action[index] *= 0.08f;
        }
        return bilateral_joint_synergy_action(environment, policy_action, stage);
    }

    struct TrainingMetrics''',
    "const float leg_assist = 0.60f"
)

insert_before(
    "src/simulation.cpp",
    "        void add_passive_feet",
    '''        [[nodiscard]] bool motor_references_node(const CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const MotorConstraint& motor = rig.motors[index];
                if (motor.enabled && (motor.a == node || motor.pivot == node || motor.c == node))
                    return true;
            }
            return false;
        }

        [[nodiscard]] std::size_t node_degree(const CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            return static_cast<std::size_t>(std::ranges::count_if(
                rig.bones, [node](const DistanceConstraint& bone)
                {
                    return bone.a == node || bone.b == node;
                }));
        }

        [[nodiscard]] bool passive_endpoint(const CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            return node < rig.nodes.size()
                && node != rig.root_node && node != rig.torso_node
                && node != rig.head_node && !rig.is_support_seed(node)
                && node_degree(rig, node) == 1u
                && !motor_references_node(rig, node);
        }

''',
    "motor_references_node"
)

replace_regex(
    "src/simulation.cpp",
    r"        void add_passive_feet\(CreatureBlueprint& rig,.*?\n        }\n\n        void calibrate_grounded_defaults",
    '''        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            auto add_foot = [&](std::uint16_t ankle)
            {
                std::array<std::uint16_t, 2> result{};
                if (ankle >= rig.nodes.size() || rig.nodes.size() > 124)
                    return result;

                const Vec2 ankle_position = rig.nodes[ankle];
                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.64f, 0.090f, 0.125f) : 0.105f;
                const auto heel = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x - heel_reach * 0.55f,
                    ankle_position.y - 0.155f
                });
                rig.radii.push_back(radius);
                const auto toe = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x + toe_reach * 0.72f,
                    ankle_position.y - 0.165f
                });
                rig.radii.push_back(radius);

                // A single rigid triangle is enough: ankle-to-heel,
                // ankle-to-toe, and heel-to-toe. Both plate endpoints are
                // semantic contacts; there is no dangling center cluster.
                rig.bones.push_back({ ankle, heel, 0.0f, 1.0f });
                rig.bones.push_back({ ankle, toe, 0.0f, 1.0f });
                rig.bones.push_back({ heel, toe, 0.0f, 1.0f });
                result = { heel, toe };
                return result;
            };

            const auto left = add_foot(rig.left_contact_node);
            const auto right = add_foot(rig.right_contact_node);
            if (left[0] != 0u && right[0] != 0u)
            {
                rig.left_contact_node = left[0];
                rig.right_contact_node = right[0];
                rig.additional_left_contact_nodes = { left[1] };
                rig.additional_right_contact_nodes = { right[1] };
            }
        }

        void calibrate_grounded_defaults''',
    "A single rigid triangle is enough"
)

replace_regex(
    "src/simulation.cpp",
    r"            float inverse_mass = 1\.0f;\n            if \(index == blueprint_\.head_node\).*?\n                inverse_mass = 1\.18f;",
    '''            float inverse_mass = 1.0f;
            if (contact_semantic)
                inverse_mass = 0.58f;
            else if (index == blueprint_.head_node)
                inverse_mass = 0.72f;
            else if (passive_endpoint(blueprint_, index))
                inverse_mass = 0.68f;
            else if (degree == 1u && index != blueprint_.root_node
                && index != blueprint_.torso_node)
                inverse_mass = 0.92f;''',
    "inverse_mass = 0.58f"
)

replace_regex(
    "src/simulation.cpp",
    r"        constexpr Vec2 gravity\{ 0\.0f, -22\.0f };\n        constexpr float damping = 0\.996f;\n        for \(Particle& particle : particles_\)\n        \{\n            const Vec2 velocity = \(particle\.position - particle\.previous\) \* damping;\n            particle\.previous = particle\.position;\n            particle\.position \+= velocity \+ gravity \* \(dt \* dt\);\n        }",
    '''        constexpr Vec2 gravity{ 0.0f, -22.0f };
        constexpr float damping = 0.996f;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            float local_damping = damping;
            if (index == blueprint_.head_node)
                local_damping = 0.92f;
            else if (passive_endpoint(blueprint_, index))
                local_damping = 0.90f;
            else if (blueprint_.is_support_seed(index))
                local_damping = 0.985f;
            else if (node_degree(blueprint_, index) == 1u)
                local_damping = 0.975f;
            const Vec2 velocity = (particle.position - particle.previous) * local_damping;
            particle.previous = particle.position;
            particle.position += velocity + gravity * (dt * dt);
        }''',
    "local_damping = 0.92f"
)

replace_text(
    "src/simulation.hpp",
    "        [[nodiscard]] bool current_display_posture_valid() const noexcept;",
    "        [[nodiscard]] bool body_integrity_valid() const noexcept;\n        [[nodiscard]] bool current_display_posture_valid() const noexcept;",
    "body_integrity_valid() const noexcept"
)
replace_text(
    "src/simulation.hpp",
    "        void solve_distance(const DistanceConstraint& constraint) noexcept;\n        void solve_motor",
    "        void solve_distance(const DistanceConstraint& constraint) noexcept;\n        void stabilize_passive_appendages() noexcept;\n        void solve_motor",
    "stabilize_passive_appendages() noexcept"
)

insert_before(
    "src/simulation.cpp",
    "    void Environment::solve_motor",
    '''    bool Environment::body_integrity_valid() const noexcept
    {
        if (particles_.size() != blueprint_.nodes.size() || particles_.empty()
            || !valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return false;

        for (const Particle& particle : particles_)
        {
            if (!std::isfinite(particle.position.x) || !std::isfinite(particle.position.y)
                || !std::isfinite(particle.previous.x) || !std::isfinite(particle.previous.y)
                || std::abs(particle.position.x) > 1000.0f
                || std::abs(particle.position.y) > 1000.0f)
                return false;
        }
        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            if (bone.a >= particles_.size() || bone.b >= particles_.size()
                || bone.rest_length <= 1.0e-5f)
                return false;
            const float ratio = length(particles_[bone.b].position
                - particles_[bone.a].position) / bone.rest_length;
            if (!std::isfinite(ratio) || ratio < 0.70f || ratio > 1.30f)
                return false;
        }

        const Vec2 root = particles_[blueprint_.root_node].position;
        const Vec2 torso_segment = particles_[blueprint_.torso_node].position - root;
        const Vec2 head_segment = particles_[blueprint_.head_node].position
            - particles_[blueprint_.torso_node].position;
        const Vec2 rest_torso_segment = blueprint_.nodes[blueprint_.torso_node]
            - blueprint_.nodes[blueprint_.root_node];
        const Vec2 rest_head_segment = blueprint_.nodes[blueprint_.head_node]
            - blueprint_.nodes[blueprint_.torso_node];
        const float torso_ratio = length(torso_segment)
            / std::max(length(rest_torso_segment), 1.0e-5f);
        const float head_ratio = length(head_segment)
            / std::max(length(rest_head_segment), 1.0e-5f);
        if (torso_ratio < 0.50f || torso_ratio > 1.50f
            || head_ratio < 0.50f || head_ratio > 1.50f)
            return false;
        if (dot(normalized(torso_segment, { 0.0f, 1.0f }),
                normalized(head_segment, { 0.0f, 1.0f })) < 0.05f)
            return false;

        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            const float rest_radius = length(blueprint_.nodes[index]
                - blueprint_.nodes[blueprint_.root_node]);
            const float current_radius = length(particles_[index].position - root);
            if (current_radius > std::max(0.70f, rest_radius * 1.65f + 0.20f))
                return false;
        }
        return true;
    }

    void Environment::stabilize_passive_appendages() noexcept
    {
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node))
            return;
        const Vec2 rest_body = blueprint_.nodes[blueprint_.torso_node]
            - blueprint_.nodes[blueprint_.root_node];
        const Vec2 current_body = particles_[blueprint_.torso_node].position
            - particles_[blueprint_.root_node].position;
        if (length(rest_body) <= 1.0e-5f || length(current_body) <= 1.0e-5f)
            return;
        const float body_rotation = signed_angle(rest_body, current_body);

        auto stabilize = [&](std::uint16_t node, float strength)
        {
            if (!valid_node(node))
                return;
            std::uint16_t parent = std::numeric_limits<std::uint16_t>::max();
            for (const DistanceConstraint& bone : blueprint_.bones)
            {
                if (bone.a == node)
                    parent = bone.b;
                else if (bone.b == node)
                    parent = bone.a;
                if (parent != std::numeric_limits<std::uint16_t>::max())
                    break;
            }
            if (!valid_node(parent))
                return;
            const Vec2 rest_offset = blueprint_.nodes[node] - blueprint_.nodes[parent];
            const Vec2 target = particles_[parent].position + rotate(rest_offset, body_rotation);
            Vec2 error = target - particles_[node].position;
            const float maximum_error = std::max(0.08f, length(rest_offset) * 0.45f);
            const float error_length = length(error);
            if (error_length > maximum_error && error_length > 1.0e-6f)
                error *= maximum_error / error_length;
            particles_[node].position += error * strength * 0.90f;
            particles_[parent].position -= error * strength * 0.10f;
            particles_[node].previous += (particles_[node].position
                - particles_[node].previous) * (strength * 0.35f);
        };

        stabilize(blueprint_.head_node, 0.055f);
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (passive_endpoint(blueprint_, index))
                stabilize(static_cast<std::uint16_t>(index), 0.040f);
        }
    }

''',
    "bool Environment::body_integrity_valid() const noexcept"
)

replace_text(
    "src/simulation.cpp",
    "            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)\n                solve_motor(blueprint_.motors[index], applied_actions[index]);\n            solve_ground(dt);",
    "            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)\n                solve_motor(blueprint_.motors[index], applied_actions[index]);\n            stabilize_passive_appendages();\n            solve_ground(dt);",
    "            stabilize_passive_appendages();"
)
replace_text(
    "src/simulation.cpp",
    "        knee_first_this_step_ = knee_before_foot_fault();",
    "        if (elapsed_seconds_ >= 0.20f && !body_integrity_valid())\n            invalidate(InvalidMotion::collapsed_posture);\n        knee_first_this_step_ = knee_before_foot_fault();",
    "elapsed_seconds_ >= 0.20f && !body_integrity_valid()"
)

replace_regex(
    "src/simulation.cpp",
    r"    bool Environment::current_display_posture_valid\(\) const noexcept\n    \{.*?\n    }\n\n    void Environment::invalidate",
    '''    bool Environment::current_display_posture_valid() const noexcept
    {
        if (!body_integrity_valid()
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node)
            || !valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node))
            return false;

        const bool supported = contact_supported(blueprint_.left_contact_node)
            || contact_supported(blueprint_.right_contact_node);
        if (!supported || non_foot_grounded_)
            return false;

        const Vec2 root = particles_[blueprint_.root_node].position;
        const Vec2 torso = particles_[blueprint_.torso_node].position;
        const Vec2 head = particles_[blueprint_.head_node].position;
        const Vec2 torso_segment = torso - root;
        const Vec2 head_segment = head - torso;
        const float rest_torso_segment = length(
            blueprint_.nodes[blueprint_.torso_node]
                - blueprint_.nodes[blueprint_.root_node]);
        const float rest_head_segment = length(
            blueprint_.nodes[blueprint_.head_node]
                - blueprint_.nodes[blueprint_.torso_node]);
        const float torso_ratio = length(torso_segment)
            / std::max(rest_torso_segment, 1.0e-5f);
        const float head_ratio = length(head_segment)
            / std::max(rest_head_segment, 1.0e-5f);
        const float alignment = dot(normalized(torso_segment, { 0.0f, 1.0f }),
            normalized(head_segment, { 0.0f, 1.0f }));
        return torso_ratio >= 0.68f && torso_ratio <= 1.32f
            && head_ratio >= 0.68f && head_ratio <= 1.32f
            && alignment >= 0.18f;
    }

    void Environment::invalidate''',
    "alignment >= 0.18f"
)

replace_regex(
    "src/app.cpp",
    r"            const float camera = particles\[rig\.root_node\]\.position\.x \+ 0\.55f;\n            const float scale = std::clamp\(inner\.size\.y / 5\.7f, 28\.0f, 43\.0f\);",
    '''            if (!environment.body_integrity_valid())
            {
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 44.0f },
                    "SAMPLE REJECTED - BODY DISCONNECTED", 1.02f, danger,
                    rect.size.x - 26.0f);
                return;
            }
            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            float maximum_y = -std::numeric_limits<float>::infinity();
            for (const sim::Particle& particle : particles)
            {
                minimum_x = std::min(minimum_x, particle.position.x - particle.radius);
                maximum_x = std::max(maximum_x, particle.position.x + particle.radius);
                maximum_y = std::max(maximum_y, particle.position.y + particle.radius);
            }
            const float camera = (minimum_x + maximum_x) * 0.5f;
            const float ground = environment.ground_height_at(camera);
            const float body_width = std::max(0.50f, maximum_x - minimum_x);
            const float body_height = std::max(0.50f, maximum_y - ground);
            const float horizontal_scale = (inner.size.x - 28.0f) / (body_width + 0.70f);
            const float vertical_scale = (inner.size.y * 0.76f) / (body_height + 0.30f);
            const float scale = std::clamp(
                std::min(horizontal_scale, vertical_scale), 16.0f, 43.0f);''',
    "SAMPLE REJECTED - BODY DISCONNECTED"
)

insert_before(
    "tests/core_tests.cpp",
    "        static void qualify_stable_stance",
    '''        static void detach_left_support_cluster(Environment& environment) noexcept
        {
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_left_support_seed(index))
                    continue;
                environment.particles_[index].position.x += 4.0f;
                environment.particles_[index].previous = environment.particles_[index].position;
            }
        }

''',
    "detach_left_support_cluster"
)

replace_text(
    "tests/core_tests.cpp",
    "humanoid.support_seed_count() == 6",
    "humanoid.support_seed_count() == 4",
    "humanoid.support_seed_count() == 4"
)

insert_before(
    "tests/core_tests.cpp",
    "    require(rl::policy_candidate_better(2u, 1.0f, 1u, 1000.0f, true),",
    '''    {
        require(humanoid.support_seed_count() == 4u,
            "humanoid feet are not two rigid heel-toe contact plates");
        sim::Environment intact{ humanoid, 0x1A7E6u };
        require(intact.body_integrity_valid(),
            "fresh humanoid body fails the full skeleton integrity gate");
        sim::EnvironmentTestAccess::qualify_stable_stance(intact);
        sim::EnvironmentTestAccess::detach_left_support_cluster(intact);
        require(!intact.body_integrity_valid(),
            "detached foot cluster passes the full skeleton integrity gate");
        require(!rl::stage_display_sample_eligible(sim::CourseStage::balance, intact),
            "detached feet can still publish into the training preview");
    }

    {
        sim::Environment authority{ humanoid, 0xA4710u };
        authority.set_course(sim::CourseStage::balance, 0.25f);
        const auto teacher = rl::balance_teacher_action(authority);
        float leg_energy = 0.0f;
        float arm_energy = 0.0f;
        for (std::size_t index = 0; index < 4; ++index)
            leg_energy += std::abs(teacher[index]);
        for (std::size_t index = 4; index < 8; ++index)
            arm_energy += std::abs(teacher[index]);
        require(leg_energy > arm_energy * 4.0f + 0.02f,
            "balance teacher still reaches for the arms before loading the feet");

        std::array<float, sim::action_count> arm_heavy{};
        arm_heavy.fill(1.0f);
        const auto effective = rl::effective_policy_action(
            authority, arm_heavy, sim::CourseStage::balance);
        float effective_legs = 0.0f;
        float effective_arms = 0.0f;
        for (std::size_t index = 0; index < 4; ++index)
            effective_legs += std::abs(effective[index]);
        for (std::size_t index = 4; index < 8; ++index)
            effective_arms += std::abs(effective[index]);
        require(effective_arms < effective_legs * 0.40f + 0.02f,
            "early balance still grants more authority to arms than legs");
    }

    {
        const std::array<sim::CreatureBlueprint, 4> passive_rigs{
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::quadruped(),
            sim::CreatureBlueprint::crawler4(),
            sim::CreatureBlueprint::hexapod()
        };
        for (std::size_t index = 0; index < passive_rigs.size(); ++index)
        {
            sim::Environment environment{ passive_rigs[index], 0x7000u + index };
            const std::array<float, sim::action_count> zero{};
            for (int frame = 0; frame < 180; ++frame)
            {
                (void)environment.step(zero);
                require(environment.body_integrity_valid(),
                    "head or passive tail escaped the articulated body");
                if (!environment.valid_motion())
                    environment.reset(0x7100u + index * 257u + static_cast<std::size_t>(frame));
            }
        }
    }

''',
    "balance teacher still reaches for the arms before loading the feet"
)

mission = read("missioncache.md")
mission = mission.replace("**Target:** EpochRunner v0.7.2", "**Target:** EpochRunner v0.7.3", 1)
mission = re.sub(
    r"\*\*Release state:\*\*.*?\n\nAll implementation, validation, publication, release-asset audit, pull-request cleanup, and branch cleanup missions are complete\.\n",
    "**Release state:** IN PROGRESS — August 2 runtime screenshots reopened body-control and preview integrity\n\n"
    "v0.7.2 remains historical release evidence. It is not accepted as the current runtime-quality baseline because live screenshots show separated foot clusters, arm-first balance attempts, uncontrolled passive heads/tails, and an incomplete-body training preview.\n",
    mission,
    count=1,
    flags=re.DOTALL,
)
section = '''
## v0.7.3 live-runtime correction

### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots
**Status:** IN PROGRESS

Acceptance requires deterministic tests, full Windows package validation, and Adam's live packaged-runtime confirmation. Static or metric-only evidence cannot close this mission.

### WALK-FOOT-030 — Connected rigid heel-toe foot plates
**Status:** IN PROGRESS

Each biped foot uses one ankle/heel/toe triangle with two semantic contacts. Any stretched bone or detached contact cluster invalidates the rollout before champion, imitation, or preview publication.

### WALK-CONTROL-031 — Feet-first control authority
**Status:** IN PROGRESS

Balance and duck lessons must load feet through knees and hips before arms receive meaningful policy authority. Bilateral coordination remains a light prior and cannot force mirrored leg collapse.

### WALK-PASSIVE-032 — Stable heads and tails on every rig
**Status:** IN PROGRESS

Head and passive-tail endpoints receive realistic mass, velocity damping, and torso-relative passive angular stabilization. They may react naturally but cannot behave as uncontrolled pendulums.

### WALK-PREVIEW-033 — Complete-body training preview
**Status:** IN PROGRESS

The training preview may publish only a current, finite, connected full-body snapshot. Its camera fits complete particle bounds; a detached or exploded body is rejected rather than showing isolated feet.

### WALK-TEST-034 — Runtime-shaped regression coverage
**Status:** IN PROGRESS

Tests cover detached semantic feet, full-skeleton stretch bounds, leg-versus-arm authority, passive head/tail containment, current-frame preview eligibility, Linux C++23 validation, and the complete Windows Vulkan package.

'''
if "## v0.7.3 live-runtime correction" not in mission:
    mission = mission.replace("## v0.7.2 packaged-runtime regression correction", section + "## v0.7.2 packaged-runtime regression correction", 1)
write("missioncache.md", mission)

write("RELEASE_NOTES_v0.7.3.md", '''# EpochRunner v0.7.3

- Reopens v0.7.2 simulation-quality claims from Adam's August 2 live screenshots.
- Replaces each dangling three-contact foot cluster with one rigid ankle/heel/toe triangle and two semantic contacts.
- Rejects stretched bones, detached feet, exploded bodies, and non-finite body snapshots before elite or preview publication.
- Makes feet, knees, and hips the primary early balance actuators while strongly gating arms until stable support exists.
- Weakens bilateral coupling so it guides coordination without forcing mirrored leg collapse.
- Gives heads and passive tails realistic mass, endpoint damping, and torso-relative passive stabilization on every rig.
- Auto-fits the complete training-preview body and refuses to render disconnected fragments as a verified sample.
- Invalidates v0.7.2 learned state with v0.7.3 training semantics and autonomy-state format 6.
- Adds deterministic runtime-shaped tests plus Linux and full Windows Vulkan package gates.
''')

print("materialized EpochRunner v0.7.3 runtime correction")
