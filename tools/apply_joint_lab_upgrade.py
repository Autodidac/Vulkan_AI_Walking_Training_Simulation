from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f"{label}: expected one match, found {text.count(old)}")
    return text.replace(old, new, 1)


def replace_section(text: str, start_marker: str, end_marker: str, replacement: str, label: str) -> str:
    start = text.find(start_marker)
    if start < 0:
        raise RuntimeError(f"{label}: start marker not found")
    end = text.find(end_marker, start)
    if end < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:start] + replacement + text[end:]


root = Path(__file__).resolve().parents[1]

simulation_header = r'''#pragma once

#include "math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace epochrunner::sim
{
    inline constexpr std::size_t action_count = 4;
    inline constexpr std::size_t observation_count = 18;

    struct Particle
    {
        Vec2 position{};
        Vec2 previous{};
        float inverse_mass{ 1.0f };
        float radius{ 0.12f };
        bool grounded{};
    };

    struct DistanceConstraint
    {
        std::uint16_t a{};
        std::uint16_t b{};
        float rest_length{};
        float stiffness{ 1.0f };
    };

    struct MotorConstraint
    {
        std::uint16_t a{};
        std::uint16_t pivot{};
        std::uint16_t c{};
        float minimum_angle{ -1.2f };
        float maximum_angle{ 1.2f };
        float neutral_angle{};
        float strength{ 0.35f };
        bool enabled{ true };
    };

    [[nodiscard]] inline float motor_target_angle(const MotorConstraint& motor, float action) noexcept
    {
        action = clamp(action, -1.0f, 1.0f);
        const float negative_span = std::max(0.0f, motor.neutral_angle - motor.minimum_angle);
        const float positive_span = std::max(0.0f, motor.maximum_angle - motor.neutral_angle);
        const float target = action < 0.0f
            ? motor.neutral_angle + action * negative_span
            : motor.neutral_angle + action * positive_span;
        return clamp(target, motor.minimum_angle, motor.maximum_angle);
    }

    struct CreatureBlueprint
    {
        std::vector<Vec2> nodes{};
        std::vector<float> radii{};
        std::vector<DistanceConstraint> bones{};
        std::array<MotorConstraint, action_count> motors{};

        std::uint16_t root_node{};
        std::uint16_t torso_node{ 1 };
        std::uint16_t head_node{ 2 };
        std::uint16_t left_contact_node{ 4 };
        std::uint16_t right_contact_node{ 6 };

        [[nodiscard]] static CreatureBlueprint chicken();
        [[nodiscard]] static CreatureBlueprint biped();
        [[nodiscard]] static CreatureBlueprint humanoid();
        [[nodiscard]] static CreatureBlueprint quadruped();
        [[nodiscard]] static CreatureBlueprint monoped();
        void rebuild_rest_lengths() noexcept;
        [[nodiscard]] bool save(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] static CreatureBlueprint load(const std::filesystem::path& path, std::string& error);
    };

    struct StepResult
    {
        float reward{};
        float forward_speed{};
        bool terminated{};
    };

    class Environment
    {
    public:
        Environment();
        explicit Environment(const CreatureBlueprint& blueprint, std::uint64_t seed = 1);

        void set_blueprint(const CreatureBlueprint& blueprint);
        void reset(std::uint64_t seed = 0);
        [[nodiscard]] StepResult step(std::span<const float, action_count> actions, float dt = 1.0f / 60.0f);
        [[nodiscard]] std::array<float, observation_count> observation() const noexcept;

        [[nodiscard]] const std::vector<Particle>& particles() const noexcept { return particles_; }
        [[nodiscard]] const CreatureBlueprint& blueprint() const noexcept { return blueprint_; }
        [[nodiscard]] float elapsed_seconds() const noexcept { return elapsed_seconds_; }
        [[nodiscard]] float distance_travelled() const noexcept { return distance_travelled_; }
        [[nodiscard]] float forward_speed() const noexcept { return forward_speed_; }
        [[nodiscard]] bool fallen() const noexcept { return fallen_; }
        [[nodiscard]] float ground_height() const noexcept { return ground_height_; }

    private:
        void solve_distance(const DistanceConstraint& constraint) noexcept;
        void solve_motor(const MotorConstraint& motor, float action) noexcept;
        void solve_ground(float dt) noexcept;
        [[nodiscard]] float joint_angle(const MotorConstraint& motor) const noexcept;
        [[nodiscard]] float torso_uprightness() const noexcept;
        [[nodiscard]] float random_unit() noexcept;
        [[nodiscard]] bool valid_node(std::uint16_t index) const noexcept;

        CreatureBlueprint blueprint_{};
        std::vector<Particle> particles_{};
        std::uint64_t random_state_{ 1 };
        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};
        Vec2 previous_pelvis_{};
        float ground_height_{};
        float elapsed_seconds_{};
        float distance_travelled_{};
        float forward_speed_{};
        float last_reward_{};
        bool fallen_{};
    };
}
'''
(root / "src/simulation.hpp").write_text(simulation_header, encoding="utf-8")

simulation_path = root / "src/simulation.cpp"
simulation = simulation_path.read_text(encoding="utf-8")

simulation = replace_once(
    simulation,
    '''        result.motors = {
            MotorConstraint{ chest, pelvis, left_knee, -1.65f, 0.75f, -0.40f, 0.54f },
            MotorConstraint{ pelvis, left_knee, left_foot, 0.05f, 2.70f, 1.25f, 0.62f },
            MotorConstraint{ chest, pelvis, right_knee, -0.75f, 1.65f, 0.40f, 0.54f },
            MotorConstraint{ pelvis, right_knee, right_foot, -2.70f, -0.05f, -1.25f, 0.62f }
        };''',
    '''        result.motors = {
            MotorConstraint{ chest, pelvis, left_knee, -1.25f, 0.55f, -0.35f, 0.32f, true },
            MotorConstraint{ pelvis, left_knee, left_foot, 0.15f, 2.35f, 1.10f, 0.38f, true },
            MotorConstraint{ chest, pelvis, right_knee, -0.55f, 1.25f, 0.35f, 0.32f, true },
            MotorConstraint{ pelvis, right_knee, right_foot, -2.35f, -0.15f, -1.10f, 0.38f, true }
        };''',
    "calmer chicken motors")

factory_block = r'''
    CreatureBlueprint CreatureBlueprint::biped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.70f }, { 0.0f, 3.72f }, { 0.05f, 4.48f },
            { -0.38f, 1.48f }, { -0.48f, 0.26f },
            { 0.38f, 1.48f }, { 0.48f, 0.26f }, { -0.72f, 3.55f }
        };
        result.radii = { 0.25f, 0.28f, 0.24f, 0.18f, 0.17f, 0.18f, 0.17f, 0.14f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.85f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3, -1.15f, 0.50f, -0.28f, 0.30f, true },
            MotorConstraint{ 0, 3, 4, 0.20f, 2.25f, 1.12f, 0.36f, true },
            MotorConstraint{ 1, 0, 5, -0.50f, 1.15f, 0.28f, 0.30f, true },
            MotorConstraint{ 0, 5, 6, -2.25f, -0.20f, -1.12f, 0.36f, true }
        };
        result.rebuild_rest_lengths();
        return result;
    }

    CreatureBlueprint CreatureBlueprint::humanoid()
    {
        CreatureBlueprint result = biped();
        result.nodes[0] = { 0.0f, 2.90f };
        result.nodes[1] = { 0.0f, 4.05f };
        result.nodes[2] = { 0.0f, 4.95f };
        result.nodes[3] = { -0.34f, 1.62f };
        result.nodes[4] = { -0.42f, 0.25f };
        result.nodes[5] = { 0.34f, 1.62f };
        result.nodes[6] = { 0.42f, 0.25f };
        result.nodes[7] = { -0.92f, 3.95f };
        result.radii = { 0.26f, 0.31f, 0.29f, 0.19f, 0.17f, 0.19f, 0.17f, 0.15f };
        result.motors[0].strength = 0.28f;
        result.motors[1].strength = 0.34f;
        result.motors[2].strength = 0.28f;
        result.motors[3].strength = 0.34f;
        result.rebuild_rest_lengths();
        return result;
    }

    CreatureBlueprint CreatureBlueprint::quadruped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.05f }, { 1.55f, 2.08f }, { 2.35f, 2.42f },
            { -0.25f, 1.10f }, { -0.48f, 0.24f },
            { 1.72f, 1.08f }, { 1.92f, 0.24f }, { -1.05f, 2.35f }
        };
        result.radii = { 0.29f, 0.30f, 0.25f, 0.18f, 0.16f, 0.18f, 0.16f, 0.14f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.95f }, { 0, 7, 0.0f, 0.82f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 1, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3, -1.55f, 0.15f, -0.72f, 0.30f, true },
            MotorConstraint{ 0, 3, 4, 0.05f, 2.20f, 1.05f, 0.35f, true },
            MotorConstraint{ 0, 1, 5, -0.15f, 1.55f, 0.72f, 0.30f, true },
            MotorConstraint{ 1, 5, 6, -2.20f, -0.05f, -1.05f, 0.35f, true }
        };
        result.rebuild_rest_lengths();
        return result;
    }

    CreatureBlueprint CreatureBlueprint::monoped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.85f }, { 0.0f, 3.86f }, { 0.0f, 4.62f },
            { 0.0f, 1.62f }, { -0.34f, 0.22f },
            { 0.0f, 0.68f }, { 0.34f, 0.22f }, { -0.68f, 3.52f }
        };
        result.radii = { 0.27f, 0.29f, 0.25f, 0.19f, 0.17f, 0.18f, 0.17f, 0.14f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.82f },
            { 0, 3, 0.0f, 1.0f }, { 3, 5, 0.0f, 1.0f },
            { 5, 4, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.92f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3, -0.95f, 0.95f, 0.0f, 0.30f, true },
            MotorConstraint{ 0, 3, 5, 0.20f, 2.20f, 1.12f, 0.36f, true },
            MotorConstraint{ 3, 5, 4, -1.30f, 0.20f, -0.45f, 0.24f, true },
            MotorConstraint{ 4, 5, 6, -1.10f, 1.10f, 0.0f, 0.18f, true }
        };
        result.rebuild_rest_lengths();
        return result;
    }

'''
simulation = replace_once(
    simulation,
    '''        result.rebuild_rest_lengths();
        return result;
    }

    void CreatureBlueprint::rebuild_rest_lengths() noexcept''',
    '''        result.rebuild_rest_lengths();
        return result;
    }
''' + factory_block + '''    void CreatureBlueprint::rebuild_rest_lengths() noexcept''',
    "insert rig factories")

serialization_block = r'''    bool CreatureBlueprint::save(const std::filesystem::path& path, std::string& error) const
    {
        std::ofstream output(path, std::ios::trunc);
        if (!output)
        {
            error = "Could not open rig file for writing: " + path.string();
            return false;
        }

        output << "EPOCHRIG 2\n";
        output << nodes.size() << ' ' << bones.size() << ' ' << motors.size() << '\n';
        output << "S " << root_node << ' ' << torso_node << ' ' << head_node << ' '
            << left_contact_node << ' ' << right_contact_node << '\n';
        output << std::setprecision(9);
        for (std::size_t index = 0; index < nodes.size(); ++index)
        {
            const float radius = index < radii.size() ? radii[index] : 0.15f;
            output << "N " << nodes[index].x << ' ' << nodes[index].y << ' ' << radius << '\n';
        }
        for (const DistanceConstraint& bone : bones)
            output << "B " << bone.a << ' ' << bone.b << ' ' << bone.rest_length << ' ' << bone.stiffness << '\n';
        for (const MotorConstraint& motor : motors)
        {
            output << "M " << (motor.enabled ? 1 : 0) << ' ' << motor.a << ' ' << motor.pivot << ' ' << motor.c << ' '
                << motor.minimum_angle << ' ' << motor.maximum_angle << ' '
                << motor.neutral_angle << ' ' << motor.strength << '\n';
        }

        if (!output)
        {
            error = "Failed while writing rig file: " + path.string();
            return false;
        }
        error.clear();
        return true;
    }

    CreatureBlueprint CreatureBlueprint::load(const std::filesystem::path& path, std::string& error)
    {
        std::ifstream input(path);
        if (!input)
        {
            error = "Could not open rig file: " + path.string();
            return chicken();
        }

        std::string magic{};
        int version{};
        std::size_t node_count{};
        std::size_t bone_count{};
        std::size_t motor_count{};
        input >> magic >> version >> node_count >> bone_count >> motor_count;
        if (!input || magic != "EPOCHRIG" || (version != 1 && version != 2)
            || node_count < 3 || node_count > 128 || bone_count > 256 || motor_count != action_count)
        {
            error = "Invalid or unsupported EpochRunner rig file.";
            return chicken();
        }

        CreatureBlueprint result{};
        if (version >= 2)
        {
            char semantic_tag{};
            input >> semantic_tag >> result.root_node >> result.torso_node >> result.head_node
                >> result.left_contact_node >> result.right_contact_node;
            if (!input || semantic_tag != 'S')
            {
                error = "Invalid rig semantic-node data.";
                return chicken();
            }
        }

        result.nodes.reserve(node_count);
        result.radii.reserve(node_count);
        result.bones.reserve(bone_count);

        for (std::size_t index = 0; index < node_count; ++index)
        {
            char tag{};
            Vec2 node{};
            float radius{};
            input >> tag >> node.x >> node.y >> radius;
            if (!input || tag != 'N' || !std::isfinite(node.x) || !std::isfinite(node.y) || radius <= 0.0f || radius > 4.0f)
            {
                error = "Invalid node data in rig file.";
                return chicken();
            }
            result.nodes.push_back(node);
            result.radii.push_back(radius);
        }

        auto valid_semantic = [node_count](std::uint16_t index) { return index < node_count; };
        if (!valid_semantic(result.root_node) || !valid_semantic(result.torso_node)
            || !valid_semantic(result.head_node) || !valid_semantic(result.left_contact_node)
            || !valid_semantic(result.right_contact_node))
        {
            error = "Rig semantic node index is out of range.";
            return chicken();
        }

        for (std::size_t index = 0; index < bone_count; ++index)
        {
            char tag{};
            DistanceConstraint bone{};
            input >> tag >> bone.a >> bone.b >> bone.rest_length >> bone.stiffness;
            if (!input || tag != 'B' || bone.a >= node_count || bone.b >= node_count || bone.a == bone.b || bone.rest_length <= 0.0f)
            {
                error = "Invalid bone data in rig file.";
                return chicken();
            }
            bone.stiffness = clamp(bone.stiffness, 0.05f, 1.0f);
            result.bones.push_back(bone);
        }

        for (std::size_t index = 0; index < motor_count; ++index)
        {
            char tag{};
            MotorConstraint motor{};
            if (version >= 2)
            {
                int enabled{};
                input >> tag >> enabled >> motor.a >> motor.pivot >> motor.c
                    >> motor.minimum_angle >> motor.maximum_angle >> motor.neutral_angle >> motor.strength;
                motor.enabled = enabled != 0;
            }
            else
            {
                input >> tag >> motor.a >> motor.pivot >> motor.c
                    >> motor.minimum_angle >> motor.maximum_angle >> motor.neutral_angle >> motor.strength;
                motor.enabled = true;
            }
            if (!input || tag != 'M' || motor.a >= node_count || motor.pivot >= node_count || motor.c >= node_count)
            {
                error = "Invalid motor data in rig file.";
                return chicken();
            }
            motor.strength = clamp(motor.strength, 0.0f, 1.0f);
            if (motor.minimum_angle > motor.maximum_angle)
                std::swap(motor.minimum_angle, motor.maximum_angle);
            motor.neutral_angle = clamp(motor.neutral_angle, motor.minimum_angle, motor.maximum_angle);
            result.motors[index] = motor;
        }

        error.clear();
        return result;
    }

'''
simulation = replace_section(
    simulation,
    "    bool CreatureBlueprint::save(",
    "    Environment::Environment()",
    serialization_block,
    "replace rig serialization")

simulation = replace_once(
    simulation,
    "particles_.push_back({ position, position, index == head ? 0.65f : 1.0f, radius, false });",
    "particles_.push_back({ position, position, index == blueprint_.head_node ? 0.65f : 1.0f, radius, false });",
    "dynamic head mass")
simulation = replace_once(
    simulation,
    "previous_pelvis_ = particles_.empty() ? Vec2{} : particles_[pelvis].position;",
    "previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};",
    "dynamic root reset")
simulation = replace_once(
    simulation,
    '''    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept
    {
        if (motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())''',
    '''    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())''',
    "skip disabled motors")
simulation = replace_once(
    simulation,
    '''        const float target = clamp(motor.neutral_angle + clamp(action, -1.0f, 1.0f) * 1.15f,
            motor.minimum_angle, motor.maximum_angle);''',
    '''        const float target = motor_target_angle(motor, action);''',
    "intuitive motor target mapping")
simulation = replace_once(
    simulation,
    '''    float Environment::joint_angle(const MotorConstraint& motor) const noexcept
    {
        if (motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())
            return 0.0f;''',
    '''    float Environment::joint_angle(const MotorConstraint& motor) const noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())
            return motor.neutral_angle;''',
    "disabled motor angle")
simulation = replace_section(
    simulation,
    "    float Environment::torso_uprightness() const noexcept",
    "    StepResult Environment::step(",
    r'''    bool Environment::valid_node(std::uint16_t index) const noexcept
    {
        return index < particles_.size() && index < blueprint_.nodes.size();
    }

    float Environment::torso_uprightness() const noexcept
    {
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node))
            return 0.0f;
        const Vec2 current = normalized(
            particles_[blueprint_.torso_node].position - particles_[blueprint_.root_node].position,
            { 0.0f, 1.0f });
        const Vec2 desired = normalized(
            blueprint_.nodes[blueprint_.torso_node] - blueprint_.nodes[blueprint_.root_node],
            { 0.0f, 1.0f });
        return clamp(dot(current, desired), -1.0f, 1.0f);
    }

''',
    "generic torso posture")
simulation = replace_once(
    simulation,
    '''        if (particles_.size() < 8)
            return { 0.0f, 0.0f, true };''',
    '''        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node) || !valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node))
            return { 0.0f, 0.0f, true };''',
    "generic step validation")
simulation = replace_once(
    simulation,
    '''        float action_energy = 0.0f;
        for (const float action : actions)
            action_energy += action * action;''',
    '''        std::array<float, action_count> effective_actions{};
        float action_energy = 0.0f;
        for (std::size_t index = 0; index < action_count; ++index)
        {
            effective_actions[index] = blueprint_.motors[index].enabled ? actions[index] : 0.0f;
            action_energy += effective_actions[index] * effective_actions[index];
        }''',
    "disabled action energy")
simulation = replace_once(
    simulation,
    '''        const Vec2 pelvis_position = particles_[pelvis].position;''',
    '''        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;''',
    "dynamic root step")
simulation = replace_once(
    simulation,
    '''        distance_travelled_ = pelvis_position.x - blueprint_.nodes[pelvis].x;''',
    '''        distance_travelled_ = pelvis_position.x - blueprint_.nodes[blueprint_.root_node].x;''',
    "dynamic distance root")
simulation = replace_once(
    simulation,
    '''        const float alternating = std::abs(actions[0] + actions[2]) + std::abs(actions[1] + actions[3]);
        const float foot_contact = (particles_[left_foot].grounded ? 1.0f : 0.0f)
            + (particles_[right_foot].grounded ? 1.0f : 0.0f);

        fallen_ = particles_[chest].position.y < 1.25f
            || particles_[head].position.y < 0.85f
            || std::abs(pelvis_position.x) > 500.0f;''',
    '''        const float alternating = std::abs(effective_actions[0] + effective_actions[2])
            + std::abs(effective_actions[1] + effective_actions[3]);
        const float foot_contact = (particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f)
            + (particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f);

        const float torso_floor = std::max(0.35f, blueprint_.nodes[blueprint_.torso_node].y * 0.30f);
        const float head_floor = std::max(0.30f, blueprint_.nodes[blueprint_.head_node].y * 0.18f);
        fallen_ = particles_[blueprint_.torso_node].position.y < torso_floor
            || particles_[blueprint_.head_node].position.y < head_floor
            || std::abs(pelvis_position.x) > 500.0f;''',
    "generic contacts and fall detection")

observation_block = r'''    std::array<float, observation_count> Environment::observation() const noexcept
    {
        std::array<float, observation_count> result{};
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.left_contact_node) || !valid_node(blueprint_.right_contact_node))
            return result;

        const Vec2 torso = normalized(
            particles_[blueprint_.torso_node].position - particles_[blueprint_.root_node].position,
            { 0.0f, 1.0f });
        const Vec2 pelvis_velocity = particles_[blueprint_.root_node].position - particles_[blueprint_.root_node].previous;
        result[0] = torso.x;
        result[1] = torso.y;
        result[2] = clamp(pelvis_velocity.x * 60.0f / 8.0f, -4.0f, 4.0f);
        result[3] = clamp(pelvis_velocity.y * 60.0f / 8.0f, -4.0f, 4.0f);

        for (std::size_t index = 0; index < action_count; ++index)
        {
            const MotorConstraint& motor = blueprint_.motors[index];
            if (!motor.enabled)
                continue;
            const float range = std::max(0.001f, motor.maximum_angle - motor.minimum_angle);
            result[4 + index] = clamp((joint_angle(motor) - motor.neutral_angle) / range * 2.0f, -2.0f, 2.0f);
            result[8 + index] = clamp(angular_velocities_[index] / 20.0f, -3.0f, 3.0f);
        }

        result[12] = particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f;
        result[13] = particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f;
        result[14] = clamp((particles_[blueprint_.left_contact_node].position.x
            - particles_[blueprint_.root_node].position.x) / 2.0f, -2.0f, 2.0f);
        result[15] = clamp((particles_[blueprint_.right_contact_node].position.x
            - particles_[blueprint_.root_node].position.x) / 2.0f, -2.0f, 2.0f);
        result[16] = clamp(particles_[blueprint_.root_node].position.y / 5.0f, 0.0f, 2.0f);
        result[17] = 1.0f;
        return result;
    }
'''
simulation = replace_section(
    simulation,
    "    std::array<float, observation_count> Environment::observation() const noexcept",
    "}\n",
    observation_block,
    "generic observations") + "}\n"

simulation_path.write_text(simulation, encoding="utf-8")

app_path = root / "src/app.cpp"
app = app_path.read_text(encoding="utf-8")

state_block = r'''        enum class Mode : std::uint8_t { editor, training, run };
        enum class RigPreset : std::uint8_t { chicken, biped, humanoid, quadruped, monoped, custom };
        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };

        render::Canvas canvas{};
        sim::CreatureBlueprint blueprint{ sim::CreatureBlueprint::chicken() };
        rl::PpoTrainer trainer{ blueprint, 32 };
        image::Image icon{};
        gui_input::InputTracker input_tracker{};
        Mode mode{ Mode::editor };
        RigPreset rig_preset{ RigPreset::chicken };
        JointTestGroup joint_test_group{ JointTestGroup::selected };
        bool training{};
        bool run_paused{};
        bool joint_lab{ true };
        bool joint_auto_sweep{};
        int updates_per_frame{ 1 };
        int selected_node{ -1 };
        int selected_motor{};
        bool dragging_node{};
        float joint_test_input{};
        float joint_test_phase{};
        float camera_x{};
        std::string status{ "READY" };
        float status_time{};
        bool quit{};
        std::filesystem::path asset_directory{};
        std::filesystem::path rig_path{ "creature.epochrig" };
        std::filesystem::path policy_path{ "creature.eppo" };
        std::array<float, 4> scripted_phase{};

        [[nodiscard]] std::string_view preset_name() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::chicken: return "CHICKEN BIPED";
            case RigPreset::biped: return "BASIC BIPED";
            case RigPreset::humanoid: return "HUMANOID";
            case RigPreset::quadruped: return "QUADRUPED";
            case RigPreset::monoped: return "MONOPED";
            case RigPreset::custom: return "CUSTOM RIG";
            }
            return "CUSTOM RIG";
        }

        [[nodiscard]] std::array<std::string_view, 4> motor_names() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::chicken:
            case RigPreset::biped:
            case RigPreset::humanoid:
                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE" };
            case RigPreset::quadruped:
                return { "REAR HIP", "REAR KNEE", "FRONT SHOULDER", "FRONT KNEE" };
            case RigPreset::monoped:
                return { "HIP", "KNEE", "ANKLE FLEX", "FOOT SPREAD" };
            case RigPreset::custom:
                return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4" };
            }
            return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4" };
        }

        void use_preset(RigPreset preset)
        {
            rig_preset = preset;
            switch (preset)
            {
            case RigPreset::chicken: blueprint = sim::CreatureBlueprint::chicken(); break;
            case RigPreset::biped: blueprint = sim::CreatureBlueprint::biped(); break;
            case RigPreset::humanoid: blueprint = sim::CreatureBlueprint::humanoid(); break;
            case RigPreset::quadruped: blueprint = sim::CreatureBlueprint::quadruped(); break;
            case RigPreset::monoped: blueprint = sim::CreatureBlueprint::monoped(); break;
            case RigPreset::custom: break;
            }
            selected_node = -1;
            selected_motor = 0;
            dragging_node = false;
            joint_test_input = 0.0f;
            joint_auto_sweep = false;
            trainer.set_blueprint(blueprint);
            set_status(std::format("{} LOADED - RESET POLICY BEFORE FRESH TRAINING", preset_name()));
        }

        [[nodiscard]] bool test_motor_active(int index) const noexcept
        {
            switch (joint_test_group)
            {
            case JointTestGroup::selected: return index == selected_motor;
            case JointTestGroup::pair_a: return index < 2;
            case JointTestGroup::pair_b: return index >= 2;
            case JointTestGroup::all: return true;
            }
            return false;
        }

        [[nodiscard]] Rect joint_lab_rect(Rect viewport) const noexcept
        {
            const float width = std::min(760.0f, std::max(360.0f, viewport.size.x - 40.0f));
            return {
                { viewport.position.x + 20.0f, viewport.position.y + viewport.size.y - 154.0f },
                { width, 136.0f }
            };
        }

        bool delete_selected_node()
        {
            if (selected_node < 0 || static_cast<std::size_t>(selected_node) >= blueprint.nodes.size())
            {
                set_status("SELECT A NODE TO DELETE");
                return false;
            }
            if (blueprint.nodes.size() <= 3)
            {
                set_status("A TRAINABLE RIG NEEDS AT LEAST THREE NODES");
                return false;
            }

            const auto removed = static_cast<std::uint16_t>(selected_node);
            blueprint.nodes.erase(blueprint.nodes.begin() + selected_node);
            blueprint.radii.erase(blueprint.radii.begin() + selected_node);
            std::erase_if(blueprint.bones, [removed](const sim::DistanceConstraint& bone)
            {
                return bone.a == removed || bone.b == removed;
            });
            for (sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a > removed) --bone.a;
                if (bone.b > removed) --bone.b;
            }

            const auto last = static_cast<std::uint16_t>(blueprint.nodes.size() - 1);
            auto remap = [removed, last](std::uint16_t& index)
            {
                if (index == removed)
                    index = 0;
                else if (index > removed)
                    --index;
                index = std::min(index, last);
            };
            remap(blueprint.root_node);
            remap(blueprint.torso_node);
            remap(blueprint.head_node);
            remap(blueprint.left_contact_node);
            remap(blueprint.right_contact_node);
            for (sim::MotorConstraint& motor : blueprint.motors)
            {
                const bool removed_endpoint = motor.a == removed || motor.pivot == removed || motor.c == removed;
                remap(motor.a);
                remap(motor.pivot);
                remap(motor.c);
                if (removed_endpoint || motor.a == motor.pivot || motor.pivot == motor.c || motor.a == motor.c)
                    motor.enabled = false;
            }

            blueprint.rebuild_rest_lengths();
            trainer.set_blueprint(blueprint);
            rig_preset = RigPreset::custom;
            selected_node = -1;
            dragging_node = false;
            set_status("NODE DELETED - AFFECTED MOTORS WERE DISABLED");
            return true;
        }

'''
app = replace_section(
    app,
    "        enum class Mode : std::uint8_t { editor, training, run };",
    "        [[nodiscard]] bool button(",
    state_block,
    "replace application editor state")

angle_slider = r'''        float angle_slider(Rect rect, std::string_view label, float radians, const InputState& input)
        {
            float degrees = radians * 180.0f / pi;
            add_text(canvas, rect.position, label, 1.45f, muted);
            Rect track{ { rect.position.x, rect.position.y + 19.0f }, { rect.size.x, 8.0f } };
            add_rounded_rect(canvas, track, 4.0f, rgb(0x101820), border, 1.0f);
            float fraction = clamp((degrees + 180.0f) / 360.0f, 0.0f, 1.0f);
            add_rounded_rect(canvas, { track.position, { track.size.x * fraction, track.size.y } }, 4.0f, accent);
            canvas.circle({ track.position.x + track.size.x * fraction, track.position.y + 4.0f }, 7.0f, white, 16);
            if (input.left_down && contains({ { track.position.x - 8.0f, track.position.y - 8.0f }, { track.size.x + 16.0f, 24.0f } }, input.mouse))
            {
                fraction = clamp((input.mouse.x - track.position.x) / track.size.x, 0.0f, 1.0f);
                degrees = lerp(-180.0f, 180.0f, fraction);
            }
            add_text(canvas, { rect.position.x + rect.size.x - 82.0f, rect.position.y }, std::format("{:+.0f} DEG", degrees), 1.35f, white);
            return degrees * pi / 180.0f;
        }

'''
app = replace_once(app, "        void graph(", angle_slider + "        void graph(", "insert degree slider")

creature_and_lab = r'''        void draw_creature(const sim::Environment& environment, Rect viewport, float camera, float scale, float alpha = 1.0f, bool joints = false)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty())
                return;
            auto p = [&](std::size_t index) { return world_to_screen(particles[index].position, viewport, camera, scale); };
            for (const sim::DistanceConstraint& bone : rig.bones)
            {
                if (bone.a >= particles.size() || bone.b >= particles.size())
                    continue;
                const float radius_a = bone.a < rig.radii.size() ? rig.radii[bone.a] : 0.15f;
                const float radius_b = bone.b < rig.radii.size() ? rig.radii[bone.b] : 0.15f;
                const float radius = std::max(0.055f, std::min(radius_a, radius_b) * 0.55f) * scale;
                canvas.capsule(p(bone.a), p(bone.b), radius, with_alpha(chicken_body, alpha), 14);
            }
            for (std::size_t index = 0; index < particles.size(); ++index)
            {
                const float radius = (index < rig.radii.size() ? rig.radii[index] : 0.15f) * scale;
                Color color = index == rig.head_node ? chicken_light : chicken_body;
                if (index == rig.left_contact_node || index == rig.right_contact_node)
                    color = chicken_leg;
                canvas.circle(p(index), radius, with_alpha(color, alpha), 20);
                if (joints)
                {
                    canvas.circle(p(index), 6.0f, index == static_cast<std::size_t>(selected_node) ? accent : white, 16);
                    canvas.circle(p(index), 2.4f, rgb(0x0b1119), 12);
                }
            }
        }

        void draw_joint_lab(Rect viewport, const InputState& input)
        {
            const Rect rect = joint_lab_rect(viewport);
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x0b1721, 0.97f), accent_dim, 1.0f);
            const auto names = motor_names();
            add_text(canvas, rect.position + Vec2{ 14.0f, 10.0f },
                std::format("JOINT LAB - {}", names[static_cast<std::size_t>(selected_motor)]), 1.55f, white);
            add_text(canvas, rect.position + Vec2{ rect.size.x - 210.0f, 11.0f },
                "GHOST = TESTED POSE", 1.15f, muted);

            const float group_width = (rect.size.x - 28.0f) * 0.25f;
            Vec2 row = rect.position + Vec2{ 14.0f, 34.0f };
            if (button({ row, { group_width - 4.0f, 28.0f } }, "SELECTED", input, joint_test_group == JointTestGroup::selected))
                joint_test_group = JointTestGroup::selected;
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 28.0f } }, "PAIR 1+2", input, joint_test_group == JointTestGroup::pair_a))
                joint_test_group = JointTestGroup::pair_a;
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 28.0f } }, "PAIR 3+4", input, joint_test_group == JointTestGroup::pair_b))
                joint_test_group = JointTestGroup::pair_b;
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, 28.0f } }, "ALL FOUR", input, joint_test_group == JointTestGroup::all))
                joint_test_group = JointTestGroup::all;

            const float command_width = (rect.size.x - 28.0f) * 0.25f;
            row.y += 34.0f;
            if (button({ row, { command_width - 4.0f, 26.0f } }, "MIN LIMIT", input))
            {
                joint_auto_sweep = false;
                joint_test_input = -1.0f;
            }
            if (button({ row + Vec2{ command_width, 0.0f }, { command_width - 4.0f, 26.0f } }, "REST / ZERO", input))
            {
                joint_auto_sweep = false;
                joint_test_input = 0.0f;
            }
            if (button({ row + Vec2{ command_width * 2.0f, 0.0f }, { command_width - 4.0f, 26.0f } }, "MAX LIMIT", input))
            {
                joint_auto_sweep = false;
                joint_test_input = 1.0f;
            }
            if (button({ row + Vec2{ command_width * 3.0f, 0.0f }, { command_width - 4.0f, 26.0f } },
                joint_auto_sweep ? "STOP SWEEP" : "AUTO SWEEP", input, joint_auto_sweep))
                joint_auto_sweep = !joint_auto_sweep;

            joint_test_input = slider(
                { rect.position + Vec2{ 14.0f, 101.0f }, { rect.size.x - 28.0f, 30.0f } },
                "TEST INPUT  -1 = MIN   0 = REST   +1 = MAX", joint_test_input, -1.0f, 1.0f, input);
        }

'''
app = replace_section(
    app,
    "        void draw_creature(",
    "        void draw_blueprint(",
    creature_and_lab,
    "replace generic creature and joint lab")

blueprint_function = r'''        void draw_blueprint(Rect viewport, const InputState& input)
        {
            constexpr float scale = 88.0f;
            draw_ground(viewport, 0.0f, scale);
            const Rect lab_rect = joint_lab_rect(viewport);

            auto screen_position = [&](std::size_t index)
            {
                return world_to_screen(blueprint.nodes[index], viewport, 0.0f, scale);
            };

            std::vector<Vec2> preview_nodes = blueprint.nodes;
            if (joint_lab)
            {
                for (int motor_index = 0; motor_index < static_cast<int>(sim::action_count); ++motor_index)
                {
                    if (!test_motor_active(motor_index))
                        continue;
                    const sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(motor_index)];
                    if (!motor.enabled || motor.a >= preview_nodes.size() || motor.pivot >= preview_nodes.size() || motor.c >= preview_nodes.size())
                        continue;

                    const Vec2 pivot = preview_nodes[motor.pivot];
                    const float current = signed_angle(preview_nodes[motor.a] - pivot, preview_nodes[motor.c] - pivot);
                    const float delta = wrap_angle(sim::motor_target_angle(motor, joint_test_input) - current);
                    std::vector<std::uint16_t> stack{ motor.c };
                    std::vector<bool> visited(preview_nodes.size(), false);
                    visited[motor.pivot] = true;
                    visited[motor.c] = true;
                    while (!stack.empty())
                    {
                        const std::uint16_t node = stack.back();
                        stack.pop_back();
                        for (const sim::DistanceConstraint& bone : blueprint.bones)
                        {
                            std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                            if (bone.a == node) next = bone.b;
                            if (bone.b == node) next = bone.a;
                            if (next < visited.size() && !visited[next])
                            {
                                visited[next] = true;
                                stack.push_back(next);
                            }
                        }
                    }
                    for (std::size_t index = 0; index < preview_nodes.size(); ++index)
                    {
                        if (visited[index] && index != motor.pivot)
                            preview_nodes[index] = pivot + rotate(preview_nodes[index] - pivot, delta);
                    }
                }

                auto preview_screen = [&](std::size_t index)
                {
                    return world_to_screen(preview_nodes[index], viewport, 0.0f, scale);
                };
                for (const sim::DistanceConstraint& bone : blueprint.bones)
                {
                    if (bone.a < preview_nodes.size() && bone.b < preview_nodes.size())
                        canvas.line(preview_screen(bone.a), preview_screen(bone.b), 9.0f, with_alpha(accent, 0.38f));
                }
                for (std::size_t index = 0; index < preview_nodes.size(); ++index)
                    canvas.circle(preview_screen(index), 5.0f, with_alpha(accent, 0.58f), 14);
            }

            for (const sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a < blueprint.nodes.size() && bone.b < blueprint.nodes.size())
                    canvas.line(screen_position(bone.a), screen_position(bone.b), 18.0f, rgb(0x835927));
            }
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const Vec2 position = screen_position(index);
                const float radius = index < blueprint.radii.size() ? blueprint.radii[index] * scale : 12.0f;
                Color color = index == blueprint.head_node ? chicken_light : chicken_body;
                if (index == blueprint.left_contact_node || index == blueprint.right_contact_node)
                    color = chicken_leg;
                canvas.circle(position, radius, color, 24);
                canvas.circle(position, 7.0f, index == static_cast<std::size_t>(selected_node) ? accent : white, 16);
                add_text(canvas, position + Vec2{ 10.0f, -7.0f }, std::to_string(index), 1.2f, white);
            }

            auto draw_motor_limits = [&](int motor_index)
            {
                const sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(motor_index)];
                if (!motor.enabled || motor.a >= blueprint.nodes.size() || motor.pivot >= blueprint.nodes.size() || motor.c >= blueprint.nodes.size())
                    return;
                const Vec2 pivot_world = blueprint.nodes[motor.pivot];
                const Vec2 first_arm = blueprint.nodes[motor.a] - pivot_world;
                const Vec2 third_arm = blueprint.nodes[motor.c] - pivot_world;
                const float arm_length = std::max(0.25f, std::min(length(first_arm), length(third_arm)) * 0.72f);
                const Vec2 reference = normalized(first_arm, { 0.0f, 1.0f });
                std::vector<Vec2> arc{};
                constexpr int segments = 28;
                arc.reserve(segments + 1);
                for (int segment = 0; segment <= segments; ++segment)
                {
                    const float t = static_cast<float>(segment) / static_cast<float>(segments);
                    const float angle = lerp(motor.minimum_angle, motor.maximum_angle, t);
                    arc.push_back(world_to_screen(pivot_world + rotate(reference, angle) * arm_length, viewport, 0.0f, scale));
                }
                canvas.polyline(arc, motor_index == selected_motor ? 4.0f : 2.0f, motor_index == selected_motor ? accent : accent_dim);

                const Vec2 pivot_screen = screen_position(motor.pivot);
                const Vec2 a_screen = screen_position(motor.a);
                const Vec2 c_screen = screen_position(motor.c);
                canvas.line(a_screen, pivot_screen, 5.0f, accent);
                canvas.line(pivot_screen, c_screen, 5.0f, yellow);
                canvas.circle(pivot_screen, 10.0f, accent, 18);
                add_text(canvas, a_screen + Vec2{ 7.0f, -13.0f }, "A", 1.25f, accent);
                add_text(canvas, pivot_screen + Vec2{ 9.0f, -14.0f }, "PIVOT", 1.15f, white);
                add_text(canvas, c_screen + Vec2{ 7.0f, -13.0f }, "C", 1.25f, yellow);

                auto ray = [&](float angle, Color color, float width)
                {
                    const Vec2 end = world_to_screen(pivot_world + rotate(reference, angle) * arm_length, viewport, 0.0f, scale);
                    canvas.line(pivot_screen, end, width, color);
                };
                ray(motor.minimum_angle, danger, 2.0f);
                ray(motor.maximum_angle, danger, 2.0f);
                ray(motor.neutral_angle, white, 2.5f);
                const float target = sim::motor_target_angle(motor, joint_test_input);
                ray(target, yellow, 4.0f);

                const auto names = motor_names();
                const float current = signed_angle(first_arm, third_arm) * 180.0f / pi;
                add_text(canvas, pivot_screen + Vec2{ 16.0f, 14.0f },
                    std::format("{}  CURRENT {:+.0f}  TARGET {:+.0f} DEG",
                        names[static_cast<std::size_t>(motor_index)], current, target * 180.0f / pi),
                    1.2f, white);
            };

            if (joint_lab)
            {
                for (int index = 0; index < static_cast<int>(sim::action_count); ++index)
                {
                    if (test_motor_active(index))
                        draw_motor_limits(index);
                }
            }
            else
            {
                draw_motor_limits(selected_motor);
            }

            const bool pointer_over_lab = joint_lab && contains(lab_rect, input.mouse);
            if (input.left_pressed && contains(viewport, input.mouse) && !pointer_over_lab)
            {
                int hit = -1;
                float best_distance = 18.0f;
                for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
                {
                    const float distance = length(screen_position(index) - input.mouse);
                    if (distance < best_distance)
                    {
                        best_distance = distance;
                        hit = static_cast<int>(index);
                    }
                }
                if (input.shift && hit < 0 && blueprint.nodes.size() < 128)
                {
                    blueprint.nodes.push_back(screen_to_world(input.mouse, viewport, 0.0f, scale));
                    blueprint.radii.push_back(0.16f);
                    selected_node = static_cast<int>(blueprint.nodes.size() - 1);
                    rig_preset = RigPreset::custom;
                    set_status("NODE ADDED - CTRL CLICK ANOTHER NODE TO CONNECT");
                }
                else if (input.control && selected_node >= 0 && hit >= 0 && selected_node != hit)
                {
                    const auto a = static_cast<std::uint16_t>(selected_node);
                    const auto b = static_cast<std::uint16_t>(hit);
                    const bool exists = std::ranges::any_of(blueprint.bones, [&](const sim::DistanceConstraint& bone)
                    {
                        return (bone.a == a && bone.b == b) || (bone.a == b && bone.b == a);
                    });
                    if (!exists)
                    {
                        blueprint.bones.push_back({ a, b, length(blueprint.nodes[b] - blueprint.nodes[a]), 1.0f });
                        trainer.set_blueprint(blueprint);
                        rig_preset = RigPreset::custom;
                        set_status("BONE CONNECTED");
                    }
                }
                else
                {
                    selected_node = hit;
                    dragging_node = hit >= 0;
                }
            }
            if (dragging_node && input.left_down && !pointer_over_lab && selected_node >= 0
                && static_cast<std::size_t>(selected_node) < blueprint.nodes.size())
                blueprint.nodes[static_cast<std::size_t>(selected_node)] = screen_to_world(input.mouse, viewport, 0.0f, scale);
            if (dragging_node && input.left_released)
            {
                dragging_node = false;
                blueprint.rebuild_rest_lengths();
                trainer.set_blueprint(blueprint);
                rig_preset = RigPreset::custom;
                set_status("RIG UPDATED");
            }
            if (input.delete_pressed)
                delete_selected_node();

            add_text(canvas, viewport.position + Vec2{ 20.0f, 18.0f },
                "CLICK NODE: SELECT   SHIFT CLICK: ADD   CTRL CLICK: CONNECT   DRAG: MOVE   DELETE: REMOVE",
                1.25f, muted);
            add_text(canvas, viewport.position + Vec2{ 20.0f, 38.0f },
                "RED RAYS = LIMITS   WHITE = REST   YELLOW = TEST TARGET   BLUE GHOST = TESTED POSE",
                1.15f, muted);
            if (joint_lab)
                draw_joint_lab(viewport, input);
        }

'''
app = replace_section(
    app,
    "        void draw_blueprint(",
    "        void draw_editor_panel(",
    blueprint_function,
    "replace blueprint editor")

editor_panel = r'''        void draw_editor_panel(Rect panel_rect, const InputState& input)
        {
            add_rounded_rect(canvas, panel_rect, 10.0f, panel, border, 1.0f);
            Vec2 cursor = panel_rect.position + Vec2{ 16.0f, 14.0f };
            add_text(canvas, cursor, "CREATURE EDITOR", 1.8f, white);
            add_text(canvas, cursor + Vec2{ 174.0f, 3.0f }, preset_name(), 1.25f, accent);
            cursor.y += 31.0f;

            const float third = (panel_rect.size.x - 44.0f) / 3.0f;
            auto preset_button = [&](Vec2 position, std::string_view label, RigPreset preset)
            {
                if (button({ position, { third, 30.0f } }, label, input, rig_preset == preset))
                    use_preset(preset);
            };
            preset_button(cursor, "BIPED", RigPreset::biped);
            preset_button(cursor + Vec2{ third + 6.0f, 0.0f }, "HUMANOID", RigPreset::humanoid);
            preset_button(cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, "QUADRUPED", RigPreset::quadruped);
            cursor.y += 36.0f;
            const float half_preset = (panel_rect.size.x - 38.0f) * 0.5f;
            if (button({ cursor, { half_preset, 30.0f } }, "CHICKEN", input, rig_preset == RigPreset::chicken))
                use_preset(RigPreset::chicken);
            if (button({ cursor + Vec2{ half_preset + 6.0f, 0.0f }, { half_preset, 30.0f } }, "MONOPED", input, rig_preset == RigPreset::monoped))
                use_preset(RigPreset::monoped);
            cursor.y += 39.0f;

            const float file_width = (panel_rect.size.x - 44.0f) / 3.0f;
            if (button({ cursor, { file_width, 30.0f } }, "RESET", input))
                use_preset(rig_preset == RigPreset::custom ? RigPreset::chicken : rig_preset);
            if (button({ cursor + Vec2{ file_width + 6.0f, 0.0f }, { file_width, 30.0f } }, "SAVE RIG", input) || input.save_pressed)
            {
                std::string error{};
                set_status(blueprint.save(rig_path, error) ? "RIG SAVED" : error);
            }
            if (button({ cursor + Vec2{ (file_width + 6.0f) * 2.0f, 0.0f }, { file_width, 30.0f } }, "LOAD RIG", input) || input.load_pressed)
            {
                std::string error{};
                blueprint = sim::CreatureBlueprint::load(rig_path, error);
                trainer.set_blueprint(blueprint);
                rig_preset = RigPreset::custom;
                selected_node = -1;
                set_status(error.empty() ? "RIG LOADED" : error);
            }
            cursor.y += 41.0f;

            bool blueprint_changed = false;
            add_text(canvas, cursor, std::format("SELECTED NODE: {}", selected_node), 1.35f, muted);
            if (button({ cursor + Vec2{ panel_rect.size.x - 146.0f, -6.0f }, { 114.0f, 28.0f } },
                "DELETE NODE", input, false, selected_node >= 0))
                delete_selected_node();
            cursor.y += 25.0f;
            if (selected_node >= 0 && static_cast<std::size_t>(selected_node) < blueprint.radii.size())
            {
                float& radius = blueprint.radii[static_cast<std::size_t>(selected_node)];
                const float updated_radius = slider(
                    { cursor, { panel_rect.size.x - 32.0f, 34.0f } }, "NODE RADIUS", radius, 0.08f, 0.60f, input);
                blueprint_changed = blueprint_changed || updated_radius != radius;
                radius = updated_radius;
                cursor.y += 43.0f;
            }

            add_text(canvas, cursor, "MOTOR CHANNELS (PPO OUTPUTS)", 1.25f, muted);
            cursor.y += 20.0f;
            for (int index = 0; index < 4; ++index)
            {
                const float width = (panel_rect.size.x - 44.0f) * 0.25f;
                if (button({ cursor + Vec2{ width * static_cast<float>(index), 0.0f }, { width - 4.0f, 28.0f } },
                    std::to_string(index + 1), input, selected_motor == index))
                {
                    selected_motor = index;
                    joint_test_group = JointTestGroup::selected;
                }
            }
            cursor.y += 35.0f;
            const auto names = motor_names();
            add_text(canvas, cursor, names[static_cast<std::size_t>(selected_motor)], 1.55f, white);
            cursor.y += 22.0f;

            sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(selected_motor)];
            add_text(canvas, cursor,
                std::format("A {}   PIVOT {}   C {}   {}", motor.a, motor.pivot, motor.c, motor.enabled ? "ENABLED" : "DISABLED"),
                1.2f, motor.enabled ? accent : danger);
            cursor.y += 20.0f;
            const float endpoint_width = (panel_rect.size.x - 44.0f) / 3.0f;
            auto set_endpoint = [&](Rect rect, std::string_view label, std::uint16_t& endpoint)
            {
                if (button(rect, label, input, false, selected_node >= 0))
                {
                    endpoint = static_cast<std::uint16_t>(selected_node);
                    const bool distinct = motor.a != motor.pivot && motor.pivot != motor.c && motor.a != motor.c;
                    motor.enabled = distinct;
                    blueprint_changed = true;
                    set_status(distinct ? "MOTOR ENDPOINT UPDATED" : "ENDPOINTS MUST BE THREE DIFFERENT NODES");
                }
            };
            set_endpoint({ cursor, { endpoint_width, 28.0f } }, "SET A", motor.a);
            set_endpoint({ cursor + Vec2{ endpoint_width + 6.0f, 0.0f }, { endpoint_width, 28.0f } }, "SET PIVOT", motor.pivot);
            set_endpoint({ cursor + Vec2{ (endpoint_width + 6.0f) * 2.0f, 0.0f }, { endpoint_width, 28.0f } }, "SET C", motor.c);
            cursor.y += 34.0f;

            const bool endpoints_valid = motor.a < blueprint.nodes.size() && motor.pivot < blueprint.nodes.size()
                && motor.c < blueprint.nodes.size() && motor.a != motor.pivot && motor.pivot != motor.c && motor.a != motor.c;
            if (button({ cursor, { (panel_rect.size.x - 38.0f) * 0.5f, 28.0f } },
                motor.enabled ? "DISABLE MOTOR" : "ENABLE MOTOR", input, motor.enabled, endpoints_valid))
            {
                motor.enabled = !motor.enabled;
                blueprint_changed = true;
            }
            if (button({ cursor + Vec2{ (panel_rect.size.x - 38.0f) * 0.5f + 6.0f, 0.0f },
                { (panel_rect.size.x - 38.0f) * 0.5f, 28.0f } },
                joint_lab ? "HIDE JOINT LAB" : "OPEN JOINT LAB", input, joint_lab))
                joint_lab = !joint_lab;
            cursor.y += 38.0f;

            auto update_angle = [&](float& value, std::string_view label)
            {
                const float updated = angle_slider({ cursor, { panel_rect.size.x - 32.0f, 34.0f } }, label, value, input);
                blueprint_changed = blueprint_changed || updated != value;
                value = updated;
                cursor.y += 42.0f;
            };
            update_angle(motor.minimum_angle, "MINIMUM LIMIT");
            update_angle(motor.maximum_angle, "MAXIMUM LIMIT");
            update_angle(motor.neutral_angle, "REST / ACTION ZERO");
            const float updated_strength = slider(
                { cursor, { panel_rect.size.x - 32.0f, 34.0f } }, "MOTOR POWER", motor.strength, 0.0f, 0.80f, input);
            blueprint_changed = blueprint_changed || updated_strength != motor.strength;
            motor.strength = updated_strength;
            cursor.y += 42.0f;

            if (motor.minimum_angle > motor.maximum_angle)
                std::swap(motor.minimum_angle, motor.maximum_angle);
            motor.neutral_angle = clamp(motor.neutral_angle, motor.minimum_angle, motor.maximum_angle);

            add_text(canvas, cursor, "MIN/MAX = HARD MOTION ARC", 1.1f, muted); cursor.y += 16.0f;
            add_text(canvas, cursor, "REST = TARGET WHEN PPO OUTPUT IS ZERO", 1.1f, muted); cursor.y += 16.0f;
            add_text(canvas, cursor, "POWER = HOW HARD THE SOLVER CORRECTS", 1.1f, muted);

            if (blueprint_changed)
            {
                rig_preset = RigPreset::custom;
                trainer.set_blueprint(blueprint);
            }
        }

'''
app = replace_section(
    app,
    "        void draw_editor_panel(",
    "        void draw_training_panel(",
    editor_panel,
    "replace editor panel")

app = replace_once(
    app,
    '''            status_time = std::max(0.0f, status_time - dt);
            process_shortcuts(input);''',
    '''            status_time = std::max(0.0f, status_time - dt);
            if (joint_lab && joint_auto_sweep)
            {
                joint_test_phase += dt;
                joint_test_input = std::sin(joint_test_phase * 1.65f);
            }
            process_shortcuts(input);''',
    "animate joint sweep")
app = replace_once(
    app,
    '''                split_options.split_fraction = clamp(300.0f / content.size.x, 0.20f, 0.42f);
                split_options.thickness = 10.0f;
                split_options.min_before = 260.0f;''',
    '''                split_options.split_fraction = clamp(410.0f / content.size.x, 0.26f, 0.48f);
                split_options.thickness = 10.0f;
                split_options.min_before = 390.0f;''',
    "widen editor panel")

app_path.write_text(app, encoding="utf-8")

# Strengthen core validation for every built-in rig shape.
tests_path = root / "tests/core_tests.cpp"
tests = tests_path.read_text(encoding="utf-8")
tests = replace_once(
    tests,
    '''    sim::CreatureBlueprint blueprint = sim::CreatureBlueprint::chicken();
    require(blueprint.nodes.size() == 8, "unexpected default node count");
    require(blueprint.radii.size() == blueprint.nodes.size(), "node/radius count mismatch");
    require(!blueprint.bones.empty(), "default blueprint has no bones");

    sim::Environment environment{ blueprint, 42 };''',
    '''    const std::array<sim::CreatureBlueprint, 5> presets{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::monoped()
    };
    for (const sim::CreatureBlueprint& preset : presets)
    {
        require(preset.nodes.size() >= 3, "preset has too few nodes");
        require(preset.radii.size() == preset.nodes.size(), "node/radius count mismatch");
        require(!preset.bones.empty(), "preset has no bones");
        require(preset.root_node < preset.nodes.size(), "root semantic index is invalid");
        require(preset.torso_node < preset.nodes.size(), "torso semantic index is invalid");
        require(preset.head_node < preset.nodes.size(), "head semantic index is invalid");
        require(preset.left_contact_node < preset.nodes.size(), "left contact semantic index is invalid");
        require(preset.right_contact_node < preset.nodes.size(), "right contact semantic index is invalid");
        for (const sim::MotorConstraint& motor : preset.motors)
        {
            require(motor.a < preset.nodes.size() && motor.pivot < preset.nodes.size() && motor.c < preset.nodes.size(),
                "preset motor endpoint is invalid");
            require(motor.minimum_angle <= motor.neutral_angle && motor.neutral_angle <= motor.maximum_angle,
                "preset motor rest angle is outside its limits");
        }

        sim::Environment preset_environment{ preset, 19 };
        const std::array<float, sim::action_count> preset_actions{};
        for (int frame = 0; frame < 120; ++frame)
        {
            const sim::StepResult result = preset_environment.step(preset_actions);
            require(std::isfinite(result.reward), "preset reward is not finite");
            require(std::isfinite(result.forward_speed), "preset speed is not finite");
            if (result.terminated)
                preset_environment.reset(19u + static_cast<unsigned>(frame));
        }
    }

    sim::CreatureBlueprint blueprint = sim::CreatureBlueprint::chicken();
    sim::Environment environment{ blueprint, 42 };''',
    "test all rig presets")
tests_path.write_text(tests, encoding="utf-8")

# Version and documentation.
cmake_path = root / "CMakeLists.txt"
cmake = cmake_path.read_text(encoding="utf-8").replace(
    "project(EpochRunner VERSION 0.1.3 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.2.0 LANGUAGES CXX)")
cmake_path.write_text(cmake, encoding="utf-8")

manifest_path = root / "vcpkg.json"
manifest = manifest_path.read_text(encoding="utf-8").replace('"version-semver": "0.1.3"', '"version-semver": "0.2.0"')
manifest_path.write_text(manifest, encoding="utf-8")

readme_path = root / "README.md"
readme = readme_path.read_text(encoding="utf-8")
readme = readme.replace(
    "  - Delete removes user-added nodes\n  - edit node radius and four motor limits/strengths\n",
    "  - Delete removes any selected node safely and disables affected motors\n"
    "  - built-in chicken, biped, humanoid, quadruped, and monoped presets\n"
    "  - named motor channels with editable A/pivot/C endpoints\n"
    "  - degree-based limits, neutral/rest angle, motor enable, and power controls\n"
    "  - Joint Lab overlays for limit arcs, live target rays, ghost poses, groups, and auto sweep\n")
readme = readme.replace(
    "| Delete | Remove a user-added node |",
    "| Delete | Remove the selected node; affected motors are disabled safely |")
readme += r'''

## Joint Lab

The editor starts with Joint Lab visible. Select motor channel 1-4 to see its
three defining nodes: **A**, **pivot**, and **C**. Red rays show the hard limits,
the white ray shows the rest target used for PPO output zero, the yellow ray is
the current test target, and the blue ghost rig shows the resulting kinematic
pose.

Use **Selected**, **Pair 1+2**, **Pair 3+4**, or **All Four** to test one motor or
a coordinated group. **Min Limit**, **Rest / Zero**, **Max Limit**, and **Auto
Sweep** make the range immediately visible before training. Angle values are
shown in degrees.

The PPO actor still exposes four bounded action channels for all presets. Each
preset maps those channels to useful joints. Custom rigs can disable a channel
or reassign its A/pivot/C nodes without changing the policy tensor dimensions.
'''
readme_path.write_text(readme, encoding="utf-8")

print("Joint Lab v0.2.0 source upgrade applied.")
