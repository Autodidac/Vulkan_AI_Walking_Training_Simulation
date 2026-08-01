#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <system_error>

namespace epochrunner::sim
{
    namespace
    {
        constexpr float degrees_to_radians(float degrees) noexcept
        {
            return degrees * pi / 180.0f;
        }

        [[nodiscard]] bool direct_bone(const CreatureBlueprint& rig, std::uint16_t a, std::uint16_t b) noexcept
        {
            return std::ranges::any_of(rig.bones, [a, b](const DistanceConstraint& bone)
            {
                return (bone.a == a && bone.b == b) || (bone.a == b && bone.b == a);
            });
        }

        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            auto add_foot = [&](std::uint16_t ankle)
            {
                if (ankle >= rig.nodes.size() || rig.nodes.size() > 124)
                    return;
                const Vec2 center = rig.nodes[ankle];
                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.72f, 0.10f, 0.15f) : 0.12f;
                const auto heel = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ center.x - heel_reach, center.y - 0.01f });
                rig.radii.push_back(radius);
                const auto toe = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ center.x + toe_reach, center.y - 0.015f });
                rig.radii.push_back(radius);
                rig.bones.push_back({ ankle, heel, 0.0f, 0.96f });
                rig.bones.push_back({ ankle, toe, 0.0f, 0.96f });
                rig.bones.push_back({ heel, toe, 0.0f, 0.88f });
            };
            add_foot(rig.left_contact_node);
            add_foot(rig.right_contact_node);
        }

        void calibrate_grounded_defaults(CreatureBlueprint& rig,
            float major_travel_degrees, float minor_travel_degrees,
            float major_linear_gain, float minor_linear_gain) noexcept
        {
            for (std::size_t index = 0; index < action_count; ++index)
            {
                const MotorConstraint& motor = rig.motors[index];
                const bool minor_joint = (index & 1u) != 0u;
                const float travel = minor_joint ? minor_travel_degrees : major_travel_degrees;
                const float linear_gain = minor_joint ? minor_linear_gain : major_linear_gain;
                const float driven_arm = motor.pivot < rig.nodes.size() && motor.c < rig.nodes.size()
                    ? length(rig.nodes[motor.c] - rig.nodes[motor.pivot]) : 1.0f;
                const float normalized_strength = clamp(
                    linear_gain / std::max(0.75f, driven_arm), 0.032f, 0.056f);
                rig.calibrate_motor(index, travel, travel, normalized_strength);
            }
        }

        void calibrate_obstacle_legs(CreatureBlueprint& rig, float travel = 46.0f) noexcept
        {
            for (std::size_t index = 0; index < action_count; ++index)
                rig.calibrate_motor(index, travel, travel, 0.043f);
        }
    }

    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.95f }, { 0.05f, 4.05f }, { 0.64f, 4.72f },
            { -0.45f, 1.78f }, { -0.30f, 0.32f },
            { 0.48f, 1.76f }, { 0.66f, 0.32f }, { -1.10f, 3.86f }
        };
        result.radii = { 0.28f, 0.32f, 0.30f, 0.20f, 0.18f, 0.20f, 0.18f, 0.16f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.92f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 56.0f, 0.044f, 0.050f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.74f }, { 0.0f, 3.84f }, { 0.02f, 4.64f },
            { -0.36f, 1.52f }, { -0.46f, 0.26f },
            { 0.36f, 1.52f }, { 0.46f, 0.26f }
        };
        result.radii = { 0.25f, 0.29f, 0.25f, 0.18f, 0.17f, 0.18f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 36.0f, 58.0f, 0.045f, 0.051f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::humanoid()
    {
        // Calibrated from the user's most human-like learned rig. Geometry and
        // asymmetric travel are retained; motor power is reduced by about 10%
        // so the useful proportions remain without the flip/fly exploit speed.
        CreatureBlueprint result{};
        result.nodes = {
            { -0.0034f, 2.8127f }, { -0.0148f, 4.0173f }, { -0.010f, 4.86f },
            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f }
        };
        result.radii = { 0.26f, 0.31f, 0.27f, 0.19f, 0.17f, 0.19f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 36.0f, 58.0f, 0.045f, 0.051f);
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
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 0, 1, 5 }, MotorConstraint{ 1, 5, 6 }
        };
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 50.0f, 0.046f, 0.052f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::crawler4()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.05f }, { 1.05f, 2.12f }, { 1.82f, 2.42f },
            { -0.72f, 0.30f }, { -0.20f, 0.28f },
            { 1.22f, 0.28f }, { 1.72f, 0.30f },
            { -0.48f, 2.10f }, { 1.52f, 2.16f }
        };
        result.radii = { 0.29f, 0.29f, 0.24f, 0.15f, 0.15f, 0.15f, 0.15f, 0.18f, 0.18f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.92f },
            { 7, 0, 0.0f, 0.98f }, { 1, 8, 0.0f, 0.98f },
            { 7, 3, 0.0f, 0.97f }, { 0, 4, 0.0f, 0.97f },
            { 1, 5, 0.0f, 0.97f }, { 8, 6, 0.0f, 0.97f },
            { 3, 4, 0.0f, 0.42f }, { 5, 6, 0.0f, 0.42f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 3;
        result.right_contact_node = 5;
        result.motors = {
            MotorConstraint{ 0, 7, 3 }, MotorConstraint{ 1, 0, 4 },
            MotorConstraint{ 0, 1, 5 }, MotorConstraint{ 1, 8, 6 }
        };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 48.0f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::hexapod()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.08f }, { 0.92f, 2.12f }, { 1.82f, 2.36f },
            { -0.92f, 0.30f }, { -0.42f, 0.27f },
            { 0.42f, 0.27f }, { 0.92f, 0.27f },
            { 1.55f, 0.28f }, { 2.02f, 0.31f },
            { -0.48f, 2.10f }, { 0.45f, 2.15f }, { 1.48f, 2.16f }
        };
        result.radii = {
            0.28f, 0.29f, 0.23f,
            0.14f, 0.14f, 0.14f, 0.14f, 0.14f, 0.14f,
            0.17f, 0.17f, 0.17f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.92f },
            { 9, 0, 0.0f, 0.98f }, { 0, 10, 0.0f, 0.98f }, { 1, 11, 0.0f, 0.98f },
            { 9, 3, 0.0f, 0.96f }, { 9, 4, 0.0f, 0.96f },
            { 10, 5, 0.0f, 0.96f }, { 10, 6, 0.0f, 0.96f },
            { 11, 7, 0.0f, 0.96f }, { 11, 8, 0.0f, 0.96f },
            { 3, 4, 0.0f, 0.42f }, { 4, 5, 0.0f, 0.36f },
            { 5, 6, 0.0f, 0.42f }, { 6, 7, 0.0f, 0.36f }, { 7, 8, 0.0f, 0.42f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 3;
        result.right_contact_node = 6;
        result.motors = {
            MotorConstraint{ 0, 9, 3 }, MotorConstraint{ 0, 9, 4 },
            MotorConstraint{ 1, 10, 5 }, MotorConstraint{ 0, 1, 11 }
        };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 50.0f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::monoped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.85f }, { 0.0f, 3.86f }, { 0.0f, 4.62f },
            { 0.0f, 1.62f }, { -0.34f, 0.22f },
            { 0.0f, 0.68f }, { 0.34f, 0.22f }
        };
        result.radii = { 0.27f, 0.29f, 0.25f, 0.19f, 0.17f, 0.18f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 5, 0.0f, 1.0f },
            { 5, 4, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.92f }
        };
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 5 },
            MotorConstraint{ 3, 5, 4 }, MotorConstraint{ 3, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 32.0f, 48.0f, 0.043f, 0.049f);
        return result;
    }

    void CreatureBlueprint::rebuild_rest_lengths() noexcept
    {
        for (DistanceConstraint& bone : bones)
        {
            if (bone.a < nodes.size() && bone.b < nodes.size())
                bone.rest_length = std::max(0.05f, length(nodes[bone.b] - nodes[bone.a]));
        }
    }

    float CreatureBlueprint::rest_joint_angle(std::size_t motor_index) const noexcept
    {
        if (motor_index >= motors.size())
            return 0.0f;
        const MotorConstraint& motor = motors[motor_index];
        if (motor.a >= nodes.size() || motor.pivot >= nodes.size() || motor.c >= nodes.size()
            || motor.a == motor.pivot || motor.pivot == motor.c || motor.a == motor.c)
            return 0.0f;
        return signed_angle(nodes[motor.a] - nodes[motor.pivot], nodes[motor.c] - nodes[motor.pivot]);
    }

    void CreatureBlueprint::calibrate_motor(std::size_t motor_index, float negative_degrees,
        float positive_degrees, float power) noexcept
    {
        if (motor_index >= motors.size())
            return;
        MotorConstraint& motor = motors[motor_index];
        const bool endpoints = motor.a < nodes.size() && motor.pivot < nodes.size() && motor.c < nodes.size()
            && motor.a != motor.pivot && motor.pivot != motor.c && motor.a != motor.c;
        motor.enabled = endpoints && direct_bone(*this, motor.a, motor.pivot) && direct_bone(*this, motor.pivot, motor.c);
        if (!endpoints)
            return;
        motor.neutral_angle = rest_joint_angle(motor_index);
        motor.minimum_angle = motor.neutral_angle - degrees_to_radians(clamp(std::abs(negative_degrees), 2.0f, 120.0f));
        motor.maximum_angle = motor.neutral_angle + degrees_to_radians(clamp(std::abs(positive_degrees), 2.0f, 120.0f));
        motor.strength = clamp(power, 0.0f, 0.20f);
    }

    void CreatureBlueprint::calibrate_all_motors(float degrees, float power) noexcept
    {
        for (std::size_t index = 0; index < motors.size(); ++index)
            calibrate_motor(index, degrees, degrees, power);
    }

    bool CreatureBlueprint::valid() const noexcept
    {
        if (nodes.size() < 3 || radii.size() != nodes.size() || bones.empty())
            return false;
        const auto semantic_valid = [this](std::uint16_t node) { return node < nodes.size(); };
        if (!semantic_valid(root_node) || !semantic_valid(torso_node) || !semantic_valid(head_node)
            || !semantic_valid(left_contact_node) || !semantic_valid(right_contact_node))
            return false;
        for (const DistanceConstraint& bone : bones)
        {
            if (bone.a >= nodes.size() || bone.b >= nodes.size() || bone.a == bone.b
                || !std::isfinite(bone.rest_length) || bone.rest_length <= 0.0f)
                return false;
        }
        for (const MotorConstraint& motor : motors)
        {
            if (!motor.enabled)
                continue;
            if (motor.a >= nodes.size() || motor.pivot >= nodes.size() || motor.c >= nodes.size()
                || motor.a == motor.pivot || motor.pivot == motor.c || motor.a == motor.c
                || motor.minimum_angle > motor.neutral_angle || motor.neutral_angle > motor.maximum_angle
                || motor.strength < 0.0f || motor.strength > 0.20f
                || !direct_bone(*this, motor.a, motor.pivot)
                || !direct_bone(*this, motor.pivot, motor.c))
                return false;
        }
        return true;
    }

    std::uint64_t CreatureBlueprint::signature() const noexcept
    {
        std::uint64_t hash = 1469598103934665603ULL;
        auto add_u64 = [&](std::uint64_t value)
        {
            for (int byte = 0; byte < 8; ++byte)
            {
                hash ^= (value >> (byte * 8)) & 0xffULL;
                hash *= 1099511628211ULL;
            }
        };
        auto add_float = [&](float value) { add_u64(std::bit_cast<std::uint32_t>(value)); };

        add_u64(nodes.size()); add_u64(bones.size()); add_u64(motors.size());
        add_u64(root_node); add_u64(torso_node); add_u64(head_node);
        add_u64(left_contact_node); add_u64(right_contact_node);
        for (std::size_t index = 0; index < nodes.size(); ++index)
        {
            add_float(nodes[index].x); add_float(nodes[index].y);
            add_float(index < radii.size() ? radii[index] : 0.15f);
        }
        for (const DistanceConstraint& bone : bones)
        {
            add_u64(bone.a); add_u64(bone.b); add_float(bone.rest_length); add_float(bone.stiffness);
        }
        for (const MotorConstraint& motor : motors)
        {
            add_u64(motor.a); add_u64(motor.pivot); add_u64(motor.c); add_u64(motor.enabled ? 1 : 0);
            add_float(motor.minimum_angle); add_float(motor.maximum_angle);
            add_float(motor.neutral_angle); add_float(motor.strength);
        }
        return hash;
    }

    bool CreatureBlueprint::save(const std::filesystem::path& path, std::string& error) const
    {
        std::filesystem::path temporary = path;
        temporary += ".tmp";
        std::filesystem::path backup = path;
        backup += ".bak";

        std::error_code filesystem_error{};
        std::filesystem::remove(temporary, filesystem_error);
        filesystem_error.clear();
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output)
            {
                error = "Could not open temporary rig file for writing: " + temporary.string();
                return false;
            }
            output << "EPOCHRIG 2\n";
            output << nodes.size() << ' ' << bones.size() << ' ' << motors.size() << '\n';
            output << "S " << root_node << ' ' << torso_node << ' ' << head_node << ' '
                << left_contact_node << ' ' << right_contact_node << '\n';
            output << std::setprecision(9);
            for (std::size_t index = 0; index < nodes.size(); ++index)
                output << "N " << nodes[index].x << ' ' << nodes[index].y << ' ' << radii[index] << '\n';
            for (const DistanceConstraint& bone : bones)
                output << "B " << bone.a << ' ' << bone.b << ' ' << bone.rest_length << ' ' << bone.stiffness << '\n';
            for (const MotorConstraint& motor : motors)
            {
                output << "M " << (motor.enabled ? 1 : 0) << ' ' << motor.a << ' ' << motor.pivot << ' ' << motor.c << ' '
                    << motor.minimum_angle << ' ' << motor.maximum_angle << ' '
                    << motor.neutral_angle << ' ' << motor.strength << '\n';
            }
            output.flush();
            if (!output)
            {
                output.close();
                std::filesystem::remove(temporary, filesystem_error);
                error = "Failed while writing temporary rig file: " + temporary.string();
                return false;
            }
        }

        std::filesystem::remove(backup, filesystem_error);
        filesystem_error.clear();
        bool moved_original = false;
        if (std::filesystem::exists(path, filesystem_error) && !filesystem_error)
        {
            std::filesystem::rename(path, backup, filesystem_error);
            if (filesystem_error)
            {
                std::filesystem::remove(temporary, filesystem_error);
                error = "Could not prepare existing rig for replacement: " + path.string();
                return false;
            }
            moved_original = true;
        }

        filesystem_error.clear();
        std::filesystem::rename(temporary, path, filesystem_error);
        if (filesystem_error)
        {
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            if (moved_original)
            {
                cleanup_error.clear();
                std::filesystem::rename(backup, path, cleanup_error);
            }
            error = "Could not publish saved rig: " + filesystem_error.message();
            return false;
        }
        if (moved_original)
        {
            filesystem_error.clear();
            std::filesystem::remove(backup, filesystem_error);
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
            return humanoid();
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
            return humanoid();
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
                return humanoid();
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
            if (!input || tag != 'N' || !std::isfinite(node.x) || !std::isfinite(node.y)
                || radius <= 0.0f || radius > 4.0f)
            {
                error = "Invalid node data in rig file.";
                return humanoid();
            }
            result.nodes.push_back(node);
            result.radii.push_back(radius);
        }

        if (version == 1)
        {
            result.root_node = 0;
            result.torso_node = static_cast<std::uint16_t>(std::min<std::size_t>(1, node_count - 1));
            result.head_node = static_cast<std::uint16_t>(std::min<std::size_t>(2, node_count - 1));
            result.left_contact_node = static_cast<std::uint16_t>(std::min<std::size_t>(4, node_count - 1));
            result.right_contact_node = static_cast<std::uint16_t>(std::min<std::size_t>(6, node_count - 1));
        }

        for (std::size_t index = 0; index < bone_count; ++index)
        {
            char tag{};
            DistanceConstraint bone{};
            input >> tag >> bone.a >> bone.b >> bone.rest_length >> bone.stiffness;
            if (!input || tag != 'B' || bone.a >= node_count || bone.b >= node_count || bone.a == bone.b
                || bone.rest_length <= 0.0f)
            {
                error = "Invalid bone data in rig file.";
                return humanoid();
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
            if (!input || tag != 'M' || motor.a >= node_count || motor.pivot >= node_count || motor.c >= node_count
                || motor.a == motor.pivot || motor.pivot == motor.c || motor.a == motor.c
                || !std::isfinite(motor.minimum_angle) || !std::isfinite(motor.maximum_angle)
                || !std::isfinite(motor.neutral_angle) || motor.minimum_angle > motor.maximum_angle)
            {
                error = "Invalid motor data in rig file.";
                return humanoid();
            }
            motor.strength = clamp(motor.strength, 0.0f, 0.20f);
            motor.neutral_angle = clamp(motor.neutral_angle, motor.minimum_angle, motor.maximum_angle);
            result.motors[index] = motor;
        }

        if (!result.valid())
        {
            error = "Rig is structurally invalid or has invalid semantic nodes.";
            return humanoid();
        }
        error.clear();
        return result;
    }

    Environment::Environment()
        : Environment(CreatureBlueprint::humanoid(), 1)
    {
    }

    Environment::Environment(const CreatureBlueprint& blueprint, std::uint64_t seed)
        : blueprint_(blueprint), random_state_(seed == 0 ? 1 : seed)
    {
        reset(seed);
    }

    void Environment::set_blueprint(const CreatureBlueprint& blueprint)
    {
        blueprint_ = blueprint;
        reset(random_state_);
    }

    void Environment::set_course(CourseStage stage, float difficulty)
    {
        course_stage_ = stage;
        course_difficulty_ = clamp(difficulty, 0.10f, 1.0f);
        reset(random_state_);
    }

    float Environment::random_unit() noexcept
    {
        random_state_ ^= random_state_ >> 12;
        random_state_ ^= random_state_ << 25;
        random_state_ ^= random_state_ >> 27;
        const std::uint64_t value = random_state_ * 2685821657736338717ULL;
        return static_cast<float>(value >> 40) * (1.0f / 16777216.0f);
    }

    float Environment::ground_height_at(float x) const noexcept
    {
        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return 0.0f;

        const float course_x = std::max(0.0f, x + course_progress());
        const float local = std::fmod(course_x, terrain_cycle_length_m);
        const float amplitude = 0.18f + course_difficulty_ * 0.42f;
        float height = 0.0f;

        if (local >= 28.0f && local < 34.0f)
        {
            const float t = (local - 28.0f) / 6.0f;
            const float smooth = t * t * (3.0f - 2.0f * t);
            height = amplitude * smooth;
        }
        else if (local >= 34.0f && local < 38.0f)
        {
            height = amplitude;
        }
        else if (local >= 38.0f && local < 44.0f)
        {
            const float t = (local - 38.0f) / 6.0f;
            const float smooth = t * t * (3.0f - 2.0f * t);
            height = amplitude * (1.0f - smooth);
        }

        if (course_stage_ >= CourseStage::uneven && !course_zone_is_flat(course_x))
        {
            const float roughness = course_difficulty_ * 0.065f;
            height += std::sin(course_x * 0.83f) * roughness;
            height += std::sin(course_x * 2.17f + 0.7f) * roughness * 0.42f;
        }
        return std::max(-0.06f, height);
    }

    void Environment::rebuild_course_features() noexcept
    {
        course_features_.clear();
        if (static_cast<std::uint8_t>(course_stage_)
            < static_cast<std::uint8_t>(CourseStage::hurdles))
            return;

        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float progress = course_progress();
        const int first_sequence = first_course_feature_sequence(root_x, progress);
        const float treadmill_velocity = -course_speed();

        auto variation_for = [](int sequence) noexcept
        {
            std::uint32_t value = static_cast<std::uint32_t>(sequence) * 747796405u + 2891336453u;
            value ^= value >> 16u;
            value *= 2246822519u;
            value ^= value >> 13u;
            return static_cast<float>(value & 0xffffu) / 65535.0f;
        };

        constexpr int visible_markers = 14;
        for (int offset = 0; offset < visible_markers; ++offset)
        {
            const int sequence = first_sequence + offset;
            if (sequence < course_safe_runway_markers)
                continue;
            const float marker_distance = course_marker_distance_m(sequence);
            if (obstacles_require_flat_zone(course_stage_, course_difficulty_)
                && !course_zone_is_flat(marker_distance))
                continue;

            const float variation = variation_for(sequence);
            const float x = course_feature_world_x(sequence, progress);
            const float ground = ground_height_at(x);
            const CourseFeatureKind kind = scheduled_course_feature(course_stage_, sequence);

            switch (kind)
            {
            case CourseFeatureKind::rock:
            {
                const float radius = 0.16f + variation * (0.15f + course_difficulty_ * 0.08f);
                course_features_.push_back({
                    kind, { x, ground + radius }, {}, radius, { treadmill_velocity, 0.0f }, sequence
                });
                break;
            }
            case CourseFeatureKind::hurdle:
            {
                const float height = 0.24f + course_difficulty_ * 0.34f + variation * 0.12f;
                course_features_.push_back({
                    kind, { x, ground + height * 0.5f }, { 0.14f, height * 0.5f }, 0.0f,
                    { treadmill_velocity, 0.0f }, sequence
                });
                break;
            }
            case CourseFeatureKind::overhead_bar:
            {
                const float clearance = 3.65f - course_difficulty_ * 0.82f - variation * 0.16f;
                course_features_.push_back({
                    kind, { x, ground + clearance + 0.12f }, { 1.05f, 0.12f }, 0.0f,
                    { treadmill_velocity, 0.0f }, sequence
                });
                break;
            }
            case CourseFeatureKind::moving_hazard:
            {
                const float oscillation = std::sin(
                    elapsed_seconds_ * (1.7f + course_difficulty_) + static_cast<float>(sequence));
                const float radius = 0.19f + course_difficulty_ * 0.10f;
                course_features_.push_back({
                    kind,
                    { x + oscillation * 0.85f, ground + 1.05f + oscillation * 0.38f },
                    {},
                    radius,
                    { treadmill_velocity + oscillation * 0.35f, 0.0f }, sequence
                });
                break;
            }
            case CourseFeatureKind::projectile:
            {
                const float throw_phase = std::fmod(
                    elapsed_seconds_ * (0.72f + course_difficulty_ * 0.28f)
                        + static_cast<float>(sequence) * 0.37f,
                    1.0f);
                const float throw_speed = 2.8f + course_difficulty_ * 2.2f;
                const float arc = 4.0f * throw_phase * (1.0f - throw_phase);
                const float radius = 0.14f + variation * 0.08f;
                course_features_.push_back({
                    kind,
                    { x + 2.2f - throw_phase * 4.4f, ground + 1.15f + arc * 1.55f },
                    {},
                    radius,
                    { treadmill_velocity - throw_speed, (1.0f - throw_phase * 2.0f) * 2.4f }, sequence
                });
                break;
            }
            }
        }
    }

    void Environment::reset(std::uint64_t seed)
    {
        if (seed != 0)
            random_state_ = seed;
        if (random_state_ == 0)
            random_state_ = 1;

        particles_.clear();
        particles_.reserve(blueprint_.nodes.size());
        const float phase = (random_unit() - 0.5f) * 0.04f;
        for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
        {
            Vec2 position = blueprint_.nodes[index];
            position.x += (random_unit() - 0.5f) * 0.008f + phase;
            position.y += (random_unit() - 0.5f) * 0.006f;
            const float radius = index < blueprint_.radii.size() ? blueprint_.radii[index] : 0.15f;
            const bool contact_semantic = index == blueprint_.left_contact_node
                || index == blueprint_.right_contact_node;
            const std::size_t degree = static_cast<std::size_t>(std::ranges::count_if(
                blueprint_.bones, [index](const DistanceConstraint& bone)
                {
                    return bone.a == index || bone.b == index;
                }));
            float inverse_mass = 1.0f;
            if (index == blueprint_.head_node)
                inverse_mass = 1.25f;
            else if (degree == 1u && !contact_semantic && index != blueprint_.root_node
                && index != blueprint_.torso_node)
                inverse_mass = 1.18f;
            particles_.push_back({ position, position, inverse_mass, radius, false });
        }

        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
        previous_torso_angle_ = torso_roll_angle();
        previous_angles_.fill(0.0f);
        angular_velocities_.fill(0.0f);
        for (std::size_t index = 0; index < action_count; ++index)
            previous_angles_[index] = joint_angle(blueprint_.motors[index]);
        elapsed_seconds_ = 0.0f;
        distance_travelled_ = 0.0f;
        forward_speed_ = 0.0f;
        last_reward_ = 0.0f;
        fallen_ = false;
        collision_count_ = 0.0f;
        airborne_seconds_ = 0.0f;
        cumulative_airborne_ = 0.0f;
        progress_window_seconds_ = 0.0f;
        progress_window_start_x_ = previous_pelvis_.x;
        micro_motion_seconds_ = 0.0f;
        action_energy_window_ = 0.0f;
        root_path_window_ = 0.0f;
        previous_root_for_path_ = previous_pelvis_;
        last_step_time_ = -100.0f;
        last_step_x_ = previous_pelvis_.x;
        maximum_speed_kmh_ = 0.0f;
        alternating_steps_ = 0;
        knee_first_faults_ = 0;
        wheel_sliding_seconds_ = 0.0f;
        body_rolling_seconds_ = 0.0f;
        head_contact_seconds_ = 0.0f;
        torso_turn_speed_ = 0.0f;
        stance_slip_speed_ = 0.0f;
        hazard_stall_seconds_ = 0.0f;
        obstacle_approach_weight_ = 0.0f;
        obstacle_lift_clearance_ = 0.0f;
        obstacle_clearance_target_ = 0.20f;
        non_foot_grounded_ = false;
        knee_first_this_step_ = false;
        last_contact_side_ = 0;
        previous_left_grounded_ = false;
        previous_right_grounded_ = false;
        collided_this_step_ = false;
        recovery_active_ = false;
        recovery_started_seconds_ = 0.0f;
        recovery_best_upright_ = 1.0f;
        recovery_events_ = 0;
        recovery_successes_ = 0;
        invalid_reason_ = InvalidMotion::none;
        rebuild_course_features();
    }

    void Environment::solve_distance(const DistanceConstraint& constraint) noexcept
    {
        if (constraint.a >= particles_.size() || constraint.b >= particles_.size())
            return;
        Particle& lhs = particles_[constraint.a];
        Particle& rhs = particles_[constraint.b];
        const Vec2 delta = rhs.position - lhs.position;
        const float distance = length(delta);
        if (distance <= 1.0e-6f)
            return;
        const float weight = lhs.inverse_mass + rhs.inverse_mass;
        if (weight <= 1.0e-6f)
            return;
        const Vec2 correction = delta * ((distance - constraint.rest_length) / distance * constraint.stiffness);
        lhs.position += correction * (lhs.inverse_mass / weight);
        rhs.position -= correction * (rhs.inverse_mass / weight);
    }

    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size()
            || motor.c >= particles_.size() || particles_.size() > 128)
            return;
        const Vec2 pivot = particles_[motor.pivot].position;
        const Vec2 reference_arm = particles_[motor.a].position - pivot;
        const Vec2 driven_arm = particles_[motor.c].position - pivot;
        if (length(reference_arm) <= 1.0e-5f || length(driven_arm) <= 1.0e-5f)
            return;
        const float target = motor_target_angle(motor, action);
        const float current = signed_angle(reference_arm, driven_arm);
        const float error = wrap_angle(current - target);
        const float correction = clamp(error, -0.24f, 0.24f) * motor.strength;

        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> stack{};
        std::size_t stack_size = 0;
        visited[motor.pivot] = true;
        visited[motor.a] = true;
        visited[motor.c] = true;
        stack[stack_size++] = motor.c;
        while (stack_size > 0)
        {
            const std::uint16_t node = stack[--stack_size];
            for (const DistanceConstraint& bone : blueprint_.bones)
            {
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == node)
                    next = bone.b;
                else if (bone.b == node)
                    next = bone.a;
                if (next < particles_.size() && !visited[next])
                {
                    visited[next] = true;
                    stack[stack_size++] = next;
                }
            }
        }
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (visited[index] && index != motor.a && index != motor.pivot)
                particles_[index].position = pivot + rotate(particles_[index].position - pivot, -correction);
        }
    }

    bool Environment::contact_cluster_contains(std::uint16_t contact_node,
        std::size_t particle_index) const noexcept
    {
        if (!valid_node(contact_node) || particle_index >= particles_.size()
            || particle_index >= blueprint_.nodes.size())
            return false;

        const float contact_height = blueprint_.nodes[contact_node].y;
        if (blueprint_.nodes[particle_index].y > contact_height + 0.18f)
            return false;

        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> queue{};
        std::size_t head = 0;
        std::size_t tail = 0;
        visited[contact_node] = true;
        queue[tail++] = contact_node;
        while (head < tail)
        {
            const std::uint16_t current = queue[head++];
            if (current == particle_index)
                return true;
            for (const DistanceConstraint& bone : blueprint_.bones)
            {
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == current)
                    next = bone.b;
                else if (bone.b == current)
                    next = bone.a;
                if (next >= blueprint_.nodes.size() || visited[next]
                    || blueprint_.nodes[next].y > contact_height + 0.18f)
                    continue;
                visited[next] = true;
                queue[tail++] = next;
            }
        }
        return false;
    }

    bool Environment::contact_supported(std::uint16_t contact_node) const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (particles_[index].grounded && contact_cluster_contains(contact_node, index))
                return true;
        }
        return false;
    }

    bool Environment::non_foot_ground_contact() const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!particles_[index].grounded)
                continue;
            const bool left_foot = contact_cluster_contains(blueprint_.left_contact_node, index);
            const bool right_foot = contact_cluster_contains(blueprint_.right_contact_node, index);
            if (!left_foot && !right_foot)
                return true;
        }
        return false;
    }

    bool Environment::head_ground_contact() const noexcept
    {
        return valid_node(blueprint_.head_node) && particles_[blueprint_.head_node].grounded;
    }

    float Environment::torso_roll_angle() const noexcept
    {
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node))
            return 0.0f;
        const Vec2 current = particles_[blueprint_.torso_node].position
            - particles_[blueprint_.root_node].position;
        const Vec2 desired = blueprint_.nodes[blueprint_.torso_node]
            - blueprint_.nodes[blueprint_.root_node];
        return signed_angle(desired, current);
    }

    float Environment::contact_cluster_front_x(std::uint16_t contact_node) const noexcept
    {
        float front = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (contact_cluster_contains(contact_node, index))
                front = std::max(front, particles_[index].position.x + particles_[index].radius);
        }
        return std::isfinite(front) ? front : 0.0f;
    }

    float Environment::contact_cluster_top_y(std::uint16_t contact_node) const noexcept
    {
        float top = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (contact_cluster_contains(contact_node, index))
                top = std::max(top, particles_[index].position.y + particles_[index].radius);
        }
        return std::isfinite(top) ? top : 0.0f;
    }

    float Environment::contact_cluster_horizontal_speed(std::uint16_t contact_node,
        float dt) const noexcept
    {
        float accumulated = 0.0f;
        std::size_t count = 0;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!particles_[index].grounded || !contact_cluster_contains(contact_node, index))
                continue;
            accumulated += std::abs((particles_[index].position.x - particles_[index].previous.x)
                / std::max(dt, 1.0e-5f));
            ++count;
        }
        return count == 0 ? 0.0f : accumulated / static_cast<float>(count);
    }

    float Environment::contact_cluster_clearance(std::uint16_t contact_node) const noexcept
    {
        float maximum = 0.0f;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!contact_cluster_contains(contact_node, index))
                continue;
            const Particle& particle = particles_[index];
            maximum = std::max(maximum, particle.position.y
                - ground_height_at(particle.position.x) - particle.radius);
        }
        return maximum;
    }

    bool Environment::knee_before_foot_fault() const noexcept
    {
        constexpr std::array<std::size_t, 2> knee_motors{ 1u, 3u };
        constexpr std::array<bool, 2> left_side{ true, false };
        for (std::size_t side = 0; side < knee_motors.size(); ++side)
        {
            const MotorConstraint& knee_motor = blueprint_.motors[knee_motors[side]];
            if (!knee_motor.enabled || !valid_node(knee_motor.pivot))
                continue;
            const std::uint16_t foot = left_side[side]
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const float knee_front = particles_[knee_motor.pivot].position.x
                + particles_[knee_motor.pivot].radius;
            const float foot_front = contact_cluster_front_x(foot);
            const float foot_top = contact_cluster_top_y(foot);
            for (const CourseFeature& feature : course_features_)
            {
                if (knee_crosses_before_foot(knee_front, foot_front, foot_top, feature))
                    return true;
            }
        }
        return false;
    }

    void Environment::solve_ground(float dt) noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            particle.grounded = false;
            const float minimum_y = ground_height_at(particle.position.x) + particle.radius;
            if (particle.position.y < minimum_y)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                const bool traction_contact = contact_cluster_contains(blueprint_.left_contact_node, index)
                    || contact_cluster_contains(blueprint_.right_contact_node, index);
                float retained_horizontal_speed = velocity.x
                    * ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact && std::abs(retained_horizontal_speed) < 0.03f)
                    retained_horizontal_speed = 0.0f;
                particle.previous.x = particle.position.x - retained_horizontal_speed * dt;
                if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y + velocity.y * dt * 0.05f;
            }
        }
    }

    void Environment::solve_course() noexcept
    {
        for (Particle& particle : particles_)
        {
            for (const CourseFeature& feature : course_features_)
            {
                if (feature.kind == CourseFeatureKind::moving_hazard
                    || feature.kind == CourseFeatureKind::rock
                    || feature.kind == CourseFeatureKind::projectile)
                {
                    const Vec2 delta = particle.position - feature.center;
                    const float distance = length(delta);
                    const float minimum = particle.radius + feature.radius;
                    if (distance >= minimum)
                        continue;
                    const Vec2 normal = distance > 1.0e-5f ? delta / distance : Vec2{ -1.0f, 0.0f };
                    const Vec2 correction = normal * (minimum - distance);
                    particle.position += correction;
                    particle.previous += correction * 0.06f;
                    if (feature.kind == CourseFeatureKind::projectile)
                        particle.previous -= feature.velocity * (1.0f / 60.0f) * 0.34f;
                    else if (feature.kind == CourseFeatureKind::moving_hazard)
                        particle.previous -= feature.velocity * (1.0f / 60.0f) * 0.12f;
                    collided_this_step_ = true;
                    continue;
                }

                const Vec2 minimum = feature.center - feature.half_extent;
                const Vec2 maximum = feature.center + feature.half_extent;
                const Vec2 nearest{
                    clamp(particle.position.x, minimum.x, maximum.x),
                    clamp(particle.position.y, minimum.y, maximum.y)
                };
                Vec2 delta = particle.position - nearest;
                float distance = length(delta);
                if (distance >= particle.radius)
                    continue;
                if (distance <= 1.0e-5f)
                {
                    const float left = std::abs(particle.position.x - minimum.x);
                    const float right = std::abs(maximum.x - particle.position.x);
                    const float bottom = std::abs(particle.position.y - minimum.y);
                    const float top = std::abs(maximum.y - particle.position.y);
                    const float nearest_side = std::min({ left, right, bottom, top });
                    if (nearest_side == left) delta = { -1.0f, 0.0f };
                    else if (nearest_side == right) delta = { 1.0f, 0.0f };
                    else if (nearest_side == bottom) delta = { 0.0f, -1.0f };
                    else delta = { 0.0f, 1.0f };
                    distance = 0.0f;
                }
                const Vec2 normal = normalized(delta, { -1.0f, 0.0f });
                const Vec2 correction = normal * (particle.radius - distance);
                particle.position += correction;
                particle.previous += correction * 0.05f;
                collided_this_step_ = true;
            }
        }
    }

    float Environment::joint_angle(const MotorConstraint& motor) const noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())
            return motor.neutral_angle;
        return signed_angle(
            particles_[motor.a].position - particles_[motor.pivot].position,
            particles_[motor.c].position - particles_[motor.pivot].position);
    }

    bool Environment::valid_node(std::uint16_t index) const noexcept
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

    void Environment::invalidate(InvalidMotion reason) noexcept
    {
        if (invalid_reason_ == InvalidMotion::none)
            invalid_reason_ = reason;
    }

    float Environment::airborne_ratio() const noexcept
    {
        return elapsed_seconds_ > 1.0e-5f ? clamp(cumulative_airborne_ / elapsed_seconds_, 0.0f, 1.0f) : 0.0f;
    }

    void Environment::update_gait_metrics(float dt, float action_energy) noexcept
    {
        const bool left = contact_supported(blueprint_.left_contact_node);
        const bool right = contact_supported(blueprint_.right_contact_node);
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;
        const int strike_side = new_left == new_right ? 0 : (new_left ? -1 : 1);
        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        if (strike_side != 0)
        {
            if (last_contact_side_ == 0)
            {
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
            else if (qualifies_alternating_step(last_contact_side_, strike_side,
                elapsed_seconds_ - last_step_time_, root_x - last_step_x_))
            {
                ++alternating_steps_;
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
        }
        previous_left_grounded_ = left;
        previous_right_grounded_ = right;

        const float left_slip = left
            ? contact_cluster_horizontal_speed(blueprint_.left_contact_node, dt) : 0.0f;
        const float right_slip = right
            ? contact_cluster_horizontal_speed(blueprint_.right_contact_node, dt) : 0.0f;
        stance_slip_speed_ = left_slip + right_slip;
        const float root_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.x
                - particles_[blueprint_.root_node].previous.x) / std::max(dt, 1.0e-5f)
            : 0.0f;
        const float torso_angle = torso_roll_angle();
        torso_turn_speed_ = wrap_angle(torso_angle - previous_torso_angle_)
            / std::max(dt, 1.0e-5f);
        previous_torso_angle_ = torso_angle;
        non_foot_grounded_ = non_foot_ground_contact();
        const bool feet_supported = left || right;
        if (rolling_body_motion(root_speed, torso_turn_speed_, torso_uprightness(),
            feet_supported, non_foot_grounded_))
            body_rolling_seconds_ += dt;
        else
            body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 2.0f);
        if (head_ground_contact())
            head_contact_seconds_ += dt;
        else
            head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);
        const float rolling_limit = course_stage_ == CourseStage::balance ? 0.55f : 0.32f;
        if (body_rolling_seconds_ > rolling_limit || head_contact_seconds_ > 0.24f)
            invalidate(InvalidMotion::body_rolling);

        if (course_stage_ != CourseStage::balance
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))
            wheel_sliding_seconds_ += dt;
        else
            wheel_sliding_seconds_ = std::max(0.0f, wheel_sliding_seconds_ - dt * 1.5f);
        if (wheel_sliding_seconds_ > 0.90f)
            invalidate(InvalidMotion::wheel_sliding);

        float nearest_hazard_dx = std::numeric_limits<float>::infinity();
        float nearest_hazard_target = 0.20f;
        for (const CourseFeature& feature : course_features_)
        {
            if (!ground_clearance_hazard(feature.kind))
                continue;
            const float dx = feature.center.x - root_x;
            if (dx < nearest_hazard_dx && dx >= -0.35f)
            {
                nearest_hazard_dx = dx;
                nearest_hazard_target = course_feature_top(feature)
                    - ground_height_at(feature.center.x) + 0.12f;
            }
        }
        obstacle_approach_weight_ = std::isfinite(nearest_hazard_dx)
            ? hazard_approach_weight(nearest_hazard_dx) : 0.0f;
        obstacle_clearance_target_ = std::max(0.18f, nearest_hazard_target);
        obstacle_lift_clearance_ = std::max(
            contact_cluster_clearance(blueprint_.left_contact_node),
            contact_cluster_clearance(blueprint_.right_contact_node));
        if (std::isfinite(nearest_hazard_dx) && hazard_quiver_motion(nearest_hazard_dx,
            root_speed, obstacle_lift_clearance_, obstacle_clearance_target_, action_energy))
            hazard_stall_seconds_ += dt;
        else
            hazard_stall_seconds_ = std::max(0.0f, hazard_stall_seconds_ - dt * 1.75f);
        if (hazard_stall_seconds_ > 1.35f)
            invalidate(InvalidMotion::hazard_quiver);

        if (!left && !right)
        {
            airborne_seconds_ += dt;
            cumulative_airborne_ += dt;
        }
        else
        {
            airborne_seconds_ = 0.0f;
        }

        if (valid_node(blueprint_.root_node))
        {
            const Vec2 root = particles_[blueprint_.root_node].position;
            root_path_window_ += length(root - previous_root_for_path_);
            previous_root_for_path_ = root;
        }
        action_energy_window_ += action_energy * dt;
        progress_window_seconds_ += dt;
        if (progress_window_seconds_ >= 1.0f && valid_node(blueprint_.root_node))
        {
            const float net_progress = std::abs(root_x - progress_window_start_x_);
            const float average_energy = action_energy_window_ / progress_window_seconds_;
            const bool high_energy_stall = average_energy > 0.10f && net_progress < 0.05f;
            const bool inefficient_vibration = average_energy > 0.16f && net_progress < 0.12f
                && root_path_window_ > std::max(0.08f, net_progress * 2.5f);
            if (course_stage_ != CourseStage::balance && (high_energy_stall || inefficient_vibration))
                micro_motion_seconds_ += progress_window_seconds_;
            else
                micro_motion_seconds_ = std::max(0.0f, micro_motion_seconds_ - 0.5f);
            progress_window_start_x_ = root_x;
            progress_window_seconds_ = 0.0f;
            action_energy_window_ = 0.0f;
            root_path_window_ = 0.0f;
        }

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;
        if (airborne_seconds_ > allowed_airtime)
            invalidate(InvalidMotion::sustained_flight);
        if (micro_motion_seconds_ >= 3.0f)
            invalidate(InvalidMotion::micro_motion);
    }

    StepResult Environment::step(std::span<const float, action_count> actions, float dt)
    {
        if (!blueprint_.valid() || !valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node) || !valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node))
            return { -5.0f, 0.0f, true, false, InvalidMotion::out_of_bounds };

        dt = clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        // Let every body settle onto its feet before the policy can apply a
        // meaningful impulse, then ease control in rather than launching it.
        const float ramp_t = clamp((elapsed_seconds_ - 0.35f) / 1.25f, 0.0f, 1.0f);
        const float control_ramp = ramp_t * ramp_t * (3.0f - 2.0f * ramp_t);
        std::array<float, action_count> applied_actions{};
        for (std::size_t index = 0; index < action_count; ++index)
            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;
        constexpr Vec2 gravity{ 0.0f, -22.0f };
        constexpr float damping = 0.996f;
        for (Particle& particle : particles_)
        {
            const Vec2 velocity = (particle.position - particle.previous) * damping;
            particle.previous = particle.position;
            particle.position += velocity + gravity * (dt * dt);
        }

        collided_this_step_ = false;
        rebuild_course_features();
        for (int iteration = 0; iteration < 14; ++iteration)
        {
            for (const DistanceConstraint& bone : blueprint_.bones)
                solve_distance(bone);
            for (std::size_t index = 0; index < action_count; ++index)
                solve_motor(blueprint_.motors[index], applied_actions[index]);
            solve_ground(dt);
            solve_course();
        }
        knee_first_this_step_ = knee_before_foot_fault();
        if (knee_first_this_step_)
            ++knee_first_faults_;
        if (collided_this_step_)
            collision_count_ += 1.0f;

        elapsed_seconds_ += dt;
        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;
        const float raw_speed = (pelvis_position.x - previous_pelvis_.x) / dt;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        const float frame_progress = pelvis_position.x - previous_pelvis_.x;
        previous_pelvis_ = pelvis_position;
        distance_travelled_ = pelvis_position.x - blueprint_.nodes[blueprint_.root_node].x;
        maximum_speed_kmh_ = std::max(maximum_speed_kmh_, std::max(std::abs(raw_speed), std::abs(forward_speed_)) * 3.6f);

        float action_energy = 0.0f;
        for (std::size_t index = 0; index < action_count; ++index)
        {
            const float effective = blueprint_.motors[index].enabled ? applied_actions[index] : 0.0f;
            action_energy += effective * effective;
            const float angle = joint_angle(blueprint_.motors[index]);
            angular_velocities_[index] = wrap_angle(angle - previous_angles_[index]) / dt;
            previous_angles_[index] = angle;
        }
        update_gait_metrics(dt, action_energy);

        const float upright = torso_uprightness();
        const float local_ground = ground_height_at(pelvis_position.x);
        const float torso_floor = local_ground + std::max(0.38f,
            (blueprint_.nodes[blueprint_.torso_node].y - blueprint_.nodes[blueprint_.root_node].y) * 0.28f);
        const float head_floor = local_ground + 0.34f;
        const float torso_y = particles_[blueprint_.torso_node].position.y;
        const float head_y = particles_[blueprint_.head_node].position.y;
        const bool geometric_fall = torso_y < torso_floor || head_y < head_floor;
        const bool hard_fall = torso_y < local_ground + 0.18f || head_y < local_ground + 0.12f;
        fallen_ = geometric_fall;

        float recovery_reward = 0.0f;
        const bool supported = contact_supported(blueprint_.left_contact_node)
            || contact_supported(blueprint_.right_contact_node);
        if (!recovery_active_ && recovery_should_start(
            collided_this_step_, upright, geometric_fall, hard_fall))
        {
            recovery_active_ = true;
            recovery_started_seconds_ = elapsed_seconds_;
            recovery_best_upright_ = upright;
            ++recovery_events_;
        }
        if (recovery_active_)
        {
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && !geometric_fall && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
            }
            else if (hard_fall || recovery_time > 3.0f)
            {
                recovery_active_ = false;
                recovery_reward -= 0.12f;
            }
        }

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;
        const float gated_upright = elapsed_seconds_ > 0.25f ? upright : 1.0f;
        const bool terminal_fall = recovery_terminal_fall(
            geometric_fall, hard_fall, recovery_active_);
        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, terminal_fall));

        const bool left_supported = contact_supported(blueprint_.left_contact_node);
        const bool right_supported = contact_supported(blueprint_.right_contact_node);
        const float left_contact = left_supported ? 1.0f : 0.0f;
        const float right_contact = right_supported ? 1.0f : 0.0f;
        const float contact = left_contact + right_contact;
        const bool single_support = left_supported != right_supported;
        const std::uint16_t swing_foot = left_supported
            ? blueprint_.right_contact_node : blueprint_.left_contact_node;
        const float swing_clearance = single_support && valid_node(swing_foot)
            ? particles_[swing_foot].position.y
                - ground_height_at(particles_[swing_foot].position.x)
                - particles_[swing_foot].radius
            : 0.0f;
        const float gait = non_foot_grounded_ ? 0.0f
            : gait_progress_multiplier(alternating_steps_, single_support, swing_clearance);
        const float safe_progress = clamp(frame_progress, -0.015f, 0.065f);
        const float collision_penalty = collided_this_step_ ? 0.070f : 0.0f;
        const float knee_first_penalty = knee_first_this_step_ ? 0.11f : 0.0f;
        const float stance_slip_penalty = clamp(stance_slip_speed_ - 0.08f, 0.0f, 4.0f) * 0.012f;
        const float wheel_penalty = wheel_sliding_motion(raw_speed,
            left_supported, right_supported, stance_slip_speed_) ? 0.055f : 0.0f;
        const float swing_reward = single_support && swing_clearance > 0.10f
            ? clamp(swing_clearance, 0.0f, 0.55f) * 0.005f : 0.0f;
        const float obstacle_lift_ratio = clamp(
            obstacle_lift_clearance_ / std::max(0.10f, obstacle_clearance_target_), 0.0f, 1.25f);
        const float obstacle_lift_reward = obstacle_approach_weight_
            * obstacle_lift_ratio * (single_support ? 0.020f : 0.007f);
        const float hazard_stall_penalty = obstacle_approach_weight_
            * clamp(hazard_stall_seconds_, 0.0f, 1.5f) * 0.022f;
        const float body_contact_penalty = non_foot_grounded_
            ? (head_ground_contact() ? 0.16f : 0.08f) : 0.0f;

        if (course_stage_ == CourseStage::balance)
        {
            last_reward_ = std::max(0.0f, upright) * 0.030f
                + contact * 0.0030f
                - std::abs(forward_speed_) * 0.0040f
                - std::abs(distance_travelled_) * 0.0015f
                - action_energy * 0.0012f
                - body_contact_penalty;
        }
        else
        {
            last_reward_ = std::max(0.0f, safe_progress) * 1.65f * gait
                + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f
                + swing_reward
                + obstacle_lift_reward
                - std::max(0.0f, -safe_progress) * 0.45f
                - action_energy * 0.0010f
                - collision_penalty
                - knee_first_penalty
                - stance_slip_penalty
                - wheel_penalty
                - hazard_stall_penalty
                - body_contact_penalty;
        }

        last_reward_ += recovery_reward;
        if (invalid_reason_ != InvalidMotion::none)
        {
            recovery_active_ = false;
            last_reward_ -= 5.0f;
        }
        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : static_cast<std::uint8_t>(course_stage_) >= static_cast<std::uint8_t>(CourseStage::hurdles)
                ? 32.0f : 24.0f;
        const bool terminated = invalid_reason_ != InvalidMotion::none || elapsed_seconds_ >= timeout;
        return { last_reward_, forward_speed_, terminated,
            invalid_reason_ == InvalidMotion::none, invalid_reason_ };
    }

    std::array<float, observation_count> Environment::observation() const noexcept
    {
        std::array<float, observation_count> result{};
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.left_contact_node) || !valid_node(blueprint_.right_contact_node))
            return result;

        const Vec2 root = particles_[blueprint_.root_node].position;
        const Vec2 torso = normalized(particles_[blueprint_.torso_node].position - root, { 0.0f, 1.0f });
        const Vec2 pelvis_velocity = particles_[blueprint_.root_node].position - particles_[blueprint_.root_node].previous;
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
            result[4 + index] = clamp(delta / span, -2.0f, 2.0f);
            result[8 + index] = clamp(angular_velocities_[index] / 18.0f, -3.0f, 3.0f);
        }
        result[12] = contact_supported(blueprint_.left_contact_node) ? 1.0f : 0.0f;
        result[13] = contact_supported(blueprint_.right_contact_node) ? 1.0f : 0.0f;
        result[14] = clamp((particles_[blueprint_.left_contact_node].position.x - root.x) / 2.0f, -2.0f, 2.0f);
        result[15] = clamp((particles_[blueprint_.right_contact_node].position.x - root.x) / 2.0f, -2.0f, 2.0f);
        result[16] = clamp((root.y - ground_height_at(root.x)) / 5.0f, 0.0f, 2.0f);
        result[17] = non_foot_grounded_ ? -1.0f
            : recovery_active_ ? clamp(torso.y, -1.0f, 1.0f) : 1.0f;
        result[18] = clamp((ground_height_at(root.x + 0.65f) - ground_height_at(root.x)) / 1.0f, -1.0f, 1.0f);
        result[19] = clamp((ground_height_at(root.x + 1.50f) - ground_height_at(root.x)) / 1.0f, -1.0f, 1.0f);
        result[20] = clamp((ground_height_at(root.x + 3.00f) - ground_height_at(root.x)) / 1.0f, -1.0f, 1.0f);

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
            result[21] = clamp(nearest_dx / 6.0f, -1.0f, 2.0f);
            switch (nearest->kind)
            {
            case CourseFeatureKind::hurdle: result[22] = -1.0f; break;
            case CourseFeatureKind::rock: result[22] = -0.5f; break;
            case CourseFeatureKind::overhead_bar: result[22] = 0.0f; break;
            case CourseFeatureKind::moving_hazard: result[22] = 0.5f; break;
            case CourseFeatureKind::projectile: result[22] = 1.0f; break;
            }
            result[23] = clamp((nearest->center.y - root.y) / 4.0f, -2.0f, 2.0f);
            result[24] = course_feature_observation_size(*nearest);
            result[25] = clamp(nearest->velocity.x / 5.0f, -1.0f, 1.0f);
        }
        result[26] = airborne_ratio();
        result[27] = clamp(static_cast<float>(alternating_steps_) / 10.0f, 0.0f, 2.0f);
        result[28] = static_cast<float>(course_stage_) / static_cast<float>(course_stage_count - 1);
        result[29] = course_difficulty_;
        return result;
    }
}
