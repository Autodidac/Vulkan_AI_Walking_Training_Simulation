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

namespace runner::sim
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

        [[nodiscard]] bool motor_references_node(const CreatureBlueprint& rig,
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

        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            auto add_foot = [&](std::uint16_t ankle)
            {
                std::array<std::uint16_t, 3> result{};
                if (ankle >= rig.nodes.size() || rig.nodes.size() > 122)
                    return result;

                const Vec2 ankle_position = rig.nodes[ankle];
                const float rear_radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.48f, 0.070f, 0.092f) : 0.080f;
                const float toe_radius = clamp(rear_radius * 0.86f, 0.060f, 0.080f);

                // Runner is a side-view simulation. Both feet point forward in
                // +X; mirroring one foot outward created the split stance seen
                // in the packaged preview.
                const auto heel = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x - heel_reach * 0.62f,
                    ankle_position.y - 0.205f
                });
                rig.radii.push_back(rear_radius);

                const auto ball = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x + toe_reach * 0.30f,
                    ankle_position.y - 0.212f
                });
                rig.radii.push_back(rear_radius);

                const auto toe = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x + toe_reach,
                    ankle_position.y - 0.195f
                });
                rig.radii.push_back(toe_radius);

                // The rear foot is a stable ankle/heel/ball triangle. The toe
                // is a separate segment hinged at the ball, so it can lift for
                // clearance and plantar-flex against the ground for push-off.
                rig.bones.push_back({ ankle, heel, 0.0f, 1.0f });
                rig.bones.push_back({ ankle, ball, 0.0f, 1.0f });
                rig.bones.push_back({ heel, ball, 0.0f, 1.0f });
                rig.bones.push_back({ ball, toe, 0.0f, 0.98f });
                result = { heel, ball, toe };
                return result;
            };

            const auto left = add_foot(rig.left_contact_node);
            const auto right = add_foot(rig.right_contact_node);
            if (left[0] != 0u && right[0] != 0u)
            {
                rig.left_contact_node = left[0];
                rig.right_contact_node = right[0];
                rig.additional_left_contact_nodes = { left[1], left[2] };
                rig.additional_right_contact_nodes = { right[1], right[2] };
            }
        }

        void calibrate_grounded_defaults(CreatureBlueprint& rig,
            float major_travel_degrees, float minor_travel_degrees,
            float major_linear_gain, float minor_linear_gain) noexcept
        {
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
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
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
                rig.calibrate_motor(index, travel, travel, 0.043f);
        }
    }

    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.40f }, { 0.72f, 2.48f },
            { 0.98f, 3.04f }, { 1.18f, 3.50f }, { 1.54f, 3.46f },
            { -0.92f, 2.64f }, { -1.36f, 2.84f },
            { -0.42f, 1.42f }, { -0.58f, 0.28f },
            { 0.42f, 1.42f }, { 0.58f, 0.28f },
            { 0.02f, 3.12f }
        };
        result.radii = {
            0.42f, 0.38f, 0.23f, 0.28f, 0.11f,
            0.24f, 0.13f, 0.18f, 0.14f, 0.18f, 0.14f, 0.27f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.98f },
            { 2, 3, 0.0f, 0.98f }, { 3, 4, 0.0f, 0.94f },
            { 0, 2, 0.0f, 0.94f }, { 1, 3, 0.0f, 0.94f },
            { 0, 5, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.88f },
            { 0, 6, 0.0f, 0.86f }, { 1, 5, 0.0f, 0.82f },
            { 0, 11, 0.0f, 0.96f }, { 1, 11, 0.0f, 0.92f },
            { 11, 2, 0.0f, 0.92f }, { 11, 3, 0.0f, 0.90f },
            { 11, 7, 0.0f, 0.84f }, { 11, 9, 0.0f, 0.84f },
            { 0, 7, 0.0f, 1.0f }, { 7, 8, 0.0f, 1.0f },
            { 0, 9, 0.0f, 1.0f }, { 9, 10, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 11, 0, 7 }, MotorConstraint{ 0, 7, 8 },
            MotorConstraint{ 11, 0, 9 }, MotorConstraint{ 0, 9, 10 }
        };
        result.active_motor_count = 4;
        result.root_node = 0;
        result.torso_node = 11;
        result.head_node = 3;
        result.left_contact_node = 8;
        result.right_contact_node = 10;
        add_passive_feet(result, 0.17f, 0.29f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 58.0f, 0.038f, 0.044f);
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
        CreatureBlueprint result{};
        result.nodes = {
            { -0.0034f, 2.8127f }, { -0.0060f, 4.2000f }, { -0.0060f, 4.9800f },
            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f },
            { -0.42f, 4.0200f }, { -0.78f, 3.43f }, { -0.60f, 2.76f },
            { 0.40f, 4.0200f }, { 0.76f, 3.43f }, { 0.58f, 2.76f }
        };
        result.radii = {
            0.26f, 0.31f, 0.27f, 0.19f, 0.17f, 0.19f, 0.17f,
            0.16f, 0.15f, 0.14f, 0.16f, 0.15f, 0.14f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f },
            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f }, { 8, 9, 0.0f, 0.96f },
            { 1, 10, 0.0f, 0.98f }, { 10, 11, 0.0f, 0.98f }, { 11, 12, 0.0f, 0.96f },
            { 7, 10, 0.0f, 0.72f },
            { 2, 7, 0.0f, 0.94f }, { 2, 10, 0.0f, 0.94f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 1, 7, 8 }, MotorConstraint{ 7, 8, 9 },
            MotorConstraint{ 1, 10, 11 }, MotorConstraint{ 10, 11, 12 }
        };
        result.active_motor_count = 8;
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        for (std::size_t index = 0; index < 4; ++index)
        {
            const bool knee = (index & 1u) != 0u;
            const MotorConstraint& motor = result.motors[index];
            const float driven_length = length(result.nodes[motor.c] - result.nodes[motor.pivot]);
            const float linear_gain = knee ? 0.051f : 0.045f;
            const float strength = linear_gain / std::max(0.75f, driven_length);
            result.calibrate_motor(index, knee ? 58.0f : 36.0f,
                knee ? 58.0f : 36.0f, strength);
        }
        result.calibrate_motor(4, 95.0f, 95.0f, 0.034f);
        result.calibrate_motor(5, 108.0f, 108.0f, 0.031f);
        result.calibrate_motor(6, 95.0f, 95.0f, 0.034f);
        result.calibrate_motor(7, 108.0f, 108.0f, 0.031f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::quadruped()
    {
        CreatureBlueprint result{};
        // A real planar quadruped: four separate legs, slightly staggered in x
        // so the near/far pairs remain visible in a side view. The two support
        // channels are diagonal pairs, allowing a stable trot with four policy
        // outputs instead of pretending that two articulated legs are four.
        result.nodes = {
            { 0.0f, 1.58f }, { 1.48f, 1.62f }, { 2.22f, 1.88f }, { -0.88f, 1.78f },
            { -0.46f, 0.24f }, { 0.08f, 0.28f },
            { 1.42f, 0.24f }, { 1.96f, 0.28f }
        };
        result.radii = { 0.30f, 0.31f, 0.25f, 0.15f, 0.16f, 0.15f, 0.16f, 0.15f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.95f }, { 0, 3, 0.0f, 0.84f },
            { 0, 4, 0.0f, 1.0f }, { 0, 5, 0.0f, 0.98f },
            { 1, 6, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.98f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        result.additional_left_contact_nodes = { 7 };
        result.additional_right_contact_nodes = { 5 };
        result.motors = {
            MotorConstraint{ 1, 0, 4 }, MotorConstraint{ 1, 0, 5 },
            MotorConstraint{ 0, 1, 6 }, MotorConstraint{ 0, 1, 7 }
        };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 52.0f);
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
        result.additional_left_contact_nodes = { 6 };
        result.additional_right_contact_nodes = { 4 };
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
            { 3, 4, 0.0f, 0.42f }, { 4, 5, 0.0f, 0.06f },
            { 5, 6, 0.0f, 0.42f }, { 6, 7, 0.0f, 0.06f }, { 7, 8, 0.0f, 0.42f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        // Each rigid foot plate belongs to one gait phase. Splitting the
        // endpoints of a single plate across left/right semantics made the
        // Stand teacher command the same plate in opposite directions.
        result.left_contact_node = 3;
        result.right_contact_node = 5;
        result.additional_left_contact_nodes = { 4, 7, 8 };
        result.additional_right_contact_nodes = { 6 };
        result.motors = {
            MotorConstraint{ 0, 9, 3 }, MotorConstraint{ 0, 10, 5 },
            MotorConstraint{ 1, 11, 7 }, MotorConstraint{ 0, 1, 2 }
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
        if (motor_index >= active_motor_count || motor_index >= motors.size())
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
        if (motor_index >= active_motor_count || motor_index >= motors.size())
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
        for (std::size_t index = 0; index < active_motor_count; ++index)
            calibrate_motor(index, degrees, degrees, power);
    }

    bool CreatureBlueprint::valid() const noexcept
    {
        if (nodes.size() < 3 || radii.size() != nodes.size() || bones.empty()
            || active_motor_count == 0 || active_motor_count > motors.size())
            return false;
        const auto semantic_valid = [this](std::uint16_t node) { return node < nodes.size(); };
        if (!semantic_valid(root_node) || !semantic_valid(torso_node) || !semantic_valid(head_node)
            || !semantic_valid(left_contact_node) || !semantic_valid(right_contact_node))
            return false;
        for (const std::uint16_t node : additional_left_contact_nodes)
        {
            if (!semantic_valid(node) || node == left_contact_node)
                return false;
        }
        for (const std::uint16_t node : additional_right_contact_nodes)
        {
            if (!semantic_valid(node) || node == right_contact_node)
                return false;
        }
        for (const DistanceConstraint& bone : bones)
        {
            if (bone.a >= nodes.size() || bone.b >= nodes.size() || bone.a == bone.b
                || !std::isfinite(bone.rest_length) || bone.rest_length <= 0.0f)
                return false;
        }
        for (std::size_t motor_index = 0; motor_index < active_motor_count; ++motor_index)
        {
            const MotorConstraint& motor = motors[motor_index];
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

        add_u64(nodes.size()); add_u64(bones.size()); add_u64(active_motor_count);
        add_u64(root_node); add_u64(torso_node); add_u64(head_node);
        add_u64(left_contact_node); add_u64(right_contact_node);
        add_u64(additional_left_contact_nodes.size());
        for (const std::uint16_t node : additional_left_contact_nodes) add_u64(node);
        add_u64(additional_right_contact_nodes.size());
        for (const std::uint16_t node : additional_right_contact_nodes) add_u64(node);
        for (std::size_t index = 0; index < nodes.size(); ++index)
        {
            add_float(nodes[index].x); add_float(nodes[index].y);
            add_float(index < radii.size() ? radii[index] : 0.15f);
        }
        for (const DistanceConstraint& bone : bones)
        {
            add_u64(bone.a); add_u64(bone.b); add_float(bone.rest_length); add_float(bone.stiffness);
        }
        for (std::size_t motor_index = 0; motor_index < active_motor_count; ++motor_index)
        {
            const MotorConstraint& motor = motors[motor_index];
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
            output << "RUNRIG 4\n";
            output << nodes.size() << ' ' << bones.size() << ' ' << active_motor_count << '\n';
            output << "S " << root_node << ' ' << torso_node << ' ' << head_node << ' '
                << left_contact_node << ' ' << right_contact_node << '\n';
            output << "L " << additional_left_contact_nodes.size();
            for (const std::uint16_t node : additional_left_contact_nodes) output << ' ' << node;
            output << '\n';
            output << "R " << additional_right_contact_nodes.size();
            for (const std::uint16_t node : additional_right_contact_nodes) output << ' ' << node;
            output << '\n';
            output << std::setprecision(9);
            for (std::size_t index = 0; index < nodes.size(); ++index)
                output << "N " << nodes[index].x << ' ' << nodes[index].y << ' ' << radii[index] << '\n';
            for (const DistanceConstraint& bone : bones)
                output << "B " << bone.a << ' ' << bone.b << ' ' << bone.rest_length << ' ' << bone.stiffness << '\n';
            for (std::size_t motor_index = 0; motor_index < active_motor_count; ++motor_index)
            {
                const MotorConstraint& motor = motors[motor_index];
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
        if (!input || magic != "RUNRIG"
            || (version != 1 && version != 2 && version != 3 && version != 4)
            || node_count < 3 || node_count > 128 || bone_count > 256
            || (motor_count != 4 && motor_count != action_count))
        {
            error = "Invalid or unsupported Runner rig file.";
            return humanoid();
        }

        CreatureBlueprint result{};
        result.active_motor_count = motor_count;
        for (MotorConstraint& motor : result.motors)
            motor.enabled = false;
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
        if (version >= 3)
        {
            auto read_supports = [&](char expected, std::vector<std::uint16_t>& nodes)
            {
                char tag{};
                std::size_t count{};
                input >> tag >> count;
                if (!input || tag != expected || count > node_count)
                    return false;
                nodes.resize(count);
                for (std::uint16_t& node : nodes)
                {
                    input >> node;
                    if (!input || node >= node_count)
                        return false;
                }
                return true;
            };
            if (!read_supports('L', result.additional_left_contact_nodes)
                || !read_supports('R', result.additional_right_contact_nodes))
            {
                error = "Invalid multi-foot support data.";
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
        if (!stage_uses_deformable_terrain(course_stage_))
            return 0.0f;
        return terrain_.height_at(x + course_progress());
    }

    float Environment::terrain_firmness_at(float x) const noexcept
    {
        return stage_uses_deformable_terrain(course_stage_)
            ? terrain_.firmness_at(x + course_progress()) : 1.0f;
    }

    float Environment::terrain_looseness_at(float x) const noexcept
    {
        return stage_uses_deformable_terrain(course_stage_)
            ? terrain_.looseness_at(x + course_progress()) : 0.0f;
    }

    void Environment::update_materials(float dt) noexcept
    {
        if (course_stage_ != CourseStage::moving_hazards)
        {
            material_particles_.clear();
            return;
        }
        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float interval = std::lerp(4.20f, 2.20f, course_difficulty_);
        while (elapsed_seconds_ >= next_material_event_seconds_)
        {
            ++material_event_sequence_;
            if (material_particles_.size() > 72u)
                std::erase_if(material_particles_, [](const MaterialParticle& item) { return !item.active; });
            const float spawn_x = root_x + 3.2f + random_unit() * 3.0f
                + (random_unit() - 0.5f) * 1.4f;
            if ((material_event_sequence_ % 4u) == 0u)
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
                constexpr std::size_t burst_count = 10u;
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
                terrain_.deposit(item.position.x + course_progress(),
                    std::clamp(item.radius * item.radius * 2.8f, 0.004f, 0.025f), 0.18f);
                item.active = false;
            }
            else
            {
                item.velocity.y = std::abs(item.velocity.y) * 0.16f;
                item.velocity.x *= 0.72f;
                if (std::abs(item.velocity.x) < 0.08f && std::abs(item.velocity.y) < 0.08f)
                {
                    terrain_.deposit(item.position.x + course_progress(), item.radius * 0.12f, item.density);
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

    void Environment::append_material_features() noexcept
    {
        int marker = -1000;
        for (const MaterialParticle& item : material_particles_)
        {
            if (!item.active)
                continue;
            course_features_.push_back({ item.kind == MaterialKind::sand
                    ? CourseFeatureKind::projectile : CourseFeatureKind::moving_hazard,
                item.position, {}, item.radius, item.velocity, marker-- });
        }
    }

    void Environment::apply_support_pressure(float dt) noexcept
    {
        if (!stage_uses_deformable_terrain(course_stage_))
            return;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!particles_[index].grounded || !blueprint_.is_support_seed(index))
                continue;
            const Particle& particle = particles_[index];
            const float slip = std::abs((particle.position.x - particle.previous.x)
                / std::max(dt, 1.0e-5f));
            const float load = std::clamp(1.0f / std::max(particle.inverse_mass, 0.15f), 0.5f, 3.5f);
            terrain_.apply_pressure(particle.position.x + course_progress(), load, slip, dt);
        }
    }

    void Environment::update_material_metrics(float dt) noexcept
    {
        if (!valid_node(blueprint_.root_node))
            return;
        const Particle& root = particles_[blueprint_.root_node];
        terrain_firmness_ = terrain_firmness_at(root.position.x);
        terrain_looseness_ = terrain_looseness_at(root.position.x);
        const float prior_burial = burial_depth_;
        burial_depth_ = 0.0f;
        obstruction_mask_ = 0u;
        auto measure = [&](std::uint16_t node, std::uint8_t mask)
        {
            if (!valid_node(node))
                return;
            const Particle& particle = particles_[node];
            const float depth = ground_height_at(particle.position.x)
                - (particle.position.y - particle.radius);
            burial_depth_ = std::max(burial_depth_, std::max(0.0f, depth));
            if (depth > particle.radius * 0.38f)
                obstruction_mask_ = static_cast<std::uint8_t>(obstruction_mask_ | mask);
        };
        measure(blueprint_.head_node, 0x1u);
        measure(blueprint_.torso_node, 0x2u);
        measure(blueprint_.left_contact_node, 0x4u);
        measure(blueprint_.right_contact_node, 0x4u);
        float left_density = 0.0f;
        float right_density = 0.0f;
        incoming_time_to_impact_ = 10.0f;
        incoming_material_velocity_ = {};
        incoming_material_density_ = 0.0f;
        for (const MaterialParticle& item : material_particles_)
        {
            if (!item.active)
                continue;
            const Vec2 delta = item.position - root.position;
            const float distance = length(delta);
            if (delta.x < 0.0f && distance < 3.5f)
                left_density += item.density;
            else if (delta.x >= 0.0f && distance < 3.5f)
                right_density += item.density;
            const Vec2 relative = item.velocity - Vec2{ forward_speed_, 0.0f };
            const float closing = -dot(normalized(delta, { 0.0f, 1.0f }), relative);
            if (closing <= 0.05f)
                continue;
            const float time = distance / std::max(closing, 0.05f);
            if (time < incoming_time_to_impact_)
            {
                incoming_time_to_impact_ = time;
                incoming_material_velocity_ = relative;
                incoming_material_density_ = item.density;
            }
        }
        const float left_surface = std::min(
            ground_height_at(root.position.x - 0.85f),
            ground_height_at(root.position.x - 1.35f));
        const float right_surface = std::min(
            ground_height_at(root.position.x + 0.85f),
            ground_height_at(root.position.x + 1.35f));
        const float left_space = root.position.y + root.radius - left_surface
            - left_density * 0.18f;
        const float right_space = root.position.y + root.radius - right_surface
            - right_density * 0.18f;
        const float space_delta = right_space - left_space;
        free_space_direction_ = std::abs(space_delta) < 0.06f
            ? 0.0f : (space_delta > 0.0f ? 1.0f : -1.0f);
        auto node_burial_depth = [&](std::uint16_t node) noexcept
        {
            if (!valid_node(node))
                return 0.0f;
            const Particle& particle = particles_[node];
            return ground_height_at(particle.position.x)
                - (particle.position.y - particle.radius);
        };
        const float head_burial = node_burial_depth(blueprint_.head_node);
        const float torso_burial = node_burial_depth(blueprint_.torso_node);
        const float left_wall = ground_height_at(root.position.x - 0.70f)
            - (root.position.y - root.radius);
        const float right_wall = ground_height_at(root.position.x + 0.70f)
            - (root.position.y - root.radius);
        const bool trapped = burial_depth_ > 0.32f
            && head_burial > 0.18f && torso_burial > 0.18f
            && left_wall > 0.18f && right_wall > 0.18f;
        buried_no_escape_seconds_ = trapped
            ? buried_no_escape_seconds_ + dt
            : std::max(0.0f, buried_no_escape_seconds_ - dt * 2.0f);
        if (buried_no_escape_seconds_ > 2.25f)
            invalidate(InvalidMotion::buried_no_escape);
        previous_burial_depth_ = prior_burial;
    }

    void Environment::rebuild_course_features() noexcept
    {
        course_features_.clear();
        if (course_stage_ != CourseStage::duck_press
            && course_stage_ != CourseStage::crouch_walk
            && course_stage_ != CourseStage::hurdles
            && course_stage_ != CourseStage::moving_hazards)
            return;

        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float progress = course_progress();
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
                const float press_anchor_x = blueprint_.root_node < blueprint_.nodes.size()
                    ? blueprint_.nodes[blueprint_.root_node].x : 0.0f;
                const float authored_reach = std::max(
                    std::abs(minimum_x - press_anchor_x),
                    std::abs(maximum_x - press_anchor_x));
                const float half_width = clamp(authored_reach + 0.34f, 0.82f, 2.80f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                // The press stays fixed over the authored station even if the
                // live rig slides, while still spanning the complete body plan.
                constexpr float half_height = 0.14f;
                course_features_.push_back({
                    CourseFeatureKind::duck_press,
                    { press_anchor_x, profile.bottom_y + half_height },
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
            const float minimum_approach = course_stage_ == CourseStage::hurdles ? 6.5f : 5.0f;
            if ((course_stage_ == CourseStage::hurdles
                    || course_stage_ == CourseStage::moving_hazards)
                && x < root_x + minimum_approach)
                continue;
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
            case CourseFeatureKind::duck_press:
                break;
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
        append_material_features();
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
            const bool contact_semantic = blueprint_.is_support_seed(index);
            const std::size_t degree = static_cast<std::size_t>(std::ranges::count_if(
                blueprint_.bones, [index](const DistanceConstraint& bone)
                {
                    return bone.a == index || bone.b == index;
                }));
            float inverse_mass = 1.0f;
            if (contact_semantic)
                inverse_mass = 0.58f;
            else if (index == blueprint_.head_node)
                inverse_mass = 0.72f;
            else if (passive_endpoint(blueprint_, index))
                inverse_mass = 0.68f;
            else if (degree == 1u && index != blueprint_.root_node
                && index != blueprint_.torso_node)
                inverse_mass = 0.92f;
            particles_.push_back({ position, position, inverse_mass, radius, false });
        }

        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
        previous_torso_angle_ = torso_roll_angle();
        previous_angles_.fill(0.0f);
        angular_velocities_.fill(0.0f);
        previous_applied_actions_.fill(0.0f);
        articulated_toe_commands_.fill(0.0f);
        previous_articulated_toe_angles_.fill(0.0f);
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
            previous_angles_[index] = joint_angle(blueprint_.motors[index]);
        for (std::size_t side = 0; side < previous_articulated_toe_angles_.size(); ++side)
        {
            MotorConstraint toe_motor{};
            if (articulated_toe_motor(side == 0u, toe_motor))
                previous_articulated_toe_angles_[side] = joint_angle(toe_motor);
        }
        elapsed_seconds_ = 0.0f;
        distance_travelled_ = 0.0f;
        forward_speed_ = 0.0f;
        last_reward_ = 0.0f;
        fallen_ = false;
        collision_count_ = 0.0f;
        airborne_seconds_ = 0.0f;
        cumulative_airborne_ = 0.0f;
        duck_seconds_ = 0.0f;
        duck_depth_ = 0.0f;
        duck_obstacle_weight_ = 0.0f;
        duck_clearance_margin_ = 0.0f;
        duck_press_hold_seconds_ = 0.0f;
        duck_body_contact_seconds_ = 0.0f;
        duck_press_max_penetration_ = 0.0f;
        duck_walk_started_seconds_ = 0.0f;
        crouch_walk_seconds_ = 0.0f;
        crouch_walk_distance_ = 0.0f;
        torso_swing_seconds_ = 0.0f;
        current_duck_hold_seconds_ = 0.0f;
        stable_stance_seconds_ = 0.0f;
        longest_stable_stance_seconds_ = 0.0f;
        stance_failure_grace_seconds_ = 0.0f;
        posture_failure_seconds_ = 0.0f;
        maximum_joint_speed_ = 0.0f;
        duck_recovery_count_ = 0;
        duck_cycle_qualified_ = false;
        duck_press_contact_this_step_ = false;
        duck_press_contact_seen_ = false;
        duck_press_hold_qualified_ = false;
        duck_press_completed_ = false;
        current_airborne_rotation_ = 0.0f;
        maximum_spin_turns_ = 0.0f;
        uncontrolled_spin_turns_ = 0.0f;
        powered_jump_count_ = 0;
        landed_jump_count_ = 0;
        spin_landing_count_ = 0;
        obstacles_passed_ = 0;
        last_passed_feature_sequence_ = course_safe_runway_markers - 1;
        duck_active_ = false;
        powered_takeoff_ = false;
        powered_takeoff_this_step_ = false;
        powered_landing_this_step_ = false;
        spin_landing_this_step_ = false;
        passed_obstacle_this_step_ = false;
        collision_contact_active_ = false;
        collision_event_this_step_ = false;
        progress_window_seconds_ = 0.0f;
        progress_window_start_x_ = previous_pelvis_.x;
        micro_motion_seconds_ = 0.0f;
        action_energy_window_ = 0.0f;
        root_path_window_ = 0.0f;
        previous_root_for_path_ = previous_pelvis_;
        last_step_time_ = -100.0f;
        last_step_x_ = previous_pelvis_.x;
        left_swing_seconds_ = 0.0f;
        right_swing_seconds_ = 0.0f;
        left_swing_clearance_ = 0.0f;
        right_swing_clearance_ = 0.0f;
        action_change_energy_ = 0.0f;
        alternating_step_this_step_ = false;
        maximum_speed_kmh_ = 0.0f;
        alternating_steps_ = 0;
        single_leg_cycles_ = 0;
        last_single_leg_landing_x_ = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        progress_window_start_steps_ = 0;
        knee_first_faults_ = 0;
        wheel_sliding_seconds_ = 0.0f;
        body_rolling_seconds_ = 0.0f;
        foot_pivot_rolling_seconds_ = 0.0f;
        zero_progress_seconds_ = 0.0f;
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
        terrain_.reset(random_state_ ^ 0xa5a5a5a5a5a5a5a5ULL, course_difficulty_);
        material_particles_.clear();
        next_material_event_seconds_ = 1.50f;
        material_event_sequence_ = 0u;
        terrain_firmness_ = 1.0f;
        terrain_looseness_ = 0.0f;
        burial_depth_ = 0.0f;
        previous_burial_depth_ = 0.0f;
        buried_no_escape_seconds_ = 0.0f;
        free_space_direction_ = 0.0f;
        incoming_material_velocity_ = {};
        incoming_time_to_impact_ = 10.0f;
        incoming_material_density_ = 0.0f;
        obstruction_mask_ = 0u;
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

    void Environment::separate_support_clusters() noexcept
    {
        std::array<std::uint16_t, 32> supports{};
        std::size_t support_count = 0;
        auto append = [&](std::uint16_t node)
        {
            if (!valid_node(node) || support_count >= supports.size())
                return;
            if (std::find(supports.begin(), supports.begin() + support_count, node)
                == supports.begin() + support_count)
                supports[support_count++] = node;
        };
        append(blueprint_.left_contact_node);
        append(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            append(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            append(node);

        for (std::size_t first = 0; first < support_count; ++first)
        {
            const std::uint16_t first_index = supports[first];
            Particle& lhs = particles_[first_index];
            for (std::size_t second = first + 1; second < support_count; ++second)
            {
                const std::uint16_t second_index = supports[second];
                if (first_index == second_index
                    || direct_bone(blueprint_, first_index, second_index))
                    continue;
                Particle& rhs = particles_[second_index];
                const float minimum_gap = lhs.radius + rhs.radius + 0.035f;
                const float horizontal = rhs.position.x - lhs.position.x;
                if (std::abs(horizontal) >= minimum_gap)
                    continue;
                float authored_direction = blueprint_.nodes[second_index].x
                    - blueprint_.nodes[first_index].x;
                if (std::abs(authored_direction) < 1.0e-4f)
                    authored_direction = horizontal;
                const float direction = authored_direction < 0.0f ? -1.0f : 1.0f;
                const float correction = (minimum_gap - std::abs(horizontal)) * 0.5f;
                lhs.position.x -= direction * correction;
                lhs.previous.x -= direction * correction * 0.35f;
                rhs.position.x += direction * correction;
                rhs.previous.x += direction * correction * 0.35f;
            }
        }
    }

    bool Environment::body_integrity_valid() const noexcept
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
            if (!std::isfinite(ratio) || ratio < 0.20f || ratio > 2.50f)
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
        if (torso_ratio < 0.25f || torso_ratio > 2.00f
            || head_ratio < 0.25f || head_ratio > 2.00f)
            return false;
        if (dot(normalized(torso_segment, { 0.0f, 1.0f }),
                normalized(head_segment, { 0.0f, 1.0f })) < -0.50f)
            return false;

        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            const float rest_radius = length(blueprint_.nodes[index]
                - blueprint_.nodes[blueprint_.root_node]);
            const float current_radius = length(particles_[index].position - root);
            if (current_radius > std::max(1.80f, rest_radius * 3.00f + 0.80f))
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

    void Environment::stabilize_balance_posture() noexcept
    {
        if (course_stage_ != CourseStage::balance
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        Vec2 rest_support{};
        Vec2 current_support{};
        std::size_t support_count = 0u;
        auto accumulate = [&](std::size_t index)
        {
            if (index >= blueprint_.nodes.size() || index >= particles_.size())
                return;
            rest_support += blueprint_.nodes[index];
            current_support += particles_[index].position;
            ++support_count;
        };
        accumulate(blueprint_.left_contact_node);
        accumulate(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate(node);
        if (support_count == 0u)
            return;

        rest_support /= static_cast<float>(support_count);
        current_support /= static_cast<float>(support_count);
        const bool horizontal = blueprint_.horizontal_body_plan();
        auto guide = [&](std::size_t node, float strength, float maximum_step)
        {
            if (node >= particles_.size() || node >= blueprint_.nodes.size()
                || blueprint_.is_support_seed(node))
                return;
            const Vec2 target = current_support + (blueprint_.nodes[node] - rest_support);
            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * strength;
            particles_[node].position += applied;
            particles_[node].previous += applied * 0.94f;
        };

        if (horizontal)
        {
            // Horizontal body plans need every articulated body link held near
            // its authored support-relative pose during the initial Stand
            // lesson. Guiding only root/torso/head leaves long multi-leg rigs
            // free to invert around their foot plates before PPO can learn.
            for (std::size_t node = 0; node < particles_.size(); ++node)
                guide(node, 0.22f, 0.060f);
        }
        else
        {
            guide(blueprint_.root_node, 0.16f, 0.035f);
            guide(blueprint_.torso_node, 0.12f, 0.030f);
            guide(blueprint_.head_node, 0.08f, 0.025f);
        }
    }

    void Environment::stabilize_duck_posture() noexcept
    {
        if (course_stage_ != CourseStage::duck_press
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        Vec2 rest_support{};
        Vec2 current_support{};
        std::size_t support_count = 0u;
        auto accumulate_support = [&](std::size_t node)
        {
            if (node >= blueprint_.nodes.size() || node >= particles_.size())
                return;
            rest_support += blueprint_.nodes[node];
            current_support += particles_[node].position;
            ++support_count;
        };
        accumulate_support(blueprint_.left_contact_node);
        accumulate_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate_support(node);
        if (support_count == 0u)
            return;
        rest_support /= static_cast<float>(support_count);
        current_support /= static_cast<float>(support_count);

        const float rest_head_top = blueprint_.nodes[blueprint_.head_node].y
            + particles_[blueprint_.head_node].radius;
        const DuckPressProfile profile = duck_press_profile(
            elapsed_seconds_, course_difficulty_, rest_head_top);
        const float rest_height = std::max(0.65f, rest_head_top - rest_support.y);
        const float requested_drop = clamp(rest_head_top - profile.bottom_y,
            0.0f, rest_height * 0.48f);
        const bool recovery_guide = duck_press_contact_seen_
            && requested_drop <= 0.001f;
        const bool settle_guide = !duck_press_contact_seen_
            && requested_drop <= 0.001f;

        const float vertical_scale = clamp(
            (rest_height - requested_drop) / rest_height, 0.52f, 1.0f);
        const float horizontal_scale = 1.0f + (1.0f - vertical_scale) * 0.12f;
        const float phase_strength = (recovery_guide || settle_guide)
            ? 1.0f : clamp(requested_drop / 0.48f, 0.0f, 1.0f);

        auto pin_support = [&](std::size_t node)
        {
            if (node >= particles_.size() || node >= blueprint_.nodes.size())
                return;
            Particle& support = particles_[node];
            const float authored_x = blueprint_.nodes[node].x;
            support.position.x = lerp(support.position.x, authored_x, 0.72f);
            support.position.y = ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.previous = support.position;
            support.grounded = true;
        };
        pin_support(blueprint_.left_contact_node);
        pin_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            pin_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            pin_support(node);

        for (std::size_t node = 0; node < particles_.size(); ++node)
        {
            if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                continue;
            const Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
            Vec2 target = current_support + Vec2{
                rest_offset.x * horizontal_scale,
                rest_offset.y * vertical_scale
            };
            const float floor = ground_height_at(target.x)
                + particles_[node].radius + 0.14f;
            if (!recovery_guide)
                target.y = std::min(target.y,
                    profile.bottom_y - particles_[node].radius - 0.035f);
            // Floor authority is final. The old order could force knees and
            // torso nodes below ground after they had already been clamped.
            target.y = std::max(target.y, floor);

            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            constexpr float maximum_step = 0.60f;
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * phase_strength;
            particles_[node].position += applied;
            particles_[node].previous += applied;
        }
    }

    bool Environment::articulated_toe_motor(bool left,
        MotorConstraint& motor) const noexcept
    {
        const std::uint16_t heel = left
            ? blueprint_.left_contact_node : blueprint_.right_contact_node;
        const auto& extra = left
            ? blueprint_.additional_left_contact_nodes
            : blueprint_.additional_right_contact_nodes;
        if (extra.size() < 2u)
            return false;
        const std::uint16_t ball = extra[0];
        const std::uint16_t toe = extra[1];
        if (!valid_node(heel) || !valid_node(ball) || !valid_node(toe))
            return false;

        std::uint16_t ankle = std::numeric_limits<std::uint16_t>::max();
        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            std::uint16_t candidate = std::numeric_limits<std::uint16_t>::max();
            if (bone.a == ball)
                candidate = bone.b;
            else if (bone.b == ball)
                candidate = bone.a;
            if (candidate == heel || candidate == toe
                || candidate >= blueprint_.nodes.size())
                continue;
            if (direct_bone(blueprint_, candidate, heel))
            {
                ankle = candidate;
                break;
            }
        }
        if (ankle >= blueprint_.nodes.size())
            return false;

        motor = MotorConstraint{ ankle, ball, toe };
        motor.neutral_angle = signed_angle(
            blueprint_.nodes[ankle] - blueprint_.nodes[ball],
            blueprint_.nodes[toe] - blueprint_.nodes[ball]);
        motor.minimum_angle = motor.neutral_angle - degrees_to_radians(42.0f);
        motor.maximum_angle = motor.neutral_angle + degrees_to_radians(36.0f);
        motor.strength = 0.022f;
        motor.enabled = true;
        return true;
    }

    void Environment::update_articulated_toe_commands(
        std::span<const float, action_count> actions, float dt) noexcept
    {
        auto update_side = [&](bool left, std::size_t side,
            std::size_t hip_index, std::size_t knee_index)
        {
            MotorConstraint toe_motor{};
            if (!articulated_toe_motor(left, toe_motor))
            {
                articulated_toe_commands_[side] = 0.0f;
                return;
            }

            const std::uint16_t heel = left
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const bool supported = contact_supported(heel);
            const float hip = hip_index < blueprint_.active_motor_count
                ? actions[hip_index] : 0.0f;
            const float knee = knee_index < blueprint_.active_motor_count
                ? actions[knee_index] : 0.0f;
            const float chain_effort = clamp(
                0.5f * (std::abs(hip) + std::abs(knee)), 0.0f, 1.0f);

            float desired = 0.0f;
            if (course_stage_ == CourseStage::duck_press)
            {
                desired = 0.16f + chain_effort * 0.24f;
            }
            else if (stage_requires_forward_gait(course_stage_)
                || stage_allows_powered_airtime(course_stage_))
            {
                desired = supported
                    ? -(0.30f + chain_effort * 0.42f)
                    : 0.46f + chain_effort * 0.18f;
            }
            articulated_toe_commands_[side] = rate_limited_toe_command(
                articulated_toe_commands_[side],
                clamp(desired, -0.90f, 0.80f), dt, supported, course_stage_);
        };

        update_side(true, 0u, 0u, 1u);
        update_side(false, 1u, 2u, 3u);
    }

    void Environment::solve_articulated_toes() noexcept
    {
        for (std::size_t side = 0; side < articulated_toe_commands_.size(); ++side)
        {
            MotorConstraint toe_motor{};
            if (articulated_toe_motor(side == 0u, toe_motor))
                solve_motor(toe_motor, articulated_toe_commands_[side]);
        }
    }

    void Environment::limit_articulated_toe_rates(float dt) noexcept
    {
        for (std::size_t side = 0; side < previous_articulated_toe_angles_.size(); ++side)
        {
            const bool left = side == 0u;
            MotorConstraint toe_motor{};
            if (!articulated_toe_motor(left, toe_motor))
                continue;
            const std::uint16_t heel = left
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const bool supported = contact_supported(heel);
            const float current = joint_angle(toe_motor);
            const float prior = previous_articulated_toe_angles_[side];
            const float maximum_delta = toe_angular_rate_limit(
                supported, course_stage_) * dt;
            const float bounded_delta = clamp(wrap_angle(current - prior),
                -maximum_delta, maximum_delta);
            const float bounded = wrap_angle(prior + bounded_delta);
            const float correction = wrap_angle(bounded - current);
            if (std::abs(correction) > 1.0e-6f
                && toe_motor.pivot < particles_.size()
                && toe_motor.c < particles_.size())
            {
                Particle& toe = particles_[toe_motor.c];
                const Vec2 pivot = particles_[toe_motor.pivot].position;
                const Vec2 corrected = pivot + rotate(toe.position - pivot, correction);
                const Vec2 translation = corrected - toe.position;
                toe.position = corrected;
                toe.previous += translation;

                const float minimum_y = ground_height_at(toe.position.x)
                    + ground_contact_offset(true, toe.radius);
                if (toe.position.y < minimum_y)
                {
                    const float lift = minimum_y - toe.position.y;
                    toe.position.y += lift;
                    toe.previous.y += lift;
                }
                toe.grounded = toe.position.y <= minimum_y + 0.0025f;
            }
            previous_articulated_toe_angles_[side] = joint_angle(toe_motor);
        }
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

        // Cutting the pivot-to-driven connection divides the rig into the driven
        // limb subtree and the complete remaining body. The complete parent side
        // must react; rotating only A's local chain leaves the pelvis and sibling
        // limbs numerically anchored against the motor.
        std::array<bool, 128> driven_component{};
        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> stack{};
        std::size_t stack_size = 0;
        visited[motor.pivot] = true;
        visited[motor.c] = true;
        driven_component[motor.c] = true;
        stack[stack_size++] = motor.c;
        while (stack_size > 0)
        {
            const std::uint16_t node = stack[--stack_size];
            for (const DistanceConstraint& bone : blueprint_.bones)
            {
                // Weak visual/spacing braces must not merge independent limbs
                // into one motor reaction component.
                if (bone.stiffness < 0.20f)
                    continue;
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == node)
                    next = bone.b;
                else if (bone.b == node)
                    next = bone.a;
                if (next < particles_.size() && !visited[next])
                {
                    visited[next] = true;
                    driven_component[next] = true;
                    stack[stack_size++] = next;
                }
            }
        }

        std::array<bool, 128> reference_component{};
        for (std::size_t index = 0; index < particles_.size(); ++index)
            reference_component[index] = !driven_component[index];

        auto inverse_rotational_inertia = [&](const std::array<bool, 128>& component) noexcept
        {
            double inertia = 0.0;
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                if (!component[index])
                    continue;
                const Particle& particle = particles_[index];
                const Vec2 arm = particle.position - pivot;
                const double radius_squared = static_cast<double>(dot(arm, arm));
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                inertia += mass * std::max(radius_squared, 1.0e-6);
            }
            return inertia > 1.0e-9 ? static_cast<float>(1.0 / inertia) : 0.0f;
        };

        auto center_of_mass = [&]() noexcept
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (const Particle& particle : particles_)
            {
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                weighted_y += static_cast<double>(particle.position.y) * mass;
                total_mass += mass;
            }
            if (total_mass <= 1.0e-9)
                return pivot;
            return Vec2{
                static_cast<float>(weighted_x / total_mass),
                static_cast<float>(weighted_y / total_mass)
            };
        };

        const float driven_inverse_inertia = inverse_rotational_inertia(driven_component);
        const float reference_inverse_inertia = inverse_rotational_inertia(reference_component);
        const float total_inverse_inertia = driven_inverse_inertia + reference_inverse_inertia;
        float driven_rotation = -correction;
        float reference_rotation = 0.0f;
        if (total_inverse_inertia > 1.0e-8f)
        {
            driven_rotation = -correction * driven_inverse_inertia / total_inverse_inertia;
            reference_rotation = correction * reference_inverse_inertia / total_inverse_inertia;
        }

        const Vec2 center_before = center_of_mass();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, driven_rotation);
            else if (index != motor.pivot)
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, reference_rotation);
        }
        const Vec2 center_correction = center_before - center_of_mass();
        for (Particle& particle : particles_)
            particle.position += center_correction;
    }

    bool Environment::contact_cluster_contains(std::uint16_t contact_node,
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

    float Environment::maximum_upper_body_motor_deviation() const noexcept
    {
        float maximum = 0.0f;
        for (std::size_t index = 4; index < blueprint_.active_motor_count; ++index)
        {
            const MotorConstraint& motor = blueprint_.motors[index];
            if (!motor.enabled)
                continue;
            maximum = std::max(maximum,
                std::abs(wrap_angle(joint_angle(motor) - motor.neutral_angle)));
        }
        return maximum;
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
            const bool traction_contact = contact_cluster_contains(
                blueprint_.left_contact_node, index)
                || contact_cluster_contains(blueprint_.right_contact_node, index);
            const float firmness = terrain_firmness_at(particle.position.x);
            const float looseness = terrain_looseness_at(particle.position.x);
            const float burial_allowance = stage_uses_deformable_terrain(course_stage_)
                ? (traction_contact ? (1.0f - firmness) * 0.055f
                    : std::min(particle.radius * 0.78f,
                        (1.0f - firmness + looseness * 0.45f) * 0.18f))
                : 0.0f;
            const float minimum_y = ground_height_at(particle.position.x)
                + ground_contact_offset(traction_contact, particle.radius) - burial_allowance;
            if (particle.position.y <= minimum_y + 0.0025f)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact && stage_uses_deformable_terrain(course_stage_))
                    retention = std::lerp(0.24f, 0.015f, firmness);
                if (blueprint_.is_support_seed(index))
                {
                    const float stance_retention = (course_stage_ == CourseStage::balance
                            || course_stage_ == CourseStage::duck_press)
                        ? 0.004f : 0.024f;
                    retention = std::min(retention, stance_retention);
                }
                particle.previous.x = particle.position.x - velocity.x * retention * dt;
                if (traction_contact)
                    particle.previous.y = particle.position.y;
                else if (velocity.y < 0.0f)
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
                if (feature.kind == CourseFeatureKind::duck_press)
                {
                    const float left = feature.center.x - feature.half_extent.x;
                    const float right = feature.center.x + feature.half_extent.x;
                    const float bottom = feature.center.y - feature.half_extent.y;
                    const float top = feature.center.y + feature.half_extent.y;
                    const bool horizontal_overlap = particle.position.x + particle.radius > left
                        && particle.position.x - particle.radius < right;
                    const bool vertical_overlap = particle.position.y + particle.radius > bottom
                        && particle.position.y - particle.radius < top;
                    if (!horizontal_overlap || !vertical_overlap)
                        continue;
                    const float penetration = particle.position.y + particle.radius - bottom;
                    duck_press_max_penetration_ = std::max(duck_press_max_penetration_, penetration);
                    if (penetration > 0.0f)
                    {
                        const Vec2 correction{ 0.0f, -penetration };
                        // Move the current and previous positions together so the
                        // vertical constraint preserves velocity instead of injecting a
                        // fresh downward impulse on every solver iteration. The press has
                        // no horizontal authority and therefore cannot drag the rig back.
                        particle.position += correction;
                        particle.previous += correction;
                        duck_press_contact_this_step_ = true;
                    }
                    continue;
                }
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

    float Environment::primary_support_span_ratio() const noexcept
    {
        if (!valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node)
            || blueprint_.left_contact_node >= blueprint_.nodes.size()
            || blueprint_.right_contact_node >= blueprint_.nodes.size())
            return 1.0f;
        const float rest_span = std::abs(
            blueprint_.nodes[blueprint_.right_contact_node].x
            - blueprint_.nodes[blueprint_.left_contact_node].x);
        if (rest_span < 0.08f)
            return 1.0f;
        const float current_span = std::abs(
            particles_[blueprint_.right_contact_node].position.x
            - particles_[blueprint_.left_contact_node].position.x);
        return current_span / rest_span;
    }

    bool Environment::current_display_posture_valid() const noexcept
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
        const Vec2 head = particles_[blueprint_.head_node].position;
        const bool support_layout_valid = !blueprint_.paired_leg_chains()
            || (primary_support_span_ratio() >= 0.48f
                && primary_support_span_ratio() <= 1.85f);
        return torso_uprightness() >= 0.60f
            && head.y > root.y + 0.20f
            && support_layout_valid;
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
        const bool was_supported = previous_left_grounded_ || previous_right_grounded_;
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;
        const int strike_side = new_left == new_right ? 0 : (new_left ? -1 : 1);
        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float left_clearance = contact_cluster_clearance(blueprint_.left_contact_node);
        const float right_clearance = contact_cluster_clearance(blueprint_.right_contact_node);
        if (!left)
        {
            left_swing_seconds_ += dt;
            left_swing_clearance_ = std::max(left_swing_clearance_, left_clearance);
        }
        if (!right)
        {
            right_swing_seconds_ += dt;
            right_swing_clearance_ = std::max(right_swing_clearance_, right_clearance);
        }

        alternating_step_this_step_ = false;
        if (strike_side != 0)
        {
            const float swing_air_seconds = new_left ? left_swing_seconds_ : right_swing_seconds_;
            const float swing_clearance = new_left ? left_swing_clearance_ : right_swing_clearance_;
            if (last_contact_side_ == 0)
            {
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
            else if (qualifies_supported_step(last_contact_side_, strike_side,
                elapsed_seconds_ - last_step_time_, root_x - last_step_x_,
                swing_air_seconds, swing_clearance))
            {
                ++alternating_steps_;
                alternating_step_this_step_ = true;
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
        }
        if (left)
        {
            left_swing_seconds_ = 0.0f;
            left_swing_clearance_ = 0.0f;
        }
        if (right)
        {
            right_swing_seconds_ = 0.0f;
            right_swing_clearance_ = 0.0f;
        }

        const float left_slip = left
            ? contact_cluster_horizontal_speed(blueprint_.left_contact_node, dt) : 0.0f;
        const float right_slip = right
            ? contact_cluster_horizontal_speed(blueprint_.right_contact_node, dt) : 0.0f;
        stance_slip_speed_ = left_slip + right_slip;
        const float root_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.x
                - particles_[blueprint_.root_node].previous.x) / std::max(dt, 1.0e-5f)
            : 0.0f;
        const float root_vertical_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.y
                - particles_[blueprint_.root_node].previous.y) / std::max(dt, 1.0e-5f)
            : 0.0f;
        const float torso_angle = torso_roll_angle();
        const float torso_delta = wrap_angle(torso_angle - previous_torso_angle_);
        torso_turn_speed_ = torso_delta / std::max(dt, 1.0e-5f);
        previous_torso_angle_ = torso_angle;
        if (course_stage_ == CourseStage::balance)
        {
            // Standing rotation is a posture failure even while both feet remain
            // planted. Track the maximum wrapped torso turn instead of counting
            // only airborne flips, so upright spinning cannot pass mastery.
            uncontrolled_spin_turns_ = std::max(uncontrolled_spin_turns_,
                std::abs(torso_angle) / (2.0f * pi));
        }
        non_foot_grounded_ = non_foot_ground_contact();
        const bool feet_supported = left || right;
        const bool airborne = !feet_supported;

        powered_takeoff_this_step_ = false;
        powered_landing_this_step_ = false;
        spin_landing_this_step_ = false;
        passed_obstacle_this_step_ = false;

        const float head_clearance = valid_node(blueprint_.head_node)
            ? particles_[blueprint_.head_node].position.y
                - ground_height_at(particles_[blueprint_.head_node].position.x)
            : 0.0f;
        const float rest_head_clearance = valid_node(blueprint_.head_node)
            ? blueprint_.nodes[blueprint_.head_node].y : 0.0f;
        duck_depth_ = std::max(0.0f, rest_head_clearance - head_clearance);
        const float current_uprightness = torso_uprightness();

        duck_obstacle_weight_ = 0.0f;
        duck_clearance_margin_ = 0.0f;
        for (const CourseFeature& feature : course_features_)
        {
            if (feature.kind != CourseFeatureKind::overhead_bar
                && feature.kind != CourseFeatureKind::duck_press)
                continue;
            const float bar_bottom = feature.center.y - feature.half_extent.y;
            const float head_top = valid_node(blueprint_.head_node)
                ? particles_[blueprint_.head_node].position.y
                    + particles_[blueprint_.head_node].radius
                : bar_bottom;
            const float clearance = bar_bottom - head_top;
            const float weight = feature.kind == CourseFeatureKind::duck_press
                ? clamp((1.10f - clearance) / 1.10f, 0.0f, 1.0f)
                : duck_obstacle_approach_weight(feature.center.x - root_x);
            if (weight <= duck_obstacle_weight_)
                continue;
            duck_obstacle_weight_ = weight;
            duck_clearance_margin_ = clearance;
        }

        const bool generic_duck = feet_supported
            && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        const bool press_duck = course_stage_ == CourseStage::duck_press
            && feet_supported && !non_foot_grounded_
            && duck_obstacle_weight_ >= 0.64f
            && duck_clearance_margin_ >= -0.10f
            && current_uprightness > 0.20f;
        duck_active_ = generic_duck || press_duck;

        const bool disallowed_duck_contact = !duck_ground_contact_allowed(
            duck_active_, non_foot_grounded_);
        if (course_stage_ == CourseStage::duck_press)
        {
            duck_body_contact_seconds_ = disallowed_duck_contact
                ? duck_body_contact_seconds_ + dt
                : std::max(0.0f, duck_body_contact_seconds_ - dt * 3.0f);
            if (duck_body_contact_seconds_ > 0.35f)
                invalidate(InvalidMotion::duck_body_contact);
        }
        else if (disallowed_duck_contact)
        {
            invalidate(InvalidMotion::duck_body_contact);
        }
        if (duck_active_ && !non_foot_grounded_)
            duck_seconds_ += dt;
        if (course_stage_ == CourseStage::crouch_walk
            && duck_active_ && !non_foot_grounded_ && feet_supported)
        {
            crouch_walk_seconds_ += dt;
            crouch_walk_distance_ += std::max(0.0f, root_speed) * dt;
        }

        float current_joint_speed = 0.0f;
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
            current_joint_speed = std::max(current_joint_speed, std::abs(angular_velocities_[index]));
        if (elapsed_seconds_ >= 1.0f)
            maximum_joint_speed_ = std::max(maximum_joint_speed_, current_joint_speed);
        const float head_height_ratio = rest_head_clearance > 1.0e-5f
            ? head_clearance / rest_head_clearance : 0.0f;
        const float support_span_ratio = primary_support_span_ratio();
        const bool support_layout_valid = !blueprint_.paired_leg_chains()
            || (support_span_ratio >= 0.55f && support_span_ratio <= 1.65f);
        const bool catastrophic_stance_failure = non_foot_grounded_
            || current_uprightness < 0.70f
            || head_height_ratio < 0.52f
            || (blueprint_.paired_leg_chains()
                && (support_span_ratio < 0.35f || support_span_ratio > 2.10f));
        const bool horizontal_body = blueprint_.horizontal_body_plan();
        const float upright_threshold = horizontal_body ? 0.78f : 0.84f;
        const float head_threshold = horizontal_body ? 0.58f : 0.62f;
        const float slip_threshold = horizontal_body ? 0.18f : 0.10f;
        const float vertical_speed_threshold = horizontal_body ? 1.85f : 1.50f;
        const float stance_grace_limit = horizontal_body ? 1.40f : 0.60f;
        const bool stable_horizontal_stance_frame = horizontal_body
            && feet_supported && support_layout_valid && !non_foot_grounded_
            && current_uprightness >= upright_threshold
            && stance_slip_speed_ <= slip_threshold
            && std::abs(torso_turn_speed_) <= 2.00f
            && current_joint_speed <= 12.0f;
        const bool stable_stance_frame = stable_horizontal_stance_frame
            || (feet_supported
                && support_layout_valid
                && current_uprightness >= upright_threshold
                && head_height_ratio >= head_threshold
                && stance_slip_speed_ <= slip_threshold
                && std::abs(torso_turn_speed_) <= 2.00f
                && current_joint_speed <= 12.0f
                && std::abs(root_vertical_speed) <= vertical_speed_threshold);
        const bool recoverable_horizontal_stance = horizontal_body
            && feet_supported && support_layout_valid && !non_foot_grounded_
            && current_uprightness >= 0.70f && head_height_ratio >= 0.54f
            && stance_slip_speed_ <= 0.35f
            && std::abs(torso_turn_speed_) <= 2.50f
            && current_joint_speed <= 12.0f
            && std::abs(root_vertical_speed) <= 2.25f;
        if (stable_stance_frame)
        {
            stance_failure_grace_seconds_ = std::max(
                0.0f, stance_failure_grace_seconds_ - dt * 2.0f);
            stable_stance_seconds_ += dt;
        }
        else if (recoverable_horizontal_stance)
        {
            stance_failure_grace_seconds_ = std::min(
                stance_grace_limit, stance_failure_grace_seconds_ + dt);
            stable_stance_seconds_ += dt * 0.60f;
        }
        else if (!catastrophic_stance_failure
            && stance_failure_grace_seconds_ < stance_grace_limit)
        {
            stance_failure_grace_seconds_ += dt;
            stable_stance_seconds_ = std::max(
                0.0f, stable_stance_seconds_ - dt * 0.10f);
        }
        else
        {
            stance_failure_grace_seconds_ = 0.0f;
            stable_stance_seconds_ = 0.0f;
        }
        longest_stable_stance_seconds_ = std::max(
            longest_stable_stance_seconds_, stable_stance_seconds_);

        if (course_stage_ == CourseStage::duck_press)
        {
            const bool press_challenge_reached = duck_press_contact_this_step_
                || duck_press_contact_seen_
                || (duck_obstacle_weight_ >= 0.78f
                    && duck_clearance_margin_ <= 0.16f);
            if (press_challenge_reached)
                duck_press_contact_seen_ = true;
            if (duck_press_contact_seen_ && duck_active_ && feet_supported
                && !non_foot_grounded_
                && duck_clearance_margin_ >= -0.10f
                && duck_clearance_margin_ <= 0.28f)
            {
                duck_press_hold_seconds_ += dt;
                if (duck_press_hold_seconds_ >= 0.55f)
                    duck_press_hold_qualified_ = true;
            }
            else if (!duck_press_hold_qualified_)
            {
                duck_press_hold_seconds_ = std::max(
                    0.0f, duck_press_hold_seconds_ - dt * 0.35f);
            }
            if (duck_press_hold_qualified_ && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f
                && feet_supported && !non_foot_grounded_
                && body_integrity_valid()
                && current_uprightness >= 0.50f
                && !duck_press_completed_)
            {
                duck_press_completed_ = true;
                duck_walk_started_seconds_ = elapsed_seconds_;
                progress_window_start_x_ = root_x;
                progress_window_start_steps_ = alternating_steps_;
                ++duck_recovery_count_;
                ++obstacles_passed_;
                passed_obstacle_this_step_ = true;
            }
        }
        else if (duck_active_)
        {
            current_duck_hold_seconds_ += dt;
            duck_cycle_qualified_ = duck_cycle_qualified_
                || current_duck_hold_seconds_ >= 0.30f;
        }
        else if (duck_cycle_qualified_ && stable_stance_seconds_ >= 0.40f)
        {
            ++duck_recovery_count_;
            current_duck_hold_seconds_ = 0.0f;
            duck_cycle_qualified_ = false;
        }
        else if (!duck_cycle_qualified_)
        {
            current_duck_hold_seconds_ = 0.0f;
        }

        if ((course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            && duck_obstacle_weight_ > 0.10f
            && std::abs(torso_turn_speed_) > 0.85f)
            torso_swing_seconds_ += dt;
        else
            torso_swing_seconds_ = std::max(0.0f, torso_swing_seconds_ - dt * 2.0f);
        if (torso_swing_seconds_ > 0.75f)
            invalidate(InvalidMotion::robotic_torso_swing);

        const bool collapsed_balance_posture = course_stage_ == CourseStage::balance
            && elapsed_seconds_ >= 1.50f
            && (!feet_supported || non_foot_grounded_
                || current_uprightness < 0.62f || head_height_ratio < 0.64f);
        posture_failure_seconds_ = collapsed_balance_posture
            ? posture_failure_seconds_ + dt
            : std::max(0.0f, posture_failure_seconds_ - dt * 2.0f);
        if (posture_failure_seconds_ >= 1.50f)
            invalidate(InvalidMotion::collapsed_posture);

        if (was_supported && airborne
            && powered_joint_launch(course_stage_, root_vertical_speed, action_energy))
        {
            powered_takeoff_ = true;
            powered_takeoff_this_step_ = true;
            current_airborne_rotation_ = 0.0f;
            ++powered_jump_count_;
        }

        if (airborne)
        {
            current_airborne_rotation_ += torso_delta;
            const float airborne_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);
            if (powered_takeoff_ && stage_allows_controlled_flips(course_stage_))
                maximum_spin_turns_ = std::max(maximum_spin_turns_, airborne_turns);
            else if (course_stage_ != CourseStage::balance)
                uncontrolled_spin_turns_ += std::abs(torso_delta) / (2.0f * pi);
        }
        else if (!was_supported)
        {
            if (blueprint_.monopedal_gait()
                && (course_stage_ == CourseStage::uneven
                    || course_stage_ == CourseStage::crouch_walk)
                && std::abs(root_x - last_single_leg_landing_x_) >= 0.040f)
            {
                ++single_leg_cycles_;
                last_single_leg_landing_x_ = root_x;
            }
            if (powered_takeoff_)
            {
                powered_landing_this_step_ = true;
                ++landed_jump_count_;
                const float landed_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);
                if (stage_allows_controlled_flips(course_stage_))
                {
                    maximum_spin_turns_ = std::max(maximum_spin_turns_, landed_turns);
                    if (landed_turns >= 0.75f && landed_turns <= 3.0f)
                    {
                        spin_landing_this_step_ = true;
                        ++spin_landing_count_;
                    }
                }
                else
                {
                    uncontrolled_spin_turns_ += landed_turns;
                }
            }
            powered_takeoff_ = false;
            current_airborne_rotation_ = 0.0f;
        }

        previous_left_grounded_ = left;
        previous_right_grounded_ = right;
        const float active_flip_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);
        const float evaluated_turns = spin_landing_this_step_
            ? maximum_spin_turns_ : active_flip_turns;
        const bool airborne_or_landing = !feet_supported || spin_landing_this_step_;
        const bool controlled_somersault = controlled_somersault_allowed(
            course_stage_, evaluated_turns, torso_turn_speed_, airborne_or_landing);
        const bool head_faces_forward = valid_node(blueprint_.head_node)
            && valid_node(blueprint_.torso_node)
            && particles_[blueprint_.head_node].position.x
                >= particles_[blueprint_.torso_node].position.x - 0.05f;
        const bool controlled_prone = forward_prone_allowed(course_stage_,
            non_foot_grounded_, head_faces_forward, torso_uprightness(), root_speed);
        if (controlled_somersault || controlled_prone)
        {
            body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 3.0f);
            head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);
        }
        else
        {
            if (rolling_body_motion(root_speed, torso_turn_speed_, torso_uprightness(),
                feet_supported, non_foot_grounded_))
                body_rolling_seconds_ += dt;
            else
                body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 2.0f);
            if (head_ground_contact())
                head_contact_seconds_ += dt;
            else
                head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);
        }
        if (!rolling_gate_active(elapsed_seconds_))
        {
            body_rolling_seconds_ = 0.0f;
            head_contact_seconds_ = 0.0f;
        }
        else if (body_rolling_seconds_ > body_rolling_limit(course_stage_, elapsed_seconds_)
            || head_contact_seconds_ > head_contact_limit(elapsed_seconds_))
        {
            invalidate(InvalidMotion::body_rolling);
        }

        const bool locomotion_required = stage_requires_forward_gait(course_stage_);
        const float recent_swing_clearance = std::max(left_clearance, right_clearance);
        if (locomotion_required && friction_driven_shuffle(root_speed,
                left, right, stance_slip_speed_, gait_cycles(), recent_swing_clearance))
            wheel_sliding_seconds_ = std::min(3.0f, wheel_sliding_seconds_ + dt);
        else
            wheel_sliding_seconds_ = std::max(0.0f, wheel_sliding_seconds_ - dt * 1.5f);
        // Sliding is a normal part of stance adjustment, crouching, and gait.
        // It is never a hard invalidation. Pure friction-driven shuffling simply
        // receives no gait credit and a mild shaping penalty until a real cycle occurs.

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

        int highest_passed_sequence = last_passed_feature_sequence_;
        for (const CourseFeature& feature : course_features_)
        {
            if (feature.kind == CourseFeatureKind::duck_press)
                continue;
            const float trailing_edge = feature.center.x + course_feature_half_width(feature);
            if (trailing_edge < root_x - 0.10f)
                highest_passed_sequence = std::max(highest_passed_sequence, feature.marker_sequence);
        }
        if (highest_passed_sequence > last_passed_feature_sequence_)
        {
            last_passed_feature_sequence_ = highest_passed_sequence;
            ++obstacles_passed_;
            passed_obstacle_this_step_ = true;
        }
        if (stage_requires_forward_gait(course_stage_) && foot_pivot_rolling_motion(root_speed,
            left, right, stance_slip_speed_, obstacle_lift_clearance_, torso_turn_speed_))
            foot_pivot_rolling_seconds_ += dt;
        else
            foot_pivot_rolling_seconds_ = std::max(0.0f, foot_pivot_rolling_seconds_ - dt * 2.5f);
        if (!rolling_gate_active(elapsed_seconds_))
            foot_pivot_rolling_seconds_ = 0.0f;
        else if (foot_pivot_rolling_seconds_ > foot_pivot_rolling_limit(elapsed_seconds_))
            invalidate(InvalidMotion::foot_pivot_rolling);

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
            if (locomotion_required && (high_energy_stall || inefficient_vibration))
                micro_motion_seconds_ += progress_window_seconds_;
            else
                micro_motion_seconds_ = std::max(0.0f, micro_motion_seconds_ - 0.5f);

            const std::uint32_t new_steps = alternating_steps_ - progress_window_start_steps_;
            const bool idle_window = locomotion_required
                && elapsed_seconds_ > rolling_gate_warmup_end_seconds
                && zero_progress_window(net_progress, new_steps,
                    obstacle_lift_clearance_, recovery_active_);
            zero_progress_seconds_ = update_zero_progress_seconds(
                zero_progress_seconds_, idle_window, progress_window_seconds_);
            progress_window_start_steps_ = alternating_steps_;
            progress_window_start_x_ = root_x;
            progress_window_seconds_ = 0.0f;
            action_energy_window_ = 0.0f;
            root_path_window_ = 0.0f;
        }

        const float allowed_airtime = allowed_airtime_for_stage(
            course_stage_, powered_takeoff_);
        if (course_stage_ != CourseStage::duck_press
            && airborne_seconds_ > allowed_airtime)
            invalidate(InvalidMotion::sustained_flight);
        if (micro_motion_seconds_ >= 3.0f)
            invalidate(InvalidMotion::micro_motion);
        if (zero_progress_seconds_ >= zero_progress_reset_seconds)
            invalidate(InvalidMotion::zero_progress);
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
        action_change_energy_ = 0.0f;
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
        {
            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;
            const float action_delta = applied_actions[index] - previous_applied_actions_[index];
            action_change_energy_ += action_delta * action_delta;
            previous_applied_actions_[index] = applied_actions[index];
        }
        constexpr Vec2 gravity{ 0.0f, -22.0f };
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
        }

        collided_this_step_ = false;
        duck_press_contact_this_step_ = false;
        duck_press_max_penetration_ = 0.0f;
        update_materials(dt);
        rebuild_course_features();
        update_articulated_toe_commands(applied_actions, dt);
        for (int iteration = 0; iteration < 14; ++iteration)
        {
            for (const DistanceConstraint& bone : blueprint_.bones)
                solve_distance(bone);
            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
                solve_motor(blueprint_.motors[index], applied_actions[index]);
            solve_articulated_toes();
            stabilize_balance_posture();
            stabilize_duck_posture();
            stabilize_passive_appendages();
            solve_ground(dt);
            solve_course();
            // Re-apply the authored crouch after collision resolution so the
            // final solver state cannot leave an intermediate knee/body link
            // under the floor or inside the platen.
            stabilize_duck_posture();
            solve_ground(dt);
            solve_course();
            // End each iteration in a floor-valid authored crouch. The target
            // is already clamped beneath the platen, so a final course shove is
            // unnecessary and would reintroduce solver-frame penetration.
            stabilize_duck_posture();
            solve_ground(dt);
            separate_support_clusters();
            if (course_stage_ == CourseStage::duck_press)
                stabilize_duck_posture();
            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. End every solver iteration with the
            // same grounded foot state that preview and gait metrics observe.
            solve_ground(dt);
        }
        // The iterative solver can otherwise reverse the toe between
        // stabilization and push-off every frame. Bound the final physical
        // hinge travel while preserving the contact and propulsion roles.
        limit_articulated_toe_rates(dt);
        apply_support_pressure(dt);
        if (stage_uses_deformable_terrain(course_stage_))
            terrain_.step(dt);
        update_material_metrics(dt);
        if (elapsed_seconds_ >= 8.00f && !body_integrity_valid())
            invalidate(InvalidMotion::collapsed_posture);
        // duck_press_max_penetration_ is diagnostic transient overlap
        // before each solver correction, not residual clipping. The
        // collision test and final clearance gate verify resolution.
        knee_first_this_step_ = knee_before_foot_fault();
        if (knee_first_this_step_)
            ++knee_first_faults_;
        collision_event_this_step_ = collided_this_step_ && !collision_contact_active_;
        if (collision_event_this_step_)
            collision_count_ += 1.0f;
        collision_contact_active_ = collided_this_step_;

        elapsed_seconds_ += dt;
        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;
        const float raw_speed = (pelvis_position.x - previous_pelvis_.x) / dt;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        const float frame_progress = pelvis_position.x - previous_pelvis_.x;
        previous_pelvis_ = pelvis_position;
        distance_travelled_ = pelvis_position.x - blueprint_.nodes[blueprint_.root_node].x;
        maximum_speed_kmh_ = std::max(maximum_speed_kmh_, std::max(std::abs(raw_speed), std::abs(forward_speed_)) * 3.6f);

        float action_energy = 0.0f;
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
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
        const bool controlled_airborne_skill = powered_takeoff_
            && !supported && stage_allows_powered_airtime(course_stage_);
        if (!recovery_active_ && !controlled_airborne_skill && recovery_should_start(
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

        const float base_allowed_airtime = allowed_airtime_for_stage(
            course_stage_, powered_takeoff_);
        // The static press is a supported compression lesson. A short solver
        // contact flicker must not terminate the entire episode before the
        // platen reaches the rig; sustained loss of support still fails.
        const float allowed_airtime = course_stage_ == CourseStage::duck_press
            ? std::max(base_allowed_airtime, 0.75f)
            : base_allowed_airtime;
        const float gated_upright = elapsed_seconds_ > 0.25f ? upright : 1.0f;
        const bool terminal_fall = recovery_terminal_fall(
            geometric_fall, hard_fall, recovery_active_);
        InvalidMotion frame_gate = classify_motion_gate(gated_upright,
            maximum_speed_kmh_, pelvis_position, airborne_seconds_, allowed_airtime,
            micro_motion_seconds_, terminal_fall, course_stage_,
            current_airborne_rotation_ / (2.0f * pi));
        if (course_stage_ == CourseStage::duck_press
            && !duck_press_completed_
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed
                || frame_gate == InvalidMotion::fallen))
            frame_gate = InvalidMotion::none;
        invalidate(frame_gate);

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
        const float collision_penalty = collision_event_this_step_ ? 0.070f : 0.0f;
        // This is intentionally a mild shaping penalty. Natural knee lead
        // is now tolerated; only a large low-foot body-first obstacle shove reaches here.
        const float knee_first_penalty = knee_first_this_step_ ? 0.028f : 0.0f;
        const float stance_slip_penalty = course_stage_ == CourseStage::balance
            ? clamp(stance_slip_speed_ - 0.08f, 0.0f, 4.0f) * 0.012f
            : 0.0f;
        const float wheel_penalty = friction_driven_shuffle(raw_speed,
            left_supported, right_supported, stance_slip_speed_, gait_cycles(),
            swing_clearance) ? 0.028f : 0.0f;
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
        const float burial_penalty = burial_depth_ * 0.22f
            + static_cast<float>(obstruction_mask_ != 0u) * 0.025f;
        const float burial_change = previous_burial_depth_ - burial_depth_;
        const float escape_alignment = free_space_direction_ == 0.0f ? 0.0f
            : std::max(0.0f, raw_speed * free_space_direction_);
        const float escape_reward = burial_depth_ > 0.03f
            ? std::max(0.0f, burial_change) * 0.25f + escape_alignment * 0.012f
            : 0.0f;

        const float forward_gait_reward = std::max(0.0f, safe_progress) * 1.65f * gait;
        const float backward_penalty = std::max(0.0f, -safe_progress) * 0.45f;
        const float duck_reward = duck_active_
            ? 0.018f + clamp(duck_depth_ - 0.48f, 0.0f, 0.80f) * 0.012f : 0.0f;
        const float obstacle_duck_reward = duck_obstacle_weight_
            * (duck_active_ ? 0.055f
                + clamp(duck_clearance_margin_, -0.30f, 0.20f) * 0.035f
                : -0.020f);
        const float premature_duck_penalty = (1.0f - duck_obstacle_weight_)
            * (duck_active_ ? 0.018f : 0.0f);
        const float jump_reward = (powered_takeoff_this_step_ ? 0.10f : 0.0f)
            + (powered_landing_this_step_ ? 0.22f : 0.0f)
            + (powered_takeoff_ && !left_supported && !right_supported ? 0.0025f : 0.0f);
        const float spin_delta_turns = std::abs(torso_turn_speed_) * dt / (2.0f * pi);
        const float spin_reward = stage_allows_controlled_flips(course_stage_) && powered_takeoff_
            ? clamp(spin_delta_turns, 0.0f, 0.08f) * 0.65f : 0.0f;
        const float spin_landing_reward = spin_landing_this_step_
            ? 0.20f + clamp(maximum_spin_turns_, 0.0f, 3.0f) * 0.08f : 0.0f;
        const bool controlled_flip_rotation = stage_allows_controlled_flips(course_stage_)
            && powered_takeoff_;
        const float uncontrolled_spin_penalty = controlled_flip_rotation ? 0.0f
            : clamp(spin_delta_turns, 0.0f, 0.08f) * 0.18f;
        const float pass_reward = passed_obstacle_this_step_ ? 0.18f : 0.0f;
        const float target_speed = 0.90f + course_difficulty_ * 1.30f;
        const bool reward_requires_locomotion = stage_requires_forward_gait(course_stage_);
        const float run_reward = reward_requires_locomotion
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;
        const float real_step_reward = alternating_step_this_step_ ? 0.070f : 0.0f;
        const float unearned_progress_penalty = alternating_steps_ == 0u
            ? std::max(0.0f, safe_progress) * 0.80f : 0.0f;
        const float double_support_shuffle_penalty = friction_driven_shuffle(raw_speed,
            left_supported, right_supported, stance_slip_speed_, gait_cycles(),
            swing_clearance) ? 0.018f : 0.0f;
        const float action_change_penalty = action_change_energy_ * 0.0025f;
        const float press_contact_reward = course_stage_ == CourseStage::duck_press
            && duck_press_contact_this_step_ && duck_active_ ? 0.045f : 0.0f;
        const float torso_swing_penalty = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.22f) * 0.030f : 0.0f;
        const float upper_body_deviation = maximum_upper_body_motor_deviation();
        const float neutral_upper_body_reward = course_stage_ == CourseStage::balance
            ? clamp(1.0f - upper_body_deviation / 0.70f, 0.0f, 1.0f) * 0.014f
            : 0.0f;
        const float upper_body_posture_penalty = course_stage_ == CourseStage::balance
            ? std::max(0.0f, upper_body_deviation - 0.30f) * 0.035f
            : 0.0f;
        const float support_span_error = blueprint_.paired_leg_chains()
            ? std::abs(primary_support_span_ratio() - 1.0f) : 0.0f;
        const float support_span_penalty = std::max(0.0f,
            support_span_error - 0.22f) * 0.090f;

        switch (course_stage_)
        {
        case CourseStage::balance:
        {
            const float stance_reward = stable_stance_seconds_ > 0.0f
                ? 0.048f + std::min(stable_stance_seconds_, 4.0f) * 0.004f
                : -0.012f;
            last_reward_ = stance_reward
                + std::max(0.0f, upright) * 0.008f
                + neutral_upper_body_reward
                + contact * 0.0010f
                - std::abs(forward_speed_) * 0.0070f
                - std::abs(distance_travelled_) * 0.0030f
                - action_energy * 0.0018f
                - stance_slip_speed_ * 0.010f
                - posture_failure_seconds_ * 0.020f
                - upper_body_posture_penalty
                - support_span_penalty
                - body_contact_penalty;
            break;
        }
        case CourseStage::duck_press:
            if (!duck_press_completed_)
            {
                last_reward_ = std::max(0.0f, upright) * 0.016f
                    + contact * 0.0015f + duck_reward + obstacle_duck_reward
                    + press_contact_reward + pass_reward
                    - std::abs(forward_speed_) * 0.0030f
                    - action_energy * 0.0009f - torso_swing_penalty
                    - support_span_penalty
                    - premature_duck_penalty - body_contact_penalty;
            }
            else
            {
                const float recovered_pose = !duck_active_ && !non_foot_grounded_
                    && stable_stance_seconds_ >= 0.40f ? 0.065f : 0.0f;
                last_reward_ = recovered_pose
                    + std::max(0.0f, upright) * 0.016f
                    + contact * 0.0015f + pass_reward
                    - std::abs(forward_speed_) * 0.0080f
                    - std::abs(distance_travelled_) * 0.0040f
                    - action_energy * 0.0009f - action_change_penalty
                    - support_span_penalty - torso_swing_penalty
                    - body_contact_penalty * 2.0f;
            }
            break;
        case CourseStage::crouch_walk:
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
            break;
        }
        case CourseStage::ramps:
            last_reward_ = std::max(0.0f, upright) * 0.010f
                + contact * 0.0008f + jump_reward
                - std::abs(forward_speed_) * 0.0020f
                - action_energy * 0.0008f - body_contact_penalty;
            break;
        case CourseStage::uneven:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f + swing_reward + run_reward + real_step_reward
                - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - stance_slip_penalty - wheel_penalty
                - body_contact_penalty;
            break;
        case CourseStage::hurdles:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.011f
                + swing_reward + run_reward + real_step_reward
                + duck_reward * 0.60f + jump_reward + obstacle_lift_reward
                + pass_reward - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;
            break;
        case CourseStage::duck_bars:
            last_reward_ = std::max(0.0f, upright) * 0.008f
                + jump_reward + spin_reward + spin_landing_reward
                - std::abs(forward_speed_) * 0.0015f
                - action_energy * 0.0009f - body_contact_penalty;
            break;
        case CourseStage::moving_hazards:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.010f
                + swing_reward + run_reward + real_step_reward
                + duck_reward * 0.45f + jump_reward + spin_reward
                + spin_landing_reward + obstacle_lift_reward + pass_reward
                + escape_reward
                - backward_penalty - unearned_progress_penalty - burial_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;
            break;
        }

        last_reward_ += recovery_reward - uncontrolled_spin_penalty;
        // Static crouch qualification explicitly requires grounded support,
        // a real press hold, feet-only ground contact, integrity, and recovery.
        // Do not let a flight reason recorded by the generic locomotion gate
        // terminate this supported compression lesson before those stronger
        // stage-specific checks can run. Every other invalid reason remains.
        if (course_stage_ == CourseStage::duck_press
            && !duck_press_completed_
            && (invalid_reason_ == InvalidMotion::sustained_flight
                || invalid_reason_ == InvalidMotion::overspeed
                || invalid_reason_ == InvalidMotion::collapsed_posture
                || invalid_reason_ == InvalidMotion::fallen))
            invalid_reason_ = InvalidMotion::none;
        if (invalid_reason_ != InvalidMotion::none)
        {
            recovery_active_ = false;
            last_reward_ -= 5.0f;
        }
        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : course_stage_ == CourseStage::duck_press ? 36.0f
            : course_stage_ == CourseStage::ramps || course_stage_ == CourseStage::duck_bars ? 20.0f
            : course_stage_ == CourseStage::moving_hazards ? 48.0f : 36.0f;
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

        constexpr std::size_t joint_angle_begin = 4;
        constexpr std::size_t joint_velocity_begin = joint_angle_begin + action_count;
        constexpr std::size_t contact_begin = joint_velocity_begin + action_count;
        static_assert(contact_begin == 20);
        static_assert(observation_count == 50);

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
        result[26] = clamp(ground_height_at(root.x + 0.65f) - ground_height_at(root.x),
            -1.0f, 1.0f);
        result[27] = clamp(ground_height_at(root.x + 1.50f) - ground_height_at(root.x),
            -1.0f, 1.0f);
        result[28] = clamp(ground_height_at(root.x + 3.00f) - ground_height_at(root.x),
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
            case CourseFeatureKind::duck_press: result[30] = 0.0f; break;
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
        result[40] = terrain_firmness_;
        result[41] = terrain_looseness_;
        result[42] = clamp(burial_depth_ / 0.80f, 0.0f, 2.0f);
        result[43] = free_space_direction_;
        result[44] = clamp(incoming_material_velocity_.x / 6.0f, -2.0f, 2.0f);
        result[45] = clamp(incoming_material_velocity_.y / 6.0f, -2.0f, 2.0f);
        result[46] = clamp(incoming_time_to_impact_ / 4.0f, 0.0f, 2.5f);
        result[47] = clamp(incoming_material_density_, 0.0f, 1.0f);
        result[48] = static_cast<float>(obstruction_mask_) / 7.0f;
        result[49] = clamp(terrain_.slope_at(root.x + course_progress()), -2.0f, 2.0f);
        return result;
    }
}
