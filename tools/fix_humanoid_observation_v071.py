from __future__ import annotations

from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


# Eight motors require eight angle channels and eight velocity channels.
# The legacy 32-float layout only had room for four of each, causing the arm
# channels to alias leg velocity, contact, and foot-position observations.
header_path = Path("src/simulation.hpp")
header = header_path.read_text(encoding="utf-8")
header = replace_once(
    header,
    "    inline constexpr std::size_t observation_count = 32;",
    "    inline constexpr std::size_t observation_count = 40;",
    "eight-motor observation count",
)
header_path.write_text(header, encoding="utf-8")

source_path = Path("src/simulation.cpp")
source = source_path.read_text(encoding="utf-8")
observation_function = r'''    std::array<float, observation_count> Environment::observation() const noexcept
    {
        std::array<float, observation_count> result{};
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.left_contact_node) || !valid_node(blueprint_.right_contact_node))
            return result;

        constexpr std::size_t joint_angle_begin = 4;
        constexpr std::size_t joint_velocity_begin = joint_angle_begin + action_count;
        constexpr std::size_t contact_begin = joint_velocity_begin + action_count;
        static_assert(contact_begin == 20);
        static_assert(observation_count == 40);

        const Vec2 root = particles_[blueprint_.root_node].position;
        const Vec2 torso = normalized(
            particles_[blueprint_.torso_node].position - root, { 0.0f, 1.0f });
        const Vec2 pelvis_velocity = particles_[blueprint_.root_node].position
            - particles_[blueprint_.root_node].previous;
        result[0] = torso.x;
        result[1] = torso.y;
        result[2] = clamp(pelvis_velocity.x * 60.0f / 6.0f, -3.0f, 3.0f);
        result[3] = clamp(pelvis_velocity.y * 60.0f / 6.0f, -3.0f, 3.0f);
        for (std::size_t index = 0; index < action_count; ++index)
        {
            const MotorConstraint& motor = blueprint_.motors[index];
            if (!motor.enabled)
                continue;
            const float delta = wrap_angle(joint_angle(motor) - motor.neutral_angle);
            const float span = delta < 0.0f
                ? std::max(0.001f, motor.neutral_angle - motor.minimum_angle)
                : std::max(0.001f, motor.maximum_angle - motor.neutral_angle);
            result[joint_angle_begin + index] = clamp(delta / span, -2.0f, 2.0f);
            result[joint_velocity_begin + index] = clamp(
                angular_velocities_[index] / 18.0f, -3.0f, 3.0f);
        }
        result[20] = contact_supported(blueprint_.left_contact_node) ? 1.0f : 0.0f;
        result[21] = contact_supported(blueprint_.right_contact_node) ? 1.0f : 0.0f;
        result[22] = clamp((particles_[blueprint_.left_contact_node].position.x - root.x) / 2.0f,
            -2.0f, 2.0f);
        result[23] = clamp((particles_[blueprint_.right_contact_node].position.x - root.x) / 2.0f,
            -2.0f, 2.0f);
        result[24] = clamp((root.y - ground_height_at(root.x)) / 5.0f, 0.0f, 2.0f);
        result[25] = non_foot_grounded_ ? -1.0f
            : recovery_active_ ? clamp(torso.y, -1.0f, 1.0f) : 1.0f;
        result[26] = clamp((ground_height_at(root.x + 0.65f) - ground_height_at(root.x)),
            -1.0f, 1.0f);
        result[27] = clamp((ground_height_at(root.x + 1.50f) - ground_height_at(root.x)),
            -1.0f, 1.0f);
        result[28] = clamp((ground_height_at(root.x + 3.00f) - ground_height_at(root.x)),
            -1.0f, 1.0f);

        const CourseFeature* nearest = nullptr;
        float nearest_dx = std::numeric_limits<float>::max();
        for (const CourseFeature& feature : course_features_)
        {
            const float dx = feature.center.x - root.x;
            if (dx >= -0.3f && dx < nearest_dx)
            {
                nearest_dx = dx;
                nearest = &feature;
            }
        }
        if (nearest != nullptr)
        {
            result[29] = clamp(nearest_dx / 6.0f, -1.0f, 2.0f);
            switch (nearest->kind)
            {
            case CourseFeatureKind::hurdle: result[30] = -1.0f; break;
            case CourseFeatureKind::rock: result[30] = -0.5f; break;
            case CourseFeatureKind::overhead_bar: result[30] = 0.0f; break;
            case CourseFeatureKind::moving_hazard: result[30] = 0.5f; break;
            case CourseFeatureKind::projectile: result[30] = 1.0f; break;
            }
            result[31] = clamp((nearest->center.y - root.y) / 4.0f, -2.0f, 2.0f);
            result[32] = course_feature_observation_size(*nearest);
            result[33] = clamp(nearest->velocity.x / 5.0f, -1.0f, 1.0f);
        }
        result[34] = airborne_ratio();
        result[35] = clamp(static_cast<float>(alternating_steps_) / 10.0f, 0.0f, 2.0f);
        result[36] = static_cast<float>(course_stage_)
            / static_cast<float>(course_stage_count - 1);
        result[37] = course_difficulty_;
        const float gait_phase = elapsed_seconds_ * 2.0f * pi * 1.25f;
        result[38] = std::sin(gait_phase);
        result[39] = std::cos(gait_phase);
        return result;
    }
'''
source, count = re.subn(
    r"    std::array<float, observation_count> Environment::observation\(\) const noexcept\n"
    r"    \{.*?\n    \}\n(?=\})",
    observation_function,
    source,
    count=1,
    flags=re.DOTALL,
)
if count != 1:
    raise RuntimeError(f"expected one observation function replacement, got {count}")
source_path.write_text(source, encoding="utf-8")

trainer_path = Path("src/ppo_trainer.cpp")
trainer = trainer_path.read_text(encoding="utf-8")
trainer = replace_once(
    trainer,
    '''        [[nodiscard]] std::array<float, sim::action_count> skill_bootstrap_action(
            const sim::Environment& environment, sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
                return {};
            const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.25f;
''',
    '''        [[nodiscard]] std::array<float, sim::action_count> skill_bootstrap_action(
            const sim::Environment& environment, sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
            {
                constexpr std::size_t joint_angle_begin = 4;
                constexpr std::size_t joint_velocity_begin =
                    joint_angle_begin + sim::action_count;
                const auto observation = environment.observation();
                std::array<float, sim::action_count> action{};
                for (std::size_t index = 0; index < action.size(); ++index)
                {
                    const float joint_error = observation[joint_angle_begin + index];
                    const float joint_speed = observation[joint_velocity_begin + index];
                    action[index] = clamp(-0.72f * joint_error - 0.16f * joint_speed,
                        -0.82f, 0.82f);
                }

                // A slightly widened, softly flexed base gives PPO time to learn
                // balance while the feedback terms damp every articulated limb.
                // Mirrored joints use opposite action signs.
                action[0] = clamp(action[0] - 0.10f, -0.82f, 0.82f);
                action[1] = clamp(action[1] + 0.08f, -0.82f, 0.82f);
                action[2] = clamp(action[2] + 0.10f, -0.82f, 0.82f);
                action[3] = clamp(action[3] - 0.08f, -0.82f, 0.82f);

                // Counter the torso lean through mirrored hips and shoulders.
                const float lean = observation[0];
                const float lateral_speed = observation[2];
                const float correction = clamp(lean * 0.55f + lateral_speed * 0.08f,
                    -0.30f, 0.30f);
                action[0] = clamp(action[0] - correction, -0.82f, 0.82f);
                action[2] = clamp(action[2] - correction, -0.82f, 0.82f);
                action[4] = clamp(action[4] + correction * 0.65f, -0.82f, 0.82f);
                action[6] = clamp(action[6] + correction * 0.65f, -0.82f, 0.82f);
                return action;
            }
            const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.25f;
''',
    "feedback balance teacher",
)
trainer_path.write_text(trainer, encoding="utf-8")

# Prove that all eight motor channels are distinct and preserved.
tests_path = Path("tests/core_tests.cpp")
tests = tests_path.read_text(encoding="utf-8")
anchor = '''    require(rl::policy_candidate_better(2u, 1.0f, 1u, 1000.0f, true),
        "higher stage-valid evidence loses to scalar reward");
'''
addition = '''    {
        sim::Environment observation_environment{ humanoid, 0x0B5E7u };
        const auto observation = observation_environment.observation();
        static_assert(sim::observation_count == 40);
        require(observation.size() == 40u,
            "eight-motor observation layout is not forty floats");
        require(observation[20] == 0.0f && observation[21] == 0.0f,
            "contact channels overlap motor channels at reset");
        require(std::isfinite(observation[18]) && std::isfinite(observation[19]),
            "right-arm angular velocity channels are missing");
    }

'''
tests = replace_once(tests, anchor, addition + anchor, "observation aliasing regression")
tests_path.write_text(tests, encoding="utf-8")
