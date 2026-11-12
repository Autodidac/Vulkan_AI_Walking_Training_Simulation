#include "simulation.hpp"
#include "locomotion_strategy.hpp"

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
            for (const CoupledMotorConstraint& coupled : rig.coupled_support_motors())
            {
                const MotorConstraint& motor = coupled.motor;
                if (motor.enabled
                    && (motor.a == node || motor.pivot == node || motor.c == node))
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

        [[nodiscard]] bool manipulator_endpoint(const CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            return node < rig.nodes.size()
                && node != rig.root_node && node != rig.torso_node
                && node != rig.head_node && !rig.is_support_seed(node)
                && node_degree(rig, node) == 1u;
        }

        [[nodiscard]] bool manipulator_branch_node(const CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const MotorConstraint& motor = rig.motors[index];
                if (rig.support_branch_mask(motor) == 0u
                    && (node == motor.pivot || node == motor.c))
                    return true;
            }
            return false;
        }


        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            // The terminal ankle joint is the one short physical support stub.
            // The forward boot is visual-only. Adding another child node left
            // the former ankle close enough to the terrain to become forbidden
            // non-foot contact during Stand.
            static_cast<void>(heel_reach);
            static_cast<void>(toe_reach);
            auto make_stub = [&](std::uint16_t contact)
            {
                if (contact >= rig.nodes.size() || contact >= rig.radii.size())
                    return;
                rig.radii[contact] = clamp(
                    rig.radii[contact] * 0.62f, 0.104f, 0.112f);
            };
            make_stub(rig.left_contact_node);
            make_stub(rig.right_contact_node);
            rig.additional_left_contact_nodes.clear();
            rig.additional_right_contact_nodes.clear();
        }

        [[nodiscard]] bool legacy_human_paired_limb_topology(
            const CreatureBlueprint& rig) noexcept
        {
            if (rig.nodes.size() != 13u || rig.radii.size() != 13u
                || rig.active_motor_count != 8u || rig.root_node != 0u
                || rig.torso_node != 1u || rig.head_node != 2u
                || rig.left_contact_node != human_left_ankle_node
                || rig.right_contact_node != human_right_ankle_node
                || !rig.additional_left_contact_nodes.empty()
                || !rig.additional_right_contact_nodes.empty())
                return false;
            constexpr std::array<MotorConstraint, 8> expected{
                MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
                MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
                MotorConstraint{ 1, 7, 8 }, MotorConstraint{ 7, 8, 9 },
                MotorConstraint{ 1, 10, 11 }, MotorConstraint{ 10, 11, 12 }
            };
            for (std::size_t index = 0; index < expected.size(); ++index)
            {
                if (!rig.motors[index].enabled
                    || rig.motors[index].a != expected[index].a
                    || rig.motors[index].pivot != expected[index].pivot
                    || rig.motors[index].c != expected[index].c)
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool promote_runrig6_human_explicit_toes(
            CreatureBlueprint& rig) noexcept
        {
            constexpr std::size_t runrig6_motor_count =
                human_right_ankle_action + 1u;
            if (rig.nodes.size() != 19u || rig.radii.size() != 19u
                || rig.active_motor_count != runrig6_motor_count)
                return false;
            const CreatureBlueprint previous = rig;
            rig.motors[human_left_toe_action] = MotorConstraint{
                human_left_heel_node, human_left_ball_node,
                human_left_toe_node };
            rig.motors[human_right_toe_action] = MotorConstraint{
                human_right_heel_node, human_right_ball_node,
                human_right_toe_node };
            rig.active_motor_count = anatomy_action_count;
            rig.calibrate_motor(human_left_toe_action, 42.0f, 36.0f, 0.022f);
            rig.calibrate_motor(human_right_toe_action, 42.0f, 36.0f, 0.022f);
            // Make the newly-recognized RUNRIG7 topology canonical before it
            // escapes the migration boundary. Otherwise Environment's normal
            // rest-length rebuild performs the first paired-leg normalization
            // and silently changes the authored rig signature at construction.
            rig.rebuild_rest_lengths();
            if (rig.human_paired_limb_topology() && rig.valid())
                return true;
            rig = previous;
            return false;
        }

        [[nodiscard]] bool promote_human_articulated_feet(
            CreatureBlueprint& rig) noexcept
        {
            if (!legacy_human_paired_limb_topology(rig))
                return false;

            // RUNRIG7 owns the complete physical Human. Nodes 0..12 and action
            // slots 0..7 retain their established meaning. The former terminal
            // support nodes 4/6 are now anatomical ankles; each receives a
            // rigid anatomical rear-foot triangle and explicit policy ankle
            // and distal toe motors. There is
            // deliberately no knee-to-heel/ball diagonal.
            auto append_foot = [&](std::uint16_t knee, std::uint16_t ankle,
                std::uint16_t expected_heel, std::uint16_t expected_ball,
                std::uint16_t expected_toe, std::size_t ankle_action,
                std::vector<std::uint16_t>& additional) noexcept
            {
                if (rig.nodes.size() != expected_heel
                    || rig.radii.size() != expected_heel)
                    return false;
                const Vec2 ankle_position = rig.nodes[ankle];
                const std::uint16_t heel = static_cast<std::uint16_t>(
                    rig.nodes.size());
                const std::uint16_t ball = static_cast<std::uint16_t>(
                    rig.nodes.size() + 1u);
                const std::uint16_t toe = static_cast<std::uint16_t>(
                    rig.nodes.size() + 2u);
                if (heel != expected_heel || ball != expected_ball
                    || toe != expected_toe)
                    return false;
                // All three neutral sole nodes share one solver-space collision
                // bottom. ground_contact_offset caps large support radii at
                // 65 mm, so visual radius alone is not the contact plane. A
                // raised ball/toe leaves the nominal stance heel-only and turns
                // gravity into an uncommanded rocking/shear impulse at spawn.
                rig.nodes.push_back(ankle_position + Vec2{ -0.055f, -0.105f });
                rig.nodes.push_back(ankle_position + Vec2{ 0.155f, -0.105f });
                rig.nodes.push_back(ankle_position + Vec2{ 0.350f, -0.110f });
                rig.radii.push_back(0.072f);
                rig.radii.push_back(0.065f);
                rig.radii.push_back(0.060f);
                rig.bones.push_back({ ankle, heel, 0.0f, 1.0f });
                rig.bones.push_back({ ankle, ball, 0.0f, 1.0f });
                rig.bones.push_back({ heel, ball, 0.0f, 0.98f });
                rig.bones.push_back({ ball, toe, 0.0f, 0.98f });
                rig.motors[ankle_action] = MotorConstraint{ knee, ankle, ball };
                additional = { ball, toe };
                return true;
            };
            if (!append_foot(3u, human_left_ankle_node,
                    human_left_heel_node, human_left_ball_node,
                    human_left_toe_node, human_left_ankle_action,
                    rig.additional_left_contact_nodes)
                || !append_foot(5u, human_right_ankle_node,
                    human_right_heel_node, human_right_ball_node,
                    human_right_toe_node, human_right_ankle_action,
                    rig.additional_right_contact_nodes))
                return false;
            rig.left_contact_node = human_left_heel_node;
            rig.right_contact_node = human_right_heel_node;
            rig.active_motor_count = human_right_ankle_action + 1u;
            rig.rebuild_rest_lengths();
            rig.calibrate_motor(human_left_ankle_action, 34.0f, 34.0f, 0.034f);
            rig.calibrate_motor(human_right_ankle_action, 34.0f, 34.0f, 0.034f);
            return promote_runrig6_human_explicit_toes(rig) && rig.valid();
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

    CreatureBlueprint CreatureBlueprint::scaffold()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.62f }, { 0.02f, 3.68f }, { 0.10f, 4.38f },
            { 0.12f, 1.46f }, { -0.14f, 0.25f },
            { 0.22f, 1.46f }, { 0.14f, 0.25f }
        };
        result.radii = { 0.24f, 0.27f, 0.23f, 0.17f, 0.15f, 0.17f, 0.15f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        add_passive_feet(result, 0.16f, 0.28f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 38.0f, 58.0f, 0.042f, 0.048f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.species_identity = CreatureSpecies::chicken;
        // Species-scale bird anatomy: compact horizontal body, counterweight tail,
        // forward head/beak, and two opposed digitigrade legs. It remains a true
        // two-leg policy subject without borrowing Human proportions or armor.
        result.nodes = {
            { -0.10f, 0.68f }, { 0.24f, 0.72f },
            { 0.46f, 0.88f }, { 0.82f, 1.02f }, { 1.04f, 1.00f },
            { -0.62f, 0.78f },
            { -0.16f, 0.40f }, { -0.24f, 0.10f },
            { 0.02f, 0.39f }, { 0.12f, 0.10f }
        };
        result.radii = {
            0.16f, 0.17f, 0.10f, 0.12f, 0.06f,
            0.08f, 0.075f, 0.06f, 0.075f, 0.06f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.98f },
            { 2, 3, 0.0f, 0.98f }, { 3, 4, 0.0f, 0.92f },
            { 0, 2, 0.0f, 0.88f }, { 1, 3, 0.0f, 0.86f },
            { 0, 5, 0.0f, 0.92f }, { 1, 5, 0.0f, 0.82f },
            { 0, 6, 0.0f, 1.0f }, { 6, 7, 0.0f, 1.0f },
            { 0, 8, 0.0f, 1.0f }, { 8, 9, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 6 }, MotorConstraint{ 0, 6, 7 },
            MotorConstraint{ 1, 0, 8 }, MotorConstraint{ 0, 8, 9 }
        };
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 3u;
        result.left_contact_node = 7u;
        result.right_contact_node = 9u;
        // Bird talons remain species-scale point supports. The Human passive-foot
        // helper deliberately widens loaded boot contacts and must not inflate a
        // 0.07 m Chicken foot into a 0.104 m skating pad.
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 52.0f, 0.040f, 0.047f);
        return result;
    }
    CreatureBlueprint CreatureBlueprint::biped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.74f }, { 0.02f, 3.84f }, { 0.10f, 4.64f },
            { 0.14f, 1.52f }, { -0.14f, 0.25f },
            { 0.22f, 1.52f }, { 0.14f, 0.25f }
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
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 40.0f, 60.0f, 0.045f, 0.051f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::humanoid()
    {
        CreatureBlueprint result{};
        result.species_identity = CreatureSpecies::human;
        result.nodes = {
            // RUNRIG6 stores both knees on the same forward-facing anatomical
            // flexion branch. Training, preview, and newly-created human.rig
            // files therefore start from the same continuous joint neutral.
            { -0.179952502f, 2.61523819f },
            { 0.00281051546f, 3.76404762f },
            { 0.0123144165f, 4.13904762f },
            { -0.233058650f, 1.51483471f },
            { -0.349952519f, 0.459101081f },
            { -0.0599525049f, 1.52010894f },
            { -0.00995250046f, 0.459101081f },
            { -0.0885858908f, 3.56166649f },
            { -0.319952488f, 3.00594616f },
            { -0.259952486f, 2.48330688f },
            { -0.201681122f, 3.50809526f },
            { -0.0599525124f, 2.92305803f },
            { -0.0799525008f, 2.39736629f }
        };
        result.radii = {
            0.26f, 0.31f, 0.27f, 0.19f, 0.1054f, 0.19f, 0.1054f,
            0.16f, 0.15f, 0.14f, 0.16f, 0.15f, 0.14f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f },
            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f },
            { 8, 9, 0.0f, 0.96f },
            { 1, 10, 0.0f, 0.98f }, { 10, 11, 0.0f, 0.98f },
            { 11, 12, 0.0f, 0.96f },
            { 7, 10, 0.0f, 0.72f },
            { 2, 7, 0.0f, 0.94f }, { 2, 10, 0.0f, 0.94f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 1, 7, 8 }, MotorConstraint{ 7, 8, 9 },
            MotorConstraint{ 1, 10, 11 }, MotorConstraint{ 10, 11, 12 }
        };
        result.active_motor_count = 8u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        result.rebuild_rest_lengths();
        for (std::size_t index = 0; index < 4u; ++index)
        {
            const bool knee = (index & 1u) != 0u;
            const MotorConstraint& motor = result.motors[index];
            const float driven_length = length(
                result.nodes[motor.c] - result.nodes[motor.pivot]);
            const float linear_gain = knee ? 0.051f : 0.045f;
            const float strength = clamp(
                linear_gain / std::max(0.75f, driven_length), 0.032f, 0.056f);
            result.calibrate_motor(index, knee ? 58.0f : 36.0f,
                knee ? 58.0f : 36.0f, strength);
        }
        // Arms remain low-authority locomotion appendages around the saved
        // authored side-rest. The body-envelope solver absorbs only outward
        // runaway momentum; it does not provide locomotion or balance force.
        result.calibrate_motor(4, 62.0f, 62.0f, 0.024f);
        result.calibrate_motor(5, 76.0f, 76.0f, 0.022f);
        result.calibrate_motor(6, 62.0f, 62.0f, 0.024f);
        result.calibrate_motor(7, 76.0f, 76.0f, 0.022f);
        const bool physical_feet_promoted =
            promote_human_articulated_feet(result);
        (void)physical_feet_promoted;
        return result;
    }

    bool CreatureBlueprint::human_paired_limb_topology() const noexcept
    {
        if (nodes.size() != 19u || radii.size() != 19u
            || active_motor_count != anatomy_action_count
            || root_node != 0u || torso_node != 1u || head_node != 2u
            || left_contact_node != human_left_heel_node
            || right_contact_node != human_right_heel_node
            || additional_left_contact_nodes
                != std::vector<std::uint16_t>{
                    human_left_ball_node, human_left_toe_node }
            || additional_right_contact_nodes
                != std::vector<std::uint16_t>{
                    human_right_ball_node, human_right_toe_node }
            || !direct_bone(*this, 3u, human_left_ankle_node)
            || !direct_bone(*this, human_left_ankle_node,
                human_left_heel_node)
            || !direct_bone(*this, human_left_ankle_node,
                human_left_ball_node)
            || !direct_bone(*this, human_left_heel_node,
                human_left_ball_node)
            || !direct_bone(*this, human_left_ball_node,
                human_left_toe_node)
            || !direct_bone(*this, 5u, human_right_ankle_node)
            || !direct_bone(*this, human_right_ankle_node,
                human_right_heel_node)
            || !direct_bone(*this, human_right_ankle_node,
                human_right_ball_node)
            || !direct_bone(*this, human_right_heel_node,
                human_right_ball_node)
            || !direct_bone(*this, human_right_ball_node,
                human_right_toe_node)
            || direct_bone(*this, 3u, human_left_heel_node)
            || direct_bone(*this, 3u, human_left_ball_node)
            || direct_bone(*this, 5u, human_right_heel_node)
            || direct_bone(*this, 5u, human_right_ball_node))
            return false;
        const std::array<MotorConstraint, anatomy_action_count> expected{
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 1, 7, 8 }, MotorConstraint{ 7, 8, 9 },
            MotorConstraint{ 1, 10, 11 }, MotorConstraint{ 10, 11, 12 },
            MotorConstraint{ 3, human_left_ankle_node,
                human_left_ball_node },
            MotorConstraint{ 5, human_right_ankle_node,
                human_right_ball_node },
            MotorConstraint{ human_left_heel_node, human_left_ball_node,
                human_left_toe_node },
            MotorConstraint{ human_right_heel_node, human_right_ball_node,
                human_right_toe_node }
        };
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            if (!motors[index].enabled
                || motors[index].a != expected[index].a
                || motors[index].pivot != expected[index].pivot
                || motors[index].c != expected[index].c)
                return false;
        }
        return true;
    }

    bool CreatureBlueprint::enforce_human_paired_segment_lengths(
        std::size_t edited_node) noexcept
    {
        if (!human_paired_limb_topology())
            return true;

        CreatureBlueprint candidate = *this;
        struct Segment
        {
            std::uint16_t from{};
            std::uint16_t to{};
            Vec2 direction{};
            float length{};
        };
        std::array<Segment, 8> segments = {
            Segment{ 0u, 3u }, Segment{ 0u, 5u },
            Segment{ 3u, 4u }, Segment{ 5u, 6u },
            Segment{ 7u, 8u }, Segment{ 10u, 11u },
            Segment{ 8u, 9u }, Segment{ 11u, 12u }
        };
        const auto capture_segment = [&](Segment& segment)
        {
            const Vec2 delta = candidate.nodes[segment.to]
                - candidate.nodes[segment.from];
            segment.length = length(delta);
            if (!std::isfinite(segment.length) || segment.length < 0.05f)
                return false;
            segment.direction = delta / segment.length;
            return std::isfinite(segment.direction.x)
                && std::isfinite(segment.direction.y);
        };
        for (Segment& segment : segments)
        {
            if (!capture_segment(segment))
                return false;
        }
        const Vec2 left_heel_offset = candidate.nodes[human_left_heel_node]
            - candidate.nodes[human_left_ankle_node];
        const Vec2 right_heel_offset = candidate.nodes[human_right_heel_node]
            - candidate.nodes[human_right_ankle_node];
        const Vec2 left_ball_offset = candidate.nodes[human_left_ball_node]
            - candidate.nodes[human_left_ankle_node];
        const Vec2 right_ball_offset = candidate.nodes[human_right_ball_node]
            - candidate.nodes[human_right_ankle_node];
        const Vec2 left_toe_offset = candidate.nodes[human_left_toe_node]
            - candidate.nodes[human_left_ball_node];
        const Vec2 right_toe_offset = candidate.nodes[human_right_toe_node]
            - candidate.nodes[human_right_ball_node];
        const bool left_foot_edited = edited_node == human_left_ankle_node
            || edited_node == human_left_heel_node
            || edited_node == human_left_ball_node
            || edited_node == human_left_toe_node;
        const bool right_foot_edited = edited_node == human_right_ankle_node
            || edited_node == human_right_heel_node
            || edited_node == human_right_ball_node
            || edited_node == human_right_toe_node;
        const auto paired_foot_offset = [left_foot_edited, right_foot_edited](
            Vec2 left, Vec2 right) noexcept
        {
            if (left_foot_edited != right_foot_edited)
                return left_foot_edited ? left : right;
            return (left + right) * 0.5f;
        };
        const Vec2 heel_offset = paired_foot_offset(
            left_heel_offset, right_heel_offset);
        const Vec2 ball_offset = paired_foot_offset(
            left_ball_offset, right_ball_offset);
        const Vec2 toe_offset = paired_foot_offset(
            left_toe_offset, right_toe_offset);
        if (!std::isfinite(heel_offset.x) || !std::isfinite(heel_offset.y)
            || !std::isfinite(ball_offset.x) || !std::isfinite(ball_offset.y)
            || !std::isfinite(toe_offset.x) || !std::isfinite(toe_offset.y)
            || length(heel_offset) < 0.05f || length(ball_offset) < 0.05f
            || length(ball_offset - heel_offset) < 0.05f
            || length(toe_offset) < 0.05f)
            return false;
        const auto pair_target = [&](const Segment& a, const Segment& b)
        {
            const bool source_a = edited_node == a.to
                || (a.from != b.from && edited_node == a.from);
            const bool source_b = edited_node == b.to
                || (a.from != b.from && edited_node == b.from);
            const float target = source_a != source_b
                ? (source_a ? a.length : b.length)
                : (a.length + b.length) * 0.5f;
            return std::isfinite(target) && target >= 0.05f
                ? target : 0.0f;
        };
        const auto pair_radius = [&](std::uint16_t a, std::uint16_t b)
        {
            const bool source_a = edited_node == a;
            const bool source_b = edited_node == b;
            const float target = source_a != source_b
                ? (source_a ? candidate.radii[a] : candidate.radii[b])
                : (candidate.radii[a] + candidate.radii[b]) * 0.5f;
            if (!std::isfinite(target) || target <= 0.0f)
                return false;
            candidate.radii[a] = target;
            candidate.radii[b] = target;
            return true;
        };

        const float thigh = pair_target(segments[0], segments[1]);
        const float shin = pair_target(segments[2], segments[3]);
        const float upper_arm = pair_target(segments[4], segments[5]);
        const float forearm = pair_target(segments[6], segments[7]);
        if (thigh == 0.0f || shin == 0.0f || upper_arm == 0.0f
            || forearm == 0.0f)
            return false;

        // Rebuild every paired chain from a single snapshot. Moving a proximal
        // joint therefore cannot rotate, shorten, or invert its distal segment.
        candidate.nodes[3] = candidate.nodes[0] + segments[0].direction * thigh;
        candidate.nodes[5] = candidate.nodes[0] + segments[1].direction * thigh;
        candidate.nodes[4] = candidate.nodes[3] + segments[2].direction * shin;
        candidate.nodes[6] = candidate.nodes[5] + segments[3].direction * shin;
        if (edited_node == std::numeric_limits<std::size_t>::max())
        {
            // Preserve each ankle endpoint and paired segment length while
            // selecting the forward-facing knee solution on both sides.
            constexpr float authored_upper_horizontal = 0.12f;
            constexpr float foot_half_span = 0.17f;
            constexpr float authored_lower_horizontal = foot_half_span
                - authored_upper_horizontal;
            if (thigh <= authored_upper_horizontal
                || shin <= authored_lower_horizontal)
                return false;
            const float knee_drop = std::sqrt(thigh * thigh
                - authored_upper_horizontal * authored_upper_horizontal);
            const float shin_drop = std::sqrt(shin * shin
                - authored_lower_horizontal * authored_lower_horizontal);
            const float left_sign = candidate.nodes[3].x <= candidate.nodes[5].x
                ? -1.0f : 1.0f;
            const auto rebuild_anatomical_leg = [&](std::uint16_t knee,
                std::uint16_t ankle, float stance_sign) noexcept
            {
                const Vec2 ankle_delta{ stance_sign * foot_half_span,
                    -(knee_drop + shin_drop) };
                const float ankle_distance = length(ankle_delta);
                if (!std::isfinite(ankle_distance) || ankle_distance <= 0.05f)
                    return false;
                const Vec2 direction = ankle_delta / ankle_distance;
                const float along = (thigh * thigh - shin * shin
                        + ankle_distance * ankle_distance)
                    / (2.0f * ankle_distance);
                const float bend_squared = thigh * thigh - along * along;
                if (!std::isfinite(along) || bend_squared <= 1.0e-8f)
                    return false;
                const Vec2 facing_normal{ -direction.y, direction.x };
                candidate.nodes[knee] = candidate.nodes[0]
                    + direction * along
                    + facing_normal * std::sqrt(bend_squared);
                candidate.nodes[ankle] = candidate.nodes[0] + ankle_delta;
                return true;
            };
            if (!rebuild_anatomical_leg(3u, human_left_ankle_node, left_sign)
                || !rebuild_anatomical_leg(5u, human_right_ankle_node,
                    -left_sign))
                return false;

            // Keep the paired arms layered beside the torso in the saved/default
            // pose. Training may swing them, but morphology normalization must
            // not turn an authored relaxed stance into a forward balance brace.
            const auto descend_toward = [](Vec2 parent, float target_x,
                float segment_length) noexcept
            {
                const float dx = clamp(target_x - parent.x,
                    -segment_length * 0.70f, segment_length * 0.70f);
                return parent + Vec2{ dx, -std::sqrt(std::max(0.0f,
                    segment_length * segment_length - dx * dx)) };
            };
            candidate.nodes[8] = descend_toward(candidate.nodes[7],
                candidate.nodes[0].x - 0.14f, upper_arm);
            candidate.nodes[11] = descend_toward(candidate.nodes[10],
                candidate.nodes[0].x + 0.12f, upper_arm);
            candidate.nodes[9] = descend_toward(candidate.nodes[8],
                candidate.nodes[0].x - 0.08f, forearm);
            candidate.nodes[12] = descend_toward(candidate.nodes[11],
                candidate.nodes[0].x + 0.10f, forearm);
        }
        else
        {
            candidate.nodes[8] = candidate.nodes[7] + segments[4].direction * upper_arm;
            candidate.nodes[11] = candidate.nodes[10] + segments[5].direction * upper_arm;
            candidate.nodes[9] = candidate.nodes[8] + segments[6].direction * forearm;
            candidate.nodes[12] = candidate.nodes[11] + segments[7].direction * forearm;
        }

        candidate.nodes[human_left_heel_node] =
            candidate.nodes[human_left_ankle_node] + heel_offset;
        candidate.nodes[human_left_ball_node] =
            candidate.nodes[human_left_ankle_node] + ball_offset;
        candidate.nodes[human_left_toe_node] =
            candidate.nodes[human_left_ball_node] + toe_offset;
        candidate.nodes[human_right_heel_node] =
            candidate.nodes[human_right_ankle_node] + heel_offset;
        candidate.nodes[human_right_ball_node] =
            candidate.nodes[human_right_ankle_node] + ball_offset;
        candidate.nodes[human_right_toe_node] =
            candidate.nodes[human_right_ball_node] + toe_offset;

        if (edited_node == std::numeric_limits<std::size_t>::max())
        {
            // The geometry migration changes joint angles, not calibrated
            // travel or strength. Rebase only the physical leg/ankle neutrals
            // so action zero is the saved pose rather than an instant branch
            // flip on the first simulation step.
            constexpr std::array<std::size_t, 6> lower_body_motors{
                0u, 1u, 2u, 3u,
                human_left_ankle_action, human_right_ankle_action
            };
            for (const std::size_t index : lower_body_motors)
            {
                if (index >= candidate.active_motor_count)
                    continue;
                MotorConstraint& motor = candidate.motors[index];
                const float negative_span = std::max(0.0f,
                    motor.neutral_angle - motor.minimum_angle);
                const float positive_span = std::max(0.0f,
                    motor.maximum_angle - motor.neutral_angle);
                const float neutral = candidate.rest_joint_angle(index);
                motor.neutral_angle = neutral;
                motor.minimum_angle = neutral - negative_span;
                motor.maximum_angle = neutral + positive_span;
            }
        }

        if (!pair_radius(3u, 5u) || !pair_radius(4u, 6u)
            || !pair_radius(7u, 10u) || !pair_radius(8u, 11u)
            || !pair_radius(9u, 12u)
            || !pair_radius(human_left_heel_node, human_right_heel_node)
            || !pair_radius(human_left_ball_node, human_right_ball_node)
            || !pair_radius(human_left_toe_node, human_right_toe_node))
            return false;
        *this = std::move(candidate);
        return true;
    }
    CreatureBlueprint CreatureBlueprint::quadruped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { -0.65f, 2.12f }, { 0.65f, 2.16f }, { 1.38f, 2.46f },
            { -0.82f, 1.18f }, { -0.94f, 0.24f },
            { -0.48f, 1.14f }, { -0.40f, 0.24f },
            { 0.48f, 1.16f }, { 0.38f, 0.24f },
            { 0.82f, 1.14f }, { 0.92f, 0.24f }
        };
        result.radii = {
            0.31f, 0.32f, 0.25f,
            0.16f, 0.14f, 0.16f, 0.14f,
            0.16f, 0.14f, 0.16f, 0.14f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.96f },
            { 0, 2, 0.0f, 0.74f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f },
            { 1, 7, 0.0f, 1.0f }, { 7, 8, 0.0f, 1.0f },
            { 1, 9, 0.0f, 1.0f }, { 9, 10, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 0, 1, 7 }, MotorConstraint{ 1, 7, 8 },
            MotorConstraint{ 0, 1, 9 }, MotorConstraint{ 1, 9, 10 }
        };
        result.active_motor_count = 8u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.additional_left_contact_nodes = { 10u };
        result.right_contact_node = 6u;
        result.additional_right_contact_nodes = { 8u };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 48.0f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::crawler4()
    {
        CreatureBlueprint result{};
        result.species_identity = CreatureSpecies::dog;
        // Medium dog-scale four-leg anatomy. Near/far pairs remain layered in
        // side view while shoulder and hip spacing preserve a readable gait.
        result.nodes = {
            { -0.46f, 0.80f }, { 0.38f, 0.84f }, { 0.78f, 1.08f },
            { -0.56f, 0.46f }, { -0.48f, 0.11f },
            { -0.33f, 0.46f }, { -0.25f, 0.11f },
            { 0.27f, 0.46f }, { 0.23f, 0.11f },
            { 0.48f, 0.46f }, { 0.52f, 0.11f }
        };
        result.radii = {
            0.23f, 0.24f, 0.20f,
            0.09f, 0.075f, 0.09f, 0.075f,
            0.09f, 0.075f, 0.09f, 0.075f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.94f },
            { 0, 2, 0.0f, 0.66f },
            { 0, 3, 0.0f, 0.98f }, { 3, 4, 0.0f, 0.98f },
            { 0, 5, 0.0f, 0.98f }, { 5, 6, 0.0f, 0.98f },
            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f },
            { 1, 9, 0.0f, 0.98f }, { 9, 10, 0.0f, 0.98f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 0, 1, 7 }, MotorConstraint{ 1, 7, 8 },
            MotorConstraint{ 0, 1, 9 }, MotorConstraint{ 1, 9, 10 }
        };
        result.active_motor_count = 8u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.additional_left_contact_nodes = { 10u };
        result.right_contact_node = 6u;
        result.additional_right_contact_nodes = { 8u };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 47.0f);
        return result;
    }
    CreatureBlueprint CreatureBlueprint::hexapod()
    {
        CreatureBlueprint result{};
        result.species_identity = CreatureSpecies::hexapod;
        // Three longitudinal pairs, each with a visible knee. The outer pairs use
        // the eight policy channels and the middle pair is coupled into opposing
        // tripods by CreatureBlueprint::coupled_support_motors().
        result.nodes = {
            { -0.54f, 0.93f }, { 0.00f, 0.98f },
            { 0.54f, 0.94f }, { 0.84f, 1.08f },
            { -0.73f, 0.49f }, { -0.84f, 0.11f },
            { -0.44f, 0.46f }, { -0.34f, 0.11f },
            { -0.16f, 0.43f }, { -0.08f, 0.11f },
            { 0.18f, 0.43f }, { 0.08f, 0.11f },
            { 0.44f, 0.46f }, { 0.34f, 0.11f },
            { 0.73f, 0.49f }, { 0.84f, 0.11f }
        };
        result.radii = {
            0.18f, 0.20f, 0.18f, 0.14f,
            0.085f, 0.065f, 0.085f, 0.065f,
            0.085f, 0.065f, 0.085f, 0.065f,
            0.085f, 0.065f, 0.085f, 0.065f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 2, 3, 0.0f, 0.94f }, { 0, 2, 0.0f, 0.68f },
            { 0, 4, 0.0f, 0.98f }, { 4, 5, 0.0f, 0.98f },
            { 0, 6, 0.0f, 0.98f }, { 6, 7, 0.0f, 0.98f },
            { 1, 8, 0.0f, 0.96f }, { 8, 9, 0.0f, 0.96f },
            { 1, 10, 0.0f, 0.96f }, { 10, 11, 0.0f, 0.96f },
            { 2, 12, 0.0f, 0.98f }, { 12, 13, 0.0f, 0.98f },
            { 2, 14, 0.0f, 0.98f }, { 14, 15, 0.0f, 0.98f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 4 }, MotorConstraint{ 0, 4, 5 },
            MotorConstraint{ 1, 0, 6 }, MotorConstraint{ 0, 6, 7 },
            MotorConstraint{ 1, 2, 12 }, MotorConstraint{ 2, 12, 13 },
            MotorConstraint{ 1, 2, 14 }, MotorConstraint{ 2, 14, 15 }
        };
        result.active_motor_count = 8u;
        result.root_node = 1u;
        result.torso_node = 2u;
        result.head_node = 3u;
        result.left_contact_node = 5u;
        result.additional_left_contact_nodes = { 9u, 13u };
        result.right_contact_node = 7u;
        result.additional_right_contact_nodes = { 11u, 15u };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 58.0f);
        return result;
    }
    CreatureBlueprint CreatureBlueprint::monoped()
    {
        CreatureBlueprint result{};
        // Keep the single support chain away from the 180-degree IK branch.
        // A perfectly collinear hip/knee/ankle has no authored bend side, so
        // an arbitrarily small solver perturbation could invert the whole rig.
        result.nodes = {
            { 0.00f, 2.75f }, { -0.03f, 3.76f }, { -0.04f, 4.52f },
            { 0.20f, 1.56f }, { -0.34f, 0.22f },
            { -0.04f, 0.67f }, { 0.34f, 0.22f }
        };
        result.radii = { 0.27f, 0.29f, 0.25f, 0.19f, 0.17f, 0.18f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 5, 0.0f, 1.0f },
            { 5, 4, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.92f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 5 },
            MotorConstraint{ 3, 5, 4 }, MotorConstraint{ 3, 5, 6 }
        };
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 32.0f, 48.0f, 0.043f, 0.049f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::for_species(CreatureSpecies species)
    {
        switch (species)
        {
        case CreatureSpecies::human: return humanoid();
        case CreatureSpecies::chicken: return chicken();
        case CreatureSpecies::dog: return crawler4();
        case CreatureSpecies::hexapod: return hexapod();
        case CreatureSpecies::custom: return scaffold();
        }
        return scaffold();
    }

    CreatureBlueprint CreatureBlueprint::load_owned_or_default(
        CreatureSpecies species, const std::filesystem::path& owned_path,
        const std::filesystem::path& legacy_path, std::string& source_note)
    {
        CreatureBlueprint fallback = for_species(species);
        std::string load_error{};
        if (std::filesystem::exists(owned_path))
        {
            if (auto loaded = load_for_species(owned_path, species, load_error))
            {
                source_note = owned_path.filename().string();
                return std::move(*loaded);
            }
            source_note = std::format("{} REJECTED; FACTORY DEFAULT: {}",
                owned_path.filename().string(), load_error);
            return fallback;
        }

        if (!legacy_path.empty() && std::filesystem::exists(legacy_path))
        {
            if (auto migrated = load_for_species(legacy_path, species, load_error))
            {
                std::string save_error{};
                if (migrated->save(owned_path, save_error))
                    source_note = std::format("{} MIGRATED TO {}",
                        legacy_path.filename().string(), owned_path.filename().string());
                else
                    source_note = std::format("{} LOADED; MIGRATION FAILED: {}",
                        legacy_path.filename().string(), save_error);
                return std::move(*migrated);
            }
            source_note = std::format("{} REJECTED FOR {}; FACTORY DEFAULT: {}",
                legacy_path.filename().string(), creature_species_slug(species), load_error);
            return fallback;
        }

        source_note = "FACTORY DEFAULT";
        return fallback;
    }

    void CreatureBlueprint::rebuild_rest_lengths() noexcept
    {
        (void)enforce_human_paired_segment_lengths();
        for (DistanceConstraint& bone : bones)
        {
            if (bone.a < nodes.size() && bone.b < nodes.size())
                bone.rest_length = std::max(0.05f,
                    length(nodes[bone.b] - nodes[bone.a]));
            bone.stiffness = clamp(bone.stiffness, 0.05f, 1.0f);
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
        // A rig signature identifies authored structure, not the least-significant
        // bit selected by decimal persistence or an idempotent normalization pass.
        // Quantizing below editor resolution keeps save/load signatures stable while
        // still invalidating checkpoints for any meaningful anatomy change.
        auto add_float = [&](float value)
        {
            constexpr double signature_units_per_world_unit = 100'000.0;
            if (!std::isfinite(value))
            {
                add_u64(std::bit_cast<std::uint32_t>(value));
                return;
            }
            const auto quantized = static_cast<std::int64_t>(
                std::llround(static_cast<double>(value)
                    * signature_units_per_world_unit));
            add_u64(static_cast<std::uint64_t>(quantized));
        };

        add_u64(species_identity.has_value() ? 1u : 0u);
        add_u64(species_identity.has_value()
            ? static_cast<std::uint64_t>(*species_identity) : 0u);
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
        CreatureBlueprint normalized = *this;
        const CreatureSpecies owned_species = normalized.presentation_species();
        if (owned_species == CreatureSpecies::human)
        {
            if (legacy_human_paired_limb_topology(normalized))
            {
                if (!promote_human_articulated_feet(normalized))
                {
                    error = "Legacy Human feet could not be promoted to RUNRIG7.";
                    return false;
                }
            }
            else if (normalized.nodes.size() == 19u
                && normalized.active_motor_count
                    == human_right_ankle_action + 1u
                && !promote_runrig6_human_explicit_toes(normalized))
            {
                error = "RUNRIG6 Human feet could not be promoted to RUNRIG7.";
                return false;
            }
        }
        if (!normalized.enforce_human_paired_segment_lengths())
        {
            error = "Human paired limb dimensions could not be normalized.";
            return false;
        }
        normalized.rebuild_rest_lengths();
        if (!normalized.valid())
        {
            error = "Rig is structurally invalid and was not saved.";
            return false;
        }
        if (!normalized.topology_compatible_with_species(owned_species))
        {
            error = "Rig topology is incompatible with its owned species identity.";
            return false;
        }
        // Saving a legacy rig is its one-time migration into a RUNRIG7
        // document whose Human signature includes the canonical physical feet
        // and independently authored ankle and toe motors.
        normalized.species_identity = owned_species;

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
            output << "RUNRIG 7\n";
            output << normalized.nodes.size() << ' ' << normalized.bones.size()
                << ' ' << normalized.active_motor_count << '\n';
            output << "P " << creature_species_slug(*normalized.species_identity) << '\n';
            output << "S " << normalized.root_node << ' ' << normalized.torso_node
                << ' ' << normalized.head_node << ' ' << normalized.left_contact_node
                << ' ' << normalized.right_contact_node << '\n';
            output << "L " << normalized.additional_left_contact_nodes.size();
            for (const std::uint16_t node : normalized.additional_left_contact_nodes)
                output << ' ' << node;
            output << '\n';
            output << "R " << normalized.additional_right_contact_nodes.size();
            for (const std::uint16_t node : normalized.additional_right_contact_nodes)
                output << ' ' << node;
            output << '\n';
            output << std::setprecision(9);
            for (std::size_t index = 0; index < normalized.nodes.size(); ++index)
            {
                output << "N " << normalized.nodes[index].x << ' '
                    << normalized.nodes[index].y << ' '
                    << normalized.radii[index] << '\n';
            }
            for (const DistanceConstraint& bone : normalized.bones)
            {
                output << "B " << bone.a << ' ' << bone.b << ' '
                    << bone.rest_length << ' ' << bone.stiffness << '\n';
            }
            for (std::size_t motor_index = 0;
                motor_index < normalized.active_motor_count; ++motor_index)
            {
                const MotorConstraint& motor = normalized.motors[motor_index];
                output << "M " << (motor.enabled ? 1 : 0) << ' ' << motor.a
                    << ' ' << motor.pivot << ' ' << motor.c << ' '
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
            || (version != 1 && version != 2 && version != 3 && version != 4
                && version != 5 && version != 6 && version != 7)
            || node_count < 3 || node_count > 128 || bone_count > 256
            || motor_count == 0u || motor_count > anatomy_action_count)
        {
            error = "Invalid or unsupported Epoch2DWalkEngine rig file.";
            return humanoid();
        }

        CreatureBlueprint result{};
        std::optional<CreatureSpecies> declared_species{};
        result.active_motor_count = motor_count;
        for (MotorConstraint& motor : result.motors)
            motor.enabled = false;
        if (version >= 5)
        {
            char species_tag{};
            std::string species_slug{};
            input >> species_tag >> species_slug;
            declared_species = creature_species_from_slug(species_slug);
            if (!input || species_tag != 'P' || !declared_species.has_value())
            {
                error = "Invalid rig species identity.";
                return humanoid();
            }
        }
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

        if (version < 6
            && (!declared_species.has_value()
                || *declared_species == CreatureSpecies::human)
            && legacy_human_paired_limb_topology(result)
            && !promote_human_articulated_feet(result))
        {
            error = "Legacy Human feet could not be promoted to RUNRIG7.";
            return humanoid();
        }
        if (version == 6
            && (!declared_species.has_value()
                || *declared_species == CreatureSpecies::human)
            && !promote_runrig6_human_explicit_toes(result))
        {
            error = "RUNRIG6 Human feet could not be promoted to RUNRIG7.";
            return humanoid();
        }
        if (!result.enforce_human_paired_segment_lengths())
        {
            error = "Human paired limb dimensions are invalid.";
            return humanoid();
        }
        result.rebuild_rest_lengths();
        if (!result.valid())
        {
            error = "Rig is structurally invalid or has invalid semantic nodes.";
            return humanoid();
        }
        if (declared_species.has_value()
            && !result.topology_compatible_with_species(*declared_species))
        {
            error = "Rig declared species does not match its topology.";
            return humanoid();
        }
        result.species_identity = declared_species;
        error.clear();
        return result;
    }

    std::optional<CreatureBlueprint> CreatureBlueprint::load_for_species(
        const std::filesystem::path& path, CreatureSpecies expected,
        std::string& error)
    {
        CreatureBlueprint candidate = load(path, error);
        if (!error.empty())
            return std::nullopt;
        const CreatureSpecies candidate_species = candidate.species_identity
            .value_or(candidate.inferred_species());
        if (candidate_species != expected
            || !candidate.topology_compatible_with_species(expected))
        {
            error = "Rig topology is "
                + std::string(creature_species_slug(candidate_species))
                + ", not " + std::string(creature_species_slug(expected)) + ".";
            return std::nullopt;
        }
        candidate.species_identity = expected;
        return candidate;
    }

    Environment::Environment()
        : Environment(CreatureBlueprint::humanoid(), 1)
    {
    }

    Environment::Environment(const CreatureBlueprint& blueprint, std::uint64_t seed)
        : blueprint_(blueprint), random_state_(seed == 0 ? 1 : seed)
    {
        blueprint_.rebuild_rest_lengths();
        reset(seed);
    }

    void Environment::set_blueprint(const CreatureBlueprint& blueprint)
    {
        blueprint_ = blueprint;
        blueprint_.rebuild_rest_lengths();
        course_layout_initialized_ = false;
        reset(random_state_);
    }

    float Environment::authored_standing_head_clearance() const noexcept
    {
        if (blueprint_.head_node >= blueprint_.nodes.size())
            return 4.30f;
        float vertical_shift = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
        {
            if (!blueprint_.is_support_seed(index))
                continue;
            const float radius = index < particles_.size()
                ? particles_[index].radius
                : index < blueprint_.radii.size() ? blueprint_.radii[index] : 0.15f;
            vertical_shift = std::max(vertical_shift,
                ground_contact_offset(true, radius) - blueprint_.nodes[index].y);
        }
        if (!std::isfinite(vertical_shift))
            vertical_shift = 0.0f;
        const float head_radius = blueprint_.head_node < particles_.size()
            ? particles_[blueprint_.head_node].radius
            : blueprint_.head_node < blueprint_.radii.size()
                ? blueprint_.radii[blueprint_.head_node] : 0.15f;
        return std::max(0.0f,
            blueprint_.nodes[blueprint_.head_node].y + head_radius + vertical_shift);
    }

    void Environment::set_course(CourseStage stage, float difficulty)
    {
        course_stage_ = stage;
        course_difficulty_ = clamp(difficulty, 0.10f, 1.0f);
        course_layout_initialized_ = false;
        reset(random_state_);
    }

    void Environment::set_gait_task(GaitTask task) noexcept
    {
        gait_task_ = task;
    }

    void Environment::set_guidance_mode(GuidanceMode mode) noexcept
    {
        if (mode == GuidanceMode::raw_policy_audit
            && guidance_mode_ != GuidanceMode::raw_policy_audit)
        {
            // A teacher-created ledge grasp is not policy state. Retire its
            // live anchors at the authority boundary so a later Raw step
            // cannot inherit an actionless hand constraint or teacher-earned
            // climb/descent credit.
            ledge_grasp_nodes_.fill(std::numeric_limits<std::uint16_t>::max());
            ledge_grasp_anchors_.fill({});
            hand_ledge_contacts_ = 0u;
            climb_support_transfers_ = 0u;
            ledge_climbs_ = 0u;
            controlled_descents_ = 0u;
            ledge_climbed_ = false;
            ledge_descending_ = false;
            previous_root_height_ = valid_node(blueprint_.root_node)
                ? particles_[blueprint_.root_node].position.y : 0.0f;
            clear_crouch_walk_guidance_state();
        }
        guidance_mode_ = mode;
    }

    void Environment::clear_crouch_walk_guidance_state() noexcept
    {
        action_support_unload_side_ = 0;
        last_action_support_unload_side_ = 0;
        support_cluster_release_hold_seconds_.fill(0.0f);
        support_toe_roll_seconds_.fill(0.0f);
        articulated_toe_commands_.fill(0.0f);
    }

    void Environment::set_physical_guidance_authority(float authority) noexcept
    {
        const float previous = physical_guidance_authority_;
        const float next = std::isfinite(authority)
            ? clamp(authority, 0.0f, 1.0f) : 0.0f;
        physical_guidance_authority_ = next;

        if (guidance_mode_ == GuidanceMode::raw_policy_audit)
        {
            clear_crouch_walk_guidance_state();
            return;
        }
        if (course_stage_ != CourseStage::crouch_walk)
            return;
        if (next <= 0.0f)
        {
            clear_crouch_walk_guidance_state();
            return;
        }
        if (next >= previous || previous <= 0.0f)
            return;

        // A falling lesson authority may shorten an already-open teacher
        // release window, but can never extend or re-arm one. This keeps the
        // stateful contact guide monotone with the same scalar as its lift.
        const float ratio = next / previous;
        for (float& hold : support_cluster_release_hold_seconds_)
            hold *= ratio;
        for (float& roll : support_toe_roll_seconds_)
            roll *= ratio;
        for (float& command : articulated_toe_commands_)
            command *= ratio;
    }

    void Environment::configure_equipment(WeaponClass weapon,
        float target_distance)
    {
        equipment_override_ = true;
        configured_weapon_class_ = weapon;
        configured_target_distance_ = clamp(target_distance, 3.0f, 24.0f);
        reset(random_state_);
    }

    void Environment::clear_equipment() noexcept
    {
        equipment_override_ = true;
        configured_weapon_class_ = WeaponClass::none;
        reset_equipment();
    }

    void Environment::disarm_equipment() noexcept
    {
        if (weapon_class_ == WeaponClass::none
            || equipment_state_ == EquipmentState::unarmed
            || equipment_state_ == EquipmentState::disarmed
            || equipment_state_ == EquipmentState::dropped)
            return;
        equipment_state_ = EquipmentState::disarmed;
        dropped_equipment_position_ = equipment_mount_position();
        dropped_equipment_velocity_ = { -0.5f, 1.0f };
        ++equipment_transition_count_;
    }

    void Environment::set_diagnostic_rigid_rotation(float radians) noexcept
    {
        if (!std::isfinite(radians) || !valid_node(blueprint_.root_node))
            return;
        const Vec2 pivot = particles_[blueprint_.root_node].position;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        for (Particle& particle : particles_)
        {
            const Vec2 offset = particle.position - pivot;
            particle.position = pivot + Vec2{
                offset.x * cosine - offset.y * sine,
                offset.x * sine + offset.y * cosine };
            particle.previous = particle.position;
            particle.grounded = false;
        }
    }

    std::uint16_t Environment::equipment_mount_node() const noexcept
    {
        std::uint16_t best = blueprint_.torso_node;
        float best_reach = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
        {
            if (index == blueprint_.head_node || index == blueprint_.root_node
                || index == blueprint_.torso_node
                || blueprint_.is_support_seed(index)
                || !manipulator_endpoint(blueprint_, index))
                continue;
            const float reach = blueprint_.nodes[index].x
                - blueprint_.nodes[blueprint_.torso_node].x;
            if (reach > best_reach)
            {
                best_reach = reach;
                best = static_cast<std::uint16_t>(index);
            }
        }
        return best;
    }

    std::uint16_t Environment::equipment_foregrip_node() const noexcept
    {
        constexpr std::uint16_t invalid =
            std::numeric_limits<std::uint16_t>::max();
        const std::uint16_t rear = equipment_mount_node();
        std::uint16_t front = invalid;
        for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
        {
            if (index == rear || index == blueprint_.head_node
                || index == blueprint_.root_node || index == blueprint_.torso_node
                || blueprint_.is_support_seed(index)
                || !manipulator_endpoint(blueprint_, index))
                continue;
            if (front != invalid)
                return invalid;
            front = static_cast<std::uint16_t>(index);
        }
        return front;
    }

    std::uint16_t Environment::equipment_mount_shoulder_node() const noexcept
    {
        constexpr std::uint16_t invalid =
            std::numeric_limits<std::uint16_t>::max();
        const std::uint16_t rear = equipment_mount_node();
        for (const MotorConstraint& distal : blueprint_.motors)
        {
            if (!distal.enabled || distal.c != rear
                || distal.pivot >= blueprint_.nodes.size())
                continue;
            for (const MotorConstraint& proximal : blueprint_.motors)
            {
                if (!proximal.enabled || proximal.a != blueprint_.torso_node
                    || proximal.c != distal.pivot
                    || proximal.pivot >= blueprint_.nodes.size())
                    continue;
                return proximal.pivot;
            }
        }
        return invalid;
    }

    std::uint16_t Environment::equipment_mount_parent_node() const noexcept
    {
        const std::uint16_t mount = equipment_mount_node();
        std::uint16_t parent = blueprint_.torso_node;
        float shortest = std::numeric_limits<float>::infinity();
        for (const DistanceConstraint& constraint : blueprint_.bones)
        {
            const bool mount_is_a = constraint.a == mount;
            const bool mount_is_b = constraint.b == mount;
            if (!mount_is_a && !mount_is_b)
                continue;
            const std::uint16_t candidate = mount_is_a ? constraint.b : constraint.a;
            if (candidate >= blueprint_.nodes.size())
                continue;
            const float authored_distance = length(
                blueprint_.nodes[candidate] - blueprint_.nodes[mount]);
            if (authored_distance < shortest)
            {
                shortest = authored_distance;
                parent = candidate;
            }
        }
        return parent;
    }

    Vec2 Environment::equipment_mount_position() const noexcept
    {
        const std::uint16_t node = equipment_mount_node();
        if (!valid_node(node))
            return {};
        return particles_[node].position;
    }

    EquipmentGripFrame Environment::equipment_grip_frame() const noexcept
    {
        EquipmentGripFrame frame{};
        if (weapon_class_ == WeaponClass::none
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node))
            return frame;
        frame.rear_node = equipment_mount_node();
        frame.front_node = equipment_foregrip_node();
        frame.rear_shoulder_node = equipment_mount_shoulder_node();
        if (!valid_node(frame.rear_node) || !valid_node(frame.front_node)
            || !valid_node(frame.rear_shoulder_node)
            || frame.rear_node == frame.front_node)
            return frame;

        frame.rear = particles_[frame.rear_node].position;
        frame.front = particles_[frame.front_node].position;
        const Vec2 grip_delta = frame.front - frame.rear;
        frame.span = length(grip_delta);
        const bool finite_grips = std::isfinite(frame.rear.x)
            && std::isfinite(frame.rear.y) && std::isfinite(frame.front.x)
            && std::isfinite(frame.front.y) && std::isfinite(frame.span);
        if (!finite_grips || frame.span <= 1.0e-4f)
            return frame;

        frame.axis = grip_delta / frame.span;
        const WeaponGeometry geometry = weapon_geometry(weapon_class_);
        frame.muzzle = frame.rear + frame.axis * geometry.rear_to_muzzle;
        frame.stock_butt = frame.rear - frame.axis * geometry.butt_to_rear_grip;

        const Vec2 body_up = normalized(
            particles_[blueprint_.torso_node].position
                - particles_[blueprint_.root_node].position,
            { 0.0f, 1.0f });
        const float facing = facing_direction() < 0.0f ? -1.0f : 1.0f;
        const Vec2 body_forward = facing * Vec2{ body_up.y, -body_up.x };
        const Vec2 shoulder = particles_[frame.rear_shoulder_node].position;
        frame.shoulder_pocket = geometry.shouldered
            ? shoulder + body_forward * 0.04f - body_up * 0.10f
            : shoulder + body_forward * 0.28f - body_up * 0.16f;
        frame.stock_seat_error = geometry.shouldered
            ? length(frame.stock_butt - frame.shoulder_pocket) : 0.0f;
        frame.resolved = std::isfinite(frame.axis.x)
            && std::isfinite(frame.axis.y) && std::isfinite(frame.muzzle.x)
            && std::isfinite(frame.muzzle.y)
            && std::isfinite(frame.stock_seat_error);
        if (!frame.resolved)
            return frame;

        const float span_tolerance = geometry.shouldered ? 0.10f : 0.06f;
        const float forward_alignment = dot(frame.axis, body_forward);
        const bool span_valid = std::abs(frame.span
            - geometry.rear_to_foregrip) <= span_tolerance;
        const bool stock_valid = !geometry.shouldered
            || frame.stock_seat_error <= 0.12f;
        frame.valid = span_valid && stock_valid
            && forward_alignment >= 0.85f;
        return frame;
    }

    Vec2 Environment::equipment_muzzle_position() const noexcept
    {
        const EquipmentGripFrame frame = equipment_grip_frame();
        if (frame.resolved
            && equipment_state_ != EquipmentState::dropped
            && equipment_state_ != EquipmentState::disarmed)
            return frame.muzzle;
        const Vec2 direction{
            std::cos(equipment_aim_angle_), std::sin(equipment_aim_angle_) };
        return equipment_display_position()
            + direction * weapon_geometry(weapon_class_).rear_to_muzzle;
    }

    Vec2 Environment::equipment_display_position() const noexcept
    {
        if (equipment_state_ == EquipmentState::dropped
            || equipment_state_ == EquipmentState::disarmed)
            return dropped_equipment_position_;
        return equipment_mount_position();
    }

    Vec2 Environment::equipment_display_direction() const noexcept
    {
        if (equipment_state_ != EquipmentState::dropped
            && equipment_state_ != EquipmentState::disarmed)
        {
            const EquipmentGripFrame frame = equipment_grip_frame();
            if (frame.resolved)
                return frame.axis;
        }
        return { std::cos(equipment_aim_angle_), std::sin(equipment_aim_angle_) };
    }

    float Environment::equipment_recommended_aim_angle() const noexcept
    {
        if (weapon_class_ == WeaponClass::none || !equipment_target_.active)
            return facing_direction() < 0.0f ? pi : 0.0f;
        const WeaponProfile profile = weapon_profile(weapon_class_);
        return wrap_angle(ballistic_low_arc_angle(equipment_muzzle_position(),
            equipment_target_.position, profile.projectile_speed,
            profile.gravity, facing_direction()) + equipment_aim_correction_);
    }

    float Environment::equipment_obstruction_fraction(Vec2 beginning,
        Vec2 ending, float radius) const noexcept
    {
        if (!std::isfinite(beginning.x) || !std::isfinite(beginning.y)
            || !std::isfinite(ending.x) || !std::isfinite(ending.y)
            || !std::isfinite(radius) || radius < 0.0f)
            return 0.0f;
        const Vec2 segment = ending - beginning;
        const float span = length(segment);
        const int samples = std::clamp(static_cast<int>(std::ceil(
            span / std::max(DeformableTerrain::fine_cell_spacing * 0.50f,
                0.0125f))), 1, 256);
        for (int sample = 1; sample <= samples; ++sample)
        {
            const float fraction = static_cast<float>(sample)
                / static_cast<float>(samples);
            const Vec2 point = beginning + segment * fraction;
            if (point.y - radius <= ground_height_at(point.x) + 0.002f)
                return fraction;
            for (const CourseFeature& feature : course_features_)
            {
                if (feature.kind == CourseFeatureKind::projectile)
                    continue;
                bool blocked{};
                if (feature.kind == CourseFeatureKind::rock
                    || feature.kind == CourseFeatureKind::moving_hazard)
                    blocked = length(point - feature.center)
                        <= radius + feature.radius;
                else
                {
                    const Vec2 minimum = feature.center - feature.half_extent
                        - Vec2{ radius, radius };
                    const Vec2 maximum = feature.center + feature.half_extent
                        + Vec2{ radius, radius };
                    blocked = point.x >= minimum.x && point.x <= maximum.x
                        && point.y >= minimum.y && point.y <= maximum.y;
                }
                if (blocked)
                    return fraction;
            }
        }
        return 2.0f;
    }

    bool Environment::equipment_target_visible() const noexcept
    {
        if (weapon_class_ == WeaponClass::none || !equipment_target_.active)
            return false;
        const WeaponProfile profile = weapon_profile(weapon_class_);
        const float angle = equipment_recommended_aim_angle();
        const Vec2 direction{ std::cos(angle), std::sin(angle) };
        const Vec2 origin = equipment_muzzle_position();
        const float horizontal_velocity = direction.x * profile.projectile_speed;
        if (std::abs(horizontal_velocity) <= 1.0e-5f)
            return false;
        const float flight_seconds = (equipment_target_.position.x - origin.x)
            / horizontal_velocity;
        if (!std::isfinite(flight_seconds) || flight_seconds <= 0.0f
            || flight_seconds > profile.projectile_lifetime_seconds)
            return false;
        const int samples = std::clamp(static_cast<int>(std::ceil(
            flight_seconds * 120.0f)), 8, 384);
        Vec2 previous = origin;
        for (int sample = 1; sample < samples; ++sample)
        {
            const float time = flight_seconds * static_cast<float>(sample)
                / static_cast<float>(samples);
            Vec2 current = origin + direction * (profile.projectile_speed * time);
            current.y -= 0.5f * profile.gravity * time * time;
            if (equipment_obstruction_fraction(previous, current,
                    profile.projectile_radius) <= 1.0f)
                return false;
            previous = current;
        }
        return true;
    }

    std::uint32_t Environment::equipment_hit_goal() const noexcept
    {
        if (course_stage_ == CourseStage::equipment_targets)
            return 3u;
        if (course_stage_ == CourseStage::combat_course)
            return 2u;
        return weapon_class_ == WeaponClass::none ? 0u : 1u;
    }

    EquipmentFireBlock Environment::equipment_fire_block() const noexcept
    {
        if (weapon_class_ == WeaponClass::none)
            return EquipmentFireBlock::unarmed;
        if (equipment_state_ == EquipmentState::reloading)
            return EquipmentFireBlock::reloading;
        if (equipment_rounds_remaining_ == 0u)
            return EquipmentFireBlock::empty;
        if (equipment_hit_goal() > 0u
            && target_hits_ >= equipment_hit_goal())
            return EquipmentFireBlock::goal_complete;
        if (!equipment_target_.active)
            return EquipmentFireBlock::no_target;
        if (!equipment_grip_valid())
            return EquipmentFireBlock::invalid_grip;
        if (equipment_body_speed_ > 0.18f)
            return EquipmentFireBlock::moving;
        if (!equipment_stopped_)
            return EquipmentFireBlock::unplanted;
        if (equipment_state_ != EquipmentState::ready)
            return EquipmentFireBlock::wrong_state;
        if (equipment_state_seconds_ < 0.05f
            || equipment_aim_settle_seconds_ < 0.30f
            || equipment_aim_rate_ > 0.28f)
            return EquipmentFireBlock::unsettled;
        if (equipment_cooldown_seconds_ > 0.0f
            || equipment_recoil_recovery_seconds_ > 0.0f)
            return EquipmentFireBlock::cooldown;
        if (granular_hazard_active() && !granular_hazard_safe())
            return EquipmentFireBlock::hazardous_footing;
        const WeaponProfile profile = weapon_profile(weapon_class_);
        const Vec2 delta = equipment_target_.position - equipment_muzzle_position();
        const float distance = length(delta);
        if (!equipment_fire_range_valid(distance, profile))
            return EquipmentFireBlock::unsafe_range;
        if (!equipment_target_visible())
            return EquipmentFireBlock::occluded;
        const float desired = equipment_recommended_aim_angle();
        const float aim_error = std::abs(wrap_angle(desired - equipment_aim_angle_));
        return std::isfinite(aim_error) && aim_error <= profile.aim_tolerance
            ? EquipmentFireBlock::none : EquipmentFireBlock::unsettled;
    }

    bool Environment::equipment_engagement_ready() const noexcept
    {
        return equipment_fire_block() == EquipmentFireBlock::none;
    }

    void Environment::record_equipment_miss(Vec2 impact) noexcept
    {
        if (equipment_target_.active)
        {
            last_shot_miss_error_ = impact.y - equipment_target_.position.y;
            equipment_aim_correction_ = aim_correction_from_miss(
                equipment_aim_correction_, last_shot_miss_error_,
                std::abs(equipment_target_.position.x
                    - equipment_muzzle_position().x));
        }
        ++shots_missed_;
    }

    void Environment::reset_equipment() noexcept
    {
        equipment_projectiles_.clear();
        equipment_projectile_sequence_ = 0u;
        shots_fired_ = 0u;
        target_hits_ = 0u;
        equipment_transition_count_ = 0u;
        target_hit_this_step_ = false;
        equipment_aim_angle_ = facing_direction() < 0.0f ? pi : 0.0f;
        previous_equipment_aim_angle_ = equipment_aim_angle_;
        equipment_commanded_aim_angle_ = equipment_aim_angle_;
        equipment_aim_rate_ = 0.0f;
        equipment_aim_settle_seconds_ = 0.0f;
        equipment_aim_correction_ = 0.0f;
        last_shot_miss_error_ = 0.0f;
        equipment_stopped_ = false;
        equipment_body_speed_ = 0.0f;
        equipment_plant_seconds_ = 0.0f;
        equipment_state_seconds_ = 0.0f;
        equipment_reload_seconds_ = 0.0f;
        equipment_recoil_recovery_seconds_ = 0.0f;
        equipment_rounds_remaining_ = 0u;
        equipment_fire_block_ = EquipmentFireBlock::unarmed;
        shots_missed_ = 0u;
        equipment_cooldown_seconds_ = 0.0f;
        dropped_equipment_position_ = {};
        dropped_equipment_velocity_ = {};

        const bool equipment_lesson = course_stage_ == CourseStage::equipment_targets
            || course_stage_ == CourseStage::combat_course;
        const bool physical_mount = blueprint_.presentation_species()
                == CreatureSpecies::human
            && equipment_mount_node() != blueprint_.torso_node
            && manipulator_branch_node(blueprint_, equipment_mount_node());
        if (!physical_mount)
            weapon_class_ = WeaponClass::none;
        else if (equipment_override_)
            weapon_class_ = configured_weapon_class_;
        else
            weapon_class_ = configured_weapon_class_;
        equipment_state_ = weapon_class_ == WeaponClass::none
            ? EquipmentState::unarmed : EquipmentState::safe_carry;
        if (weapon_class_ != WeaponClass::none)
        {
            equipment_rounds_remaining_ =
                weapon_profile(weapon_class_).magazine_capacity;
            equipment_fire_block_ = EquipmentFireBlock::unplanted;
        }
        const EquipmentGripFrame grip = equipment_grip_frame();
        if (weapon_class_ != WeaponClass::none && grip.resolved)
        {
            equipment_aim_angle_ = std::atan2(grip.axis.y, grip.axis.x);
            previous_equipment_aim_angle_ = equipment_aim_angle_;
            equipment_commanded_aim_angle_ = equipment_aim_angle_;
        }

        equipment_target_ = {};
        if (weapon_class_ == WeaponClass::none
            || (!equipment_lesson && !equipment_override_))
            return;
        const WeaponProfile profile = weapon_profile(weapon_class_);
        const float range_fraction = 0.25f
            + static_cast<float>((random_state_ >> 8u) % 3u) * 0.25f;
        const float target_distance = equipment_override_
            ? configured_target_distance_
            : lerp(profile.minimum_engagement_distance,
                profile.maximum_engagement_distance, range_fraction);
        equipment_target_.position.x = equipment_muzzle_position().x
            + target_distance * facing_direction();
        equipment_target_.position.y = ground_height_at(equipment_target_.position.x)
            + 0.85f + static_cast<float>((random_state_ >> 16u) % 4u) * 0.42f;
        equipment_target_.radius = 0.28f + course_difficulty_ * 0.08f;
        equipment_target_.active = true;
    }

    void Environment::update_equipment(
        std::span<const float, action_count> actions, float dt) noexcept
    {
        target_hit_this_step_ = false;
        equipment_cooldown_seconds_ = std::max(0.0f,
            equipment_cooldown_seconds_ - dt);
        equipment_recoil_recovery_seconds_ = std::max(0.0f,
            equipment_recoil_recovery_seconds_ - dt);
        equipment_reload_seconds_ = std::max(0.0f,
            equipment_reload_seconds_ - dt);
        equipment_state_seconds_ += dt;

        const float local_aim = clamp(actions[equipment_aim_action], -1.0f, 1.0f)
            * (pi * 0.42f);
        equipment_commanded_aim_angle_ = wrap_angle(facing_direction() < 0.0f
            ? pi - local_aim : local_aim);
        previous_equipment_aim_angle_ = equipment_aim_angle_;
        const EquipmentGripFrame grip = equipment_grip_frame();
        if (grip.resolved)
        {
            equipment_aim_angle_ = std::atan2(grip.axis.y, grip.axis.x);
        }
        equipment_aim_rate_ = dt > 1.0e-5f
            ? std::abs(wrap_angle(equipment_aim_angle_
                - previous_equipment_aim_angle_)) / dt
            : 0.0f;
        equipment_body_speed_ = 0.0f;
        if (valid_node(blueprint_.root_node) && dt > 1.0e-5f)
            equipment_body_speed_ = std::abs(
                particles_[blueprint_.root_node].position.x
                    - previous_pelvis_.x) / dt;
        const bool plant_frame = equipment_body_speed_ <= 0.18f
            && left_supported() && right_supported()
            && stance_slip_speed_ <= 0.12f
            && torso_uprightness() >= 0.72f;
        equipment_plant_seconds_ = plant_frame
            ? equipment_plant_seconds_ + dt : 0.0f;
        equipment_stopped_ = equipment_plant_seconds_ >= 0.16f;
        auto transition = [&](EquipmentState next)
        {
            if (equipment_state_ == next)
                return;
            equipment_state_ = next;
            equipment_state_seconds_ = 0.0f;
            if (next != EquipmentState::ready)
                equipment_aim_settle_seconds_ = 0.0f;
            ++equipment_transition_count_;
        };

        const float state_action = clamp(actions[equipment_state_action], -1.0f, 1.0f);
        if (weapon_class_ != WeaponClass::none)
        {
            const WeaponProfile profile = weapon_profile(weapon_class_);
            const float physical_error = std::abs(wrap_angle(
                equipment_recommended_aim_angle() - equipment_aim_angle_));
            const bool settled = equipment_stopped_
                && physical_error <= profile.aim_tolerance
                && equipment_aim_rate_ <= 0.28f;
            equipment_aim_settle_seconds_ = settled
                    && (equipment_state_ == EquipmentState::aiming
                        || equipment_state_ == EquipmentState::ready)
                ? equipment_aim_settle_seconds_ + dt : 0.0f;
            const bool engage_requested = state_action > 0.55f;
            const bool aim_requested = state_action > 0.70f;

            if (equipment_state_ == EquipmentState::reloading)
            {
                if (equipment_reload_seconds_ <= 0.0f)
                {
                    equipment_rounds_remaining_ = profile.magazine_capacity;
                    transition(equipment_stopped_
                        ? EquipmentState::gun_stance
                        : EquipmentState::low_ready);
                }
            }
            else if (equipment_state_ == EquipmentState::disarmed
                || equipment_state_ == EquipmentState::dropped)
            {
                const float pickup_distance = length(
                    equipment_mount_position() - dropped_equipment_position_);
                if (state_action > 0.72f && pickup_distance <= 1.15f)
                    transition(EquipmentState::safe_carry);
            }
            else if (state_action < -0.72f)
            {
                transition(EquipmentState::dropped);
                dropped_equipment_position_ = equipment_mount_position();
                dropped_equipment_velocity_ = { -0.25f, 0.65f };
            }
            else if (state_action > 0.28f && state_action <= 0.55f)
                transition(EquipmentState::low_ready);
            else if (state_action < -0.18f)
                transition(EquipmentState::safe_carry);
            else if (engage_requested && !equipment_stopped_)
                transition(EquipmentState::low_ready);
            else if (engage_requested
                && (equipment_state_ == EquipmentState::safe_carry
                    || equipment_state_ == EquipmentState::low_ready))
                transition(EquipmentState::gun_stance);
            else if (equipment_state_ == EquipmentState::gun_stance
                && aim_requested && equipment_state_seconds_ >= 0.16f)
                transition(EquipmentState::aiming);
            else if (equipment_state_ == EquipmentState::aiming
                && !aim_requested)
                transition(EquipmentState::gun_stance);
            else if (equipment_state_ == EquipmentState::aiming
                && settled && equipment_aim_settle_seconds_ >= 0.30f
                && equipment_recoil_recovery_seconds_ <= 0.0f)
                transition(EquipmentState::ready);
            else if (equipment_state_ == EquipmentState::ready
                && (!aim_requested || !settled))
                transition(aim_requested
                    ? EquipmentState::aiming : EquipmentState::gun_stance);

            if (equipment_rounds_remaining_ == 0u && aim_requested
                && equipment_target_.active && equipment_stopped_
                && equipment_state_ != EquipmentState::dropped
                && equipment_state_ != EquipmentState::disarmed
                && equipment_state_ != EquipmentState::reloading)
            {
                equipment_reload_seconds_ = profile.reload_seconds;
                transition(EquipmentState::reloading);
            }
        }

        if ((equipment_state_ == EquipmentState::dropped
                || equipment_state_ == EquipmentState::disarmed)
            && weapon_class_ != WeaponClass::none)
        {
            dropped_equipment_velocity_.y -= 12.0f * dt;
            dropped_equipment_position_ += dropped_equipment_velocity_ * dt;
            const float ground = ground_height_at(dropped_equipment_position_.x);
            if (dropped_equipment_position_.y < ground + 0.08f)
            {
                dropped_equipment_position_.y = ground + 0.08f;
                dropped_equipment_velocity_.y = 0.0f;
                dropped_equipment_velocity_.x *= std::exp(-4.0f * dt);
            }
        }

        const WeaponProfile profile = weapon_profile(weapon_class_);
        equipment_fire_block_ = equipment_fire_block();
        if (actions[equipment_trigger_action] > 0.45f
            && equipment_fire_block_ == EquipmentFireBlock::none)
        {
            const Vec2 muzzle = grip.muzzle;
            const Vec2 direction = grip.axis;
            equipment_projectiles_.push_back({
                weapon_class_, muzzle,
                direction * profile.projectile_speed, profile.projectile_radius,
                ++equipment_projectile_sequence_, true, false,
                profile.projectile_lifetime_seconds });
            equipment_cooldown_seconds_ = profile.cooldown_seconds;
            equipment_recoil_recovery_seconds_ = std::max(
                0.12f, profile.cooldown_seconds * 0.65f);
            --equipment_rounds_remaining_;
            ++shots_fired_;
            if (valid_node(grip.rear_node))
                particles_[grip.rear_node].previous += direction
                    * (profile.recoil * dt * 0.60f);
            if (valid_node(grip.front_node))
                particles_[grip.front_node].previous += direction
                    * (profile.recoil * dt * 0.40f);
            transition(EquipmentState::aiming);
        }

        for (EquipmentProjectile& projectile : equipment_projectiles_)
        {
            if (!projectile.active)
                continue;
            const Vec2 beginning = projectile.position;
            const WeaponProfile projectile_profile = weapon_profile(projectile.weapon);
            projectile.velocity.y -= projectile_profile.gravity * dt;
            const Vec2 ending = projectile.position + projectile.velocity * dt;
            const float target_fraction = equipment_target_.active
                ? swept_circle_hit_fraction(beginning, ending,
                    equipment_target_.position,
                    projectile.radius + equipment_target_.radius)
                : 2.0f;
            const float obstruction_fraction = equipment_obstruction_fraction(
                beginning, ending, projectile.radius);
            if (obstruction_fraction <= 1.0f
                && obstruction_fraction <= target_fraction)
            {
                projectile.position = beginning
                    + (ending - beginning) * obstruction_fraction;
                if (!projectile.miss_recorded)
                {
                    record_equipment_miss(projectile.position);
                    projectile.miss_recorded = true;
                }
                projectile.active = false;
                continue;
            }

            projectile.position = ending;
            if (equipment_target_.active && target_fraction <= 1.0f)
            {
                projectile.active = false;
                ++target_hits_;
                target_hit_this_step_ = true;
                equipment_aim_correction_ *= 0.35f;
                last_shot_miss_error_ = 0.0f;
                ++equipment_target_.sequence;
                if (target_hits_ >= equipment_hit_goal())
                {
                    equipment_target_.active = false;
                    continue;
                }
                // Multiple hits represent multiple rounds striking one fixed
                // range target. The target is never teleported between shots.
                continue;
            }
            if (equipment_target_.active && !projectile.miss_recorded)
            {
                const float facing = facing_direction();
                const float before = (beginning.x - equipment_target_.position.x)
                    * facing;
                const float after = (projectile.position.x
                    - equipment_target_.position.x) * facing;
                if (before < 0.0f && after >= 0.0f)
                {
                    const float span = projectile.position.x - beginning.x;
                    const float fraction = std::abs(span) > 1.0e-6f
                        ? clamp((equipment_target_.position.x - beginning.x)
                            / span, 0.0f, 1.0f) : 1.0f;
                    const float crossing_y = lerp(
                        beginning.y, projectile.position.y, fraction);
                    record_equipment_miss(
                        { equipment_target_.position.x, crossing_y });
                    projectile.miss_recorded = true;
                }
            }
            projectile.lifetime_remaining_seconds -= dt;
            if (projectile.lifetime_remaining_seconds <= 0.0f)
            {
                if (!projectile.miss_recorded)
                {
                    record_equipment_miss(projectile.position);
                    projectile.miss_recorded = true;
                }
                projectile.active = false;
            }
        }
        std::erase_if(equipment_projectiles_,
            [](const EquipmentProjectile& item) { return !item.active; });
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
        return terrain_.height_at(terrain_sample_x(x, course_progress()));
    }

    float Environment::terrain_firmness_at(float x) const noexcept
    {
        return stage_uses_deformable_terrain(course_stage_)
            ? terrain_.firmness_at(terrain_sample_x(x, course_progress())) : 1.0f;
    }

    float Environment::terrain_looseness_at(float x) const noexcept
    {
        return stage_uses_deformable_terrain(course_stage_)
            ? terrain_.looseness_at(terrain_sample_x(x, course_progress())) : 0.0f;
    }

    void Environment::apply_water_forces(float dt) noexcept
    {
        water_submersion_ = 0.0f;
        if (!stage_uses_deformable_terrain(course_stage_))
            return;

        const float safe_dt = std::max(dt, 1.0e-5f);
        float body_submersion_sum = 0.0f;
        float body_weight_sum = 0.0f;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            const float source_x = terrain_sample_x(
                particle.position.x, course_progress());
            const float depth = terrain_.water_depth_at(source_x);
            if (depth <= 0.001f)
                continue;
            const float surface = terrain_.water_surface_at(source_x);
            const float diameter = std::max(0.02f, particle.radius * 2.0f);
            const float submerged = clamp(
                (surface - (particle.position.y - particle.radius)) / diameter,
                0.0f, 1.0f);
            const bool primary_body = index == blueprint_.root_node
                || index == blueprint_.torso_node || index == blueprint_.head_node;
            const float morphology_weight = primary_body ? 2.0f
                : blueprint_.is_support_seed(index) ? 0.15f : 1.0f;
            body_submersion_sum += submerged * morphology_weight;
            body_weight_sum += morphology_weight;
            if (submerged <= 0.0f)
                continue;

            const Vec2 velocity = (particle.position - particle.previous) / safe_dt;
            const float drag_rate = 2.4f + submerged * 5.2f;
            const float retained = std::exp(-drag_rate * safe_dt);
            particle.previous = particle.position - velocity * (retained * safe_dt);
            const float displaced_mass = 1.0f / std::max(0.15f, particle.inverse_mass);
            particle.position.y += 15.0f * submerged
                * std::min(1.35f, displaced_mass) * safe_dt * safe_dt;
        }
        // Locomotion intent describes body immersion, not the wettest toe.
        // Per-particle buoyancy and drag above remain fully physical.
        water_submersion_ = body_weight_sum > 0.0f
            ? body_submersion_sum / body_weight_sum : 0.0f;
    }

    void Environment::clear_dynamic_materials() noexcept
    {
        material_particles_.clear();
        granular_block_ = {};
        granular_block_present_ = false;
        granular_block_safe_ = false;
        granular_block_settled_seconds_ = 0.0f;
        granular_hazard_hold_seconds_ = 0.0f;
        granular_hazard_safe_seconds_ = 0.0f;
    }

    void Environment::append_dynamic_material_features()
    {
        if (granular_block_present_)
            course_features_.push_back(granular_block_);
    }

    void Environment::update_materials(float dt) noexcept
    {
        const bool mixed_hazards = course_stage_ == CourseStage::moving_hazards
            || course_stage_ == CourseStage::combat_course;
        if (!mixed_hazards)
        {
            clear_dynamic_materials();
            return;
        }

        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float direction = shuttle_enabled()
            ? shuttle_state_.locomotion_direction : facing_direction();
        const float interval = std::lerp(4.80f, 2.80f, course_difficulty_);
        const float required_travel = 8.0f + course_difficulty_ * 4.0f;
        const bool gait_ready = advanced_material_pressure_ready(
            distance_travelled_, required_travel, gait_cycles(), limb_crossings_,
            blueprint_.paired_leg_chains());
        const bool shuttle_ready = !shuttle_enabled()
            || (shuttle_state_.phase == ShuttlePhase::traverse
                && shuttle_dynamic_course_ready(shuttle_state_.completed_turns,
                    gait_cycles(), longest_stable_stance_seconds_));
        if (!gait_ready || !shuttle_ready)
        {
            clear_dynamic_materials();
            next_material_event_seconds_ = elapsed_seconds_ + interval;
            return;
        }

        granular_hazard_hold_seconds_ = std::max(0.0f,
            granular_hazard_hold_seconds_ - dt);
        if (granular_block_present_ && !granular_block_safe_)
        {
            granular_block_.velocity.y -= 12.0f * dt;
            granular_block_.center += granular_block_.velocity * dt;
            const float ground = ground_height_at(granular_block_.center.x);
            const float bottom = granular_block_.center.y
                - granular_block_.half_extent.y;
            if (bottom <= ground)
            {
                granular_block_.center.y = ground + granular_block_.half_extent.y;
                granular_block_.velocity.y = std::abs(granular_block_.velocity.y) * 0.10f;
                granular_block_.velocity.x *= std::exp(-2.8f * dt);
                if (length(granular_block_.velocity) < 0.12f)
                    granular_block_settled_seconds_ += dt;
                else
                    granular_block_settled_seconds_ = 0.0f;
                if (granular_block_settled_seconds_ >= 0.85f)
                {
                    granular_block_safe_ = true;
                    granular_block_.velocity = {};
                }
            }
            else
            {
                granular_block_settled_seconds_ = 0.0f;
            }
            if (std::abs(granular_block_.center.x - root_x) > 14.0f
                || granular_block_.center.y < DeformableTerrain::world_bottom - 1.0f
                || granular_block_.center.y > 20.0f)
            {
                granular_block_present_ = false;
                granular_block_safe_ = false;
                granular_block_settled_seconds_ = 0.0f;
            }
        }

        const bool event_active_before_spawn = granular_hazard_active();
        if (!event_active_before_spawn
            && elapsed_seconds_ >= next_material_event_seconds_)
        {
            ++material_event_sequence_;
            const std::uint32_t selector = (material_event_sequence_ - 1u) % 4u;
            const float ahead = 3.1f + random_unit() * 2.4f;
            const float spawn_x = root_x + direction * ahead;
            const float spawn_ground = ground_height_at(spawn_x);
            granular_hazard_safe_seconds_ = 0.0f;
            if (selector == 3u)
            {
                const float half_width = 0.25f + course_difficulty_ * 0.15f;
                const float half_height = 0.20f + course_difficulty_ * 0.12f;
                granular_block_ = { CourseFeatureKind::hurdle,
                    { spawn_x, spawn_ground + 3.8f + random_unit() * 1.4f },
                    { half_width, half_height }, 0.0f,
                    { -direction * (0.35f + course_difficulty_ * 0.55f),
                        -0.20f - random_unit() * 0.45f },
                    50'000 + static_cast<int>(material_event_sequence_) };
                granular_block_present_ = true;
                granular_block_safe_ = false;
                granular_block_settled_seconds_ = 0.0f;
            }
            else
            {
                const bool cascade = selector == 2u;
                const std::size_t burst_count = (cascade ? 14u : 8u)
                    + static_cast<std::size_t>(std::lround(course_difficulty_
                        * (cascade ? 8.0f : 6.0f)));
                if (material_particles_.size() + burst_count > 96u)
                    material_particles_.clear();
                for (std::size_t index = 0; index < burst_count; ++index)
                {
                    const MaterialKind kind = selector == 0u
                        || (cascade && (index & 1u) == 0u)
                        ? MaterialKind::sand : MaterialKind::dirt;
                    const float spread = (static_cast<float>(index)
                        - static_cast<float>(burst_count - 1u) * 0.5f)
                        * (cascade ? 0.15f : 0.12f);
                    material_particles_.push_back({ kind,
                        { spawn_x + spread, spawn_ground + (cascade ? 4.4f : 3.6f)
                            + random_unit() * (cascade ? 2.2f : 1.8f) },
                        { -direction * (0.10f + random_unit()
                            * (cascade ? 0.50f : 0.35f)),
                            -0.10f - random_unit() * 0.35f },
                        0.052f + random_unit() * (cascade ? 0.040f : 0.030f),
                        kind == MaterialKind::sand ? 0.42f : 0.60f, true });
                }
            }
            next_material_event_seconds_ = elapsed_seconds_ + interval;
        }

        const float treadmill = course_speed();
        for (MaterialParticle& item : material_particles_)
        {
            if (!item.active)
                continue;
            item.velocity.y -= 13.0f * dt;
            item.position += item.velocity * dt;
            item.position.x -= treadmill * dt;

            for (Particle& particle : particles_)
            {
                const Vec2 delta = particle.position - item.position;
                const float distance = length(delta);
                const float minimum = particle.radius + item.radius;
                if (distance >= minimum)
                    continue;
                const Vec2 normal = distance > 1.0e-5f
                    ? delta / distance : Vec2{ -direction, 0.0f };
                particle.position += normal * (minimum - distance);
                const float impulse = item.kind == MaterialKind::rock
                    ? 0.30f : item.kind == MaterialKind::dirt ? 0.14f : 0.08f;
                particle.previous -= item.velocity * dt * impulse;
                item.velocity -= normal * dot(item.velocity, normal) * 0.42f;
                collided_this_step_ = true;
            }

            for (const CourseFeature& feature : course_features_)
            {
                if (feature.half_extent.x <= 0.0f
                    || feature.half_extent.y <= 0.0f)
                    continue;
                const float left = feature.center.x - feature.half_extent.x;
                const float right = feature.center.x + feature.half_extent.x;
                const float top = feature.center.y + feature.half_extent.y;
                if (item.position.x + item.radius < left
                    || item.position.x - item.radius > right
                    || item.position.y - item.radius > top
                    || item.position.y < feature.center.y)
                    continue;
                item.position.y = top + item.radius;
                item.velocity.y = std::abs(item.velocity.y) * 0.12f;
                item.velocity.x *= 0.78f;
            }

            const float ground = ground_height_at(item.position.x);
            if (item.position.y - item.radius > ground)
                continue;
            item.position.y = ground + item.radius;
            if (item.kind == MaterialKind::sand || item.kind == MaterialKind::dirt)
            {
                const sandhybrid::Material material = item.kind == MaterialKind::sand
                    ? sandhybrid::Material::sand : sandhybrid::Material::dirt;
                const float firmness = item.kind == MaterialKind::sand ? 0.18f : 0.48f;
                terrain_.deposit(terrain_sample_x(item.position.x, course_progress()),
                    std::clamp(item.radius * item.radius * 2.8f, 0.004f, 0.025f),
                    firmness, material);
                item.active = false;
            }
            else
            {
                item.velocity.y = std::abs(item.velocity.y) * 0.16f;
                item.velocity.x *= 0.72f;
                if (std::abs(item.velocity.x) < 0.08f
                    && std::abs(item.velocity.y) < 0.08f)
                {
                    terrain_.deposit(terrain_sample_x(item.position.x, course_progress()),
                        item.radius * 0.12f, item.density,
                        sandhybrid::Material::dirt);
                    item.active = false;
                }
            }
        }
        std::erase_if(material_particles_, [root_x](const MaterialParticle& item)
        {
            return !item.active || std::abs(item.position.x - root_x) > 14.0f
                || item.position.y < DeformableTerrain::world_bottom - 1.0f
                || item.position.y > 20.0f;
        });

        if (granular_hazard_active())
            granular_hazard_safe_seconds_ = 0.0f;
        else
            granular_hazard_safe_seconds_ += dt;
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
            terrain_.apply_pressure(terrain_sample_x(particle.position.x, course_progress()),
                load, slip, dt);
        }
    }

    void Environment::update_material_metrics(float dt) noexcept
    {
        if (!valid_node(blueprint_.root_node))
            return;
        const Particle& root = particles_[blueprint_.root_node];
        terrain_firmness_ = terrain_firmness_at(root.position.x);
        terrain_looseness_ = terrain_looseness_at(root.position.x);
        previous_water_depth_ = water_depth_;
        water_depth_ = water_depth_at(root.position.x);
        const float direction = locomotion_direction() == 0.0f
            ? facing_direction() : locomotion_direction();
        const float ahead_depth = water_depth_at(root.position.x
            + direction * locomotion::shore_exit_lookahead_m);
        WaterTraversalPhase next_phase = WaterTraversalPhase::dry;
        if (water_depth_ > locomotion::wading_depth_threshold)
            next_phase = ahead_depth <= locomotion::wading_depth_threshold
                ? WaterTraversalPhase::shore_exit
                : water_submersion_
                        >= locomotion::swimming_body_submersion_threshold
                    ? WaterTraversalPhase::swimming
                    : WaterTraversalPhase::wading;
        else if (previous_water_depth_ > locomotion::wading_depth_threshold)
            next_phase = WaterTraversalPhase::shore_exit;
        else if (ahead_depth > locomotion::water_approach_depth_threshold)
            next_phase = WaterTraversalPhase::approach;
        water_phase_seconds_ = next_phase == water_traversal_phase_
            ? water_phase_seconds_ + dt : 0.0f;
        water_traversal_phase_ = next_phase;
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

    void Environment::mirror_rig_about_root() noexcept
    {
        if (!valid_node(blueprint_.root_node))
            return;
        const float root_x = particles_[blueprint_.root_node].position.x;
        for (Particle& particle : particles_)
        {
            // Turning is a deliberate zero-speed lesson boundary. Clearing
            // Verlet velocity prevents the pre-turn stride from becoming an
            // equal-and-opposite launch on the first reverse-facing step. The
            // complete articulated plant changes facing here so physics,
            // contacts, observations, motors, equipment, and presentation all
            // share one world-space pose instead of mirroring only the picture.
            particle.position.x = root_x * 2.0f - particle.position.x;
            particle.previous = particle.position;
        }
        equipment_aim_angle_ = wrap_angle(pi - equipment_aim_angle_);
        equipment_commanded_aim_angle_ = wrap_angle(
            pi - equipment_commanded_aim_angle_);
        previous_equipment_aim_angle_ = equipment_aim_angle_;
        equipment_aim_rate_ = 0.0f;
        equipment_aim_settle_seconds_ = 0.0f;
        previous_pelvis_ = particles_[blueprint_.root_node].position;
        previous_root_for_path_ = previous_pelvis_;
        previous_torso_angle_ = torso_roll_angle();
        torso_turn_speed_ = 0.0f;
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
        {
            previous_angles_[index] = joint_angle(blueprint_.motors[index]);
            angular_velocities_[index] = 0.0f;
            previous_applied_actions_[index] = 0.0f;
        }
        if (support_contact_latch_.size() != particles_.size())
            support_contact_latch_.assign(particles_.size(), 0u);
        else
            std::fill(support_contact_latch_.begin(), support_contact_latch_.end(), std::uint8_t{0});
        support_contact_anchor_x_.resize(particles_.size());
        support_traction_capacity_.assign(particles_.size(), 0.0f);
        support_traction_used_.assign(particles_.size(), 0.0f);
        support_normal_constraint_lambdas_.assign(particles_.size(), 0.0f);
        support_contact_tangent_impulses_.assign(particles_.size(), 0.0f);
        support_cluster_traction_capacity_.fill(0.0f);
        support_cluster_plate_traction_capacity_.fill(0.0f);
        support_cluster_toe_traction_capacity_.fill(0.0f);
        support_cluster_traction_used_.fill(0.0f);
        support_cluster_traction_impulse_.fill(0.0f);
        support_cluster_plate_membership_mask_.fill(0u);
        support_cluster_normal_impulse_.fill(0.0f);
        support_cluster_normal_impulse_moment_x_.fill(0.0f);
        support_cluster_normal_cop_x_.fill(0.0f);
        support_cluster_tangential_impulse_.fill(0.0f);
        human_support_feedback_ = {};
        human_gait_coordinator_ = {};
        support_friction_work_joules_ = 0.0f;
        support_friction_positive_work_joules_ = 0.0f;
        support_cluster_contact_age_seconds_.fill(0.0f);
        for (std::size_t index = 0; index < particles_.size(); ++index)
            support_contact_anchor_x_[index] = particles_[index].position.x;
        for (std::size_t side = 0;
            side < previous_articulated_toe_angles_.size(); ++side)
        {
            MotorConstraint toe_motor{};
            if (articulated_toe_motor(side == 0u, toe_motor))
                previous_articulated_toe_angles_[side] =
                    joint_angle(toe_motor);
        }
        left_foot_phase_ = detect_foot_contact_phase(true);
        right_foot_phase_ = detect_foot_contact_phase(false);
        previous_left_grounded_ = left_supported();
        previous_right_grounded_ = right_supported();
        for (std::size_t index = 0; index < previous_support_grounded_.size(); ++index)
        {
            previous_support_grounded_[index] = blueprint_.is_support_seed(index)
                && particles_[index].grounded ? 1u : 0u;
        }
    }

    void Environment::update_shuttle(float root_x, float root_speed, float dt) noexcept
    {
        if (!shuttle_enabled())
        {
            shuttle_state_ = {};
            return;
        }
        const ShuttlePhase prior_phase = shuttle_state_.phase;
        const std::uint32_t prior_turns = shuttle_state_.completed_turns;
        const bool settled_support = left_supported() && right_supported()
            && !non_foot_grounded() && uprightness() >= 0.88f
            && std::isfinite(root_speed)
            && std::abs(root_speed) <= shuttle_brake_speed;
        shuttle_state_ = advance_shuttle_state(
            shuttle_state_, root_x, dt, root_speed, settled_support);
        // Assisted shuttle lessons may reset the articulated pose at their
        // authored zero-speed turn boundary. Raw policy evaluation cannot
        // mirror the body or erase its velocity outside the action/physics
        // path; the controller must negotiate the direction change itself.
        if (shuttle_state_.completed_turns != prior_turns
            && !raw_policy_audit())
        {
            mirror_rig_about_root();
            // SETTLING must earn support after the mirror; pre-turn stance time
            // cannot authorize an immediate automatic Walk restart.
            stable_stance_seconds_ = 0.0f;
        }
        if (shuttle_state_.phase != prior_phase)
        {
            progress_window_seconds_ = 0.0f;
            progress_window_start_x_ = root_x;
            progress_window_start_steps_ = gait_cycles();
            action_energy_window_ = 0.0f;
            root_path_window_ = 0.0f;
            if (valid_node(blueprint_.root_node))
                previous_root_for_path_ = particles_[blueprint_.root_node].position;
            micro_motion_seconds_ = 0.0f;
            zero_progress_seconds_ = 0.0f;
            foot_pivot_rolling_seconds_ = 0.0f;
            clear_dynamic_materials();
            next_material_event_seconds_ = elapsed_seconds_
                + std::lerp(4.80f, 2.80f, course_difficulty_);
        }
        if (shuttle_state_.completed_turns != prior_turns
            && equipment_target_.active)
        {
            const float target_distance = equipment_override_
                ? configured_target_distance_ : 8.0f;
            equipment_target_.position.x = root_x
                + target_distance * shuttle_state_.facing_direction;
            equipment_target_.position.y =
                ground_height_at(equipment_target_.position.x) + 1.20f;
        }
    }

    void Environment::rebuild_course_features() noexcept
    {
        course_features_.clear();
        // Preview locomotion uses a finite physical shuttle. Obstacles are
        // regenerated only ahead of a proven gait and are absent while backing
        // or turning, so completed challenges never linger behind the rig.
        if (course_stage_ != CourseStage::duck_press
            && course_stage_ != CourseStage::uneven
            && course_stage_ != CourseStage::shuttle
            && course_stage_ != CourseStage::crouch_walk
            && course_stage_ != CourseStage::hurdles
            && course_stage_ != CourseStage::moving_hazards
            && course_stage_ != CourseStage::climb_descent
            && course_stage_ != CourseStage::equipment_targets
            && course_stage_ != CourseStage::combat_course)
            return;

        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float progress = course_progress();
        if (shuttle_enabled())
        {
            const bool dynamic_ready = shuttle_dynamic_course_ready(
                shuttle_state_.completed_turns, gait_cycles(),
                longest_stable_stance_seconds_);
            if (shuttle_state_.phase != ShuttlePhase::traverse
                || (course_stage_ == CourseStage::shuttle && !dynamic_ready))
                return;

            if (course_stage_ == CourseStage::moving_hazards
                || course_stage_ == CourseStage::combat_course)
                return;
            const float direction = shuttle_state_.locomotion_direction;
            const std::array<float, 2> stations = direction > 0.0f
                ? std::array<float, 2>{ 2.0f, 6.0f }
                : std::array<float, 2>{ 6.0f, 2.0f };
            for (std::size_t slot = 0; slot < stations.size(); ++slot)
            {
                const float x = stations[slot];
                const float ahead = (x - root_x) * direction;
                if (ahead < 3.2f)
                    continue;
                const float ground = ground_height_at(x);
                CourseFeatureKind kind = CourseFeatureKind::rock;
                if (course_stage_ == CourseStage::shuttle)
                    kind = slot == 0u ? CourseFeatureKind::rock
                        : CourseFeatureKind::hurdle;
                else if (course_stage_ == CourseStage::crouch_walk)
                    kind = slot == 0u ? CourseFeatureKind::overhead_bar
                        : CourseFeatureKind::rock;
                else if (course_stage_ == CourseStage::hurdles)
                    kind = slot == 0u ? CourseFeatureKind::hurdle
                        : CourseFeatureKind::overhead_bar;
                else if (course_stage_ == CourseStage::moving_hazards
                    || course_stage_ == CourseStage::combat_course)
                    kind = slot == 0u ? CourseFeatureKind::moving_hazard
                        : CourseFeatureKind::projectile;

                const int sequence = 10'000
                    + static_cast<int>(shuttle_state_.completed_turns * 8u + slot);
                if (kind == CourseFeatureKind::rock)
                {
                    const float radius = 0.18f + course_difficulty_ * 0.09f;
                    course_features_.push_back({
                        kind, { x, ground + radius }, {}, radius, {}, sequence });
                }
                else if (kind == CourseFeatureKind::hurdle)
                {
                    const float height = 0.26f + course_difficulty_ * 0.28f;
                    course_features_.push_back({
                        kind, { x, ground + height * 0.5f },
                        { 0.14f, height * 0.5f }, 0.0f, {}, sequence });
                }
                else if (kind == CourseFeatureKind::overhead_bar)
                {
                    const float clearance = 3.42f - course_difficulty_ * 0.55f;
                    course_features_.push_back({
                        kind, { x, ground + clearance + 0.12f },
                        { 0.95f, 0.12f }, 0.0f, {}, sequence });
                }
                else
                {
                    const float oscillation = std::sin(
                        elapsed_seconds_ * 1.8f + static_cast<float>(sequence));
                    const float radius = kind == CourseFeatureKind::projectile
                        ? 0.16f : 0.22f;
                    course_features_.push_back({
                        kind,
                        { x + oscillation * 0.42f,
                            ground + 1.05f + oscillation * 0.30f },
                        {}, radius,
                        { oscillation * 0.28f, 0.0f }, sequence });
                }
            }
            return;
        }
        if (course_stage_ == CourseStage::uneven)
            return;
        if (course_stage_ == CourseStage::duck_press)
        {
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
                const float rest_head_top = ground_height_at(press_anchor_x)
                    + authored_standing_head_clearance();
                const float authored_reach = std::max(
                    std::abs(minimum_x - press_anchor_x),
                    std::abs(maximum_x - press_anchor_x));
                const float half_width = clamp(authored_reach + 0.34f, 0.82f, 2.80f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top,
                    blueprint_.horizontal_multi_support_plan());
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
        if (course_stage_ == CourseStage::equipment_targets)
            return;
        if (course_stage_ == CourseStage::climb_descent)
        {
            float reachable_hand_height = 1.45f;
            for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
            {
                if (index == blueprint_.head_node || blueprint_.is_support_seed(index)
                    || !manipulator_endpoint(blueprint_, index))
                    continue;
                reachable_hand_height = std::max(reachable_hand_height,
                    blueprint_.nodes[index].y + particles_[index].radius * 0.5f);
            }
            const float top = clamp(reachable_hand_height - 0.10f,
                1.15f, 2.75f);
            const float left = 10.0f + course_difficulty_ * 2.0f;
            const float right = left + 6.0f;
            ledge_top_height_ = top;
            ledge_left_edge_ = left;
            course_features_.push_back({
                CourseFeatureKind::ledge,
                { 0.5f * (left + right), top * 0.5f },
                { 0.5f * (right - left), top * 0.5f }, 0.0f,
                {}, 9000
            });
            return;
        }
        if (course_stage_ == CourseStage::crouch_walk)
        {
            const float rest_head_top = authored_standing_head_clearance();
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
            case CourseFeatureKind::ledge:
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
    }

    void Environment::reset(std::uint64_t seed)
    {
        if (seed != 0)
            random_state_ = seed;
        if (random_state_ == 0)
            random_state_ = 1;

        particles_.clear();
        particles_.reserve(blueprint_.nodes.size());
        const bool avian = blueprint_.avian_gait();
        const auto avian_structural_node = [&](std::size_t node) noexcept
        {
            if (node == blueprint_.root_node || node == blueprint_.torso_node
                || blueprint_.is_support_seed(node))
                return true;
            for (std::size_t index = 0; index < blueprint_.active_motor_count;
                ++index)
            {
                const MotorConstraint& motor = blueprint_.motors[index];
                if (motor.enabled && (node == motor.pivot || node == motor.c))
                    return true;
            }
            return false;
        };
        std::size_t avian_art_nodes = 0u;
        if (avian)
        {
            for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
            {
                if (!avian_structural_node(index))
                    ++avian_art_nodes;
            }
        }
        // Physical mass describes the locomotor, not the number of points used
        // to outline its head and tail. Only passive avian artwork nodes share
        // a bounded aggregate mass; pelvis, trunk, driven joints, and supports
        // retain their authored physical mass.
        const float avian_art_mass_normalizer = avian_art_nodes > 3u
            ? static_cast<float>(avian_art_nodes) / 3.0f : 1.0f;
        const float phase = (random_unit() - 0.5f) * 0.04f;
        for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
        {
            Vec2 position = blueprint_.nodes[index];
            const bool contact_semantic = blueprint_.is_support_seed(index);
            position.x += (random_unit() - 0.5f) * 0.008f + phase;
            const float vertical_jitter = (random_unit() - 0.5f) * 0.006f;
            position.y += contact_semantic ? 0.0f : vertical_jitter;
            const float radius = index < blueprint_.radii.size() ? blueprint_.radii[index] : 0.15f;
            const std::size_t degree = static_cast<std::size_t>(std::ranges::count_if(
                blueprint_.bones, [index](const DistanceConstraint& bone)
                {
                    return bone.a == index || bone.b == index;
                }));
            float inverse_mass = 1.0f;
            if (contact_semantic)
            {
                const bool articulated_human_contact =
                    blueprint_.human_casual_gait_plan()
                    && index != blueprint_.left_contact_node
                    && index != blueprint_.right_contact_node;
                inverse_mass = articulated_human_contact ? 2.40f : 0.58f;
            }
            else if (index == blueprint_.head_node)
                inverse_mass = 0.72f;
            else if (manipulator_branch_node(blueprint_, index))
                inverse_mass = 4.0f;
            else if (passive_endpoint(blueprint_, index))
                inverse_mass = 0.68f;
            else if (degree == 1u && index != blueprint_.root_node
                && index != blueprint_.torso_node)
                inverse_mass = 0.92f;
            if (avian && !avian_structural_node(index))
                inverse_mass *= avian_art_mass_normalizer;
            particles_.push_back({ position, position, inverse_mass, radius, false });
        }

        if (!course_layout_initialized_)
        {
            const std::uint64_t stage_bits =
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(course_stage_)) << 48u;
            const std::uint64_t difficulty_bits = static_cast<std::uint64_t>(
                std::lround(course_difficulty_ * 1000.0f)) << 24u;
            course_layout_seed_ = random_state_ ^ 0xa5a5a5a5a5a5a5a5ULL
                ^ stage_bits ^ difficulty_bits;
            if (course_layout_seed_ == 0u)
                course_layout_seed_ = 1u;
            terrain_.reset(course_layout_seed_, course_difficulty_);
            course_layout_initialized_ = true;
        }
        float vertical_shift = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!blueprint_.is_support_seed(index))
                continue;
            const Particle& support = particles_[index];
            const float ground = stage_uses_deformable_terrain(course_stage_)
                ? terrain_.height_at(support.position.x) : 0.0f;
            const float target = ground + ground_contact_offset(true, support.radius);
            vertical_shift = std::max(vertical_shift, target - support.position.y);
        }
        if (std::isfinite(vertical_shift))
        {
            for (Particle& particle : particles_)
            {
                particle.position.y += vertical_shift;
                particle.previous.y += vertical_shift;
            }
        }

        support_contact_latch_.assign(particles_.size(), 0u);
        support_contact_anchor_x_.resize(particles_.size());
        support_traction_capacity_.assign(particles_.size(), 0.0f);
        support_traction_used_.assign(particles_.size(), 0.0f);
        support_normal_constraint_lambdas_.assign(particles_.size(), 0.0f);
        support_contact_tangent_impulses_.assign(particles_.size(), 0.0f);
        support_cluster_traction_capacity_.fill(0.0f);
        support_cluster_plate_traction_capacity_.fill(0.0f);
        support_cluster_toe_traction_capacity_.fill(0.0f);
        support_cluster_traction_used_.fill(0.0f);
        support_cluster_traction_impulse_.fill(0.0f);
        support_cluster_plate_membership_mask_.fill(0u);
        support_cluster_normal_impulse_.fill(0.0f);
        support_cluster_normal_impulse_moment_x_.fill(0.0f);
        support_cluster_normal_cop_x_.fill(0.0f);
        support_cluster_tangential_impulse_.fill(0.0f);
        human_support_feedback_ = {};
        human_gait_coordinator_ = {};
        support_friction_work_joules_ = 0.0f;
        support_friction_positive_work_joules_ = 0.0f;
        support_cluster_contact_age_seconds_.fill(0.0f);
        previous_support_grounded_.assign(particles_.size(), 0u);
        support_swing_seconds_.assign(particles_.size(), 0.0f);
        support_swing_clearance_.assign(particles_.size(), 0.0f);
        for (std::size_t index = 0; index < particles_.size(); ++index)
            support_contact_anchor_x_[index] = particles_[index].position.x;
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
        shuttle_distance_travelled_ = 0.0f;
        shuttle_state_ = {};
        forward_speed_ = course_speed();
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
        duck_posture_failure_seconds_ = 0.0f;
        current_valid_crouch_seconds_ = 0.0f;
        longest_valid_crouch_seconds_ = 0.0f;
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
        left_swing_started_behind_ = false;
        right_swing_started_behind_ = false;
        left_swing_crossed_ = false;
        right_swing_crossed_ = false;
        limb_crossings_ = 0u;
        lower_leg_scissored_this_step_ = false;
        lower_leg_scissor_seconds_ = 0.0f;
        maximum_lower_leg_scissor_seconds_ = 0.0f;
        backward_brace_ratio_ = 0.0f;
        backward_brace_seconds_ = 0.0f;
        maximum_backward_brace_seconds_ = 0.0f;
        heel_strike_count_ = 0u;
        toe_off_count_ = 0u;
        left_foot_phase_ = FootContactPhase::airborne;
        right_foot_phase_ = FootContactPhase::airborne;
        action_change_energy_ = 0.0f;
        alternating_step_this_step_ = false;
        single_leg_cycle_this_step_ = false;
        limb_crossing_this_step_ = false;
        maximum_speed_kmh_ = 0.0f;
        alternating_steps_ = 0;
        single_leg_cycles_ = 0;
        last_topology_support_phase_ = std::numeric_limits<std::size_t>::max();
        last_topology_transfer_seconds_ = -100.0f;
        last_topology_transfer_x_ = previous_pelvis_.x;
        last_single_leg_landing_x_ = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        progress_window_start_steps_ = 0;
        knee_first_faults_ = 0;
        wheel_sliding_seconds_ = 0.0f;
        body_rolling_seconds_ = 0.0f;
        foot_pivot_rolling_seconds_ = 0.0f;
        zero_progress_seconds_ = 0.0f;
        head_contact_seconds_ = 0.0f;
        last_support_transfer_seconds_ = -100.0f;
        torso_turn_speed_ = 0.0f;
        stance_slip_speed_ = 0.0f;
        stance_slip_distance_ = 0.0f;
        available_traction_ = 0.0f;
        used_traction_ = 0.0f;
        hazard_stall_seconds_ = 0.0f;
        obstacle_approach_weight_ = 0.0f;
        obstacle_lift_clearance_ = 0.0f;
        obstacle_clearance_target_ = 0.20f;
        non_foot_grounded_ = false;
        knee_first_this_step_ = false;
        last_contact_side_ = 0;
        action_support_unload_side_ = 0;
        last_action_support_unload_side_ = 0;
        support_cluster_release_hold_seconds_.fill(0.0f);
        support_toe_roll_seconds_.fill(0.0f);
        last_landing_air_seconds_ = 0.0f;
        last_landing_clearance_ = 0.0f;
        last_landing_displacement_ = 0.0f;
        previous_water_depth_ = 0.0f;
        water_traversal_phase_ = WaterTraversalPhase::dry;
        water_phase_seconds_ = 0.0f;
        last_landing_side_ = 0;
        last_landing_qualified_ = false;
        previous_left_grounded_ = false;
        previous_right_grounded_ = false;
        collided_this_step_ = false;
        recovery_active_ = false;
        recovery_started_seconds_ = 0.0f;
        recovery_best_upright_ = 1.0f;
        recovery_events_ = 0;
        recovery_successes_ = 0;
        clear_dynamic_materials();
        next_material_event_seconds_ = std::lerp(9.0f, 6.0f, course_difficulty_);
        material_event_sequence_ = 0u;
        terrain_firmness_ = 1.0f;
        terrain_looseness_ = 0.0f;
        water_depth_ = 0.0f;
        water_submersion_ = 0.0f;
        burial_depth_ = 0.0f;
        previous_burial_depth_ = 0.0f;
        buried_no_escape_seconds_ = 0.0f;
        free_space_direction_ = 0.0f;
        incoming_material_velocity_ = {};
        incoming_time_to_impact_ = 10.0f;
        incoming_material_density_ = 0.0f;
        obstruction_mask_ = 0u;
        ledge_grasp_nodes_.fill(std::numeric_limits<std::uint16_t>::max());
        ledge_grasp_anchors_.fill({});
        hand_ledge_contacts_ = 0u;
        climb_support_transfers_ = 0u;
        ledge_climbs_ = 0u;
        controlled_descents_ = 0u;
        ledge_climbed_ = false;
        ledge_descending_ = false;
        ledge_top_height_ = 0.0f;
        ledge_left_edge_ = 0.0f;
        previous_root_height_ = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.y : 0.0f;
        invalid_reason_ = InvalidMotion::none;
        reset_equipment();
        rebuild_course_features();
        append_dynamic_material_features();
    }

    Environment::AuthoritySnapshot Environment::authority_snapshot(float dt) const noexcept
    {
        if (!raw_authority_telemetry_enabled_ || particles_.empty())
            return {};
        const float safe_dt = std::max(dt, 1.0e-5f);
        double weighted_velocity_x{};
        double weighted_velocity_y{};
        double weighted_position_x{};
        double weighted_position_y{};
        double weighted_history_x{};
        double weighted_history_y{};
        double total_mass{};
        for (const Particle& particle : particles_)
        {
            const double mass = 1.0 / static_cast<double>(
                std::max(particle.inverse_mass, 1.0e-5f));
            weighted_velocity_x += static_cast<double>(
                (particle.position.x - particle.previous.x) / safe_dt) * mass;
            weighted_velocity_y += static_cast<double>(
                (particle.position.y - particle.previous.y) / safe_dt) * mass;
            weighted_position_x += static_cast<double>(particle.position.x) * mass;
            weighted_position_y += static_cast<double>(particle.position.y) * mass;
            weighted_history_x += static_cast<double>(particle.previous.x) * mass;
            weighted_history_y += static_cast<double>(particle.previous.y) * mass;
            total_mass += mass;
        }
        AuthoritySnapshot snapshot{};
        if (total_mass > 1.0e-9)
        {
            snapshot.center_velocity_x = static_cast<float>(
                weighted_velocity_x / total_mass);
            snapshot.center_velocity_y = static_cast<float>(
                weighted_velocity_y / total_mass);
            snapshot.center_position_x = static_cast<float>(
                weighted_position_x / total_mass);
            snapshot.center_position_y = static_cast<float>(
                weighted_position_y / total_mass);
            snapshot.center_history_x = static_cast<float>(
                weighted_history_x / total_mass);
            snapshot.center_history_y = static_cast<float>(
                weighted_history_y / total_mass);
        }
        snapshot.root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        snapshot.root_y = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.y : 0.0f;
        snapshot.torso_x = valid_node(blueprint_.torso_node)
            ? particles_[blueprint_.torso_node].position.x : 0.0f;
        snapshot.torso_y = valid_node(blueprint_.torso_node)
            ? particles_[blueprint_.torso_node].position.y : 0.0f;
        return snapshot;
    }

    void Environment::update_human_support_feedback(float dt) noexcept
    {
        human_support_feedback_ = {};
        if (!blueprint_.human_casual_gait_plan() || particles_.empty())
            return;

        const float safe_dt = std::max(dt, 1.0e-5f);
        double total_mass{};
        double weighted_x{};
        double weighted_y{};
        double weighted_velocity_x{};
        double weighted_velocity_y{};
        for (const Particle& particle : particles_)
        {
            if (!std::isfinite(particle.inverse_mass)
                || particle.inverse_mass <= 0.0f
                || !std::isfinite(particle.position.x)
                || !std::isfinite(particle.position.y)
                || !std::isfinite(particle.previous.x)
                || !std::isfinite(particle.previous.y))
                return;
            const double mass = 1.0 / static_cast<double>(
                std::max(particle.inverse_mass, 1.0e-5f));
            const double velocity_x = static_cast<double>(
                (particle.position.x - particle.previous.x) / safe_dt);
            const double velocity_y = static_cast<double>(
                (particle.position.y - particle.previous.y) / safe_dt);
            weighted_x += static_cast<double>(particle.position.x) * mass;
            weighted_y += static_cast<double>(particle.position.y) * mass;
            weighted_velocity_x += velocity_x * mass;
            weighted_velocity_y += velocity_y * mass;
            total_mass += mass;
        }
        if (!std::isfinite(total_mass) || total_mass <= 1.0e-9)
            return;

        HumanSupportFeedback feedback{};
        feedback.center_of_mass = {
            static_cast<float>(weighted_x / total_mass),
            static_cast<float>(weighted_y / total_mass) };
        feedback.center_of_mass_velocity = {
            static_cast<float>(weighted_velocity_x / total_mass),
            static_cast<float>(weighted_velocity_y / total_mass) };

        const float left_load = std::isfinite(support_cluster_normal_impulse_[0])
            ? std::max(0.0f, support_cluster_normal_impulse_[0]) : 0.0f;
        const float right_load = std::isfinite(support_cluster_normal_impulse_[1])
            ? std::max(0.0f, support_cluster_normal_impulse_[1]) : 0.0f;
        feedback.total_normal_impulse = left_load + right_load;
        if (!std::isfinite(feedback.total_normal_impulse)
            || feedback.total_normal_impulse <= 1.0e-7f)
        {
            human_support_feedback_ = feedback;
            return;
        }

        const float aggregate_moment =
            support_cluster_normal_impulse_moment_x_[0]
            + support_cluster_normal_impulse_moment_x_[1];
        if (!std::isfinite(aggregate_moment))
            return;
        feedback.aggregate_cop_x = aggregate_moment
            / feedback.total_normal_impulse;
        const float center_height = feedback.center_of_mass.y
            - ground_height_at(feedback.aggregate_cop_x);
        constexpr float gravity_magnitude = 22.0f;
        const float omega = std::sqrt(gravity_magnitude
            / std::max(center_height, 0.25f));
        feedback.capture_point_x = feedback.center_of_mass.x
            + feedback.center_of_mass_velocity.x / omega;

        float authored_leg_scale{};
        std::size_t leg_count{};
        for (const std::size_t hip_index : { 0u, 2u })
        {
            const std::size_t knee_index = hip_index + 1u;
            if (knee_index >= blueprint_.active_motor_count)
                continue;
            const MotorConstraint& hip = blueprint_.motors[hip_index];
            const MotorConstraint& knee = blueprint_.motors[knee_index];
            if (!hip.enabled || !knee.enabled
                || hip.pivot >= blueprint_.nodes.size()
                || hip.c >= blueprint_.nodes.size()
                || knee.pivot >= blueprint_.nodes.size()
                || knee.c >= blueprint_.nodes.size())
                continue;
            authored_leg_scale += length(blueprint_.nodes[hip.c]
                    - blueprint_.nodes[hip.pivot])
                + length(blueprint_.nodes[knee.c]
                    - blueprint_.nodes[knee.pivot]);
            ++leg_count;
        }
        if (leg_count > 0u)
            authored_leg_scale /= static_cast<float>(leg_count);
        else if (blueprint_.root_node < blueprint_.nodes.size()
            && blueprint_.torso_node < blueprint_.nodes.size())
        {
            authored_leg_scale = length(
                blueprint_.nodes[blueprint_.torso_node]
                - blueprint_.nodes[blueprint_.root_node]);
        }
        authored_leg_scale = std::max(authored_leg_scale, 0.25f);
        feedback.capture_error_x = clamp(
            (feedback.capture_point_x - feedback.aggregate_cop_x)
                / authored_leg_scale,
            -2.0f, 2.0f);
        feedback.load_asymmetry = clamp(
            (right_load - left_load) / feedback.total_normal_impulse,
            -1.0f, 1.0f);
        feedback.valid = std::isfinite(feedback.center_of_mass.x)
            && std::isfinite(feedback.center_of_mass.y)
            && std::isfinite(feedback.center_of_mass_velocity.x)
            && std::isfinite(feedback.center_of_mass_velocity.y)
            && std::isfinite(feedback.aggregate_cop_x)
            && std::isfinite(feedback.capture_point_x)
            && std::isfinite(feedback.capture_error_x)
            && std::isfinite(feedback.load_asymmetry);
        if (!feedback.valid)
            feedback = {};
        human_support_feedback_ = feedback;
    }

    float Environment::human_sole_clearance(const Particle& particle) const noexcept
    {
        const float x = particle.position.x;
        const float soil_sink = stage_uses_deformable_terrain(course_stage_)
            ? support_contact_soil_sink(terrain_firmness_at(x)) : 0.0f;
        // A sole resting at its resolved contact plane is neither airborne
        // nor buried below the legal contact tolerance. Use the same soil
        // allowance as the physical collision solve, not the material surface.
        return particle.position.y - ground_height_at(x)
            - ground_contact_offset(true, particle.radius) + soil_sink;
    }

    void Environment::update_human_gait_coordinator(
        std::span<const float, action_count> actions, float dt) noexcept
    {
        HumanGaitCoordinatorInput input{};
        input.moving = blueprint_.human_casual_gait_plan()
            && stage_requires_forward_gait(course_stage_)
            && std::abs(locomotion_direction()) >= 0.5f
            && !locomotion_stop_requested_ && valid_motion();
        input.non_foot_contact = non_foot_grounded();
        input.facing_local_direction = locomotion_direction()
            * facing_direction();
        input.left_supported = left_supported();
        input.right_supported = right_supported();
        struct SoleKinematics
        {
            float minimum_clearance{ std::numeric_limits<float>::infinity() };
            float maximum_clearance{ -std::numeric_limits<float>::infinity() };
            float velocity_y_sum{};
            std::size_t velocity_sample_count{};
            float maximum_abs_velocity_y{};
            float maximum_abs_velocity_x{};
            std::array<float, 3> velocity_y{};
        };
        const auto sole_kinematics = [&](bool left) noexcept
        {
            const std::array<std::uint16_t, 3> nodes = left
                ? std::array<std::uint16_t, 3>{ human_left_heel_node,
                    human_left_ball_node, human_left_toe_node }
                : std::array<std::uint16_t, 3>{ human_right_heel_node,
                    human_right_ball_node, human_right_toe_node };
            SoleKinematics result{};
            for (const std::uint16_t node : nodes)
            {
                if (!valid_node(node))
                    continue;
                const Particle& particle = particles_[node];
                const float clearance = human_sole_clearance(particle);
                const float velocity_y = (particle.position.y
                    - particle.previous.y) / std::max(dt, 1.0e-6f);
                const float velocity_x = (particle.position.x
                    - particle.previous.x) / std::max(dt, 1.0e-6f);
                result.maximum_clearance = std::max(
                    result.maximum_clearance, clearance);
                result.minimum_clearance = std::min(
                    result.minimum_clearance, clearance);
                result.velocity_y_sum += velocity_y;
                if (result.velocity_sample_count < result.velocity_y.size())
                    result.velocity_y[result.velocity_sample_count] = velocity_y;
                ++result.velocity_sample_count;
                result.maximum_abs_velocity_y = std::max(
                    result.maximum_abs_velocity_y, std::abs(velocity_y));
                result.maximum_abs_velocity_x = std::max(
                    result.maximum_abs_velocity_x, std::abs(velocity_x));
            }
            return result;
        };
        SoleKinematics left = sole_kinematics(true);
        SoleKinematics right = sole_kinematics(false);
        const auto average_velocity_y = [](const SoleKinematics& sole) noexcept
        {
            return sole.velocity_sample_count > 0u
                ? sole.velocity_y_sum
                    / static_cast<float>(sole.velocity_sample_count) : 0.0f;
        };
        const auto maximum_velocity_y_deviation = [&](
            const SoleKinematics& sole) noexcept
        {
            const float average = average_velocity_y(sole);
            float maximum{};
            for (std::size_t index = 0; index < sole.velocity_sample_count
                && index < sole.velocity_y.size(); ++index)
                maximum = std::max(maximum,
                    std::abs(sole.velocity_y[index] - average));
            return maximum;
        };
        input.left_minimum_clearance = left.minimum_clearance;
        input.right_minimum_clearance = right.minimum_clearance;
        input.left_average_velocity_y = average_velocity_y(left);
        input.right_average_velocity_y = average_velocity_y(right);
        input.left_maximum_abs_velocity_y = left.maximum_abs_velocity_y;
        input.right_maximum_abs_velocity_y = right.maximum_abs_velocity_y;
        input.left_maximum_velocity_y_deviation =
            maximum_velocity_y_deviation(left);
        input.right_maximum_velocity_y_deviation =
            maximum_velocity_y_deviation(right);
        input.left_maximum_abs_velocity_x = left.maximum_abs_velocity_x;
        input.right_maximum_abs_velocity_x = right.maximum_abs_velocity_x;
        input.left_maximum_clearance = left.maximum_clearance;
        input.right_maximum_clearance = right.maximum_clearance;
        input.left_normal_impulse = std::max(0.0f,
            support_cluster_normal_impulse_[0]);
        input.right_normal_impulse = std::max(0.0f,
            support_cluster_normal_impulse_[1]);
        const auto heel_reaction = [&](std::uint16_t node) noexcept
        {
            return valid_node(node) && node < support_normal_constraint_lambdas_.size()
                    && particles_[node].grounded
                ? std::max(0.0f, support_normal_constraint_lambdas_[node]
                    / std::max(dt, 1.0e-6f)) : 0.0f;
        };
        input.left_heel_normal_impulse = heel_reaction(human_left_heel_node);
        input.right_heel_normal_impulse = heel_reaction(human_right_heel_node);
        const float control_ramp = policy_control_ramp(elapsed_seconds_
            + policy_control_ramp_start_seconds);
        const auto terminal_alignment_distance = [&](bool left) noexcept
        {
            const std::size_t ankle = left
                ? human_left_ankle_action : human_right_ankle_action;
            const std::size_t toe = left
                ? human_left_toe_action : human_right_toe_action;
            const auto remaining = [&](std::size_t index) noexcept
            {
                if (index >= actions.size()
                    || index >= previous_applied_actions_.size())
                    return 0.0f;
                const float requested = clamp(actions[index], -1.0f, 1.0f)
                    * control_ramp;
                return std::abs(requested - previous_applied_actions_[index]);
            };
            return std::max(remaining(ankle), remaining(toe));
        };
        input.left_terminal_alignment_distance =
            terminal_alignment_distance(true);
        input.right_terminal_alignment_distance =
            terminal_alignment_distance(false);
        const float cadence = gait_task_nominal_cadence_hz(
            gait_task_, elapsed_seconds_);
        // Use the same integrated control clock as the Human teacher target.
        // The raw wall clock starts while motor targets are still blended to
        // the authored pose; advancing this coordinator on that clock makes a
        // nominal exchange precede any finite action capable of unloading it.
        const float gait_clock_seconds = shuttle_enabled()
            ? shuttle_state_.phase_seconds : elapsed_seconds_;
        const float control_seconds = policy_control_phase_seconds(
            gait_clock_seconds);
        const float previous_control_seconds = policy_control_phase_seconds(
            std::max(0.0f, gait_clock_seconds - dt));
        const float control_dt = std::max(0.0f,
            control_seconds - previous_control_seconds);
        human_gait_coordinator_ = step_human_gait_coordinator(
            human_gait_coordinator_, input,
            2.0f * pi * cadence * control_dt, control_dt);
    }

    void Environment::record_authority_delta(AuthoritySource source,
        const AuthoritySnapshot& before, float dt) noexcept
    {
        if (!raw_authority_telemetry_enabled_)
            return;
        const AuthoritySnapshot after = authority_snapshot(dt);
        AuthoritySourceTelemetry& telemetry = raw_authority_telemetry_.sources[
            static_cast<std::size_t>(source)];
        const float velocity_delta = after.center_velocity_x
            - before.center_velocity_x;
        const float velocity_delta_y = after.center_velocity_y
            - before.center_velocity_y;
        const float center_position_delta_x = after.center_position_x
            - before.center_position_x;
        const float center_position_delta_y = after.center_position_y
            - before.center_position_y;
        const float center_history_delta_x = after.center_history_x
            - before.center_history_x;
        const float center_history_delta_y = after.center_history_y
            - before.center_history_y;
        const float root_delta = after.root_x - before.root_x;
        const float root_delta_y = after.root_y - before.root_y;
        const float torso_delta = after.torso_x - before.torso_x;
        const float torso_delta_y = after.torso_y - before.torso_y;
        telemetry.center_velocity_delta_x += velocity_delta;
        telemetry.center_velocity_delta_y += velocity_delta_y;
        telemetry.maximum_absolute_center_velocity_delta_x = std::max(
            telemetry.maximum_absolute_center_velocity_delta_x,
            std::abs(velocity_delta));
        telemetry.maximum_absolute_center_velocity_delta_y = std::max(
            telemetry.maximum_absolute_center_velocity_delta_y,
            std::abs(velocity_delta_y));
        telemetry.positive_horizontal_specific_energy += std::max(0.0f,
            0.5f * (after.center_velocity_x * after.center_velocity_x
                - before.center_velocity_x * before.center_velocity_x));
        telemetry.center_position_delta_x += center_position_delta_x;
        telemetry.center_position_delta_y += center_position_delta_y;
        telemetry.center_history_delta_x += center_history_delta_x;
        telemetry.center_history_delta_y += center_history_delta_y;
        telemetry.root_position_delta_x += root_delta;
        telemetry.root_position_delta_y += root_delta_y;
        telemetry.torso_position_delta_x += torso_delta;
        telemetry.torso_position_delta_y += torso_delta_y;
        constexpr float meaningful_delta = 1.0e-7f;
        if (std::abs(velocity_delta) > meaningful_delta
            || std::abs(velocity_delta_y) > meaningful_delta
            || std::abs(center_position_delta_x) > meaningful_delta
            || std::abs(center_position_delta_y) > meaningful_delta
            || std::abs(center_history_delta_x) > meaningful_delta
            || std::abs(center_history_delta_y) > meaningful_delta
            || std::abs(root_delta) > meaningful_delta
            || std::abs(root_delta_y) > meaningful_delta
            || std::abs(torso_delta) > meaningful_delta
            || std::abs(torso_delta_y) > meaningful_delta)
            ++telemetry.applications;

        const bool direct_helper = source == AuthoritySource::posture_helper
            || source == AuthoritySource::manipulator_helper
            || source == AuthoritySource::ledge_training_scaffold
            || source == AuthoritySource::structure_projection;
        if (direct_helper && (std::abs(root_delta) > meaningful_delta
                || std::abs(root_delta_y) > meaningful_delta))
            ++raw_authority_telemetry_.root_drive_corrections;
        if (direct_helper && (std::abs(torso_delta) > meaningful_delta
                || std::abs(torso_delta_y) > meaningful_delta))
            ++raw_authority_telemetry_.torso_drive_corrections;
    }

    void Environment::record_maximum_bone_error() noexcept
    {
        if (!raw_authority_telemetry_enabled_)
            return;
        raw_authority_telemetry_.maximum_bone_error_ratio = 0.0f;
        raw_authority_telemetry_.maximum_bone_error_index =
            std::numeric_limits<std::size_t>::max();
        const std::size_t leg_motors = std::min<std::size_t>(
            4u, blueprint_.active_motor_count);
        for (std::size_t bone_index = 0;
            bone_index < blueprint_.bones.size(); ++bone_index)
        {
            const DistanceConstraint& bone = blueprint_.bones[bone_index];
            bool primary_leg_segment = false;
            for (std::size_t motor_index = 0;
                motor_index < leg_motors; ++motor_index)
            {
                const MotorConstraint& motor = blueprint_.motors[motor_index];
                primary_leg_segment = primary_leg_segment
                    || (bone.a == motor.pivot && bone.b == motor.c)
                    || (bone.b == motor.pivot && bone.a == motor.c);
            }
            if (!primary_leg_segment || bone.a >= particles_.size()
                || bone.b >= particles_.size() || bone.rest_length <= 1.0e-6f)
                continue;
            const float error = bone_length_error_ratio(
                length(particles_[bone.b].position - particles_[bone.a].position),
                bone.rest_length);
            if (error > raw_authority_telemetry_.maximum_bone_error_ratio)
            {
                raw_authority_telemetry_.maximum_bone_error_ratio = error;
                raw_authority_telemetry_.maximum_bone_error_index = bone_index;
            }
        }
    }

    void Environment::solve_distance(const DistanceConstraint& constraint,
        bool preserve_velocity) noexcept
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

        const Vec2 correction = delta
            * ((distance - constraint.rest_length) / distance
                * clamp(constraint.stiffness, 0.05f, 1.0f));
        const Vec2 lhs_shift = correction * (lhs.inverse_mass / weight);
        const Vec2 rhs_shift = -correction * (rhs.inverse_mass / weight);
        lhs.position += lhs_shift;
        rhs.position += rhs_shift;
        if (preserve_velocity)
        {
            // This mode is reserved for final anatomical closure after the
            // physical motor/contact solve. Translating Verlet history with the
            // mass-balanced correction prevents a numerical bone-length cleanup
            // from becoming a late joint or root impulse.
            lhs.previous += lhs_shift;
            rhs.previous += rhs_shift;
        }
    }

    void Environment::project_structure_rigid(float dt) noexcept
    {
        static_cast<void>(dt);
        const float crouch_authority = course_stage_ == CourseStage::crouch_walk
            ? effective_physical_guidance_authority() : 1.0f;
        if (course_stage_ == CourseStage::crouch_walk
            && crouch_authority <= 0.0f)
            return;
        // A moving Human must expose what its physical motors and contacts can
        // actually hold. The paired-leg reconstruction below can translate the
        // complete upper body and directly rewrite both knees after the motor
        // solve, which is authored pose authority even when Verlet history is
        // translated with it. Keep it only for non-translating lesson poses.
        if (blueprint_.human_casual_gait_plan()
            && course_stage_ != CourseStage::balance
            && course_stage_ != CourseStage::duck_press)
            return;
        const bool crouch_walking_stage = course_stage_ == CourseStage::crouch_walk;
        const bool structure_projected_stage = course_stage_ == CourseStage::uneven
            || course_stage_ == CourseStage::shuttle
            || crouch_walking_stage
            || course_stage_ == CourseStage::hurdles
            || course_stage_ == CourseStage::moving_hazards;
        if (!structure_projected_stage
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan())
            return;

        struct LegChain
        {
            std::uint16_t hip{};
            std::uint16_t knee{};
            std::uint16_t ankle{};
            std::uint16_t contact{};
            float upper_length{};
            float lower_length{};
            bool supported{};
        };

        auto make_chain = [&](std::size_t hip_motor_index,
            std::size_t knee_motor_index, bool left) noexcept
        {
            const MotorConstraint& hip_motor = blueprint_.motors[hip_motor_index];
            const MotorConstraint& knee_motor = blueprint_.motors[knee_motor_index];
            const std::uint16_t contact = left
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            return LegChain{
                hip_motor.pivot,
                hip_motor.c,
                knee_motor.c,
                contact,
                length(blueprint_.nodes[hip_motor.c]
                    - blueprint_.nodes[hip_motor.pivot]),
                length(blueprint_.nodes[knee_motor.c]
                    - blueprint_.nodes[hip_motor.c]),
                contact_supported(contact)
            };
        };

        std::array<LegChain, 2> legs{
            make_chain(0u, 1u, true), make_chain(2u, 3u, false)
        };
        for (const LegChain& leg : legs)
        {
            if (!valid_node(leg.hip) || !valid_node(leg.knee)
                || !valid_node(leg.ankle) || !valid_node(leg.contact)
                || leg.upper_length <= 1.0e-5f
                || leg.lower_length <= 1.0e-5f)
                return;
        }

        const std::size_t supported_count = static_cast<std::size_t>(legs[0].supported)
            + static_cast<std::size_t>(legs[1].supported);
        const float single_support_ratio = crouch_walking_stage ? 0.58f
            : course_stage_ == CourseStage::hurdles
            ? 0.72f : course_stage_ == CourseStage::moving_hazards
                ? 0.76f : 0.80f;
        const float minimum_stance_ratio = crouch_walking_stage
            ? 0.58f : supported_count >= 2u
            ? std::max(0.84f, single_support_ratio) : single_support_ratio;

        // A two-link leg can keep both bone lengths yet still fold until the
        // knee appears to telescope into the pelvis. Lift the complete upper
        // body just enough to restore reserve above each planted support.
        float required_upper_body_lift = 0.0f;
        for (const LegChain& leg : legs)
        {
            if (!leg.supported)
                continue;
            const Vec2 hip = particles_[leg.hip].position;
            const Vec2 ankle = particles_[leg.ankle].position;
            const float maximum_extension = leg.upper_length + leg.lower_length;
            const float target_extension = maximum_extension * minimum_stance_ratio;
            const float horizontal = hip.x - ankle.x;
            const float horizontal_squared = std::min(
                horizontal * horizontal,
                target_extension * target_extension);
            const float target_vertical = std::sqrt(std::max(0.0f,
                target_extension * target_extension - horizontal_squared));
            required_upper_body_lift = std::max(required_upper_body_lift,
                target_vertical - (hip.y - ankle.y));
        }

        if (required_upper_body_lift > 0.0f)
        {
            const Vec2 lift{ 0.0f,
                required_upper_body_lift * crouch_authority };
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                const bool knee = index == legs[0].knee || index == legs[1].knee;
                const bool ankle = index == legs[0].ankle
                    || index == legs[1].ankle;
                if (knee || ankle || blueprint_.is_support_seed(index))
                    continue;
                particles_[index].position += lift;
                particles_[index].previous += lift;
            }
        }

        auto solve_chain_ik = [&](const LegChain& leg)
        {
            Particle& hip_particle = particles_[leg.hip];
            Particle& knee_particle = particles_[leg.knee];
            Particle& ankle_particle = particles_[leg.ankle];
            Vec2 hip = hip_particle.position;
            Vec2 ankle = ankle_particle.position;
            Vec2 hip_to_ankle = ankle - hip;
            float distance = length(hip_to_ankle);
            const float minimum_reach = std::abs(
                leg.upper_length - leg.lower_length) + 0.0001f;
            const float maximum_reach = leg.upper_length + leg.lower_length - 0.0001f;

            if (distance > maximum_reach)
            {
                const Vec2 direction = normalized(hip_to_ankle, { 0.0f, -1.0f });
                const Vec2 correction = direction
                    * ((distance - maximum_reach) * crouch_authority);
                if (leg.supported)
                {
                    for (std::size_t index = 0; index < particles_.size(); ++index)
                    {
                        const bool knee = index == legs[0].knee || index == legs[1].knee;
                        const bool ankle_node = index == legs[0].ankle
                            || index == legs[1].ankle;
                        if (knee || ankle_node || blueprint_.is_support_seed(index))
                            continue;
                        particles_[index].position += correction;
                        particles_[index].previous += correction;
                    }
                    hip = hip_particle.position;
                }
                else
                {
                    ankle_particle.position -= correction;
                    ankle_particle.previous -= correction;
                    ankle = ankle_particle.position;
                }
                hip_to_ankle = ankle - hip;
                distance = length(hip_to_ankle);
            }
            if (distance < minimum_reach)
            {
                const Vec2 direction = normalized(hip_to_ankle, { 0.0f, -1.0f });
                const Vec2 correction = direction
                    * ((minimum_reach - distance) * crouch_authority);
                if (leg.supported)
                {
                    const Vec2 lift{ 0.0f,
                        (minimum_reach - distance) * crouch_authority };
                    for (std::size_t index = 0; index < particles_.size(); ++index)
                    {
                        const bool knee = index == legs[0].knee || index == legs[1].knee;
                        const bool ankle_node = index == legs[0].ankle
                            || index == legs[1].ankle;
                        if (knee || ankle_node || blueprint_.is_support_seed(index))
                            continue;
                        particles_[index].position += lift;
                        particles_[index].previous += lift;
                    }
                    hip = hip_particle.position;
                }
                else
                {
                    ankle_particle.position += correction;
                    ankle_particle.previous += correction;
                    ankle = ankle_particle.position;
                }
                hip_to_ankle = ankle - hip;
                distance = length(hip_to_ankle);
            }

            const Vec2 axis = normalized(hip_to_ankle, { 0.0f, -1.0f });
            const Vec2 perpendicular{ -axis.y, axis.x };
            const float safe_distance = std::clamp(distance,
                minimum_reach, maximum_reach);
            const float along = (
                leg.upper_length * leg.upper_length
                - leg.lower_length * leg.lower_length
                + safe_distance * safe_distance)
                / (2.0f * safe_distance);
            const float height = std::sqrt(std::max(0.0f,
                leg.upper_length * leg.upper_length - along * along));
            const Vec2 base = hip + axis * along;

            // Both knees share one facing-local sagittal bend side. Preserving
            // each prior side independently locked a crab/X fold into the solve,
            // while a fixed world sign broke physical reflection on shuttle turns.
            const float bend_sign = facing_direction();
            const Vec2 target_knee = base + perpendicular * (height * bend_sign);
            const Vec2 knee_delta = (target_knee - knee_particle.position)
                * crouch_authority;
            knee_particle.position += knee_delta;
            knee_particle.previous += knee_delta;
        };

        // Reconstruct both knees from exact two-link geometry. This preserves
        // natural swing-leg bend while preventing a planted stance chain from
        // folding into a visually compressed telescoping leg.
        constexpr int chain_convergence_passes = 16;
        for (int pass = 0; pass < chain_convergence_passes; ++pass)
        {
            solve_chain_ik(legs[0]);
            solve_chain_ik(legs[1]);
        }
    }

    void Environment::separate_support_clusters() noexcept
    {
        // Side-view locomotion requires the near and far legs to pass through
        // the same screen-space lane. Human heel/ball/toe nodes are two
        // graph-connected semantic soles, not six independent legs; forcing
        // those nodes apart injects a horizontal impulse and tears the boot
        // art away from its physical anchors. Preserve the fused-support guard
        // only for genuinely independent static multi-support branches.
        if (stage_requires_forward_gait(course_stage_)
            || blueprint_.human_casual_gait_plan())
            return;
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
                float authored_direction = (blueprint_.nodes[second_index].x
                    - blueprint_.nodes[first_index].x) * facing_direction();
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

        const auto support_mean_x = [&](std::uint16_t primary,
                                        const std::vector<std::uint16_t>& additional) noexcept
        {
            float sum = 0.0f;
            std::size_t count = 0;
            const auto accumulate = [&](std::uint16_t node)
            {
                if (!valid_node(node))
                    return;
                sum += particles_[node].position.x;
                ++count;
            };
            accumulate(primary);
            for (const std::uint16_t node : additional)
                accumulate(node);
            return count == 0 ? 0.0f : sum / static_cast<float>(count);
        };
        const float authored_direction =
            (blueprint_.nodes[blueprint_.right_contact_node].x
                - blueprint_.nodes[blueprint_.left_contact_node].x)
            * facing_direction() < 0.0f ? -1.0f : 1.0f;
        const float current_gap = authored_direction
            * (support_mean_x(blueprint_.right_contact_node, blueprint_.additional_right_contact_nodes)
                - support_mean_x(blueprint_.left_contact_node, blueprint_.additional_left_contact_nodes));
        constexpr float minimum_cluster_gap = 0.080f;
        if (current_gap < minimum_cluster_gap)
        {
            const float correction = 0.5f * (minimum_cluster_gap - current_gap);
            const auto shift = [&](std::uint16_t node, float amount)
            {
                if (!valid_node(node))
                    return;
                particles_[node].position.x += amount;
                particles_[node].previous.x += amount * 0.35f;
            };
            shift(blueprint_.left_contact_node, -authored_direction * correction);
            for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
                shift(node, -authored_direction * correction);
            shift(blueprint_.right_contact_node, authored_direction * correction);
            for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
                shift(node, authored_direction * correction);
        }
    }

    float Environment::maximum_bone_length_error_ratio() const noexcept
    {
        float maximum_error = 0.0f;
        if (!stage_requires_forward_gait(course_stage_)
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan())
            return 0.0f;
        const std::size_t leg_motors = std::min<std::size_t>(
            4u, blueprint_.active_motor_count);
        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            bool primary_leg_segment = false;
            for (std::size_t index = 0; index < leg_motors; ++index)
            {
                const MotorConstraint& motor = blueprint_.motors[index];
                primary_leg_segment = primary_leg_segment
                    || ((bone.a == motor.pivot && bone.b == motor.c)
                        || (bone.b == motor.pivot && bone.a == motor.c));
            }
            if (!primary_leg_segment)
                continue;
            if (bone.a >= particles_.size() || bone.b >= particles_.size()
                || bone.rest_length <= 1.0e-6f)
                return std::numeric_limits<float>::infinity();
            maximum_error = std::max(maximum_error, bone_length_error_ratio(
                length(particles_[bone.b].position - particles_[bone.a].position),
                bone.rest_length));
        }
        return maximum_error;
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
        const bool physical_moving_human = blueprint_.human_casual_gait_plan()
            && course_stage_ != CourseStage::balance
            && course_stage_ != CourseStage::duck_press;
        if (raw_policy_audit() || physical_moving_human)
        {
            // Raw Stand uses a compliant internal neck joint. Moving stages
            // keep their passive travel stop. Neither acts on the world or
            // applies back support.
            if (!valid_node(blueprint_.root_node)
                || !valid_node(blueprint_.torso_node)
                || !valid_node(blueprint_.head_node))
                return;
            MotorConstraint neck{
                blueprint_.root_node,
                blueprint_.torso_node,
                blueprint_.head_node };
            neck.neutral_angle = signed_angle(
                blueprint_.nodes[blueprint_.root_node]
                    - blueprint_.nodes[blueprint_.torso_node],
                blueprint_.nodes[blueprint_.head_node]
                    - blueprint_.nodes[blueprint_.torso_node]);
            constexpr float neck_range = degrees_to_radians(65.0f);
            neck.minimum_angle = neck.neutral_angle - neck_range;
            neck.maximum_angle = neck.neutral_angle + neck_range;
            neck.strength = 0.10f;
            neck.enabled = true;
            const float local_current = joint_angle(neck) * facing_direction();
            const float displacement = wrap_angle(
                local_current - neck.neutral_angle);
            if (blueprint_.human_casual_gait_plan()
                && course_stage_ == CourseStage::balance)
            {
                const Vec2 previous_parent = particles_[neck.a].previous
                    - particles_[neck.pivot].previous;
                const Vec2 previous_head = particles_[neck.c].previous
                    - particles_[neck.pivot].previous;
                const float previous_angle = signed_angle(previous_parent,
                    previous_head) * facing_direction();
                const float angular_change = wrap_angle(local_current - previous_angle);
                const float spring_target = clamp(-angular_change * 2.5f,
                    -neck_range, neck_range);
                solve_motor(neck, spring_target / neck_range);
            }
            else if (displacement < -neck_range)
                solve_motor(neck, -1.0f);
            else if (displacement > neck_range)
                solve_motor(neck, 1.0f);
            return;
        }
        const float guide_authority = course_stage_ == CourseStage::crouch_walk
            ? effective_physical_guidance_authority() : 1.0f;
        if (guide_authority <= 0.0f)
            return;
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node))
            return;
        Vec2 rest_body = blueprint_.nodes[blueprint_.torso_node]
            - blueprint_.nodes[blueprint_.root_node];
        rest_body.x *= facing_direction();
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
            Vec2 rest_offset = blueprint_.nodes[node] - blueprint_.nodes[parent];
            rest_offset.x *= facing_direction();
            const Vec2 target = particles_[parent].position + rotate(rest_offset, body_rotation);
            Vec2 error = target - particles_[node].position;
            const float maximum_error = std::max(0.08f, length(rest_offset) * 0.45f);
            const float error_length = length(error);
            if (error_length > maximum_error && error_length > 1.0e-6f)
                error *= maximum_error / error_length;
            Particle& endpoint = particles_[node];
            Particle& attachment = particles_[parent];
            // Preserve the established assisted pose guide. It is explicitly
            // reported as teacher composition and never runs in raw inference.
            endpoint.position += error * strength * 0.90f * guide_authority;
            attachment.position -= error * strength * 0.10f * guide_authority;
            endpoint.previous += (endpoint.position
                - endpoint.previous) * (strength * 0.35f * guide_authority);
        };

        stabilize(blueprint_.head_node, 0.055f);
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (passive_endpoint(blueprint_, index))
                stabilize(static_cast<std::uint16_t>(index), 0.040f);
        }
    }
    void Environment::stabilize_human_manipulator_envelope() noexcept
    {
        const float guide_authority = course_stage_ == CourseStage::crouch_walk
            ? effective_physical_guidance_authority() : 1.0f;
        if (raw_policy_audit()
            || guide_authority <= 0.0f
            || !blueprint_.human_casual_gait_plan()
            || !valid_node(blueprint_.root_node)
            || blueprint_.root_node >= blueprint_.nodes.size()
            || (equipment_directive_ == EquipmentDirective::passive
                && (course_stage_ == CourseStage::uneven
                    || course_stage_ == CourseStage::shuttle
                    || course_stage_ == CourseStage::crouch_walk)))
            return;
        const bool equipment_pose_owned = weapon_class_ != WeaponClass::none
            && equipment_directive_ != EquipmentDirective::passive;
        const bool gait_stage = !equipment_pose_owned
            && (course_stage_ == CourseStage::uneven
                || course_stage_ == CourseStage::shuttle
                || course_stage_ == CourseStage::crouch_walk);
        if (gait_stage)
        {
            // Motors remain the source of the arm pose. This projection only
            // applies their already-conditioned targets to the driven arm
            // nodes, without rotating or translating the torso as a reaction
            // mass. It prevents balance impulses from turning relaxed arms
            // into synchronized zombie reaches.
            for (std::size_t shoulder_index = 0;
                shoulder_index < blueprint_.active_motor_count; ++shoulder_index)
            {
                const MotorConstraint& shoulder = blueprint_.motors[shoulder_index];
                if (!shoulder.enabled || shoulder.a != blueprint_.torso_node
                    || shoulder.pivot >= particles_.size()
                    || shoulder.c >= particles_.size()
                    || blueprint_.is_support_seed(shoulder.c))
                    continue;
                std::size_t elbow_index = blueprint_.active_motor_count;
                for (std::size_t candidate = 0;
                    candidate < blueprint_.active_motor_count; ++candidate)
                {
                    const MotorConstraint& elbow = blueprint_.motors[candidate];
                    if (candidate != shoulder_index && elbow.enabled
                        && elbow.pivot == shoulder.c
                        && elbow.a == shoulder.pivot
                        && elbow.c < particles_.size()
                        && !blueprint_.is_support_seed(elbow.c))
                    {
                        elbow_index = candidate;
                        break;
                    }
                }
                if (elbow_index >= blueprint_.active_motor_count)
                    continue;
                const MotorConstraint& elbow = blueprint_.motors[elbow_index];
                const Vec2 shoulder_pivot = particles_[shoulder.pivot].position;
                const float upper_length = length(blueprint_.nodes[shoulder.c]
                    - blueprint_.nodes[shoulder.pivot]);
                const float lower_length = length(blueprint_.nodes[elbow.c]
                    - blueprint_.nodes[elbow.pivot]);
                if (upper_length <= 1.0e-5f || lower_length <= 1.0e-5f)
                    continue;
                const bool left_arm = shoulder_index < 6u;
                const std::uint16_t own_contact = left_arm
                    ? blueprint_.left_contact_node : blueprint_.right_contact_node;
                const std::uint16_t other_contact = left_arm
                    ? blueprint_.right_contact_node : blueprint_.left_contact_node;
                if (!valid_node(own_contact) || !valid_node(other_contact))
                    continue;
                const float physical_leg_phase = clamp(
                    (particles_[own_contact].position.x
                        - particles_[other_contact].position.x)
                        * facing_direction() / 0.80f, -1.0f, 1.0f);
                const float gait_mix = gait_transition_mix();
                const float arm_amplitude = course_stage_ == CourseStage::crouch_walk
                    ? 0.035f : std::lerp(0.060f, 0.160f, gait_mix);
                Vec2 hand_offset{
                    -physical_leg_phase * arm_amplitude * facing_direction(),
                    -0.94f * (upper_length + lower_length) };
                const float minimum_reach = std::abs(upper_length - lower_length)
                    + 1.0e-4f;
                const float maximum_reach = (upper_length + lower_length) * 0.985f;
                float reach = length(hand_offset);
                if (reach <= 1.0e-5f)
                    continue;
                hand_offset *= clamp(reach, minimum_reach, maximum_reach) / reach;
                reach = length(hand_offset);
                const Vec2 direction = hand_offset / reach;
                const float along = (upper_length * upper_length
                    - lower_length * lower_length + reach * reach) / (2.0f * reach);
                const float height = std::sqrt(std::max(0.0f,
                    upper_length * upper_length - along * along));
                const Vec2 perpendicular{ -direction.y, direction.x };
                Vec2 authored_upper = blueprint_.nodes[shoulder.c]
                    - blueprint_.nodes[shoulder.pivot];
                authored_upper.x *= facing_direction();
                const float authored_cross = hand_offset.x * authored_upper.y
                    - hand_offset.y * authored_upper.x;
                const Vec2 desired_elbow = shoulder_pivot + direction * along
                    + perpendicular * (authored_cross >= 0.0f ? height : -height);
                const Vec2 desired_hand = shoulder_pivot + hand_offset;
                auto project = [&](std::uint16_t node, Vec2 target, float strength)
                {
                    Particle& particle = particles_[node];
                    Vec2 correction = target - particle.position;
                    const float correction_length = length(correction);
                    if (correction_length > 0.18f && correction_length > 1.0e-6f)
                        correction *= 0.18f / correction_length;
                    const Vec2 applied = correction * strength
                        * guide_authority;
                    particle.position += applied;
                    particle.previous += applied;
                };
                project(shoulder.c, desired_elbow, 0.72f);
                project(elbow.c, desired_hand, 0.82f);
            }
        }

        const Particle& root = particles_[blueprint_.root_node];
        const Vec2 root_velocity = root.position - root.previous;
        for (std::size_t node = 0; node < particles_.size(); ++node)
        {
            if (!manipulator_endpoint(blueprint_, node)
                || node >= blueprint_.nodes.size())
                continue;

            Particle& endpoint = particles_[node];
            const Vec2 offset = endpoint.position - root.position;
            const float radius = length(offset);
            if (radius <= 1.0e-5f)
                continue;

            const float rest_radius = length(
                blueprint_.nodes[node] - blueprint_.nodes[blueprint_.root_node]);
            const float integrity_radius = std::max(
                1.80f, rest_radius * 3.0f + 0.80f);
            const float soft_radius = std::min(
                integrity_radius - 0.30f, rest_radius + 0.70f);
            const float hard_radius = integrity_radius - 0.12f;
            const Vec2 direction = offset / radius;
            const Vec2 relative_velocity =
                (endpoint.position - endpoint.previous) - root_velocity;
            const float outward_velocity = dot(relative_velocity, direction);
            const float normalized = clamp(
                (radius - soft_radius)
                    / std::max(0.05f, hard_radius - soft_radius),
                0.0f, 1.0f);
            const float strength = normalized * normalized
                * (3.0f - 2.0f * normalized) * guide_authority;
            if (outward_velocity > 0.0f && strength > 0.0f)
                endpoint.previous += direction * outward_velocity * strength;

            if (radius > hard_radius)
            {
                const Vec2 correction = direction
                    * (std::min(radius - hard_radius, 0.018f)
                        * guide_authority);
                endpoint.position -= correction;
                endpoint.previous -= correction;
            }
        }
    }
    void Environment::stabilize_balance_posture() noexcept
    {
        // This helper rotates or translates authored body nodes and, in Stand,
        // can move the pelvis itself. Its authority must remain explicit so a
        // reduced training hint cannot silently keep holding the complete body.
        // Raw evaluation exposes only motors and physical constraints.
        if (raw_policy_audit()
            || (blueprint_.human_casual_gait_plan()
                && stage_requires_forward_gait(course_stage_)))
            return;
        const bool balance_lesson = course_stage_ == CourseStage::balance;
        const float guide_authority = balance_lesson
                || course_stage_ == CourseStage::crouch_walk
            ? effective_physical_guidance_authority() : 1.0f;
        if (guide_authority <= 0.0f)
            return;
        const bool equipment_stance_lesson =
            course_stage_ == CourseStage::equipment_targets
            && !raw_policy_audit();
        const bool supported_stance_lesson = balance_lesson
            || equipment_stance_lesson;
        const bool guided_avian = blueprint_.avian_gait();
        const bool guided_monoped = blueprint_.monopedal_gait();
        const bool locomotion_core_guide = stage_requires_forward_gait(course_stage_)
            && (!blueprint_.horizontal_body_plan()
                || guided_avian || guided_monoped);
        if ((!supported_stance_lesson && !locomotion_core_guide)
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        if (locomotion_core_guide)
        {
            if (guided_avian || guided_monoped)
            {
                Vec2 rest_support{};
                Vec2 current_support{};
                std::size_t support_count{};
                for (std::size_t node = 0; node < particles_.size(); ++node)
                {
                    if (!blueprint_.is_support_seed(node)
                        || !particles_[node].grounded)
                        continue;
                    rest_support += blueprint_.nodes[node];
                    current_support += particles_[node].position;
                    ++support_count;
                }
                // A monoped's two authored contact seeds are the heel and toe
                // of one foot. Requiring both to remain planted disabled its
                // support guide during every legitimate rocker transfer.
                const std::size_t required_supports = 1u;
                if (support_count >= required_supports)
                {
                    rest_support /= static_cast<float>(support_count);
                    current_support /= static_cast<float>(support_count);
                    Vec2 rest_root_offset =
                        blueprint_.nodes[blueprint_.root_node] - rest_support;
                    rest_root_offset.x *= facing_direction();
                    Vec2 correction = current_support + rest_root_offset
                        - particles_[blueprint_.root_node].position;
                    const float magnitude = length(correction);
                    const float maximum_step = guided_monoped ? 0.030f : 0.015f;
                    if (magnitude > maximum_step && magnitude > 1.0e-6f)
                        correction *= maximum_step / magnitude;
                    const Vec2 applied = correction
                        * (guided_monoped ? 0.16f : 0.055f)
                        * guide_authority;
                    for (std::size_t node = 0; node < particles_.size(); ++node)
                    {
                        bool driven_leg_node = blueprint_.is_support_seed(node);
                        for (std::size_t motor_index = 0;
                            motor_index < blueprint_.active_motor_count;
                            ++motor_index)
                        {
                            const MotorConstraint& motor =
                                blueprint_.motors[motor_index];
                            driven_leg_node = driven_leg_node
                                || node == motor.pivot || node == motor.c;
                        }
                        if (driven_leg_node && node != blueprint_.root_node
                            && node != blueprint_.torso_node)
                            continue;
                        particles_[node].position += applied;
                        particles_[node].previous += applied;
                    }
                }
            }

            const Vec2 root = particles_[blueprint_.root_node].position;
            const Vec2 rest_body = blueprint_.nodes[blueprint_.torso_node]
                - blueprint_.nodes[blueprint_.root_node];
            const Vec2 current_body = particles_[blueprint_.torso_node].position - root;
            if (length(rest_body) > 1.0e-5f && length(current_body) > 1.0e-5f)
            {
                Vec2 authored_body = rest_body;
                authored_body.x *= facing_direction();
                const bool human_casual = blueprint_.human_casual_gait_plan();
                const bool casual_guided_gait = human_casual
                    && (course_stage_ == CourseStage::uneven
                        || course_stage_ == CourseStage::crouch_walk);
                const float travel_bias = guided_monoped ? 0.008f
                    : guided_avian ? 0.010f
                    : human_casual ? 0.006f : 0.012f;
                const Vec2 desired_body = normalized(
                    { authored_body.x + travel_bias * locomotion_direction()
                        * length(rest_body), authored_body.y },
                    normalized(authored_body, { 0.0f, 1.0f }))
                    * length(rest_body);
                const float body_rotation = signed_angle(desired_body, current_body);
                bool appendaged_biped = false;
                for (std::size_t index = 0;
                    index < blueprint_.active_motor_count; ++index)
                {
                    appendaged_biped = appendaged_biped
                        || blueprint_.support_branch_mask(
                            blueprint_.motors[index]) == 0u;
                }
                const float correction_gain = guided_monoped ? 0.060f
                    : guided_avian ? 0.040f
                    : casual_guided_gait ? 0.160f
                    : human_casual ? 0.070f
                    : appendaged_biped ? 0.028f : 0.025f;
                const float maximum_correction = guided_monoped ? 0.0140f
                    : guided_avian ? 0.0080f
                    : casual_guided_gait ? 0.0250f
                    : human_casual ? 0.0140f
                    : appendaged_biped ? 0.0060f : 0.0060f;
                const float correction = guide_authority
                    * clamp(-body_rotation * correction_gain,
                        -maximum_correction, maximum_correction);
                for (std::size_t node = 0; node < particles_.size(); ++node)
                {
                    bool guided_body_node = (guided_avian || guided_monoped)
                        && !blueprint_.is_support_seed(node);
                    if (guided_body_node)
                    {
                        for (std::size_t motor_index = 0;
                            motor_index < blueprint_.active_motor_count;
                            ++motor_index)
                        {
                            const MotorConstraint& motor =
                                blueprint_.motors[motor_index];
                            guided_body_node = guided_body_node
                                && node != motor.pivot && node != motor.c;
                        }
                    }
                    const bool upper_body_node = node == blueprint_.torso_node
                        || node == blueprint_.head_node
                        || (!human_casual
                            && manipulator_branch_node(blueprint_, node))
                        || guided_body_node;
                    if (!upper_body_node || node == blueprint_.root_node)
                        continue;
                    Particle& particle = particles_[node];
                    const Vec2 corrected = root + rotate(particle.position - root,
                        correction);
                    const Vec2 translation = corrected - particle.position;
                    particle.position = corrected;
                    particle.previous += translation;
                }
            }
            return;
        }

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
            Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
            rest_offset.x *= facing_direction();
            const Vec2 target = current_support + rest_offset;
            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * (strength * guide_authority);
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
        if (course_stage_ == CourseStage::crouch_walk)
        {
            const float guide_authority =
                effective_physical_guidance_authority();
            if (!blueprint_.human_casual_gait_plan()
                || !valid_node(blueprint_.root_node)
                || guide_authority <= 0.0f)
                return;
            if (crouch_posture_guide_applied_this_step_)
                return;
            crouch_posture_guide_applied_this_step_ = true;
            Vec2 rest_support{};
            Vec2 current_support{};
            std::size_t support_count{};
            auto accumulate = [&](std::uint16_t node)
            {
                if (!valid_node(node))
                    return;
                rest_support += blueprint_.nodes[node];
                current_support += Vec2{
                    particles_[node].position.x,
                    ground_height_at(particles_[node].position.x)
                        + ground_contact_offset(true, particles_[node].radius) };
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
            const float rest_height = blueprint_.nodes[blueprint_.root_node].y
                - rest_support.y;
            const float lowering_blend = clamp(elapsed_seconds_ / 1.50f,
                0.0f, 1.0f);
            const float desired_root_y = current_support.y + rest_height
                - 0.52f * lowering_blend;
            // This constraint has no horizontal component. World translation
            // therefore remains the result of planted-foot reaction forces;
            // it can lower the pelvis into a crouch but cannot pull it down a
            // rail or manufacture odometer progress.
            const float correction = guide_authority
                * clamp(desired_root_y
                    - particles_[blueprint_.root_node].position.y,
                    -std::max(0.020f, 0.90f * last_step_dt_), 0.0f);
            // Position and Verlet history receive the same correction below,
            // so this is solver convergence rather than a velocity impulse.
            // A dt-scaled cap lost against the fixed 14 constraint passes at
            // higher update rates and made crouch depth frequency-dependent.
            for (std::size_t node = 0; node < particles_.size(); ++node)
            {
                if (node != blueprint_.root_node
                    && node != blueprint_.torso_node
                    && node != blueprint_.head_node
                    && !manipulator_branch_node(blueprint_, node))
                    continue;
                particles_[node].position.y += correction;
                particles_[node].previous.y += correction;
            }
            return;
        }
        const bool moving_crouch = false;
        if (course_stage_ != CourseStage::duck_press
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        Vec2 rest_support{};
        std::size_t support_count = 0u;
        auto accumulate_rest_support = [&](std::size_t node)
        {
            if (node >= blueprint_.nodes.size() || node >= particles_.size())
                return;
            rest_support += blueprint_.nodes[node];
            ++support_count;
        };
        accumulate_rest_support(blueprint_.left_contact_node);
        accumulate_rest_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate_rest_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate_rest_support(node);
        if (support_count == 0u)
            return;
        rest_support /= static_cast<float>(support_count);

        const float press_anchor_x = blueprint_.root_node < blueprint_.nodes.size()
            ? blueprint_.nodes[blueprint_.root_node].x : 0.0f;
        const float rest_head_top = ground_height_at(press_anchor_x)
            + authored_standing_head_clearance();
        const float moving_crouch_ratio = clamp(elapsed_seconds_ / 1.50f,
            0.0f, 1.0f);
        const float moving_crouch_blend = moving_crouch_ratio
            * moving_crouch_ratio * (3.0f - 2.0f * moving_crouch_ratio);
        const DuckPressProfile profile = moving_crouch
            ? DuckPressProfile{ rest_head_top - 0.72f * moving_crouch_blend, 0.0f,
                false, true, false }
            : duck_press_profile(elapsed_seconds_, course_difficulty_, rest_head_top,
                blueprint_.horizontal_multi_support_plan());
        const float rest_height = std::max(0.65f, rest_head_top - rest_support.y);
        const float requested_drop = clamp(rest_head_top - profile.bottom_y,
            0.0f, rest_height * 0.48f);
        const bool recovery_guide = !moving_crouch && duck_press_contact_seen_
            && requested_drop <= 0.001f;
        const bool settle_guide = !moving_crouch && !duck_press_contact_seen_
            && requested_drop <= 0.001f;
        const float phase_strength = moving_crouch ? 1.0f
            : (recovery_guide || settle_guide)
            ? 1.0f : clamp(requested_drop / 0.48f, 0.0f, 1.0f);

        Vec2 current_support{};
        auto accumulate_current_support = [&](std::size_t node)
        {
            if (node < particles_.size())
                current_support += particles_[node].position;
        };
        accumulate_current_support(blueprint_.left_contact_node);
        accumulate_current_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate_current_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate_current_support(node);
        current_support /= static_cast<float>(support_count);

        if (blueprint_.paired_leg_chains())
        {
            const std::uint16_t left_knee = blueprint_.motors[1].pivot;
            const std::uint16_t right_knee = blueprint_.motors[3].pivot;
            const std::uint16_t left_ankle = blueprint_.motors[1].c;
            const std::uint16_t right_ankle = blueprint_.motors[3].c;
            const float guide_strength = moving_crouch ? 0.055f
                : recovery_guide ? 0.88f
                : 0.28f + phase_strength * 0.36f;

            for (std::size_t node = 0; node < particles_.size(); ++node)
            {
                if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                    continue;
                Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
                rest_offset.x *= facing_direction();
                Vec2 target = current_support + rest_offset;
                if (node == left_knee || node == right_knee)
                {
                    target.y -= requested_drop * 0.48f;
                    // Both knees flex toward the current facing direction in a
                    // sagittal side view. Opposing knee signs created the crab
                    // stance even though the support semantics were correct.
                    target.x += facing_direction() * requested_drop * 0.22f;
                }
                else if (node == left_ankle || node == right_ankle)
                {
                    target.y -= requested_drop * 0.04f;
                }
                else
                {
                    // Translate the pelvis and complete upper body downward as
                    // one unit. This preserves the authored torso axis instead
                    // of shrinking it into the forward bow seen in v0.7.14.
                    target.y -= requested_drop;
                }
                const float floor = ground_height_at(target.x)
                    + particles_[node].radius + 0.08f;
                target.y = std::max(target.y, floor);
                if (!recovery_guide && node == blueprint_.head_node)
                    target.y = std::min(target.y,
                        profile.bottom_y - particles_[node].radius - 0.035f);

                Vec2 correction = target - particles_[node].position;
                const float magnitude = length(correction);
                const float maximum_step = moving_crouch ? 0.040f
                    : recovery_guide ? 0.32f : 0.22f;
                if (magnitude > maximum_step && magnitude > 1.0e-6f)
                    correction *= maximum_step / magnitude;
                const Vec2 applied = correction * guide_strength;
                particles_[node].position += applied;
                particles_[node].previous += applied * 0.94f;
            }
            return;
        }

        const bool horizontal_body = blueprint_.horizontal_multi_support_plan();
        const float minimum_vertical_scale = horizontal_body ? 0.82f : 0.58f;
        const float vertical_scale = clamp(
            (rest_height - requested_drop) / rest_height,
            minimum_vertical_scale, 1.0f);
        // A quadruped crouches by flexing four support chains, not by widening
        // and flattening the entire body into the floor.
        const float horizontal_scale = horizontal_body
            ? 1.0f : 1.0f + (1.0f - vertical_scale) * 0.08f;
        for (std::size_t node = 0; node < particles_.size(); ++node)
        {
            if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                continue;
            Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
            rest_offset.x *= facing_direction();
            Vec2 target = current_support + Vec2{
                rest_offset.x * horizontal_scale,
                rest_offset.y * vertical_scale
            };
            const float floor = ground_height_at(target.x)
                + particles_[node].radius + 0.14f;
            if (!recovery_guide)
                target.y = std::min(target.y,
                    profile.bottom_y - particles_[node].radius - 0.035f);
            target.y = std::max(target.y, floor);
            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            const float maximum_step = horizontal_body ? 0.080f : 0.055f;
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const float guide_strength = recovery_guide
                ? (horizontal_body ? 0.82f : 0.48f)
                : (horizontal_body ? 0.55f : 0.32f) * phase_strength;
            const Vec2 applied = correction * guide_strength;
            particles_[node].position += applied;
            particles_[node].previous += applied * 0.94f;
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

        if (!direct_bone(blueprint_, heel, ball)
            || !direct_bone(blueprint_, ball, toe))
            return false;

        motor = MotorConstraint{ heel, ball, toe };
        motor.neutral_angle = signed_angle(
            blueprint_.nodes[heel] - blueprint_.nodes[ball],
            blueprint_.nodes[toe] - blueprint_.nodes[ball]);
        motor.minimum_angle = motor.neutral_angle - degrees_to_radians(42.0f);
        motor.maximum_angle = motor.neutral_angle + degrees_to_radians(36.0f);
        motor.strength = 0.022f;
        motor.enabled = true;
        return true;
    }

    void Environment::update_articulated_toe_commands(
        std::span<const float, anatomy_action_count> actions, float dt) noexcept
    {
        if (blueprint_.active_motor_count > human_right_toe_action
            && blueprint_.motors[human_left_toe_action].enabled
            && blueprint_.motors[human_right_toe_action].enabled)
        {
            // RUNRIG7 toes are ordinary policy motors. The legacy inferred
            // tendon must be exactly dormant so it cannot add a second hidden
            // source of work to those same physical hinges.
            articulated_toe_commands_.fill(0.0f);
            return;
        }
        const float guide_authority = course_stage_ == CourseStage::crouch_walk
            ? effective_physical_guidance_authority() : 1.0f;
        const bool raw = raw_policy_audit() || guide_authority <= 0.0f;
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
            const bool supported = !raw && contact_supported(heel);
            const float hip = hip_index < blueprint_.active_motor_count
                ? actions[hip_index] : 0.0f;
            const float knee = knee_index < blueprint_.active_motor_count
                ? actions[knee_index] : 0.0f;
            const float chain_effort = clamp(
                0.5f * (std::abs(hip) + std::abs(knee)), 0.0f, 1.0f);

            float desired = 0.0f;
            if (raw)
            {
                // Human exposes no dedicated toe action. Any inferred
                // contact-phase tendon would therefore be a hidden actuator,
                // even if correlated with a hip/knee command. Raw evaluation
                // leaves this hinge passive; only the ten explicit anatomy
                // actions may do work.
                articulated_toe_commands_[side] = 0.0f;
                return;
            }
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
            desired *= guide_authority;
            articulated_toe_commands_[side] = rate_limited_toe_command(
                articulated_toe_commands_[side],
                clamp(desired, -0.90f, 0.80f), dt, supported, course_stage_);
        };

        update_side(true, 0u, 0u, 1u);
        update_side(false, 1u, 2u, 3u);
        if (raw_authority_telemetry_enabled_ && raw)
        {
            for (const float command : articulated_toe_commands_)
            {
                if (std::abs(command) > 1.0e-7f)
                    ++raw_authority_telemetry_.powered_toe_commands;
            }
        }
    }

    void Environment::solve_articulated_toes() noexcept
    {
        if (blueprint_.active_motor_count > human_right_toe_action
            && blueprint_.motors[human_left_toe_action].enabled
            && blueprint_.motors[human_right_toe_action].enabled)
            return;
        for (std::size_t side = 0; side < articulated_toe_commands_.size(); ++side)
        {
            const float guide_authority = course_stage_ == CourseStage::crouch_walk
                ? effective_physical_guidance_authority() : 1.0f;
            // In raw mode a neutral derived command is a passive hinge, not a
            // hidden neutral-angle servo. Nonzero tendon work must trace back
            // to current or rate-limited policy leg output.
            if ((raw_policy_audit() || guide_authority <= 0.0f)
                && articulated_toe_commands_[side] == 0.0f)
                continue;
            MotorConstraint toe_motor{};
            if (articulated_toe_motor(side == 0u, toe_motor))
            {
                toe_motor.strength *= guide_authority;
                solve_motor(toe_motor, articulated_toe_commands_[side]);
            }
        }
    }

    void Environment::solve_passive_articulated_toe_stop(bool left) noexcept
    {
        MotorConstraint toe_motor{};
        if (!articulated_toe_motor(left, toe_motor))
            return;
        // Converge the nonlinear angle constraint locally once at the end of
        // the physical step. Reapplying this hard stop after every motor pass
        // would multiply its authority by the solver iteration count.
        constexpr int passive_limit_passes = 8;
        for (int pass = 0; pass < passive_limit_passes; ++pass)
        {
            Particle& parent = particles_[toe_motor.a];
            Particle& pivot = particles_[toe_motor.pivot];
            Particle& driven = particles_[toe_motor.c];
            const Vec2 parent_arm = parent.position - pivot.position;
            const Vec2 driven_arm = driven.position - pivot.position;
            const float parent_length_squared = length_squared(parent_arm);
            const float driven_length_squared = length_squared(driven_arm);
            if (parent_length_squared <= 1.0e-8f
                || driven_length_squared <= 1.0e-8f)
                return;

            const float facing = facing_direction();
            const float current_local = signed_angle(
                parent_arm, driven_arm) * facing;
            const float deviation = wrap_angle(
                current_local - toe_motor.neutral_angle);
            const float minimum_deviation = toe_motor.minimum_angle
                - toe_motor.neutral_angle;
            const float maximum_deviation = toe_motor.maximum_angle
                - toe_motor.neutral_angle;
            const float bounded_deviation = clamp(deviation,
                minimum_deviation, maximum_deviation);
            const float constraint_error = deviation - bounded_deviation;
            if (std::abs(constraint_error) <= 1.0e-7f)
                return;

            // This is a unilateral passive joint stop, not a neutral-angle
            // servo: it is exactly dormant throughout the anatomical range.
            // Outside the range, the three-particle angular-constraint
            // gradients apply equal and opposite impulses through the real
            // heel-ball-toe hinge. Their sum is zero, so the stop cannot
            // translate the rig or inject a hidden root/contact command.
            const Vec2 parent_gradient = -perpendicular(parent_arm)
                / parent_length_squared * facing;
            const Vec2 driven_gradient = perpendicular(driven_arm)
                / driven_length_squared * facing;
            const Vec2 pivot_gradient = -(parent_gradient + driven_gradient);
            const float inverse_effective_inertia =
                parent.inverse_mass * length_squared(parent_gradient)
                + pivot.inverse_mass * length_squared(pivot_gradient)
                + driven.inverse_mass * length_squared(driven_gradient);
            if (inverse_effective_inertia <= 1.0e-8f)
                return;

            const float lambda = -constraint_error
                / inverse_effective_inertia;
            parent.position += parent_gradient
                * (parent.inverse_mass * lambda);
            pivot.position += pivot_gradient
                * (pivot.inverse_mass * lambda);
            driven.position += driven_gradient
                * (driven.inverse_mass * lambda);
        }
    }

    void Environment::damp_passive_articulated_toe_velocity(
        bool left, float dt) noexcept
    {
        if (!(dt > 0.0f))
            return;
        MotorConstraint toe_motor{};
        if (!articulated_toe_motor(left, toe_motor))
            return;

        Particle& parent = particles_[toe_motor.a];
        Particle& pivot = particles_[toe_motor.pivot];
        Particle& driven = particles_[toe_motor.c];
        const Vec2 parent_arm = parent.position - pivot.position;
        const Vec2 driven_arm = driven.position - pivot.position;
        const float parent_length_squared = length_squared(parent_arm);
        const float driven_length_squared = length_squared(driven_arm);
        if (parent_length_squared <= 1.0e-8f
            || driven_length_squared <= 1.0e-8f)
            return;

        const float facing = facing_direction();
        const Vec2 parent_gradient = -perpendicular(parent_arm)
            / parent_length_squared * facing;
        const Vec2 driven_gradient = perpendicular(driven_arm)
            / driven_length_squared * facing;
        const Vec2 pivot_gradient = -(parent_gradient + driven_gradient);
        const float inverse_effective_inertia =
            parent.inverse_mass * length_squared(parent_gradient)
            + pivot.inverse_mass * length_squared(pivot_gradient)
            + driven.inverse_mass * length_squared(driven_gradient);
        if (inverse_effective_inertia <= 1.0e-8f)
            return;

        const Vec2 parent_velocity = (parent.position - parent.previous) / dt;
        const Vec2 pivot_velocity = (pivot.position - pivot.previous) / dt;
        const Vec2 driven_velocity = (driven.position - driven.previous) / dt;
        const float relative_angular_velocity =
            dot(parent_gradient, parent_velocity)
            + dot(pivot_gradient, pivot_velocity)
            + dot(driven_gradient, driven_velocity);
        // A reciprocal viscous hinge removes relative MTP kinetic energy but
        // has no preferred angle and is exactly dormant at zero relative rate.
        // Applying the equal-and-opposite generalized impulse through Verlet
        // history leaves positions/contact membership untouched and preserves
        // linear momentum because the three gradients sum to zero.
        constexpr float passive_velocity_reduction = 0.70f;
        const float impulse = -passive_velocity_reduction
            * relative_angular_velocity / inverse_effective_inertia;
        parent.previous -= parent_gradient
            * (parent.inverse_mass * impulse * dt);
        pivot.previous -= pivot_gradient
            * (pivot.inverse_mass * impulse * dt);
        driven.previous -= driven_gradient
            * (driven.inverse_mass * impulse * dt);
    }

    void Environment::limit_articulated_toe_rates(float dt) noexcept
    {
        if (blueprint_.active_motor_count > human_right_toe_action
            && blueprint_.motors[human_left_toe_action].enabled
            && blueprint_.motors[human_right_toe_action].enabled)
            return;
        for (std::size_t side = 0; side < previous_articulated_toe_angles_.size(); ++side)
        {
            const bool left = side == 0u;
            MotorConstraint toe_motor{};
            if (!articulated_toe_motor(left, toe_motor))
                continue;
            const float guide_authority = course_stage_ == CourseStage::crouch_walk
                ? effective_physical_guidance_authority() : 1.0f;
            if (raw_policy_audit() || guide_authority <= 0.0f)
            {
                // Raw toes are an unpowered distance/contact hinge. The
                // assisted rate limiter below rotates only the distal node and
                // is therefore an authored projector, not reciprocal passive
                // physics. Never invoke it in the raw authority path.
                previous_articulated_toe_angles_[side] =
                    joint_angle(toe_motor);
                continue;
            }
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
            const float correction = wrap_angle(bounded - current)
                * guide_authority;
            if (std::abs(correction) > 1.0e-6f
                && toe_motor.pivot < particles_.size()
                && toe_motor.c < particles_.size())
            {
                Particle& toe = particles_[toe_motor.c];
                const Vec2 pivot = particles_[toe_motor.pivot].position;
                const Vec2 rotated = rotate(toe.position - pivot, correction);
                const float rest_length = length(
                    blueprint_.nodes[toe_motor.c]
                        - blueprint_.nodes[toe_motor.pivot]);
                Vec2 direction = normalized(rotated,
                    normalized(blueprint_.nodes[toe_motor.c]
                        - blueprint_.nodes[toe_motor.pivot],
                        { facing_direction(), 0.0f }));
                Vec2 corrected = pivot + direction * rest_length;
                const Vec2 translation = corrected - toe.position;
                toe.position = corrected;
                toe.previous += translation;

                const float minimum_y = ground_height_at(toe.position.x)
                    + ground_contact_offset(true, toe.radius);
                if (toe.position.y < minimum_y)
                {
                    const float bounded_y = std::min(minimum_y,
                        pivot.y + rest_length);
                    const float dy = clamp(bounded_y - pivot.y,
                        -rest_length, rest_length);
                    const float dx = std::sqrt(std::max(0.0f,
                        rest_length * rest_length - dy * dy));
                    const float horizontal_sign = std::abs(direction.x) > 1.0e-5f
                        ? (direction.x < 0.0f ? -1.0f : 1.0f)
                        : facing_direction();
                    corrected = { pivot.x + horizontal_sign * dx,
                        pivot.y + dy };
                    const Vec2 floor_translation = corrected - toe.position;
                    toe.position = corrected;
                    toe.previous += floor_translation;
                }
                const float resolved_floor = ground_height_at(toe.position.x)
                    + ground_contact_offset(true, toe.radius);
                toe.grounded = toe.position.y <= resolved_floor + 0.0025f;
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
        const float target = motor_target_angle(motor, action)
            * facing_direction();
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

    void Environment::solve_powered_motor_xpbd(
        const MotorConstraint& motor, float action, float previous_action,
        float dt,
        float& accumulated_lambda, float& positive_work_used,
        std::size_t action_slot) noexcept
    {
        if (!motor.enabled || motor.strength <= 1.0e-6f
            || motor.a >= particles_.size()
            || motor.pivot >= particles_.size() || motor.c >= particles_.size()
            || dt <= 1.0e-6f)
            return;

        Particle& parent = particles_[motor.a];
        Particle& pivot = particles_[motor.pivot];
        Particle& driven = particles_[motor.c];
        const Vec2 parent_arm = parent.position - pivot.position;
        const Vec2 driven_arm = driven.position - pivot.position;
        const float parent_length_squared = length_squared(parent_arm);
        const float driven_length_squared = length_squared(driven_arm);
        if (parent_length_squared <= 1.0e-8f
            || driven_length_squared <= 1.0e-8f)
            return;

        const float facing = facing_direction();
        const Vec2 parent_gradient = -perpendicular(parent_arm)
            / parent_length_squared * facing;
        const Vec2 driven_gradient = perpendicular(driven_arm)
            / driven_length_squared * facing;
        const Vec2 pivot_gradient = -(parent_gradient + driven_gradient);
        const std::array<Vec2, 3> gradients{
            parent_gradient, pivot_gradient, driven_gradient };
        const std::array<Particle*, 3> joint{ &parent, &pivot, &driven };

        float inverse_effective_inertia{};
        float angle_rate{};
        std::array<Vec2, 3> velocities{};
        for (std::size_t index = 0; index < joint.size(); ++index)
        {
            velocities[index] = (joint[index]->position
                - joint[index]->previous) / dt;
            inverse_effective_inertia += joint[index]->inverse_mass
                * length_squared(gradients[index]);
            angle_rate += dot(gradients[index], velocities[index]);
        }
        if (inverse_effective_inertia <= 1.0e-8f)
            return;

        float total_mass{};
        for (const Particle& particle : particles_)
            total_mass += 1.0f / std::max(particle.inverse_mass, 1.0e-5f);
        const float strength_fraction = clamp(motor.strength / 0.056f,
            0.0f, 1.0f);
        constexpr float gravity_magnitude = 22.0f;
        const float parent_length = std::sqrt(parent_length_squared);
        const float driven_length = std::sqrt(driven_length_squared);
        // Joint torque capacity belongs to the articulated hinge, not only to
        // whichever side happens to be stored as `c`. Using the short distal
        // arm made an anatomical ankle roughly six times weaker than the same
        // finite actuator at a hip or knee. The longer of the two real arms is
        // a symmetric morphology scale; all torque, impulse and work ceilings
        // below remain unchanged and finite.
        const float actuator_length = std::max(parent_length, driven_length);
        const float maximum_torque = total_mass * gravity_magnitude
            * actuator_length * (0.10f + 0.30f * strength_fraction);
        // Travel defines what a joint may request; it must not also make the
        // same finite actuator softer. A physical position-controlled hinge
        // reaches its declared torque limit at a bounded tracking error,
        // independently of whether its anatomical range is 34 or 76 degrees.
        // Torque, lambda and positive-work limits below remain unchanged.
        constexpr float full_torque_tracking_error = 0.250f;
        const float stiffness = maximum_torque
            / full_torque_tracking_error;
        const float compliance = 1.0f / std::max(stiffness, 1.0e-5f);
        const float alpha = compliance / (dt * dt);
        const float current_local = signed_angle(parent_arm, driven_arm) * facing;
        const float target_local = motor_target_angle(motor, action);
        const float constraint_error = wrap_angle(current_local - target_local);
        float damping_seconds{};
        float target_angle_rate{};
        if (action_slot == human_left_toe_action
            || action_slot == human_right_toe_action)
        {
            // The explicit MTP motor is a finite reciprocal PD actuator. Its
            // derivative term tracks the moving applied target, rather than
            // resisting authored slew, and is included in the XPBD denominator
            // so repeated solver passes converge instead of multiplying drag.
            constexpr float damping_ratio = 0.35f;
            damping_seconds = 2.0f * damping_ratio
                / std::sqrt(std::max(stiffness * inverse_effective_inertia,
                    1.0e-8f));
            target_angle_rate = wrap_angle(target_local
                    - motor_target_angle(motor, previous_action)) / dt;
        }
        const float relative_angle_rate = angle_rate - target_angle_rate;
        float delta_lambda = (-constraint_error - alpha * accumulated_lambda
                - damping_seconds * relative_angle_rate)
            / (inverse_effective_inertia + alpha
                + damping_seconds * inverse_effective_inertia / dt);

        const float maximum_lambda = maximum_torque * dt * dt;
        const float bounded_lambda = clamp(accumulated_lambda + delta_lambda,
            -maximum_lambda, maximum_lambda);
        delta_lambda = bounded_lambda - accumulated_lambda;
        if (std::abs(delta_lambda) <= 1.0e-10f)
            return;

        // A position-level XPBD correction is a real motor impulse because
        // Verlet history is intentionally unchanged. Bound its measured
        // positive work once per fixed step, independently of solver count.
        const float impulse = delta_lambda / dt;
        const float requested_work = impulse * angle_rate
            + 0.5f * impulse * impulse * inverse_effective_inertia;
        const float maximum_positive_work = maximum_torque * 2.0f * dt;
        const float remaining_positive_work = std::max(0.0f,
            maximum_positive_work - positive_work_used);
        if (requested_work > remaining_positive_work
            && std::abs(impulse) > 1.0e-9f)
        {
            const float impulse_sign = impulse < 0.0f ? -1.0f : 1.0f;
            const float signed_rate = impulse_sign * angle_rate;
            const float discriminant = signed_rate * signed_rate
                + 2.0f * inverse_effective_inertia
                    * remaining_positive_work;
            const float bounded_magnitude = std::max(0.0f,
                (-signed_rate + std::sqrt(std::max(0.0f, discriminant)))
                    / inverse_effective_inertia);
            delta_lambda = impulse_sign * std::min(
                std::abs(impulse), bounded_magnitude) * dt;
        }
        if (std::abs(delta_lambda) <= 1.0e-10f)
            return;

        Vec2 linear_before{};
        Vec2 linear_after{};
        double angular_before{};
        double angular_after{};
        double kinetic_before{};
        double kinetic_after{};
        for (std::size_t index = 0; index < joint.size(); ++index)
        {
            const float mass = 1.0f / std::max(
                joint[index]->inverse_mass, 1.0e-5f);
            const Vec2 displacement = gradients[index]
                * (joint[index]->inverse_mass * delta_lambda);
            const Vec2 resolved_velocity = velocities[index]
                + displacement / dt;
            linear_before += velocities[index] * mass;
            linear_after += resolved_velocity * mass;
            angular_before += static_cast<double>(mass) * static_cast<double>(
                cross(joint[index]->position, velocities[index]));
            kinetic_before += 0.5 * static_cast<double>(mass)
                * static_cast<double>(length_squared(velocities[index]));
            joint[index]->position += displacement;
            angular_after += static_cast<double>(mass) * static_cast<double>(
                cross(joint[index]->position, resolved_velocity));
            kinetic_after += 0.5 * static_cast<double>(mass)
                * static_cast<double>(length_squared(resolved_velocity));
        }
        accumulated_lambda += delta_lambda;

        const float work = static_cast<float>(kinetic_after - kinetic_before);
        positive_work_used += std::max(0.0f, work);
        if (raw_authority_telemetry_enabled_
            && action_slot < anatomy_action_count)
        {
            raw_authority_telemetry_.motor_torque[action_slot] =
                accumulated_lambda / (dt * dt);
            raw_authority_telemetry_.motor_work_joules[action_slot] += work;
            raw_authority_telemetry_.motor_positive_work_joules[action_slot]
                += std::max(0.0f, work);
            raw_authority_telemetry_.motor_maximum_linear_momentum_error =
                std::max(raw_authority_telemetry_
                        .motor_maximum_linear_momentum_error,
                    length(linear_after - linear_before));
            raw_authority_telemetry_.motor_maximum_angular_momentum_error =
                std::max(raw_authority_telemetry_
                        .motor_maximum_angular_momentum_error,
                    std::abs(static_cast<float>(angular_after
                        - angular_before)));
        }
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

    std::size_t Environment::support_seed_grounded_count(bool left) const noexcept
    {
        std::size_t count{};
        const auto count_node = [this, &count](std::uint16_t node)
        {
            if (valid_node(node) && particles_[node].grounded)
                ++count;
        };
        count_node(left ? blueprint_.left_contact_node : blueprint_.right_contact_node);
        const auto& additional = left ? blueprint_.additional_left_contact_nodes
                                      : blueprint_.additional_right_contact_nodes;
        for (const std::uint16_t node : additional)
            count_node(node);
        return count;
    }

    std::size_t Environment::support_seed_lifted_count(float minimum_clearance) const noexcept
    {
        std::size_t count{};
        const auto count_node = [this, minimum_clearance, &count](std::uint16_t node)
        {
            if (!valid_node(node))
                return;
            const Particle& particle = particles_[node];
            const float clearance = particle.position.y - particle.radius
                - ground_height_at(particle.position.x);
            if (!particle.grounded && clearance >= minimum_clearance)
                ++count;
        };
        count_node(blueprint_.left_contact_node);
        count_node(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            count_node(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            count_node(node);
        return count;
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
            if (!motor.enabled || blueprint_.support_branch_mask(motor) != 0u)
                continue;
            const float local_angle = joint_angle(motor) * facing_direction();
            maximum = std::max(maximum,
                std::abs(wrap_angle(local_angle - motor.neutral_angle)));
        }
        return maximum;
    }

    CrouchPostureEvidence Environment::current_crouch_posture() const noexcept
    {
        CrouchPostureEvidence evidence{};
        evidence.paired_leg_chains = blueprint_.paired_leg_chains();
        evidence.horizontal_body = blueprint_.horizontal_multi_support_plan();
        const bool left = left_supported();
        const bool right = right_supported();
        evidence.left_supported = left;
        evidence.right_supported = right;
        evidence.feet_supported = evidence.paired_leg_chains
            ? left && right : left || right;
        evidence.non_foot_grounded = non_foot_ground_contact();
        evidence.torso_pitch = std::abs(torso_roll_angle());

        Vec2 rest_support{};
        Vec2 current_support{};
        std::size_t support_count = 0u;
        float minimum_support_x = std::numeric_limits<float>::infinity();
        float maximum_support_x = -std::numeric_limits<float>::infinity();
        auto accumulate = [&](std::size_t node)
        {
            if (node >= blueprint_.nodes.size() || node >= particles_.size())
                return;
            rest_support += blueprint_.nodes[node];
            current_support += particles_[node].position;
            minimum_support_x = std::min(minimum_support_x,
                particles_[node].position.x - particles_[node].radius);
            maximum_support_x = std::max(maximum_support_x,
                particles_[node].position.x + particles_[node].radius);
            ++support_count;
        };
        accumulate(blueprint_.left_contact_node);
        accumulate(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate(node);
        if (support_count > 0u && valid_node(blueprint_.root_node))
        {
            rest_support /= static_cast<float>(support_count);
            current_support /= static_cast<float>(support_count);
            const float rest_height = blueprint_.nodes[blueprint_.root_node].y
                - rest_support.y;
            const float current_height = particles_[blueprint_.root_node].position.y
                - current_support.y;
            evidence.pelvis_drop = std::max(0.0f, rest_height - current_height);

            double weighted_x = 0.0;
            double total_mass = 0.0;
            for (const Particle& particle : particles_)
            {
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                total_mass += mass;
            }
            if (total_mass > 1.0e-9
                && std::isfinite(minimum_support_x)
                && std::isfinite(maximum_support_x))
            {
                const float center_of_mass_x = static_cast<float>(weighted_x / total_mass);
                evidence.support_margin = std::min(
                    center_of_mass_x - minimum_support_x,
                    maximum_support_x - center_of_mass_x);
            }
        }

        if (evidence.paired_leg_chains)
        {
            const MotorConstraint& left_knee = blueprint_.motors[1];
            const MotorConstraint& right_knee = blueprint_.motors[3];
            // Flexion evidence is a magnitude in the local joint frame. The old
            // left-positive/right-negative convention encoded a single mirrored
            // rest drawing and rejected the same-facing sagittal crouch.
            evidence.left_knee_flex = std::abs(wrap_angle(
                joint_angle(left_knee) - left_knee.neutral_angle));
            evidence.right_knee_flex = std::abs(wrap_angle(
                joint_angle(right_knee) - right_knee.neutral_angle));
        }
        else
        {
            evidence.left_knee_flex = evidence.pelvis_drop;
            evidence.right_knee_flex = evidence.pelvis_drop;
        }
        return evidence;
    }

    float Environment::torso_roll_angle() const noexcept
    {
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node))
            return 0.0f;
        const Vec2 current = particles_[blueprint_.torso_node].position
            - particles_[blueprint_.root_node].position;
        Vec2 authored = blueprint_.nodes[blueprint_.torso_node]
            - blueprint_.nodes[blueprint_.root_node];
        authored.x *= facing_direction();
        const Vec2 desired = blueprint_.horizontal_body_plan()
            ? authored : Vec2{ 0.0f, std::max(0.01f, length(authored)) };
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

    float Environment::contact_cluster_center_x(
        std::uint16_t contact_node) const noexcept
    {
        float accumulated = 0.0f;
        std::size_t count = 0u;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!contact_cluster_contains(contact_node, index))
                continue;
            accumulated += particles_[index].position.x;
            ++count;
        }
        return count == 0u ? 0.0f : accumulated / static_cast<float>(count);
    }

    FootContactPhase Environment::detect_foot_contact_phase(bool left) const noexcept
    {
        const std::uint16_t heel = left
            ? blueprint_.left_contact_node : blueprint_.right_contact_node;
        const auto& extra = left
            ? blueprint_.additional_left_contact_nodes
            : blueprint_.additional_right_contact_nodes;
        const bool heel_grounded = valid_node(heel) && particles_[heel].grounded;
        if (extra.size() < 2u)
            return heel_grounded ? FootContactPhase::flat : FootContactPhase::airborne;
        const std::uint16_t ball = extra[0];
        const std::uint16_t toe = extra[1];
        const bool ball_grounded = valid_node(ball) && particles_[ball].grounded;
        const bool toe_grounded = valid_node(toe) && particles_[toe].grounded;
        return classify_foot_contact_phase(
            heel_grounded, ball_grounded, toe_grounded);
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
        const float requested = locomotion_direction();
        const float direction = std::abs(requested) >= 0.5f
            ? (requested < 0.0f ? -1.0f : 1.0f)
            : (facing_direction() < 0.0f ? -1.0f : 1.0f);
        for (std::size_t side = 0; side < knee_motors.size(); ++side)
        {
            const MotorConstraint& knee_motor = blueprint_.motors[knee_motors[side]];
            if (!knee_motor.enabled || !valid_node(knee_motor.pivot))
                continue;
            const std::uint16_t foot = left_side[side]
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const Particle& knee = particles_[knee_motor.pivot];
            const float knee_front = (knee.position.x
                + knee.radius * direction) * direction;
            float foot_front = -std::numeric_limits<float>::infinity();
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                if (!contact_cluster_contains(foot, index))
                    continue;
                const Particle& item = particles_[index];
                foot_front = std::max(foot_front,
                    (item.position.x + item.radius * direction) * direction);
            }
            const float foot_top = contact_cluster_top_y(foot);
            for (const CourseFeature& feature : course_features_)
            {
                CourseFeature oriented = feature;
                oriented.center.x *= direction;
                oriented.velocity.x *= direction;
                if (knee_crosses_before_foot(
                        knee_front, foot_front, foot_top, oriented))
                    return true;
            }
        }
        return false;
    }

    bool Environment::solve_raw_support_tangent_manifold(float dt) noexcept
    {
        if (!blueprint_.human_casual_gait_plan() || dt <= 1.0e-6f)
            return false;

        struct ContactState
        {
            std::size_t node{};
            std::size_t side{};
            bool toe{};
            float mass{};
            float velocity{};
            float selected_capacity{};
            float old_constraint_impulse{};
            float new_constraint_impulse{};
            float stick_displacement{};
            bool sticking{};
        };
        std::array<ContactState, human_support_contact_nodes.size()> contacts{};
        std::size_t contact_count{};
        std::array<std::uint8_t, 2> plate_membership{};
        std::array<float, 2> plate_constraint_impulse{};
        std::array<float, 2> plate_capacity{};
        std::array<float, 2> toe_capacity{};
        std::array<float, 2> traction_used{};
        std::array<float, 2> current_tangent{};

        const auto support_slot = [](std::size_t node) noexcept
        {
            for (std::size_t slot = 0;
                slot < human_support_contact_nodes.size(); ++slot)
            {
                if (human_support_contact_nodes[slot] == node)
                    return slot;
            }
            return human_support_contact_nodes.size();
        };
        const auto solve_contact = [](float mass, float velocity,
                                      float old_impulse,
                                      float capacity) noexcept
        {
            const float coulomb_minimum = -capacity - old_impulse;
            const float coulomb_maximum = capacity - old_impulse;
            float minimum_delta{};
            float maximum_delta{};
            constexpr float stationary_velocity = 1.0e-6f;
            if (velocity > stationary_velocity)
            {
                minimum_delta = std::max(0.0f, coulomb_minimum);
                maximum_delta = std::min(coulomb_maximum,
                    2.0f * mass * velocity);
            }
            else if (velocity < -stationary_velocity)
            {
                minimum_delta = std::max(coulomb_minimum,
                    2.0f * mass * velocity);
                maximum_delta = std::min(0.0f, coulomb_maximum);
            }
            if (minimum_delta > maximum_delta)
                return old_impulse;
            return old_impulse + clamp(mass * velocity,
                minimum_delta, maximum_delta);
        };

        // Each real heel, ball, and toe contact owns its relative tangential
        // slip and finite Coulomb interval. Rigid-foot distance constraints
        // exchange the resulting impulses physically; averaging opposed heel
        // and ball velocities here would erase the rolling moment and can
        // accelerate one contact in its current slip direction.
        for (const std::size_t node : human_support_contact_nodes)
        {
            if (node >= particles_.size())
                continue;
            const bool left = contact_cluster_contains(
                blueprint_.left_contact_node, node);
            const bool right = contact_cluster_contains(
                blueprint_.right_contact_node, node);
            if (left == right)
                continue;
            const std::size_t side = left ? 0u : 1u;
            const bool toe = node == (left
                ? human_left_toe_node : human_right_toe_node);
            const bool active = blueprint_.is_support_seed(node)
                && particles_[node].grounded
                && support_contact_latch_[node] != 0u;
            if (!active)
            {
                support_contact_tangent_impulses_[node] = 0.0f;
                continue;
            }

            if (!toe)
            {
                const std::size_t heel = left
                    ? human_left_heel_node : human_right_heel_node;
                plate_membership[side] |= static_cast<std::uint8_t>(
                    node == heel ? 0x1u : 0x2u);
            }
            Particle& particle = particles_[node];
            const float mass = 1.0f / std::max(
                particle.inverse_mass, 1.0e-5f);
            const float velocity = (particle.position.x
                - particle.previous.x) / dt;
            const float old_impulse = support_contact_tangent_impulses_[node];
            const float normal_impulse = std::max(0.0f,
                support_normal_constraint_lambdas_[node] / dt);
            const ContactTraction traction = contact_traction(
                terrain_firmness_at(particle.position.x),
                terrain_looseness_at(particle.position.x),
                water_depth_at(particle.position.x), particle.radius,
                particle.inverse_mass, dt);
            const float static_capacity = traction.static_coefficient
                * normal_impulse;
            const float dynamic_capacity = traction.dynamic_coefficient
                * normal_impulse;
            const float displacement = particle.position.x
                - support_contact_anchor_x_[node];
            const float position_impulse = mass * displacement / dt;
            const float velocity_impulse = mass * velocity;
            const float shared_impulse = clamp(position_impulse,
                std::min(0.0f, velocity_impulse), std::max(0.0f, velocity_impulse));
            const float stick_impulse = old_impulse + position_impulse
                + (velocity_impulse - shared_impulse);
            const bool sticking = normal_impulse > 1.0e-7f
                && std::abs(displacement) <= traction.static_capture_distance
                && std::abs(position_impulse)
                    + std::abs(velocity_impulse - shared_impulse) <= static_capacity
                && std::abs(stick_impulse) <= static_capacity;
            const float capacity = sticking ? static_capacity : dynamic_capacity;
            // When a moving foot rolls, the per-pass normal load and its
            // active Coulomb budget can shrink. Discard excess accumulated
            // reservation without undoing already dissipated velocity; that
            // would turn unloading into a propulsive impulse. The remaining
            // budget governs the next physical velocity correction.
            const float active_impulse = !sticking && stage_requires_forward_gait(course_stage_)
                ? clamp(old_impulse, -capacity, capacity) : old_impulse;
            ContactState& contact = contacts[contact_count++];
            contact.node = node;
            contact.side = side;
            contact.toe = toe;
            contact.mass = mass;
            contact.velocity = velocity;
            contact.selected_capacity = capacity;
            contact.old_constraint_impulse = active_impulse;
            contact.new_constraint_impulse = sticking ? stick_impulse
                : solve_contact(mass, velocity, active_impulse, capacity);
            contact.stick_displacement = displacement;
            contact.sticking = sticking;
        }

        support_cluster_plate_membership_mask_ = plate_membership;
        for (std::size_t index = 0; index < contact_count; ++index)
        {
            const ContactState& contact = contacts[index];
            Particle& particle = particles_[contact.node];
            const float delta_constraint = contact.new_constraint_impulse
                - contact.old_constraint_impulse;
            const float velocity_change = contact.sticking
                ? contact.velocity : delta_constraint / contact.mass;
            const float velocity_after = contact.velocity - velocity_change;
            const float physical_impulse = -contact.mass * velocity_change;
            const float work = physical_impulse * 0.5f
                * (contact.velocity + velocity_after);
            if (contact.sticking)
            {
                // The finite static patch opposes solver displacement, not
                // just history velocity. Split geometric correction from the
                // dissipative velocity response so it cannot launch the sole.
                particle.position.x -= contact.stick_displacement;
                particle.previous.x = particle.position.x;
            }
            else
                particle.previous.x += velocity_change * dt;
            support_contact_tangent_impulses_[contact.node] =
                contact.new_constraint_impulse;
            support_traction_capacity_[contact.node] =
                contact.selected_capacity;
            support_traction_used_[contact.node] =
                std::abs(contact.new_constraint_impulse);
            support_friction_work_joules_ += work;
            support_friction_positive_work_joules_ += std::max(0.0f, work);
            stance_slip_distance_ = std::max(stance_slip_distance_,
                std::max(std::abs(particle.position.x
                        - support_contact_anchor_x_[contact.node]),
                    std::abs(velocity_after) * dt));

            const std::size_t side = contact.side;
            current_tangent[side] += physical_impulse;
            traction_used[side] += std::abs(
                contact.new_constraint_impulse);
            if (contact.toe)
            {
                toe_capacity[side] += contact.selected_capacity;
            }
            else
            {
                plate_constraint_impulse[side] +=
                    contact.new_constraint_impulse;
                plate_capacity[side] += contact.selected_capacity;
            }
            if (raw_authority_telemetry_enabled_)
            {
                const std::size_t slot = support_slot(contact.node);
                if (slot < human_support_contact_nodes.size())
                {
                    raw_authority_telemetry_
                        .support_contact_tangential_impulse[slot]
                        += physical_impulse;
                }
            }
        }

        for (std::size_t side = 0; side < 2u; ++side)
        {
            if (stage_requires_forward_gait(course_stage_))
                support_cluster_tangential_impulse_[side] = current_tangent[side];
            else
                support_cluster_tangential_impulse_[side] += current_tangent[side];
            support_cluster_traction_impulse_[side] =
                plate_constraint_impulse[side];
            support_cluster_plate_traction_capacity_[side] = std::max(
                support_cluster_plate_traction_capacity_[side],
                plate_capacity[side]);
            support_cluster_toe_traction_capacity_[side] = std::max(
                support_cluster_toe_traction_capacity_[side],
                toe_capacity[side]);
            support_cluster_traction_capacity_[side] =
                support_cluster_plate_traction_capacity_[side]
                + support_cluster_toe_traction_capacity_[side];
            support_cluster_traction_used_[side] = traction_used[side];
            const float normal_impulse =
                support_cluster_normal_impulse_[side];
            support_cluster_normal_cop_x_[side] =
                normal_impulse > 1.0e-7f
                ? support_cluster_normal_impulse_moment_x_[side]
                    / normal_impulse
                : 0.0f;
        }
        available_traction_ = support_cluster_traction_capacity_[0]
            + support_cluster_traction_capacity_[1];
        used_traction_ = support_cluster_traction_used_[0]
            + support_cluster_traction_used_[1];
        return true;
    }
    void Environment::solve_ground(float dt) noexcept
    {
        const float safe_dt = std::max(dt, 1.0e-5f);
        if (support_contact_latch_.size() != particles_.size())
            support_contact_latch_.assign(particles_.size(), 0u);
        if (support_contact_anchor_x_.size() != particles_.size())
        {
            support_contact_anchor_x_.resize(particles_.size());
            for (std::size_t index = 0; index < particles_.size(); ++index)
                support_contact_anchor_x_[index] = particles_[index].position.x;
        }
        if (support_traction_capacity_.size() != particles_.size())
            support_traction_capacity_.assign(particles_.size(), 0.0f);
        if (support_traction_used_.size() != particles_.size())
            support_traction_used_.assign(particles_.size(), 0.0f);
        if (support_normal_constraint_lambdas_.size() != particles_.size())
            support_normal_constraint_lambdas_.assign(particles_.size(), 0.0f);
        if (support_contact_tangent_impulses_.size() != particles_.size())
            support_contact_tangent_impulses_.assign(particles_.size(), 0.0f);

        const auto raw_support_contact_slot = [](std::size_t node) noexcept
        {
            for (std::size_t slot = 0;
                slot < human_support_contact_nodes.size(); ++slot)
            {
                if (human_support_contact_nodes[slot] == node)
                    return slot;
            }
            return human_support_contact_nodes.size();
        };
        const auto raw_sole_plate_contains = [this](std::size_t side,
            std::size_t node) noexcept
        {
            const std::uint16_t heel = side == 0u
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const auto& additional = side == 0u
                ? blueprint_.additional_left_contact_nodes
                : blueprint_.additional_right_contact_nodes;
            return node == heel || (!additional.empty()
                && node == additional.front());
        };
        const auto raw_articulated_toe = [this](std::size_t node) noexcept
        {
            return (blueprint_.additional_left_contact_nodes.size() >= 2u
                    && node == blueprint_.additional_left_contact_nodes[1])
                || (blueprint_.additional_right_contact_nodes.size() >= 2u
                    && node == blueprint_.additional_right_contact_nodes[1]);
        };

        float root_upward_speed = 0.0f;
        if (valid_node(blueprint_.root_node))
        {
            const Particle& root = particles_[blueprint_.root_node];
            root_upward_speed = (root.position.y - root.previous.y) / safe_dt;
        }
        const bool powered_release = powered_joint_launch(
            course_stage_, root_upward_speed, action_change_energy_);
        const bool static_support = !raw_policy_audit()
            && (course_stage_ == CourseStage::balance
                || course_stage_ == CourseStage::duck_press);
        const bool shared_physical_human_sole =
            blueprint_.human_casual_gait_plan() && !static_support;
        // Moving Human heel/ball/toe contacts are physical unilateral
        // constraints. A gait action may identify the intended swing side for
        // policy telemetry, but it cannot open collision, clear accumulated
        // contact impulses, or hold the opposite sole to a world-space rail.
        // The explicit hip/knee/ankle motors must earn unload by geometrically
        // separating each contact node from the plane.

        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            const bool was_grounded = particle.grounded;
            const Vec2 velocity = (particle.position - particle.previous) / safe_dt;
            particle.grounded = false;
            const bool traction_contact = contact_cluster_contains(
                blueprint_.left_contact_node, index)
                || contact_cluster_contains(blueprint_.right_contact_node, index);
            const bool semantic_support = blueprint_.is_support_seed(index);
            const float firmness = terrain_firmness_at(particle.position.x);
            const float looseness = terrain_looseness_at(particle.position.x);
            const float burial_allowance = stage_uses_deformable_terrain(course_stage_)
                ? (traction_contact ? support_contact_soil_sink(firmness)
                    : std::min(particle.radius * 0.78f,
                        (1.0f - firmness + looseness * 0.45f) * 0.18f))
                : 0.0f;
            float contact_ground = ground_height_at(particle.position.x);
            if (course_stage_ == CourseStage::climb_descent
                && particle.position.x >= ledge_left_edge_
                && particle.position.x <= ledge_left_edge_ + 6.0f
                && particle.position.y - particle.radius
                    >= ledge_top_height_ - 0.35f)
                contact_ground = std::max(contact_ground, ledge_top_height_);
            const float minimum_y = contact_ground
                + ground_contact_offset(traction_contact, particle.radius) - burial_allowance;
            const float separation = particle.position.y - minimum_y;
            const bool in_left_cluster = semantic_support
                && contact_cluster_contains(blueprint_.left_contact_node, index);
            const bool in_right_cluster = semantic_support
                && contact_cluster_contains(blueprint_.right_contact_node, index);
            const bool raw_unilateral_support = shared_physical_human_sole
                && semantic_support && traction_contact;
            constexpr float unilateral_separation_epsilon = 1.0e-5f;
            const bool raw_support_separating = raw_unilateral_support
                && separation >= -unilateral_separation_epsilon
                && velocity.y > moving_contact_release_speed_mps;
            const bool raw_support_clear_of_plane = raw_unilateral_support
                && (raw_support_separating
                    || (separation > 0.0025f
                        && support_normal_constraint_lambdas_[index]
                            <= 1.0e-8f));
            const bool velocity_release_requested =
                support_contact_release_requested(semantic_support,
                    static_support, powered_release, velocity.y);
            const bool release_requested = raw_unilateral_support
                ? raw_support_clear_of_plane
                : velocity_release_requested;
            const bool contact_latched = semantic_support
                && (was_grounded || support_contact_latch_[index] != 0u);
            const bool actual_contact = separation <= 0.0025f
                && !release_requested;
            const bool persistent_contact = planted_contact_persists(
                contact_latched, semantic_support, static_support,
                separation, velocity.y, release_requested);
            if (actual_contact || persistent_contact)
            {
                if (raw_authority_telemetry_enabled_)
                {
                    if (semantic_support)
                        ++raw_authority_telemetry_.support_contact_corrections;
                    else
                        ++raw_authority_telemetry_.non_support_contact_corrections;
                }
                const bool newly_latched = semantic_support
                    && support_contact_latch_[index] == 0u;
                if (newly_latched)
                    support_contact_anchor_x_[index] = particle.position.x;
                if (semantic_support && static_support)
                {
                    particle.position.x = lerp(particle.position.x,
                        support_contact_anchor_x_[index], 0.72f);
                    particle.position.y = ground_height_at(particle.position.x)
                        + ground_contact_offset(true, particle.radius);
                    particle.previous = particle.position;
                    particle.grounded = true;
                    support_contact_latch_[index] = 1u;
                    continue;
                }

                const bool anchored_support_plant = semantic_support
                    && traction_contact && !static_support
                    && !release_requested;
                float resolved_minimum_y = minimum_y;
                bool static_traction = false;
                int raw_sole = -1;
                bool raw_toe_contact = false;
                float remaining_contact_budget{};
                bool position_capture_complete{};
                if (anchored_support_plant)
                {
                    const float contact_water_depth = water_depth_at(
                        particle.position.x);
                    const int raw_side = in_left_cluster ? 0
                        : in_right_cluster ? 1 : -1;
                    const bool shared_raw_plate = shared_physical_human_sole
                        && raw_side >= 0
                        && raw_sole_plate_contains(
                            static_cast<std::size_t>(raw_side), index);
                    raw_toe_contact = shared_physical_human_sole
                        && raw_articulated_toe(index);
                    raw_sole = shared_raw_plate ? raw_side : -1;
                    if (shared_raw_plate || raw_toe_contact)
                    {
                        // Raw Human heel+ball contacts form one rear-foot plate;
                        // the passive toe is its own unilateral contact. Both
                        // share position and velocity traction within a finite
                        // Coulomb budget in the manifold solve below.
                        resolved_minimum_y = minimum_y;
                    }
                    else
                    {
                    const float contact_radius = particle.radius;
                    const float contact_inverse_mass = particle.inverse_mass;
                    const ContactTraction traction = contact_traction(
                        firmness, looseness, contact_water_depth,
                        contact_radius, contact_inverse_mass, safe_dt);
                    const float slip = particle.position.x
                        - support_contact_anchor_x_[index];
                    const bool inside_static_capture = std::abs(slip)
                        <= traction.static_capture_distance;
                    const TerrainRegion contact_region = terrain_region_at(
                        particle.position.x);
                    const bool yielding_contact =
                        contact_region == TerrainRegion::waterlogged
                        || contact_region == TerrainRegion::shallow_water;
                    const float material_traction_scale = yielding_contact
                        ? 4.0f : 1.0f;
                    float morphology_bearing_scale =
                        blueprint_.human_casual_gait_plan()
                            ? yielding_contact ? 3.0f : 7.0f
                        : blueprint_.monopedal_gait() ? 3.0f
                        : yielding_contact && blueprint_.support_seed_count() >= 6u
                            ? 1.5f : 1.0f;
                    const float morphology_traction_scale = material_traction_scale
                        * morphology_bearing_scale;
                    const float raw_correction_budget = (inside_static_capture
                        ? traction.maximum_static_correction
                        : traction.maximum_dynamic_correction)
                            * morphology_traction_scale;
                    // Dynamic friction dissipates only a bounded span of slip
                    // per fixed step. Large error is not projected back to the
                    // old anchor, which would inject a backward recoil and make
                    // loose material behave like a hidden rail.
                    const float correction_budget = inside_static_capture
                        ? raw_correction_budget : std::min(raw_correction_budget,
                            traction.static_capture_distance * 2.5f);
                    support_traction_capacity_[index] = std::max(
                        support_traction_capacity_[index], correction_budget);
                    remaining_contact_budget = std::max(0.0f,
                        support_traction_capacity_[index]
                            - support_traction_used_[index]);
                    const float correction = clamp(slip,
                        -remaining_contact_budget, remaining_contact_budget);
                    support_traction_used_[index] += std::abs(correction);
                    remaining_contact_budget = std::max(0.0f,
                        remaining_contact_budget - std::abs(correction));
                    position_capture_complete = inside_static_capture
                        && std::abs(slip) <= std::abs(correction) + 1.0e-7f;
                    particle.position.x -= correction;
                    available_traction_ = 0.0f;
                    used_traction_ = 0.0f;
                    for (std::size_t contact = 0;
                        contact < support_traction_capacity_.size(); ++contact)
                    {
                        available_traction_ += support_traction_capacity_[contact];
                        used_traction_ += support_traction_used_[contact];
                    }
                    const float residual_slip = particle.position.x
                        - support_contact_anchor_x_[index];
                    stance_slip_distance_ = std::max(stance_slip_distance_,
                        std::abs(residual_slip));
                    const float anchored_firmness = terrain_firmness_at(
                        particle.position.x);
                    const float anchored_burial = stage_uses_deformable_terrain(
                        course_stage_)
                        ? support_contact_soil_sink(anchored_firmness) : 0.0f;
                    float anchored_ground = ground_height_at(particle.position.x);
                    if (course_stage_ == CourseStage::climb_descent
                        && particle.position.x >= ledge_left_edge_
                        && particle.position.x <= ledge_left_edge_ + 6.0f
                        && particle.position.y - particle.radius
                            >= ledge_top_height_ - 0.35f)
                        anchored_ground = std::max(anchored_ground,
                            ledge_top_height_);
                    resolved_minimum_y = anchored_ground
                        + ground_contact_offset(true, particle.radius)
                        - anchored_burial;
                    }
                }
                const bool raw_xpbd_support = shared_physical_human_sole
                    && semantic_support && traction_contact
                    && (raw_sole >= 0 || raw_toe_contact);
                if (raw_xpbd_support)
                {
                    // Unilateral accumulated normal XPBD constraint C >= 0.
                    // Its lambda has position-constraint units; lambda / dt is
                    // the actual external normal impulse used by Coulomb below.
                    // No whole-body pseudo-load or per-contact multiplier enters
                    // this solve, and the Verlet history is deliberately left
                    // unchanged so the reaction is represented physically.
                    float& lambda = support_normal_constraint_lambdas_[index];
                    const float constraint = particle.position.y
                        - resolved_minimum_y;
                    const float old_lambda = lambda;
                    const float delta_lambda = -constraint
                        / std::max(particle.inverse_mass, 1.0e-8f);
                    lambda = std::max(0.0f, lambda + delta_lambda);
                    float applied_lambda = lambda - old_lambda;
                    particle.position.y += particle.inverse_mass
                        * applied_lambda;
                    // Position-level complementarity alone has no reaction
                    // when a contact is exactly on the plane. Resolve any
                    // remaining inward Verlet velocity as the velocity-level
                    // half of the same unilateral constraint, and accumulate
                    // its impulse in lambda so the Coulomb solve sees the real
                    // normal capacity. This changes history only: it cannot
                    // lift the contact or manufacture an upward position kick.
                    const float resolved_vertical_velocity =
                        (particle.position.y - particle.previous.y) / safe_dt;
                    if (resolved_vertical_velocity < 0.0f)
                    {
                        const float velocity_lambda =
                            -resolved_vertical_velocity * safe_dt
                            / std::max(particle.inverse_mass, 1.0e-8f);
                        lambda += velocity_lambda;
                        applied_lambda += velocity_lambda;
                        particle.previous.y = particle.position.y;
                    }
                    const int side = in_left_cluster ? 0
                        : in_right_cluster ? 1 : -1;
                    if (side >= 0)
                    {
                        const float normal_impulse = applied_lambda / safe_dt;
                        support_cluster_normal_impulse_[
                            static_cast<std::size_t>(side)] += normal_impulse;
                        support_cluster_normal_impulse_moment_x_[
                            static_cast<std::size_t>(side)]
                            += particle.position.x * normal_impulse;
                        if (raw_authority_telemetry_enabled_)
                        {
                            const std::size_t slot =
                                raw_support_contact_slot(index);
                            if (slot < human_support_contact_nodes.size())
                            {
                                raw_authority_telemetry_
                                    .support_contact_normal_impulse[slot]
                                    += normal_impulse;
                            }
                        }
                    }
                }
                else
                    particle.position.y = resolved_minimum_y;
                particle.grounded = true;
                if (semantic_support)
                {
                    support_contact_latch_[index] = 1u;
                    if (!anchored_support_plant)
                        support_contact_anchor_x_[index] = particle.position.x;
                }
                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact)
                {
                    const bool left_toe = blueprint_.additional_left_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_left_contact_nodes[1];
                    const bool right_toe = blueprint_.additional_right_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_right_contact_nodes[1];
                    retention = foot_friction_retention(velocity.x,
                        firmness, looseness, false,
                        left_toe || right_toe);
                }
                float retained_horizontal_velocity = velocity.x * retention;
                if (raw_sole >= 0 || raw_toe_contact)
                {
                    // Resolve tangent traction after the complete normal pass.
                    retained_horizontal_velocity = velocity.x;
                }
                else
                {
                    static_traction = position_capture_complete;
                    if (anchored_support_plant && static_traction)
                        retained_horizontal_velocity = 0.0f;
                }
                particle.previous.x = particle.position.x
                    - retained_horizontal_velocity * safe_dt;
                if (anchored_support_plant && raw_sole < 0
                    && !raw_toe_contact && !static_traction)
                    support_contact_anchor_x_[index] = particle.position.x;
                if (raw_xpbd_support)
                {
                    // The accumulated normal constraint already changed current
                    // position relative to unchanged Verlet history. Rewriting
                    // history here would add a second, unreported rebound and
                    // make the next solver pass release a neutral heel/ball.
                }
                else if (traction_contact)
                    particle.previous.y = particle.position.y;
                else if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y
                        + velocity.y * safe_dt * 0.05f;
            }
            else if (semantic_support && release_requested
                && separation < 0.0f)
            {
                if (raw_authority_telemetry_enabled_)
                    ++raw_authority_telemetry_.support_contact_corrections;
                // Releasing a sole removes its latch and tangential traction,
                // not its collision shape. Translate Verlet history with the
                // normal projection so an unloaded heel/ball/toe cannot tunnel
                // through an edge or turn penetration cleanup into a launch.
                const float normal_correction = minimum_y
                    - particle.position.y;
                particle.position.y += normal_correction;
                particle.previous.y += normal_correction;
                support_contact_latch_[index] = 0u;
                support_normal_constraint_lambdas_[index] = 0.0f;
                support_contact_tangent_impulses_[index] = 0.0f;
            }
            else if (semantic_support && !static_support
                && (release_requested
                    || separation > moving_contact_slop_m
                    || velocity.y > moving_contact_release_speed_mps))
            {
                support_contact_latch_[index] = 0u;
                support_normal_constraint_lambdas_[index] = 0.0f;
                support_contact_tangent_impulses_[index] = 0.0f;
            }
        }

        if (shared_physical_human_sole
            && raw_support_tangential_impulses_enabled_
            && !solve_raw_support_tangent_manifold(safe_dt))
        {
            // The rigid rear-foot plate spends one finite Coulomb impulse across
            // heel+ball. The passive toe is a separate unilateral contact, so it
            // cannot multiply the plate's normal capacity or turn the foot into
            // a three-node positional rail. Every tangential correction changes
            // Verlet history only and is therefore an explicit contact impulse.
            for (std::size_t side = 0; side < 2u; ++side)
            {
                const std::uint16_t contact_node = side == 0u
                    ? blueprint_.left_contact_node
                    : blueprint_.right_contact_node;
                float toe_capacity{};
                float toe_used{};
                float toe_constraint_impulse{};
                for (std::size_t index = 0; index < particles_.size(); ++index)
                {
                    if (!contact_cluster_contains(contact_node, index)
                        || !raw_articulated_toe(index))
                        continue;
                    toe_constraint_impulse +=
                        support_contact_tangent_impulses_[index];
                    if (!blueprint_.is_support_seed(index)
                        || !particles_[index].grounded
                        || support_contact_latch_[index] == 0u)
                        continue;

                    Particle& particle = particles_[index];
                    const float mass = 1.0f
                        / std::max(particle.inverse_mass, 1.0e-5f);
                    const ContactTraction traction = contact_traction(
                        terrain_firmness_at(particle.position.x),
                        terrain_looseness_at(particle.position.x),
                        water_depth_at(particle.position.x), particle.radius,
                        particle.inverse_mass, safe_dt);
                    const float normal_impulse = std::max(0.0f,
                        support_normal_constraint_lambdas_[index] / safe_dt);
                    const float velocity_before = (particle.position.x
                        - particle.previous.x) / safe_dt;
                    const float before_impulse =
                        support_contact_tangent_impulses_[index];
                    const float requested_impulse = before_impulse
                        + mass * velocity_before;
                    const float static_budget = traction.static_coefficient
                        * normal_impulse;
                    const float dynamic_budget = traction.dynamic_coefficient
                        * normal_impulse;
                    const bool static_contact = std::abs(requested_impulse)
                        <= static_budget;
                    const float capacity = static_contact
                        ? static_budget : dynamic_budget;
                    const float after_impulse = clamp(requested_impulse,
                        -capacity, capacity);
                    const float delta_impulse = after_impulse - before_impulse;
                    const float velocity_change = delta_impulse / mass;
                    const float velocity_after = velocity_before
                        - velocity_change;
                    particle.previous.x += velocity_change * safe_dt;
                    support_contact_tangent_impulses_[index] = after_impulse;

                    const float physical_impulse = -delta_impulse;
                    const float friction_work = physical_impulse * 0.5f
                        * (velocity_before + velocity_after);
                    support_friction_work_joules_ += friction_work;
                    support_friction_positive_work_joules_ += std::max(
                        0.0f, friction_work);
                    toe_capacity = capacity;
                    toe_used = std::abs(after_impulse);
                    toe_constraint_impulse = after_impulse;
                    support_traction_capacity_[index] = capacity;
                    support_traction_used_[index] = toe_used;
                    stance_slip_distance_ = std::max(stance_slip_distance_,
                        std::max(std::abs(particle.position.x
                                - support_contact_anchor_x_[index]),
                            std::abs(velocity_after) * safe_dt));
                    if (raw_authority_telemetry_enabled_)
                    {
                        const std::size_t slot = raw_support_contact_slot(index);
                        if (slot < human_support_contact_nodes.size())
                        {
                            raw_authority_telemetry_
                                .support_contact_tangential_impulse[slot]
                                += physical_impulse;
                        }
                    }
                }

                float contact_mass{};
                float contact_radius_squared{};
                float weighted_firmness{};
                float weighted_looseness{};
                float weighted_water_depth{};
                float weighted_slip{};
                float plate_normal_impulse{};
                float material_weight{};
                std::size_t contact_count{};
                for (std::size_t index = 0; index < particles_.size(); ++index)
                {
                    if (!raw_sole_plate_contains(side, index)
                        || !blueprint_.is_support_seed(index)
                        || !particles_[index].grounded
                        || support_contact_latch_[index] == 0u)
                        continue;
                    const Particle& particle = particles_[index];
                    const float mass = 1.0f
                        / std::max(particle.inverse_mass, 1.0e-5f);
                    const float area_weight = std::max(
                        particle.radius * particle.radius, 4.0e-4f);
                    contact_mass += mass;
                    contact_radius_squared += area_weight;
                    material_weight += area_weight;
                    weighted_firmness += terrain_firmness_at(
                        particle.position.x) * area_weight;
                    weighted_looseness += terrain_looseness_at(
                        particle.position.x) * area_weight;
                    weighted_water_depth += water_depth_at(
                        particle.position.x) * area_weight;
                    weighted_slip += (particle.position.x
                        - support_contact_anchor_x_[index]) * mass;
                    plate_normal_impulse += std::max(0.0f,
                        support_normal_constraint_lambdas_[index] / safe_dt);
                    ++contact_count;
                }
                float plate_capacity{};
                if (contact_count == 0u || contact_mass <= 1.0e-5f
                    || material_weight <= 1.0e-7f)
                {
                    plate_capacity = 0.0f;
                }
                else
                {
                    const float firmness = weighted_firmness / material_weight;
                    const float looseness = weighted_looseness / material_weight;
                    const float water_depth = weighted_water_depth / material_weight;
                    const ContactTraction traction = contact_traction(
                        firmness, looseness, water_depth,
                        std::sqrt(contact_radius_squared),
                        1.0f / contact_mass, safe_dt);
                    float aggregate_velocity{};
                    for (std::size_t index = 0;
                        index < particles_.size(); ++index)
                    {
                        if (!raw_sole_plate_contains(side, index)
                            || !blueprint_.is_support_seed(index)
                            || !particles_[index].grounded
                            || support_contact_latch_[index] == 0u)
                            continue;
                        const Particle& particle = particles_[index];
                        const float mass = 1.0f / std::max(
                            particle.inverse_mass, 1.0e-5f);
                        aggregate_velocity += (particle.position.x
                            - particle.previous.x) / safe_dt * mass;
                    }
                    aggregate_velocity /= contact_mass;
                    const float before_impulse =
                        support_cluster_traction_impulse_[side];
                    const float requested_impulse = before_impulse
                        + contact_mass * aggregate_velocity;
                    const float static_budget = traction.static_coefficient
                        * plate_normal_impulse;
                    const float dynamic_budget = traction.dynamic_coefficient
                        * plate_normal_impulse;
                    const bool static_contact = std::abs(requested_impulse)
                        <= static_budget;
                    plate_capacity = static_contact
                        ? static_budget : dynamic_budget;
                    const float after_impulse = clamp(requested_impulse,
                        -plate_capacity, plate_capacity);
                    const float delta_impulse = after_impulse - before_impulse;
                    const float velocity_change = delta_impulse / contact_mass;
                    const float velocity_after = aggregate_velocity
                        - velocity_change;
                    const float physical_impulse = -delta_impulse;
                    const float friction_work = physical_impulse * 0.5f
                        * (aggregate_velocity + velocity_after);
                    support_cluster_traction_impulse_[side] = after_impulse;
                    support_friction_work_joules_ += friction_work;
                    support_friction_positive_work_joules_ += std::max(
                        0.0f, friction_work);

                    for (std::size_t index = 0;
                        index < particles_.size(); ++index)
                    {
                        if (!raw_sole_plate_contains(side, index)
                            || !blueprint_.is_support_seed(index)
                            || !particles_[index].grounded
                            || support_contact_latch_[index] == 0u)
                            continue;
                        Particle& particle = particles_[index];
                        const float mass = 1.0f / std::max(
                            particle.inverse_mass, 1.0e-5f);
                        particle.previous.x += velocity_change * safe_dt;
                        const float fraction = mass / contact_mass;
                        support_traction_capacity_[index] =
                            plate_capacity * fraction;
                        support_traction_used_[index] =
                            std::abs(after_impulse) * fraction;
                        stance_slip_distance_ = std::max(
                            stance_slip_distance_,
                            std::max(std::abs(particle.position.x
                                    - support_contact_anchor_x_[index]),
                                std::abs(velocity_after) * safe_dt));
                        if (raw_authority_telemetry_enabled_)
                        {
                            const std::size_t slot =
                                raw_support_contact_slot(index);
                            if (slot < human_support_contact_nodes.size())
                            {
                                raw_authority_telemetry_
                                    .support_contact_tangential_impulse[slot]
                                    += physical_impulse * fraction;
                            }
                        }
                    }
                    static_cast<void>(weighted_slip);
                }

                support_cluster_traction_capacity_[side] = std::max(
                    support_cluster_traction_capacity_[side],
                    plate_capacity + toe_capacity);
                support_cluster_traction_used_[side] = std::abs(
                        support_cluster_traction_impulse_[side])
                    + toe_used;
                support_cluster_tangential_impulse_[side] = -(
                    support_cluster_traction_impulse_[side]
                        + toe_constraint_impulse);
                const float normal_impulse =
                    support_cluster_normal_impulse_[side];
                support_cluster_normal_cop_x_[side] = normal_impulse > 1.0e-7f
                    ? support_cluster_normal_impulse_moment_x_[side]
                        / normal_impulse
                    : 0.0f;
            }
            available_traction_ = support_cluster_traction_capacity_[0]
                + support_cluster_traction_capacity_[1];
            used_traction_ = support_cluster_traction_used_[0]
                + support_cluster_traction_used_[1];
        }
    }

    void Environment::apply_ledge_training_scaffold() noexcept
    {
        if (guidance_mode_ != GuidanceMode::assisted)
            return;

        for (std::size_t particle_index = 0;
            particle_index < particles_.size(); ++particle_index)
        {
            Particle& particle = particles_[particle_index];
            for (const CourseFeature& feature : course_features_)
            {
                if (feature.kind != CourseFeatureKind::ledge)
                    continue;
                const float left = feature.center.x - feature.half_extent.x;
                const float top = feature.center.y + feature.half_extent.y;
                const bool grasp_candidate = particle_index != blueprint_.head_node
                    && particle_index != blueprint_.root_node
                    && particle_index != blueprint_.torso_node
                    && !blueprint_.is_support_seed(particle_index)
                    && manipulator_endpoint(blueprint_, particle_index);
                const bool near_edge = std::abs(particle.position.x - left)
                        <= particle.radius + 0.18f
                    && std::abs(particle.position.y - top)
                        <= particle.radius + 0.22f;
                if (grasp_candidate && near_edge)
                {
                    std::size_t slot = ledge_grasp_nodes_.size();
                    for (std::size_t index = 0;
                        index < ledge_grasp_nodes_.size(); ++index)
                    {
                        if (ledge_grasp_nodes_[index] == particle_index)
                        {
                            slot = index;
                            break;
                        }
                        if (slot == ledge_grasp_nodes_.size()
                            && ledge_grasp_nodes_[index]
                                == std::numeric_limits<std::uint16_t>::max())
                            slot = index;
                    }
                    if (slot < ledge_grasp_nodes_.size()
                        && ledge_grasp_nodes_[slot] != particle_index)
                    {
                        ledge_grasp_nodes_[slot] =
                            static_cast<std::uint16_t>(particle_index);
                        ledge_grasp_anchors_[slot] = {
                            left - particle.radius * 0.20f,
                            top + particle.radius * 0.18f };
                        ++hand_ledge_contacts_;
                    }
                }
                for (std::size_t slot = 0;
                    slot < ledge_grasp_nodes_.size(); ++slot)
                {
                    if (ledge_grasp_nodes_[slot] != particle_index)
                        continue;
                    const Vec2 correction =
                        (ledge_grasp_anchors_[slot] - particle.position) * 0.62f;
                    particle.position += correction;
                    particle.previous += correction;
                }
            }
        }
    }

    void Environment::solve_course(float dt) noexcept
    {
        for (std::size_t particle_index = 0;
            particle_index < particles_.size(); ++particle_index)
        {
            Particle& particle = particles_[particle_index];
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
                        particle.previous -= feature.velocity * dt * 0.34f;
                    else if (feature.kind == CourseFeatureKind::moving_hazard)
                        particle.previous -= feature.velocity * dt * 0.12f;
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
                if (feature.kind == CourseFeatureKind::ledge
                    && blueprint_.is_support_seed(particle_index)
                    && normal.y > 0.60f)
                    particle.grounded = true;
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

    MotorDiagnostic Environment::motor_diagnostic(
        std::size_t motor_index) const noexcept
    {
        MotorDiagnostic diagnostic{};
        if (motor_index >= blueprint_.active_motor_count
            || motor_index >= anatomy_action_count)
            return diagnostic;
        const MotorConstraint& motor = blueprint_.motors[motor_index];
        diagnostic.available = true;
        diagnostic.enabled = motor.enabled;
        diagnostic.support_branch = blueprint_.support_branch_mask(motor) != 0u;
        diagnostic.parent = motor.a;
        diagnostic.pivot = motor.pivot;
        diagnostic.driven = motor.c;
        diagnostic.action_slot = motor_index;
        diagnostic.authored_neutral = motor.neutral_angle;
        diagnostic.minimum = motor.minimum_angle;
        diagnostic.maximum = motor.maximum_angle;
        diagnostic.current_angle = joint_angle(motor) * facing_direction();
        diagnostic.angular_velocity = angular_velocities_[motor_index]
            * facing_direction();
        diagnostic.applied_action = previous_applied_actions_[motor_index];
        diagnostic.target_angle = motor_target_angle(
            motor, diagnostic.applied_action);
        return diagnostic;
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
        Vec2 authored = blueprint_.nodes[blueprint_.torso_node]
            - blueprint_.nodes[blueprint_.root_node];
        authored.x *= facing_direction();
        const Vec2 desired = normalized(authored,
            blueprint_.horizontal_body_plan()
                ? Vec2{ 1.0f, 0.0f } : Vec2{ 0.0f, 1.0f });
        return clamp(dot(current, desired), -1.0f, 1.0f);
    }

    float Environment::primary_support_span_ratio() const noexcept
    {
        if (!valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node)
            || blueprint_.left_contact_node >= blueprint_.nodes.size()
            || blueprint_.right_contact_node >= blueprint_.nodes.size())
            return 1.0f;
        const float rest_span = std::max(0.42f, std::abs(
            blueprint_.nodes[blueprint_.right_contact_node].x
            - blueprint_.nodes[blueprint_.left_contact_node].x));
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

    void Environment::update_climb_metrics(float dt) noexcept
    {
        if (!valid_node(blueprint_.root_node))
            return;
        const Particle& root = particles_[blueprint_.root_node];
        const float vertical_speed = (root.position.y - previous_root_height_)
            / std::max(dt, 1.0e-5f);
        previous_root_height_ = root.position.y;
        if (course_stage_ != CourseStage::climb_descent)
            return;

        const std::size_t grasp_count = static_cast<std::size_t>(
            std::count_if(ledge_grasp_nodes_.begin(), ledge_grasp_nodes_.end(),
                [](std::uint16_t node)
                {
                    return node != std::numeric_limits<std::uint16_t>::max();
                }));
        bool support_on_top = false;
        bool support_on_lower_ground = false;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!blueprint_.is_support_seed(index) || !particles_[index].grounded)
                continue;
            support_on_top = support_on_top
                || (particles_[index].position.x >= ledge_left_edge_
                    && particles_[index].position.y
                        >= ledge_top_height_ - particles_[index].radius - 0.10f);
            support_on_lower_ground = support_on_lower_ground
                || (particles_[index].position.x < ledge_left_edge_ - 0.05f
                    && particles_[index].position.y
                        < ledge_top_height_ - 0.30f);
        }

        if (!ledge_climbed_ && grasp_count > 0u && support_on_top
            && powered_jump_count_ == 0u
            && root.position.y >= ledge_top_height_ + 0.22f)
        {
            ledge_climbed_ = true;
            ++ledge_climbs_;
            ++climb_support_transfers_;
            ledge_grasp_nodes_.fill(std::numeric_limits<std::uint16_t>::max());
        }
        else if (ledge_climbed_ && !ledge_descending_ && grasp_count > 0u
            && root.position.x <= ledge_left_edge_ + 0.75f
            && vertical_speed <= 0.10f)
        {
            ledge_descending_ = true;
            ++climb_support_transfers_;
        }
        if (ledge_descending_ && support_on_lower_ground
            && root.position.y < ledge_top_height_ + 0.55f
            && vertical_speed >= -2.20f && powered_jump_count_ == 0u)
        {
            ++controlled_descents_;
            ledge_descending_ = false;
            ledge_grasp_nodes_.fill(std::numeric_limits<std::uint16_t>::max());
        }
    }
    void Environment::update_gait_metrics(float dt, float action_energy) noexcept
    {
        const bool left = contact_supported(blueprint_.left_contact_node);
        const bool right = contact_supported(blueprint_.right_contact_node);
        const bool locomotion_demanded = locomotion_required();
        // Heel lift and toe roll are still planted support. A leg enters swing
        // only after its complete semantic foot cluster loses ground contact.
        const bool left_swinging = !left;
        const bool right_swinging = !right;
        const bool was_supported = previous_left_grounded_ || previous_right_grounded_;
        // A foot is one semantic support cluster. Heel/toe rolling within a
        // planted foot must not manufacture simultaneous landings that erase
        // the real opposite-foot transfer event.
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;
        const int strike_side = new_left == new_right ? 0 : (new_left ? -1 : 1);
        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float locomotion_x = terrain_sample_x(root_x, course_progress());
        const float left_clearance = contact_cluster_clearance(blueprint_.left_contact_node);
        const float right_clearance = contact_cluster_clearance(blueprint_.right_contact_node);
        const float facing = facing_direction();
        const float left_center = (contact_cluster_center_x(
            blueprint_.left_contact_node) - root_x) * facing;
        const float right_center = (contact_cluster_center_x(
            blueprint_.right_contact_node) - root_x) * facing;
        lower_leg_scissored_this_step_ = false;
        if (locomotion_demanded
            && measure_forward_gait_faults(shuttle_state_.phase, elapsed_seconds_)
            && blueprint_.paired_leg_chains() && blueprint_.active_motor_count >= 4u)
        {
            const MotorConstraint& left_shank = blueprint_.motors[1];
            const MotorConstraint& right_shank = blueprint_.motors[3];
            if (valid_node(left_shank.pivot) && valid_node(left_shank.c)
                && valid_node(right_shank.pivot) && valid_node(right_shank.c))
            {
                const bool strict_crossing = strict_segment_crossing(
                    particles_[left_shank.pivot].position,
                    particles_[left_shank.c].position,
                    particles_[right_shank.pivot].position,
                    particles_[right_shank.c].position);
                const MotorConstraint& left_thigh = blueprint_.motors[0];
                const MotorConstraint& right_thigh = blueprint_.motors[2];
                const float mean_leg_length = 0.5f * (
                    length(blueprint_.nodes[left_thigh.c]
                        - blueprint_.nodes[left_thigh.pivot])
                    + length(blueprint_.nodes[left_shank.c]
                        - blueprint_.nodes[left_shank.pivot])
                    + length(blueprint_.nodes[right_thigh.c]
                        - blueprint_.nodes[right_thigh.pivot])
                    + length(blueprint_.nodes[right_shank.c]
                        - blueprint_.nodes[right_shank.pivot]));
                const bool awkward_pose = awkward_paired_passing_pose(
                    particles_[left_shank.pivot].position.x
                        - particles_[right_shank.pivot].position.x,
                    particles_[left_shank.c].position.x
                        - particles_[right_shank.c].position.x,
                    mean_leg_length);
                lower_leg_scissored_this_step_ = pathological_lower_leg_crossing(
                    strict_crossing, left_swinging, right_swinging, awkward_pose);
            }
        }
        lower_leg_scissor_seconds_ = contiguous_condition_seconds(
            lower_leg_scissored_this_step_, lower_leg_scissor_seconds_, dt);
        maximum_lower_leg_scissor_seconds_ = std::max(
            maximum_lower_leg_scissor_seconds_, lower_leg_scissor_seconds_);

        backward_brace_ratio_ = 0.0f;
        const bool measure_backward_brace = measure_forward_gait_faults(
                shuttle_state_.phase, elapsed_seconds_)
            && locomotion_demanded
            && !blueprint_.horizontal_body_plan() && !non_foot_grounded_
            && (left || right) && valid_node(blueprint_.root_node)
            && valid_node(blueprint_.torso_node);
        if (measure_backward_brace)
        {
            const Vec2 authored_axis = blueprint_.nodes[blueprint_.torso_node]
                - blueprint_.nodes[blueprint_.root_node];
            const Vec2 current_axis = particles_[blueprint_.torso_node].position
                - particles_[blueprint_.root_node].position;
            backward_brace_ratio_ = directional_backward_brace_ratio(
                authored_axis, current_axis, locomotion_direction());
        }
        backward_brace_seconds_ = contiguous_condition_seconds(
            measure_backward_brace
                && backward_brace_ratio_ > backward_brace_activation_ratio,
            backward_brace_seconds_, dt);
        maximum_backward_brace_seconds_ = std::max(
            maximum_backward_brace_seconds_, backward_brace_seconds_);
        if (left_swinging && left_swing_seconds_ <= 0.0f)
        {
            left_swing_started_behind_ = left_center < right_center - 0.035f;
            left_swing_crossed_ = false;
        }
        if (right_swinging && right_swing_seconds_ <= 0.0f)
        {
            right_swing_started_behind_ = right_center < left_center - 0.035f;
            right_swing_crossed_ = false;
        }
        if (left_swinging)
        {
            left_swing_seconds_ += dt;
            left_swing_clearance_ = std::max(left_swing_clearance_, left_clearance);
            left_swing_crossed_ = left_swing_crossed_
                || completes_side_view_crossing(left_swing_started_behind_,
                    left_center, right_center, left_clearance);
        }
        if (right_swinging)
        {
            right_swing_seconds_ += dt;
            right_swing_clearance_ = std::max(right_swing_clearance_, right_clearance);
            right_swing_crossed_ = right_swing_crossed_
                || completes_side_view_crossing(right_swing_started_behind_,
                    right_center, left_center, right_clearance);
        }

        const FootContactPhase next_left_phase = detect_foot_contact_phase(true);
        const FootContactPhase next_right_phase = detect_foot_contact_phase(false);
        if (next_left_phase == FootContactPhase::heel_strike
            && left_foot_phase_ != FootContactPhase::heel_strike)
            ++heel_strike_count_;
        if (next_right_phase == FootContactPhase::heel_strike
            && right_foot_phase_ != FootContactPhase::heel_strike)
            ++heel_strike_count_;
        if (next_left_phase == FootContactPhase::toe_off
            && left_foot_phase_ != FootContactPhase::toe_off)
            ++toe_off_count_;
        if (next_right_phase == FootContactPhase::toe_off
            && right_foot_phase_ != FootContactPhase::toe_off)
            ++toe_off_count_;
        left_foot_phase_ = next_left_phase;
        right_foot_phase_ = next_right_phase;

        alternating_step_this_step_ = false;
        single_leg_cycle_this_step_ = false;
        limb_crossing_this_step_ = false;
        const bool branch_level_support_transfers = blueprint_.horizontal_body_plan()
            && blueprint_.horizontal_multi_support_plan();
        if (branch_level_support_transfers)
        {
            for (std::size_t node = 0; node < particles_.size(); ++node)
            {
                if (!blueprint_.is_support_seed(node))
                    continue;
                const bool grounded = particles_[node].grounded;
                const bool was_grounded = node < previous_support_grounded_.size()
                    && previous_support_grounded_[node] != 0u;
                if (!grounded)
                {
                    support_swing_seconds_[node] += dt;
                    const float clearance = particles_[node].position.y
                        - particles_[node].radius
                        - ground_height_at(particles_[node].position.x);
                    support_swing_clearance_[node] = std::max(
                        support_swing_clearance_[node], clearance);
                    continue;
                }
                if (was_grounded)
                    continue;

                std::size_t preceding_supports = 0u;
                const float authored_x = blueprint_.nodes[node].x;
                for (std::size_t candidate = 0;
                    candidate < blueprint_.nodes.size(); ++candidate)
                {
                    if (blueprint_.is_support_seed(candidate)
                        && blueprint_.nodes[candidate].x < authored_x - 0.02f)
                        ++preceding_supports;
                }
                const std::size_t side = blueprint_.is_right_support_seed(node)
                    ? 1u : 0u;
                const std::size_t phase = (preceding_supports / 2u + side) & 1u;
                const bool qualified = qualifies_topology_support_transfer(
                    true, true, support_swing_seconds_[node],
                    support_swing_clearance_[node],
                    elapsed_seconds_ - last_topology_transfer_seconds_,
                    locomotion_x - last_topology_transfer_x_,
                    last_topology_support_phase_, phase);
                if (qualified)
                {
                    ++alternating_steps_;
                    alternating_step_this_step_ = true;
                    last_support_transfer_seconds_ = elapsed_seconds_;
                }
                const bool real_landing = support_swing_seconds_[node] >= 0.05f
                    && support_swing_clearance_[node] >= 0.015f;
                if (real_landing
                    && (last_topology_support_phase_
                            == std::numeric_limits<std::size_t>::max()
                        || phase != last_topology_support_phase_))
                {
                    last_topology_support_phase_ = phase;
                    last_topology_transfer_seconds_ = elapsed_seconds_;
                    last_topology_transfer_x_ = locomotion_x;
                }
                support_swing_seconds_[node] = 0.0f;
                support_swing_clearance_[node] = 0.0f;
            }
        }
        if (strike_side != 0 && !branch_level_support_transfers)
        {
            const float swing_air_seconds = new_left ? left_swing_seconds_ : right_swing_seconds_;
            const float swing_clearance = new_left ? left_swing_clearance_ : right_swing_clearance_;
            last_landing_air_seconds_ = swing_air_seconds;
            last_landing_clearance_ = swing_clearance;
            last_landing_displacement_ = locomotion_x - last_step_x_;
            last_landing_side_ = strike_side;
            last_landing_qualified_ = false;
            const bool recent_transfer_evidence = blueprint_.monopedal_gait()
                ? swing_air_seconds >= 0.05f && swing_clearance >= 0.035f
                : swing_air_seconds >= 0.06f && swing_clearance >= 0.015f;
            if (recent_transfer_evidence)
                last_support_transfer_seconds_ = elapsed_seconds_;
            if (last_contact_side_ != 0)
            {
                const bool swing_crossed = new_left
                    ? left_swing_started_behind_ && left_swing_crossed_
                    : right_swing_started_behind_ && right_swing_crossed_;
                const bool qualified_transfer = blueprint_.monopedal_gait()
                    ? qualifies_monoped_support_transfer(
                        last_contact_side_, strike_side,
                        elapsed_seconds_ - last_step_time_,
                        locomotion_x - last_step_x_,
                        swing_air_seconds, swing_clearance)
                    : qualifies_supported_step(last_contact_side_, strike_side,
                        elapsed_seconds_ - last_step_time_,
                        locomotion_x - last_step_x_,
                        swing_air_seconds, swing_clearance);
                if (qualified_transfer)
                {
                    last_landing_qualified_ = true;
                    ++alternating_steps_;
                    if (swing_crossed)
                    {
                        ++limb_crossings_;
                        limb_crossing_this_step_ = true;
                    }
                    alternating_step_this_step_ = true;
                }
            }
            // A marginal landing must not permanently poison gait telemetry.
            // Keep the strict counter thresholds above, but advance the support
            // candidate whenever a real airborne transfer occurred so the next
            // opposite landing is measured against the immediately prior step.
            if (recent_transfer_evidence)
            {
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = locomotion_x;
                if (new_left)
                    left_swing_crossed_ = false;
                else
                    right_swing_crossed_ = false;
            }
        }
        if (!left_swinging)
        {
            left_swing_seconds_ = 0.0f;
            left_swing_clearance_ = 0.0f;
            left_swing_started_behind_ = false;
        }
        if (!right_swinging)
        {
            right_swing_seconds_ = 0.0f;
            right_swing_clearance_ = 0.0f;
            right_swing_started_behind_ = false;
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
            ? authored_standing_head_clearance()
                - particles_[blueprint_.head_node].radius
            : 0.0f;
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
            const float travel_direction = std::abs(locomotion_direction()) >= 0.5f
                ? locomotion_direction() : facing_direction();
            const float weight = feature.kind == CourseFeatureKind::duck_press
                ? clamp((1.10f - clearance) / 1.10f, 0.0f, 1.0f)
                : duck_obstacle_approach_weight(
                    (feature.center.x - root_x) * travel_direction);
            if (weight <= duck_obstacle_weight_)
                continue;
            duck_obstacle_weight_ = weight;
            duck_clearance_margin_ = clearance;
        }

        const CrouchPostureEvidence crouch_posture = current_crouch_posture();
        const bool horizontal_press = blueprint_.horizontal_multi_support_plan();
        const bool horizontal_compression = horizontal_press
            && feet_supported && !non_foot_grounded_
            && current_uprightness > 0.52f
            && duck_depth_ >= 0.08f
            && crouch_posture.pelvis_drop >= 0.08f
            && crouch_posture.support_margin >= -0.24f;
        const bool physical_crouch = (course_stage_ == CourseStage::crouch_walk
                ? crouch_walk_posture_qualified(crouch_posture)
                : crouch_posture_qualified(crouch_posture))
            || horizontal_compression;
        const bool generic_duck = course_stage_ != CourseStage::duck_press
            && physical_crouch
            && current_uprightness > 0.60f
            && duck_depth_ >= (horizontal_press ? 0.10f
                : course_stage_ == CourseStage::crouch_walk ? 0.16f : 0.48f);
        const bool press_duck = course_stage_ == CourseStage::duck_press
            && physical_crouch
            && duck_obstacle_weight_ >= (horizontal_press ? 0.48f : 0.64f)
            && duck_clearance_margin_ >= -0.10f
            && current_uprightness > 0.45f;
        duck_active_ = generic_duck || press_duck;

        const bool crouch_challenge = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            && duck_obstacle_weight_ >= (horizontal_press ? 0.48f : 0.72f)
            && duck_depth_ >= (horizontal_press ? 0.08f : 0.30f)
            && feet_supported && !non_foot_grounded_;
        duck_posture_failure_seconds_ = crouch_challenge && !physical_crouch
            ? duck_posture_failure_seconds_ + dt
            : std::max(0.0f, duck_posture_failure_seconds_ - dt * 2.0f);
        const bool horizontal_crouch_plan =
            blueprint_.horizontal_multi_support_plan();
        if (!horizontal_crouch_plan && duck_posture_failure_seconds_ > 1.10f)
            invalidate(InvalidMotion::duck_hip_hinge);

        const bool disallowed_duck_contact = !duck_ground_contact_allowed(
            duck_active_, non_foot_grounded_);
        if (course_stage_ == CourseStage::duck_press)
        {
            duck_body_contact_seconds_ = disallowed_duck_contact
                ? duck_body_contact_seconds_ + dt
                : std::max(0.0f, duck_body_contact_seconds_ - dt * 3.0f);
            const float contact_limit = blueprint_.horizontal_multi_support_plan()
                ? 0.90f : 0.35f;
            if (duck_body_contact_seconds_ > contact_limit)
                invalidate(InvalidMotion::duck_body_contact);
        }
        else if (disallowed_duck_contact)
        {
            invalidate(InvalidMotion::duck_body_contact);
        }
        if (duck_active_ && !non_foot_grounded_)
        {
            duck_seconds_ += dt;
            current_valid_crouch_seconds_ += dt;
            longest_valid_crouch_seconds_ = std::max(
                longest_valid_crouch_seconds_, current_valid_crouch_seconds_);
        }
        else
        {
            current_valid_crouch_seconds_ = 0.0f;
        }
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
                || (duck_obstacle_weight_ >= (horizontal_press ? 0.62f : 0.78f)
                    && duck_clearance_margin_ <= (horizontal_press ? 0.24f : 0.16f));
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
            const bool horizontal_recovery =
                blueprint_.horizontal_multi_support_plan();
            const float recovery_uprightness = horizontal_recovery ? 0.70f : 0.78f;
            const float recovery_head_ratio = horizontal_recovery ? 0.66f : 0.82f;
            const float recovery_hold = horizontal_recovery ? 0.70f : 0.55f;
            const bool recovered_frame = duck_press_hold_qualified_
                && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f
                && feet_supported && !non_foot_grounded_
                && body_integrity_valid() && !duck_active_
                && current_uprightness >= recovery_uprightness
                && head_height_ratio >= recovery_head_ratio
                && std::abs(torso_angle) <= 0.40f
                && stance_slip_speed_ <= 0.16f;
            current_duck_hold_seconds_ = recovered_frame
                ? current_duck_hold_seconds_ + dt
                : std::max(0.0f, current_duck_hold_seconds_ - dt * 2.0f);
            if (current_duck_hold_seconds_ >= recovery_hold
                && !duck_press_completed_)
            {
                duck_press_completed_ = true;
                duck_walk_started_seconds_ = elapsed_seconds_;
                progress_window_start_x_ = root_x;
                progress_window_start_steps_ = gait_cycles();
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
                && stage_requires_forward_gait(course_stage_)
                && std::abs(locomotion_x - last_single_leg_landing_x_) >= 0.040f)
            {
                ++single_leg_cycles_;
                single_leg_cycle_this_step_ = true;
                last_single_leg_landing_x_ = locomotion_x;
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
        for (std::size_t index = 0; index < previous_support_grounded_.size(); ++index)
        {
            previous_support_grounded_[index] = blueprint_.is_support_seed(index)
                && particles_[index].grounded ? 1u : 0u;
        }
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
        else
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

        const bool intentional_shuttle_hold = shuttle_enabled()
            && (shuttle_state_.phase == ShuttlePhase::braking
                || shuttle_state_.phase == ShuttlePhase::turning
                || shuttle_state_.phase == ShuttlePhase::settling);
        const bool moving_stage_contact_checks =
            stage_requires_forward_gait(course_stage_)
            && !intentional_shuttle_hold;
        const bool locomotion_required = this->locomotion_required();
        const float recent_swing_clearance = std::max(left_clearance, right_clearance);
        if (moving_stage_contact_checks && friction_driven_shuffle(root_speed,
                left, right, stance_slip_speed_, gait_cycles(), recent_swing_clearance))
            wheel_sliding_seconds_ = std::min(3.0f, wheel_sliding_seconds_ + dt);
        else
            wheel_sliding_seconds_ = std::max(0.0f, wheel_sliding_seconds_ - dt * 1.5f);
        // Sliding is a normal part of stance adjustment, crouching, and gait.
        // It is never a hard invalidation. Pure friction-driven shuffling simply
        // receives no gait credit and a mild shaping penalty until a real cycle occurs.

        const float gait_direction = std::abs(locomotion_direction()) >= 0.5f
            ? locomotion_direction() : facing_direction();
        float nearest_hazard_dx = std::numeric_limits<float>::infinity();
        float nearest_hazard_target = 0.20f;
        for (const CourseFeature& feature : course_features_)
        {
            if (!ground_clearance_hazard(feature.kind))
                continue;
            const float dx = (feature.center.x - root_x) * gait_direction;
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
            const float passed_distance = (root_x - feature.center.x) * gait_direction;
            if (passed_distance > course_feature_half_width(feature) + 0.10f)
                highest_passed_sequence = std::max(
                    highest_passed_sequence, feature.marker_sequence);
        }
        if (highest_passed_sequence > last_passed_feature_sequence_)
        {
            last_passed_feature_sequence_ = highest_passed_sequence;
            ++obstacles_passed_;
            passed_obstacle_this_step_ = true;
        }
        const std::size_t lifted_supports = support_seed_lifted_count(0.085f);
        const bool recent_support_transfer = elapsed_seconds_
            - last_support_transfer_seconds_ <= 0.90f;
        if (moving_stage_contact_checks && foot_pivot_rolling_motion(root_speed,
            left, right, stance_slip_speed_, obstacle_lift_clearance_, torso_turn_speed_,
            blueprint_.support_seed_count(), lifted_supports, recent_support_transfer,
            blueprint_.monopedal_gait() ? 1u : blueprint_.support_seed_count()))
            foot_pivot_rolling_seconds_ += dt;
        else if (!left || !right || lifted_supports > 0u
            || recent_support_transfer)
            foot_pivot_rolling_seconds_ = 0.0f;
        else
            foot_pivot_rolling_seconds_ = std::max(0.0f, foot_pivot_rolling_seconds_ - dt * 2.5f);
        if (!rolling_gate_active(elapsed_seconds_))
            foot_pivot_rolling_seconds_ = 0.0f;
        else if (foot_pivot_rolling_seconds_ > foot_pivot_rolling_limit(elapsed_seconds_))
            invalidate(InvalidMotion::foot_pivot_rolling);

        if (locomotion_required && std::isfinite(nearest_hazard_dx)
            && hazard_quiver_motion(nearest_hazard_dx,
            root_speed, obstacle_lift_clearance_, obstacle_clearance_target_, action_energy))
            hazard_stall_seconds_ += dt;
        else
            hazard_stall_seconds_ = std::max(0.0f, hazard_stall_seconds_ - dt * 1.75f);
        const bool terrain_step_recovery = ground_height_at(root_x + 0.70f)
            - ground_height_at(root_x) > 0.12f;
        if (hazard_stall_seconds_ > (terrain_step_recovery ? 2.40f : 1.35f))
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
            const std::uint32_t new_steps = gait_cycles() - progress_window_start_steps_;
            if (micro_motion_window(average_energy, net_progress,
                    root_path_window_, new_steps, locomotion_required,
                    recovery_active_ || terrain_step_recovery))
                micro_motion_seconds_ += progress_window_seconds_;
            else
                micro_motion_seconds_ = std::max(0.0f, micro_motion_seconds_ - 0.5f);

            const bool idle_window = locomotion_required
                && elapsed_seconds_ > rolling_gate_warmup_end_seconds
                && zero_progress_window(net_progress, new_steps,
                    obstacle_lift_clearance_, recovery_active_ || terrain_step_recovery);
            zero_progress_seconds_ = update_zero_progress_seconds(
                zero_progress_seconds_, idle_window, progress_window_seconds_);
            progress_window_start_steps_ = gait_cycles();
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
        // A completed crouch-walk lesson remains a valid retained preview.
        // Local settling after the rig has already demonstrated the required
        // low, alternating translation must not erase that physical evidence
        // and restart the preview. Falls, contacts, bounds, rolling and every
        // other safety gate remain active; only stall accumulators are retired.
        const bool crouch_walk_mastered = course_stage_
                == CourseStage::crouch_walk
            && crouch_walk_distance_ >= 1.50f
            && gait_cycles() >= 6u
            && duck_seconds_ >= 3.50f;
        if (crouch_walk_mastered)
            micro_motion_seconds_ = zero_progress_seconds_ = 0.0f;

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
            return { -5.0f, 0.0f, 0.0f, true, false, InvalidMotion::out_of_bounds };

        dt = clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        last_step_dt_ = dt;
        raw_authority_telemetry_ = {};
        const AuthoritySnapshot authority_step_start = authority_snapshot(dt);
        const std::array<bool, 2> support_at_step_start{
            left_supported(), right_supported() };
        crouch_posture_guide_applied_this_step_ = false;
        const float crouch_walk_guidance_authority = course_stage_
                == CourseStage::crouch_walk
            ? effective_physical_guidance_authority() : 1.0f;
        const bool guidance_assisted = guidance_mode_ == GuidanceMode::assisted
            && (course_stage_ != CourseStage::crouch_walk
                || crouch_walk_guidance_authority > 0.0f);
        for (float& hold : support_cluster_release_hold_seconds_)
            hold = std::max(0.0f, hold - dt);
        for (float& roll : support_toe_roll_seconds_)
            roll = std::max(0.0f, roll - dt);
        // Let every body settle onto its feet before the policy can apply a
        // meaningful impulse, then ease control in rather than launching it.
        const bool physical_moving_human_control =
            blueprint_.human_casual_gait_plan()
            && course_stage_ != CourseStage::balance
            && course_stage_ != CourseStage::duck_press;
        const float control_ramp = policy_control_ramp(elapsed_seconds_
            + (physical_moving_human_control
                ? policy_control_ramp_start_seconds : 0.0f));
        std::array<float, anatomy_action_count> applied_actions{};
        const std::array<float, anatomy_action_count> prior_applied_actions =
            previous_applied_actions_;
        action_change_energy_ = 0.0f;
        const bool symmetric_human_actuator = physical_moving_human_control
            || (raw_policy_audit() && blueprint_.human_casual_gait_plan());
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
        {
            raw_authority_telemetry_.commanded_actions[index] =
                clamp(actions[index], -1.0f, 1.0f);
            float desired_action = clamp(actions[index], -1.0f, 1.0f) * control_ramp;
            if (course_stage_ == CourseStage::balance && symmetric_human_actuator
                && index >= 4u && index < 8u)
            {
                // A relaxed Stand setpoint for the four existing arm motors.
                // Policy outputs keep bounded residual movement; no particle
                // positions are set and the arm reactions still load the rig.
                const MotorConstraint& motor = blueprint_.motors[index];
                const bool shoulder = (index & 1u) == 0u;
                // Use the same rest frame each step rather than tracking the
                // moving shoulder reference with a slow position actuator.
                const Vec2 parent = shoulder
                    ? blueprint_.nodes[motor.a] - blueprint_.nodes[motor.pivot]
                    : Vec2{ -0.02f, 1.0f };
                const Vec2 rest{ shoulder ? 0.02f : 0.10f, -1.0f };
                const float angle = signed_angle(parent, rest);
                const float delta = wrap_angle(angle - motor.neutral_angle);
                const float range = delta < 0.0f
                    ? motor.neutral_angle - motor.minimum_angle
                    : motor.maximum_angle - motor.neutral_angle;
                desired_action = clamp(delta / std::max(range, 1.0e-5f)
                    + 0.12f * desired_action, -1.0f, 1.0f);
            }
            if (symmetric_human_actuator)
            {
                // Moving Human training and Raw evaluation share this finite,
                // symmetric actuator bandwidth. It cannot choose a phase,
                // contact, direction, root pose, or replacement action, and
                // there is no plant discontinuity at the learned handoff.
                constexpr float target_slew_rate = 1.0f;
                const float maximum_delta = target_slew_rate * dt;
                applied_actions[index] = previous_applied_actions_[index]
                    + clamp(desired_action - previous_applied_actions_[index],
                        -maximum_delta, maximum_delta);
            }
            else
            {
                applied_actions[index] = conditioned_single_support_motor_action(
                    blueprint_, index, previous_applied_actions_[index],
                    desired_action, dt);
            }
            const float action_delta = applied_actions[index] - previous_applied_actions_[index];
            action_change_energy_ += action_delta * action_delta;
            previous_applied_actions_[index] = applied_actions[index];
            raw_authority_telemetry_.actuator_actions[index] =
                applied_actions[index];
        }
        action_support_unload_side_ = 0;
        const bool assisted_crouch_unload = guidance_assisted
            && course_stage_ == CourseStage::crouch_walk
            && blueprint_.human_casual_gait_plan()
            && blueprint_.active_motor_count >= 4u
            && blueprint_.additional_left_contact_nodes.size() >= 2u
            && blueprint_.additional_right_contact_nodes.size() >= 2u;
        if (assisted_crouch_unload)
        {
            const bool left_contact = contact_supported(blueprint_.left_contact_node);
            const bool right_contact = contact_supported(blueprint_.right_contact_node);
            const int requested_side = action_requested_swing_side(
                blueprint_, applied_actions, facing_direction());
            const bool release_windows_idle =
                support_cluster_release_hold_seconds_[0] <= 0.0f
                && support_cluster_release_hold_seconds_[1] <= 0.0f
                && support_toe_roll_seconds_[0] <= 0.0f
                && support_toe_roll_seconds_[1] <= 0.0f;
            last_action_support_unload_side_ = rearm_support_unload_side(
                last_action_support_unload_side_, requested_side,
                release_windows_idle);
            // The assisted crouch lesson owns an explicitly classified foot-lift
            // helper below. Ordinary Human gait never consumes this selected
            // side or its timers: its unilateral contacts release only through
            // finite hip/knee/ankle geometry and outward normal velocity.
            if (left_contact && right_contact
                && release_windows_idle
                && requested_side != 0
                && requested_side != last_action_support_unload_side_)
            {
                if (raw_authority_telemetry_enabled_)
                    ++raw_authority_telemetry_.action_support_release_requests;
                if (requested_side < 0)
                {
                    support_toe_roll_seconds_[0] = 0.075f
                        * crouch_walk_guidance_authority;
                    support_cluster_release_hold_seconds_[0] = 0.145f
                        * crouch_walk_guidance_authority;
                }
                else
                {
                    support_toe_roll_seconds_[1] = 0.075f
                        * crouch_walk_guidance_authority;
                    support_cluster_release_hold_seconds_[1] = 0.145f
                        * crouch_walk_guidance_authority;
                }
                last_action_support_unload_side_ = requested_side;
            }
            if (support_cluster_release_hold_seconds_[0] > 0.0f)
                action_support_unload_side_ = -1;
            else if (support_cluster_release_hold_seconds_[1] > 0.0f)
                action_support_unload_side_ = 1;
            if (raw_authority_telemetry_enabled_
                && action_support_unload_side_ != 0)
                ++raw_authority_telemetry_.action_support_release_active_frames;
        }
        const int native_crouch_swing_side = guidance_assisted
            && course_stage_ == CourseStage::crouch_walk
            && blueprint_.human_casual_gait_plan()
            ? (!left_supported() && right_supported() ? -1
                : (left_supported() && !right_supported() ? 1 : 0))
            : 0;
        constexpr Vec2 gravity{ 0.0f, -22.0f };
        constexpr float damping_at_60_hz = 0.996f;
        {
            const AuthoritySnapshot before = authority_snapshot(dt);
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                Particle& particle = particles_[index];
                float local_damping = damping_at_60_hz;
                // Differential per-node drag changes the mass-weighted body
                // momentum whenever head, hands and feet have different X
                // velocities. Raw evaluation uses one isotropic coefficient
                // for every particle, so integration is a passive whole-rig
                // contraction of velocity rather than an implicit body drive.
                if (!raw_policy_audit() && !physical_moving_human_control)
                {
                    if (index == blueprint_.head_node)
                        local_damping = 0.92f;
                    else if (blueprint_.human_casual_gait_plan()
                        && course_stage_ == CourseStage::uneven
                        && manipulator_branch_node(blueprint_, index))
                        local_damping = 0.90f;
                    else if (passive_endpoint(blueprint_, index))
                        local_damping = 0.90f;
                    else if (blueprint_.is_support_seed(index))
                        local_damping = 0.985f;
                    else if (node_degree(blueprint_, index) == 1u)
                        local_damping = 0.975f;
                }
                local_damping = std::pow(local_damping, dt * 60.0f);
                const Vec2 velocity = (particle.position - particle.previous) * local_damping;
                particle.previous = particle.position;
                particle.position += velocity + gravity * (dt * dt);
            }
            record_authority_delta(AuthoritySource::integration, before, dt);
        }
        if (guidance_assisted
            && course_stage_ == CourseStage::crouch_walk
            && blueprint_.human_casual_gait_plan()
            && action_support_unload_side_ != 0)
        {
            const std::uint16_t swing_contact = action_support_unload_side_ < 0
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            if (valid_node(swing_contact))
            {
                Particle& foot = particles_[swing_contact];
                const float clearance = foot.position.y
                    - ground_height_at(foot.position.x) - foot.radius;
                constexpr float target_clearance = 0.16f;
                const float lift = crouch_walk_guidance_authority
                    * std::min(std::max(0.0f,
                        target_clearance - clearance),
                    std::min(0.018f, 0.55f * dt));
                foot.position.y += lift;
                // Retain half the generated upward velocity. The remaining
                // half is a damped contact-unload impulse, not a teleport of
                // the pelvis or the whole body.
                foot.previous.y += lift * 0.50f;
            }
        }

        stance_slip_distance_ = 0.0f;
        available_traction_ = 0.0f;
        used_traction_ = 0.0f;
        std::fill(support_traction_capacity_.begin(), support_traction_capacity_.end(), 0.0f);
        std::fill(support_traction_used_.begin(), support_traction_used_.end(), 0.0f);
        std::fill(support_normal_constraint_lambdas_.begin(),
            support_normal_constraint_lambdas_.end(), 0.0f);
        std::fill(support_contact_tangent_impulses_.begin(),
            support_contact_tangent_impulses_.end(), 0.0f);
        support_cluster_traction_capacity_.fill(0.0f);
        support_cluster_plate_traction_capacity_.fill(0.0f);
        support_cluster_toe_traction_capacity_.fill(0.0f);
        support_cluster_traction_used_.fill(0.0f);
        support_cluster_traction_impulse_.fill(0.0f);
        support_cluster_plate_membership_mask_.fill(0u);
        support_cluster_normal_impulse_.fill(0.0f);
        support_cluster_normal_impulse_moment_x_.fill(0.0f);
        support_cluster_normal_cop_x_.fill(0.0f);
        support_cluster_tangential_impulse_.fill(0.0f);
        support_friction_work_joules_ = 0.0f;
        support_friction_positive_work_joules_ = 0.0f;
        {
            const AuthoritySnapshot before = authority_snapshot(dt);
            apply_water_forces(dt);
            record_authority_delta(AuthoritySource::water, before, dt);
        }
        collided_this_step_ = false;
        duck_press_contact_this_step_ = false;
        duck_press_max_penetration_ = 0.0f;
        update_materials(dt);
        rebuild_course_features();
        append_dynamic_material_features();
        update_articulated_toe_commands(applied_actions, dt);
        powered_motor_lambdas_.fill(0.0f);
        powered_motor_positive_work_.fill(0.0f);
        coupled_motor_lambdas_.fill(0.0f);
        coupled_motor_positive_work_.fill(0.0f);
        const bool shared_physical_human_actuator =
            blueprint_.human_casual_gait_plan()
            && course_stage_ != CourseStage::balance
            && course_stage_ != CourseStage::duck_press;
        const float equipment_arm_pose_authority = course_stage_
                == CourseStage::crouch_walk
            ? crouch_walk_guidance_authority : 1.0f;
        const bool physical_equipment_arm_pose = guidance_assisted
            && equipment_arm_pose_authority > 0.0f
            && weapon_class_ != WeaponClass::none
            && blueprint_.presentation_species() == CreatureSpecies::human
            && blueprint_.human_casual_gait_plan()
            && (equipment_directive_ != EquipmentDirective::passive
                || course_stage_ == CourseStage::equipment_targets
                || course_stage_ == CourseStage::combat_course);
        const int solver_iterations = std::clamp(
            solver_iteration_count_, 1, 32);
        for (int iteration = 0; iteration < solver_iterations; ++iteration)
        {
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                for (const DistanceConstraint& bone : blueprint_.bones)
                    solve_distance(bone);
                record_authority_delta(
                    AuthoritySource::distance_constraints, before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                for (std::size_t index = 0;
                    index < blueprint_.active_motor_count; ++index)
                {
                    MotorConstraint motor = blueprint_.motors[index];
                    if (physical_equipment_arm_pose)
                    {
                        const MotorConstraint equipment_motor =
                            human_equipment_motor_envelope(motor, index);
                        motor.minimum_angle = lerp(motor.minimum_angle,
                            equipment_motor.minimum_angle,
                            equipment_arm_pose_authority);
                        motor.maximum_angle = lerp(motor.maximum_angle,
                            equipment_motor.maximum_angle,
                            equipment_arm_pose_authority);
                        motor.strength = lerp(motor.strength,
                            equipment_motor.strength,
                            equipment_arm_pose_authority);
                    }
                    if (raw_policy_audit() || shared_physical_human_actuator)
                    {
                        solve_powered_motor_xpbd(motor,
                            applied_actions[index], prior_applied_actions[index],
                            dt,
                            powered_motor_lambdas_[index],
                            powered_motor_positive_work_[index], index);
                    }
                    else
                        solve_motor(motor, applied_actions[index]);
                }
                std::size_t coupled_index{};
                for (const CoupledMotorConstraint& coupled :
                    blueprint_.coupled_support_motors())
                {
                    if (!coupled.motor.enabled
                        || coupled.source_action >= applied_actions.size())
                    {
                        ++coupled_index;
                        continue;
                    }
                    const float action = applied_actions[coupled.source_action]
                        * coupled.action_scale;
                    if (raw_policy_audit() || shared_physical_human_actuator)
                    {
                        solve_powered_motor_xpbd(coupled.motor, action,
                            prior_applied_actions[coupled.source_action]
                                * coupled.action_scale, dt,
                            coupled_motor_lambdas_[coupled_index],
                            coupled_motor_positive_work_[coupled_index],
                            coupled.source_action);
                    }
                    else
                        solve_motor(coupled.motor, action);
                    ++coupled_index;
                }
                record_authority_delta(AuthoritySource::joint_motors, before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                solve_articulated_toes();
                record_authority_delta(AuthoritySource::articulated_toe,
                    before, dt);
            }
            if (native_crouch_swing_side != 0
                && elapsed_seconds_ >= 1.0f)
            {
                const std::uint16_t swing_contact = native_crouch_swing_side < 0
                    ? blueprint_.left_contact_node : blueprint_.right_contact_node;
                for (std::size_t node = 0; node < particles_.size(); ++node)
                {
                    if (!contact_cluster_contains(swing_contact, node))
                        continue;
                    Particle& foot_seed = particles_[node];
                    const float target_y = ground_height_at(foot_seed.position.x)
                        + foot_seed.radius + 0.10f;
                    const float correction = crouch_walk_guidance_authority
                        * clamp(target_y
                            - foot_seed.position.y,
                        0.0f, (6.0f * dt) / 14.0f);
                    foot_seed.position.y += correction;
                    // This projection starts only after native collision has
                    // released the boot. Move Verlet history with it so the
                    // clearance target cannot inject locomotion energy; motor
                    // work and planted-foot reaction still own translation.
                    foot_seed.previous.y += correction;
                }
            }
            if (guidance_assisted
                && course_stage_ == CourseStage::crouch_walk
                && blueprint_.human_casual_gait_plan()
                && action_support_unload_side_ != 0)
            {
                const std::uint16_t swing_contact = action_support_unload_side_ < 0
                    ? blueprint_.left_contact_node : blueprint_.right_contact_node;
                if (valid_node(swing_contact))
                {
                    Particle& foot = particles_[swing_contact];
                    const float target_y = ground_height_at(foot.position.x)
                        + foot.radius + 0.12f;
                    // Project at a physical rate, divided across solver passes.
                    // A per-pass position cap made the same authored gait launch
                    // the body at 60/240 Hz while appearing stable at 20 Hz.
                    const float correction = crouch_walk_guidance_authority
                        * clamp(target_y - foot.position.y,
                            0.0f, (3.0f * dt) / 14.0f);
                    foot.position.y += correction;
                    // Split correction into a kinematic clearance projection
                    // and a bounded physical lift. This keeps the generated
                    // swing velocity invariant with the physics frequency;
                    // leaving all correction in position launched the 240 Hz
                    // body, while preserving all of it suppressed the step.
                    const float generated_lift = std::min(correction,
                        (0.40f * dt) / 14.0f);
                    foot.previous.y += correction - generated_lift;
                }
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                if (guidance_assisted)
                // Authored posture remains assisted composition only. A raw
                // Human must hold attitude through its policy joint torques.
                    stabilize_balance_posture();
                record_authority_delta(AuthoritySource::posture_helper,
                    before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                if (guidance_assisted)
                // The authored hand envelope is teacher-only; raw arms remain
                // the result of their four explicit network motor outputs.
                    stabilize_human_manipulator_envelope();
                record_authority_delta(AuthoritySource::manipulator_helper,
                    before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                if (guidance_assisted || blueprint_.human_casual_gait_plan())
                // Raw Human uses only the internal neck joint or travel stop.
                    stabilize_passive_appendages();
                record_authority_delta(AuthoritySource::passive_anatomy,
                    before, dt);
            }
            if (guidance_assisted)
            {
                stabilize_duck_posture();
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                solve_ground(dt);
                record_authority_delta(AuthoritySource::ground_contact,
                    before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                apply_ledge_training_scaffold();
                record_authority_delta(
                    AuthoritySource::ledge_training_scaffold, before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                solve_course(dt);
                record_authority_delta(AuthoritySource::course_collision,
                    before, dt);
            }
            // Re-apply the authored crouch after collision resolution so the
            // final solver state cannot leave an intermediate knee/body link
            // under the floor or inside the platen.
            if (guidance_assisted)
                stabilize_duck_posture();
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                solve_ground(dt);
                record_authority_delta(AuthoritySource::ground_contact,
                    before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                apply_ledge_training_scaffold();
                record_authority_delta(
                    AuthoritySource::ledge_training_scaffold, before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                solve_course(dt);
                record_authority_delta(AuthoritySource::course_collision,
                    before, dt);
            }
            // End each iteration in a floor-valid authored crouch. The target
            // is already clamped beneath the platen, so a final course shove is
            // unnecessary and would reintroduce solver-frame penetration.
            if (guidance_assisted)
                stabilize_duck_posture();
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                solve_ground(dt);
                record_authority_delta(AuthoritySource::ground_contact,
                    before, dt);
            }
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                separate_support_clusters();
                record_authority_delta(AuthoritySource::support_separation,
                    before, dt);
            }
            if (guidance_assisted
                && course_stage_ == CourseStage::duck_press)
                stabilize_duck_posture();
            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. Only the ground solver may establish
            // support; static friction retains its measured contact anchor while
            // moving stages release through bounded distance and speed gates.
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                solve_ground(dt);
                record_authority_delta(AuthoritySource::ground_contact,
                    before, dt);
            }
        }
        if ((raw_policy_audit() || shared_physical_human_actuator)
            && blueprint_.human_casual_gait_plan())
        {
            // Close the physical upper/lower-leg joints and the two anatomical
            // rear-foot triangles together with ground contact. Ending on a
            // per-node ground correction after the foot constraints can stretch
            // ankle-to-heel/ball by almost two percent, even while the ankle
            // motor reports its relative target reached; the visibly tilted
            // plate then becomes a toe-only pole. Every distance solve remains
            // mass-balanced and velocity-neutral. Unlike the assisted structure
            // projector this has no authored stance, reach, phase, or whole-body
            // translation target.
            constexpr int anatomical_convergence_passes = 8;
            for (int pass = 0; pass < anatomical_convergence_passes; ++pass)
            {
                const AuthoritySnapshot before = authority_snapshot(dt);
                for (const DistanceConstraint& bone : blueprint_.bones)
                {
                    bool anatomical_leg_or_foot_bone = false;
                    for (std::size_t motor_index = 0;
                        motor_index < 4u && motor_index
                            < blueprint_.active_motor_count; ++motor_index)
                    {
                        const MotorConstraint& motor =
                            blueprint_.motors[motor_index];
                        anatomical_leg_or_foot_bone = anatomical_leg_or_foot_bone
                            || (bone.a == motor.pivot && bone.b == motor.c)
                            || (bone.b == motor.pivot && bone.a == motor.c);
                    }
                    const auto in_foot = [](std::uint16_t node,
                        bool left) noexcept
                    {
                        return left
                            ? node == human_left_ankle_node
                                || node == human_left_heel_node
                                || node == human_left_ball_node
                                || node == human_left_toe_node
                            : node == human_right_ankle_node
                                || node == human_right_heel_node
                                || node == human_right_ball_node
                                || node == human_right_toe_node;
                    };
                    anatomical_leg_or_foot_bone = anatomical_leg_or_foot_bone
                        || (in_foot(bone.a, true) && in_foot(bone.b, true))
                        || (in_foot(bone.a, false) && in_foot(bone.b, false));
                    if (anatomical_leg_or_foot_bone)
                        solve_distance(bone, true);
                }
                record_authority_delta(AuthoritySource::final_leg_constraints,
                    before, dt);
                const AuthoritySnapshot before_ground = authority_snapshot(dt);
                solve_ground(dt);
                record_authority_delta(AuthoritySource::ground_contact,
                    before_ground, dt);
            }
        }
        // The iterative solver can otherwise reverse the toe between
        // stabilization and push-off every frame. Bound the final physical
        // hinge travel while preserving the contact and propulsion roles.
        {
            const AuthoritySnapshot before = authority_snapshot(dt);
            limit_articulated_toe_rates(dt);
            record_authority_delta(AuthoritySource::toe_rate_limit, before, dt);
        }
        apply_support_pressure(dt);
        {
            const AuthoritySnapshot before = authority_snapshot(dt);
            project_structure_rigid(dt);
            record_authority_delta(AuthoritySource::structure_projection,
                before, dt);
        }
        const std::array<bool, 2> support_at_step_end{
            left_supported(), right_supported() };
        // Static friction may form a new patch once a sliding, loaded stance
        // settles. Keep the old anchor throughout the solve and audit the slip
        // against it. Move only the anchor for the *next* step, never the foot
        // or its velocity, and do not re-anchor on a fresh touch or toe-off.
        if (shared_physical_human_actuator && raw_support_tangential_impulses_enabled_
            && stage_requires_forward_gait(course_stage_)
            && human_gait_coordinator_.completed_handoffs > 0u)
        {
            const bool stance_left = human_gait_coordinator_.outgoing_side > 0;
            const std::size_t side = stance_left ? 0u : 1u;
            if (support_at_step_start[side] && support_at_step_end[side])
            {
                for (const auto node : human_support_contact_nodes)
                {
                    if (!valid_node(node) || !particles_[node].grounded
                        || support_contact_latch_[node] == 0u
                        || !contact_cluster_contains(stance_left
                            ? blueprint_.left_contact_node : blueprint_.right_contact_node, node))
                        continue;
                    const Particle& particle = particles_[node];
                    const float x = particle.position.x;
                    const auto traction = contact_traction(terrain_firmness_at(x),
                        terrain_looseness_at(x), water_depth_at(x), particle.radius,
                        particle.inverse_mass, dt);
                    if (sliding_support_can_reacquire_static_anchor(traction,
                        x - support_contact_anchor_x_[node], (x - particle.previous.x) / dt,
                        support_normal_constraint_lambdas_[node] / dt,
                        support_contact_tangent_impulses_[node]))
                        support_contact_anchor_x_[node] = x;
                }
            }
        }
        for (std::size_t side = 0; side < support_at_step_end.size(); ++side)
        {
            support_cluster_contact_age_seconds_[side] =
                support_at_step_end[side]
                ? (support_at_step_start[side]
                    ? support_cluster_contact_age_seconds_[side] + dt : dt)
                : 0.0f;
        }
        const float structural_error = maximum_bone_length_error_ratio();
        record_maximum_bone_error();
        if (elapsed_seconds_ >= 0.10f
            && (!std::isfinite(structural_error) || structural_error > 0.020f))
            invalidate(InvalidMotion::structural_compression);
        if (stage_uses_deformable_terrain(course_stage_))
            terrain_.step(dt);
        update_material_metrics(dt);
        {
            const AuthoritySnapshot before = authority_snapshot(dt);
            update_equipment(actions, dt);
            record_authority_delta(AuthoritySource::equipment, before, dt);
        }
        update_human_support_feedback(dt);
        update_human_gait_coordinator(actions, dt);
        if (raw_authority_telemetry_enabled_)
        {
            for (std::size_t side = 0;
                side < support_cluster_traction_capacity_.size(); ++side)
            {
                raw_authority_telemetry_.support_traction_capacity[side]
                    = support_cluster_traction_capacity_[side];
                raw_authority_telemetry_.support_traction_used[side]
                    = support_cluster_traction_used_[side];
                raw_authority_telemetry_.support_normal_impulse[side]
                    = support_cluster_normal_impulse_[side];
                raw_authority_telemetry_.support_normal_impulse_moment_x[side]
                    = support_cluster_normal_impulse_moment_x_[side];
                raw_authority_telemetry_.support_normal_cop_x[side]
                    = support_cluster_normal_cop_x_[side];
                // The support cache describes the current solve. The audit
                // reports physical impulses from the whole fixed step, even
                // if a contact was subsequently released during convergence.
                const auto& contact_tangent = raw_authority_telemetry_
                    .support_contact_tangential_impulse;
                raw_authority_telemetry_.support_tangential_impulse[side] =
                    stage_requires_forward_gait(course_stage_)
                            && (raw_policy_audit() || shared_physical_human_actuator)
                            && blueprint_.human_casual_gait_plan()
                        ? contact_tangent[side * 3u] + contact_tangent[side * 3u + 1u]
                            + contact_tangent[side * 3u + 2u]
                        : support_cluster_tangential_impulse_[side];
            }
            raw_authority_telemetry_.friction_work_joules =
                support_friction_work_joules_;
            raw_authority_telemetry_.friction_positive_work_joules =
                support_friction_positive_work_joules_;
            raw_authority_telemetry_.course_motion_corrections =
                course_motion_enabled_ ? 1u : 0u;
            raw_authority_telemetry_.total_center_velocity_delta_x =
                authority_snapshot(dt).center_velocity_x
                    - authority_step_start.center_velocity_x;
            raw_authority_telemetry_.total_center_velocity_delta_y =
                authority_snapshot(dt).center_velocity_y
                    - authority_step_start.center_velocity_y;
        }
        update_climb_metrics(dt);
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
        const float step_direction = locomotion_direction();
        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;
        const float world_displacement = pelvis_position.x - previous_pelvis_.x;
        const float raw_world_speed = world_displacement / dt;
        const float odometer_progress = accepted_directed_odometer_progress(
            world_displacement, dt, step_direction);
        const float moving_course_speed = course_speed();
        const float raw_speed = raw_world_speed + moving_course_speed;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        const float frame_progress = terrain_relative_frame_progress(
            pelvis_position.x, previous_pelvis_.x, moving_course_speed, dt);
        previous_pelvis_ = pelvis_position;
        if (shuttle_enabled())
            shuttle_distance_travelled_ += odometer_progress;
        update_shuttle(pelvis_position.x, raw_world_speed, dt);
        distance_travelled_ = shuttle_enabled() ? shuttle_distance_travelled_
            : terrain_relative_distance(pelvis_position.x,
                blueprint_.nodes[blueprint_.root_node].x, course_progress());
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

        locomotion::Signals motion_signals{};
        const float step_lookahead_direction = std::abs(step_direction) >= 0.5f
            ? step_direction : facing_direction();
        motion_signals.uprightness = upright;
        motion_signals.root_x = pelvis_position.x;
        motion_signals.left_support_x = particles_[blueprint_.left_contact_node].position.x;
        motion_signals.right_support_x = particles_[blueprint_.right_contact_node].position.x;
        motion_signals.left_supported = contact_supported(blueprint_.left_contact_node);
        motion_signals.right_supported = contact_supported(blueprint_.right_contact_node);
        motion_signals.near_rise = ground_height_at(pelvis_position.x + 0.65f * step_lookahead_direction) - local_ground;
        motion_signals.mid_rise = ground_height_at(pelvis_position.x + 1.50f * step_lookahead_direction) - local_ground;
        motion_signals.far_rise = ground_height_at(pelvis_position.x + 3.00f * step_lookahead_direction) - local_ground;
        motion_signals.left_escape_rise = ground_height_at(pelvis_position.x - 0.85f) - local_ground;
        motion_signals.right_escape_rise = ground_height_at(pelvis_position.x + 0.85f) - local_ground;
        motion_signals.slope = terrain_.slope_at(
            terrain_sample_x(pelvis_position.x, course_progress()))
            * step_lookahead_direction;
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
        motion_signals.requested_direction = step_direction;
        motion_signals.turning = shuttle_enabled()
            && (shuttle_state_.phase == ShuttlePhase::turning
                || shuttle_state_.phase == ShuttlePhase::settling);
        motion_signals.dynamic_hazard_active = granular_hazard_active();
        motion_signals.dynamic_hazard_safe = granular_hazard_safe();
        motion_signals.zero_progress_seconds = zero_progress_seconds_;
        motion_signals.micro_motion_seconds = micro_motion_seconds_;
        motion_signals.hazard_stall_seconds = hazard_stall_seconds_;
        const float material_probe_x = pelvis_position.x
            + step_lookahead_direction * 0.75f;
        motion_signals.terrain_firmness = std::min(terrain_firmness_,
            terrain_firmness_at(material_probe_x));
        motion_signals.terrain_looseness = std::max(terrain_looseness_,
            terrain_looseness_at(material_probe_x));
        motion_signals.water_depth = water_depth_;
        motion_signals.water_submersion = water_submersion_;
        motion_signals.water_depth_ahead = water_depth_at(
            pelvis_position.x + step_lookahead_direction * 0.75f);
        motion_signals.saturated_depth_ahead = water_depth_at(
            pelvis_position.x + step_lookahead_direction
                * locomotion::shore_exit_lookahead_m);
        motion_signals.shore_exit_ahead = water_depth_
                > locomotion::wading_depth_threshold
            && water_depth_at(pelvis_position.x + step_lookahead_direction
                * locomotion::shore_exit_lookahead_m)
                <= locomotion::wading_depth_threshold;
        for (const CourseFeature& feature : course_features_)
        {
            const bool granular_block = feature.marker_sequence >= 50'000;
            if (feature.kind != CourseFeatureKind::moving_hazard
                && feature.kind != CourseFeatureKind::projectile
                && !granular_block)
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
                motion_signals.incoming_density = granular_block ? 0.95f
                    : feature.kind == CourseFeatureKind::moving_hazard
                        ? 0.90f : 0.65f;
            }
        }
        const locomotion::Plan motion_plan = locomotion::plan(motion_signals);
        const bool emergency_crawl = course_stage_ == CourseStage::moving_hazards
            && motion_plan.emergency_crawl;

        float recovery_reward = 0.0f;
        const bool supported = contact_supported(blueprint_.left_contact_node)
            || contact_supported(blueprint_.right_contact_node);
        const bool controlled_airborne_skill = powered_takeoff_
            && !supported && stage_allows_powered_airtime(course_stage_);
        if (!recovery_active_ && !controlled_airborne_skill && recovery_should_start(
            collided_this_step_, upright, geometric_fall, hard_fall && !emergency_crawl))
        {
            recovery_active_ = true;
            recovery_started_seconds_ = elapsed_seconds_;
            recovery_best_upright_ = upright;
            ++recovery_events_;
        }
        if (recovery_active_)
        {
            const float prior_recovery_best = recovery_best_upright_;
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            recovery_reward += std::max(0.0f,
                recovery_best_upright_ - prior_recovery_best) * 0.18f;
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && !geometric_fall && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
                recovery_reward += 0.12f;
            }
            else if ((hard_fall && !emergency_crawl)
                || recovery_time > (emergency_crawl ? 5.5f : 3.0f))
            {
                recovery_active_ = false;
                recovery_reward -= 0.12f;
            }
        }

        const float base_allowed_airtime = allowed_airtime_for_stage(
            course_stage_, powered_takeoff_);
        const bool aquatic_contact = water_depth_ > 0.02f
            || water_submersion_ > 0.05f
            || water_traversal_phase_ == WaterTraversalPhase::shore_exit;
        // The static press is a supported compression lesson. A short solver
        // contact flicker must not terminate the entire episode before the
        // platen reaches the rig; sustained loss of support still fails.
        const float allowed_airtime = course_stage_ == CourseStage::duck_press
            ? std::max(base_allowed_airtime, 0.75f)
            : aquatic_contact ? std::max(base_allowed_airtime, 1.80f)
            : base_allowed_airtime;
        const float gated_upright = elapsed_seconds_ > 0.25f ? upright : 1.0f;
        const bool terminal_fall = recovery_terminal_fall(
            geometric_fall, hard_fall && !emergency_crawl,
            recovery_active_ || emergency_crawl);
        InvalidMotion frame_gate = classify_motion_gate(gated_upright,
            maximum_speed_kmh_, pelvis_position, airborne_seconds_, allowed_airtime,
            micro_motion_seconds_, terminal_fall, course_stage_,
            current_airborne_rotation_ / (2.0f * pi));
        const bool duck_recovery_settling = course_stage_ == CourseStage::duck_press
            && duck_press_completed_ && duck_recovery_count_ >= 1u
            && elapsed_seconds_ - duck_walk_started_seconds_ <= 1.25f;
        if (course_stage_ == CourseStage::duck_press
            && (!duck_press_completed_ || duck_recovery_settling)
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed
                || frame_gate == InvalidMotion::collapsed_posture
                || frame_gate == InvalidMotion::fallen))
        {
            if (duck_recovery_settling
                && frame_gate == InvalidMotion::overspeed)
            {
                maximum_speed_kmh_ = std::min(49.0f,
                    std::abs(forward_speed_) * 3.6f);
            }
            frame_gate = InvalidMotion::none;
        }
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
            : gait_progress_multiplier(gait_cycles(), single_support, swing_clearance);
        const float directed_progress = frame_progress * motion_plan.direction;
        const float safe_progress = clamp(directed_progress, -0.015f, 0.065f);
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
            ? emergency_crawl
                ? (head_ground_contact() ? 0.10f : 0.012f)
                : (head_ground_contact() ? 0.16f : 0.08f)
            : 0.0f;
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
        const float target_speed = std::max(0.10f, motion_plan.target_speed);
        const bool reward_requires_locomotion = locomotion_required();
        // A planted request still has physical world speed. Multiplying that
        // speed by a zero direction made a moving stop sample look perfectly
        // settled to PPO and removed the only deceleration learning signal.
        const float signed_speed = motion_plan.direction == 0.0f
            ? forward_speed_ : forward_speed_ * motion_plan.direction;
        const float speed_quality = locomotion::target_speed_reward(motion_plan, signed_speed);
        const float run_reward = reward_requires_locomotion
            ? speed_quality * (0.0025f + gait * 0.0075f) : 0.0f;
        const float balance_reward = reward_requires_locomotion
            ? motion_plan.balance_reserve * 0.010f : 0.0f;
        const float stop_reward = locomotion_stop_requested_
            && motion_plan.intent == locomotion::Intent::hold
            ? speed_quality * 0.020f
                - std::abs(forward_speed_) * 0.020f : 0.0f;
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
        const bool paired_forward_gait = reward_requires_locomotion
            && blueprint_.paired_leg_chains();
        const bool human_stride_topology = paired_forward_gait
            && blueprint_.human_casual_gait_plan();
        float authored_leg_length = 0.0f;
        if (human_stride_topology)
        {
            for (std::size_t index = 0; index < 4u; ++index)
            {
                const MotorConstraint& motor = blueprint_.motors[index];
                if (valid_node(motor.pivot) && valid_node(motor.c))
                    authored_leg_length += length(blueprint_.nodes[motor.c]
                        - blueprint_.nodes[motor.pivot]);
            }
            authored_leg_length *= 0.5f;
        }
        const float measured_step_cadence =
            elapsed_seconds_ > forward_gait_quality_grace_seconds
            ? static_cast<float>(gait_cycles())
                / std::max(0.25f,
                    elapsed_seconds_ - forward_gait_quality_grace_seconds)
            : 0.0f;
        const CasualGaitEvidence casual_gait = casual_gait_evidence(
            authored_leg_length,
            std::abs(contact_cluster_center_x(blueprint_.left_contact_node)
                - contact_cluster_center_x(blueprint_.right_contact_node)),
            swing_clearance, measured_step_cadence, backward_brace_ratio_);
        const bool casual_gait_ready = human_stride_topology
            && gait_cycles() >= 2u
            && elapsed_seconds_ >= forward_gait_quality_grace_seconds + 0.50f;
        const bool physical_step_event = alternating_step_this_step_
            || single_leg_cycle_this_step_;
        const float casual_gait_reward = casual_gait_ready
            ? casual_gait.quality * 0.006f
                + (physical_step_event ? casual_gait.quality * 0.040f : 0.0f)
            : 0.0f;
        const float casual_gait_penalty = casual_gait_ready
            && physical_step_event
            ? (1.0f - casual_gait.quality) * 0.030f : 0.0f;
        const float real_step_reward = ((alternating_step_this_step_
                || single_leg_cycle_this_step_) ? 0.070f : 0.0f)
            + sagittal_crossing_shaping_reward(reward_requires_locomotion,
                paired_forward_gait, limb_crossing_this_step_)
            - lateral_crab_shaping_penalty(reward_requires_locomotion,
                paired_forward_gait, alternating_steps_, limb_crossings_,
                distance_travelled_, elapsed_seconds_, primary_support_span_ratio())
            - lower_leg_scissor_shaping_penalty(reward_requires_locomotion,
                paired_forward_gait, lower_leg_scissor_seconds_);
        const float unearned_progress_penalty = gait_cycles() == 0u
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
                last_reward_ = std::max(0.0f, upright) * (0.96f * dt)
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
                    + std::max(0.0f, upright) * (0.96f * dt)
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
        case CourseStage::shuttle:
        case CourseStage::uneven:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f + swing_reward + run_reward + real_step_reward
                + balance_reward + step_up_reward + brake_reward
                - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - stance_slip_penalty - wheel_penalty
                - controlled_overspeed_penalty - body_contact_penalty;
            break;
        case CourseStage::hurdles:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.011f
                + swing_reward + run_reward + real_step_reward
                + balance_reward + step_up_reward + brake_reward
                + duck_reward * 0.60f + jump_reward + obstacle_lift_reward
                + pass_reward - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - controlled_overspeed_penalty - body_contact_penalty;
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
                + balance_reward + step_up_reward + brake_reward + crawl_escape_reward
                + duck_reward * 0.45f + jump_reward + spin_reward
                + spin_landing_reward + obstacle_lift_reward + pass_reward
                + escape_reward
                - backward_penalty - unearned_progress_penalty - burial_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - controlled_overspeed_penalty - body_contact_penalty;
            break;
        case CourseStage::climb_descent:
        {
            const float ledge_distance = std::abs(ledge_left_edge_ - pelvis_position.x);
            const float approach = clamp(1.0f - ledge_distance / 8.0f, 0.0f, 1.0f);
            last_reward_ = std::max(0.0f, upright) * 0.010f
                + approach * 0.012f
                + static_cast<float>(hand_ledge_contacts_ > 0u) * 0.035f
                + static_cast<float>(ledge_climbs_ > 0u) * 0.080f
                + static_cast<float>(controlled_descents_ > 0u) * 0.120f
                + contact * 0.0010f
                - static_cast<float>(powered_jump_count_ > 0u) * 0.25f
                - collision_penalty - body_contact_penalty;
            break;
        }
        case CourseStage::equipment_targets:
        {
            const float desired = equipment_recommended_aim_angle();
            const float aim_quality = clamp(1.0f
                - std::abs(wrap_angle(desired - equipment_aim_angle_)) / 0.55f,
                0.0f, 1.0f);
            last_reward_ = std::max(0.0f, upright) * 0.012f
                + static_cast<float>(equipment_state_ == EquipmentState::safe_carry) * 0.006f
                + static_cast<float>(equipment_state_ == EquipmentState::ready) * 0.010f
                + aim_quality * 0.026f
                + static_cast<float>(target_hit_this_step_) * 0.55f
                - std::abs(forward_speed_) * 0.004f
                - body_contact_penalty;
            break;
        }
        case CourseStage::combat_course:
        {
            const float desired = equipment_recommended_aim_angle();
            const float aim_quality = clamp(1.0f
                - std::abs(wrap_angle(desired - equipment_aim_angle_)) / 0.70f,
                0.0f, 1.0f);
            last_reward_ = forward_gait_reward
                + std::max(0.0f, upright) * 0.010f
                + swing_reward + run_reward + real_step_reward
                + balance_reward + brake_reward + crawl_escape_reward
                + obstacle_lift_reward + pass_reward + escape_reward
                + static_cast<float>(equipment_state_ == EquipmentState::safe_carry) * 0.004f
                + static_cast<float>(equipment_state_ == EquipmentState::ready) * 0.006f
                + aim_quality * 0.015f
                + static_cast<float>(target_hit_this_step_) * 0.40f
                - backward_penalty - unearned_progress_penalty - burial_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - controlled_overspeed_penalty - body_contact_penalty;
            break;
        }
        }

        const float backward_brace_penalty = reward_requires_locomotion
            ? std::max(0.0f, backward_brace_ratio_ - 0.04f) * 0.32f : 0.0f;
        const float flight_penalty = unsupported_locomotion_penalty(
            reward_requires_locomotion, powered_takeoff_, airborne_seconds_);
        last_reward_ += recovery_reward + casual_gait_reward + stop_reward
            - casual_gait_penalty - uncontrolled_spin_penalty
            - backward_brace_penalty - flight_penalty;
        // Static crouch qualification explicitly requires grounded support,
        // a real press hold, feet-only ground contact, integrity, and recovery.
        // Do not let a flight reason recorded by the generic locomotion gate
        // terminate this supported compression lesson before those stronger
        // stage-specific checks can run. Every other invalid reason remains.
        if (course_stage_ == CourseStage::duck_press
            && (!duck_press_completed_ || duck_recovery_settling)
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
        const bool timed_out = elapsed_seconds_ >= trial_time_limit();
        const bool terminated = invalid_reason_ != InvalidMotion::none || timed_out;
        const TrialTerminalCause terminal_cause = invalid_reason_ != InvalidMotion::none
            ? TrialTerminalCause::invalid_motion
            : timed_out ? TrialTerminalCause::time_limit
            : TrialTerminalCause::none;
        return { last_reward_, forward_speed_, odometer_progress, terminated,
            invalid_reason_ == InvalidMotion::none, invalid_reason_, terminal_cause };
    }

    std::array<float, observation_count> Environment::observation() const noexcept
    {
        std::array<float, observation_count> result{};
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.left_contact_node) || !valid_node(blueprint_.right_contact_node))
            return result;

        static_assert(joint_angle_observation_index(0u) == 4u);
        static_assert(joint_angle_observation_index(7u) == 11u);
        static_assert(joint_velocity_observation_index(0u) == 12u);
        static_assert(joint_velocity_observation_index(7u) == 19u);
        static_assert(contact_observation_begin == 20u);
        static_assert(torso_turn_rate_observation == 63u);
        static_assert(joint_angle_observation_index(
            human_left_ankle_action) == left_ankle_angle_observation);
        static_assert(joint_angle_observation_index(
            human_right_ankle_action) == right_ankle_angle_observation);
        static_assert(joint_velocity_observation_index(
            human_left_ankle_action) == left_ankle_velocity_observation);
        static_assert(joint_velocity_observation_index(
            human_right_ankle_action) == right_ankle_velocity_observation);

        const Vec2 root = particles_[blueprint_.root_node].position;
        const float facing = facing_direction();
        Vec2 torso = normalized(
            particles_[blueprint_.torso_node].position - root, { 0.0f, 1.0f });
        torso.x *= facing;
        const Vec2 pelvis_velocity = particles_[blueprint_.root_node].position
            - particles_[blueprint_.root_node].previous;
        result[0] = torso.x;
        result[1] = torso.y;
        result[2] = clamp(pelvis_velocity.x * facing
            / last_step_dt_ / 6.0f, -3.0f, 3.0f);
        result[3] = clamp(pelvis_velocity.y / last_step_dt_ / 6.0f, -3.0f, 3.0f);
        for (std::size_t index = 0; index < anatomy_action_count; ++index)
        {
            if (index >= blueprint_.active_motor_count)
                continue;
            const MotorConstraint& motor = blueprint_.motors[index];
            if (!motor.enabled)
                continue;
            const float delta = wrap_angle(
                joint_angle(motor) * facing - motor.neutral_angle);
            const float span = delta < 0.0f
                ? std::max(0.001f, motor.neutral_angle - motor.minimum_angle)
                : std::max(0.001f, motor.maximum_angle - motor.neutral_angle);
            result[joint_angle_observation_index(index)] = clamp(
                delta / span, -2.0f, 2.0f);
            result[joint_velocity_observation_index(index)] = clamp(
                angular_velocities_[index] * facing / 18.0f, -3.0f, 3.0f);
        }
        result[20] = contact_supported(blueprint_.left_contact_node) ? 1.0f : 0.0f;
        result[21] = contact_supported(blueprint_.right_contact_node) ? 1.0f : 0.0f;
        result[22] = clamp((particles_[blueprint_.left_contact_node].position.x - root.x)
            * facing / 2.0f, -2.0f, 2.0f);
        result[23] = clamp((particles_[blueprint_.right_contact_node].position.x - root.x)
            * facing / 2.0f, -2.0f, 2.0f);
        result[24] = clamp((root.y - ground_height_at(root.x)) / 5.0f, 0.0f, 2.0f);
        result[25] = non_foot_grounded_ ? -1.0f
            : recovery_active_ ? clamp(torso.y, -1.0f, 1.0f) : 1.0f;
        const float requested_direction = locomotion_direction();
        const float lookahead_direction = std::abs(requested_direction) >= 0.5f
            ? requested_direction : facing_direction();
        result[26] = clamp(ground_height_at(root.x + 0.65f * lookahead_direction)
            - ground_height_at(root.x), -1.0f, 1.0f);
        result[27] = clamp(ground_height_at(root.x + 1.50f * lookahead_direction)
            - ground_height_at(root.x), -1.0f, 1.0f);
        result[28] = clamp(ground_height_at(root.x + 3.00f * lookahead_direction)
            - ground_height_at(root.x), -1.0f, 1.0f);

        const CourseFeature* nearest = nullptr;
        float nearest_dx = std::numeric_limits<float>::max();
        for (const CourseFeature& feature : course_features_)
        {
            const float dx = (feature.center.x - root.x) * lookahead_direction;
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
            case CourseFeatureKind::ledge: result[30] = 0.25f; break;
            case CourseFeatureKind::moving_hazard: result[30] = 0.5f; break;
            case CourseFeatureKind::projectile: result[30] = 1.0f; break;
            }
            result[31] = clamp((nearest->center.y - root.y) / 4.0f, -2.0f, 2.0f);
            result[32] = course_feature_observation_size(*nearest);
            result[33] = clamp(nearest->velocity.x * lookahead_direction / 5.0f, -1.0f, 1.0f);
        }
        result[34] = airborne_ratio();
        result[35] = clamp(static_cast<float>(gait_cycles()) / 10.0f, 0.0f, 2.0f);
        result[36] = static_cast<float>(course_stage_curriculum_index(course_stage_))
            / static_cast<float>(course_stage_count - 1);
        result[37] = course_difficulty_;
        const float gait_phase = elapsed_seconds_ * 2.0f * pi * (course_stage_
            == CourseStage::uneven ? gait_task_nominal_cadence_hz(gait_task_,
                elapsed_seconds_) : authored_foundational_gait_cadence_hz(blueprint_));
        result[38] = std::sin(gait_phase);
        result[39] = std::cos(gait_phase);
        result[40] = terrain_firmness_;
        result[41] = terrain_looseness_;
        result[42] = clamp(burial_depth_ / 0.80f, 0.0f, 2.0f);
        const float policy_direction = shuttle_enabled()
            || locomotion_stop_requested_ ? requested_direction
            : obstruction_mask_ != 0u && std::abs(free_space_direction_) >= 0.5f
                ? free_space_direction_ : requested_direction;
        result[43] = policy_direction * facing;
        result[44] = facing_direction();
        result[45] = shuttle_enabled()
            ? static_cast<float>(shuttle_phase())
                / 3.0f
            : course_stage_ == CourseStage::uneven
                ? static_cast<float>(gait_task_index(gait_task_))
                    / static_cast<float>(gait_task_count - 1u) : 0.0f;
        result[46] = clamp(incoming_material_velocity_.x * facing / 6.0f,
            -2.0f, 2.0f);
        result[47] = clamp(incoming_material_velocity_.y / 6.0f, -2.0f, 2.0f);
        result[48] = clamp(incoming_time_to_impact_ / 4.0f, 0.0f, 2.5f);
        result[49] = clamp(incoming_material_density_, 0.0f, 1.0f);
        result[50] = static_cast<float>(obstruction_mask_) / 7.0f;
        result[51] = clamp(terrain_.slope_at(
            terrain_sample_x(root.x, course_progress()))
            * lookahead_direction, -2.0f, 2.0f);
        result[52] = clamp(water_depth_ / 0.80f, 0.0f, 2.0f);
        result[53] = clamp(water_submersion_, 0.0f, 1.0f);
        result[54] = static_cast<float>(terrain_region_at(root.x))
            / static_cast<float>(TerrainRegion::hole);
        result[55] = static_cast<float>(equipment_state_)
            / static_cast<float>(equipment_state_count - 1u);
        result[56] = static_cast<float>(weapon_class_)
            / static_cast<float>(WeaponClass::launcher);
        if (equipment_target_.active)
        {
            const Vec2 target_delta = equipment_target_.position - root;
            result[57] = clamp(target_delta.x * facing / 16.0f, -2.0f, 2.0f);
            result[58] = clamp(target_delta.y / 8.0f, -2.0f, 2.0f);
            const float desired = equipment_recommended_aim_angle();
            result[59] = clamp(wrap_angle(desired - equipment_aim_angle_) / pi,
                -1.0f, 1.0f);
        }
        const WeaponProfile profile = weapon_profile(weapon_class_);
        result[60] = profile.cooldown_seconds > 0.0f
            ? clamp(equipment_cooldown_seconds_ / profile.cooldown_seconds,
                0.0f, 1.0f) : 0.0f;
        result[61] = clamp(static_cast<float>(target_hits_) / 5.0f,
            0.0f, 2.0f);
        result[62] = static_cast<float>(equipment_directive_)
            / static_cast<float>(equipment_directive_count - 1u);
        // A feed-forward policy can see torso attitude above, but cannot infer
        // whether the same pose is falling into or recovering from a rotation.
        // Publish the already-measured angular velocity in the local facing
        // frame so a raw controller can provide damping through joint actions.
        result[torso_turn_rate_observation] = clamp(
            torso_turn_speed_ * facing / 6.0f, -3.0f, 3.0f);
        if (blueprint_.human_casual_gait_plan())
        {
            const auto append_sole = [&](bool left) noexcept
            {
                const std::array<std::uint16_t, 3> nodes = left
                    ? std::array<std::uint16_t, 3>{ human_left_heel_node,
                        human_left_ball_node, human_left_toe_node }
                    : std::array<std::uint16_t, 3>{ human_right_heel_node,
                        human_right_ball_node, human_right_toe_node };
                float minimum_clearance = std::numeric_limits<float>::max();
                float maximum_clearance = std::numeric_limits<float>::lowest();
                Vec2 velocity_sum{};
                std::size_t velocity_count{};
                for (const std::uint16_t node : nodes)
                {
                    if (!valid_node(node))
                        continue;
                    const Particle& particle = particles_[node];
                    const Vec2 velocity = (particle.position
                        - particle.previous) / last_step_dt_;
                    const float clearance = human_sole_clearance(particle);
                    maximum_clearance = std::max(maximum_clearance, clearance);
                    minimum_clearance = std::min(minimum_clearance, clearance);
                    velocity_sum += velocity;
                    ++velocity_count;
                }
                if (!std::isfinite(minimum_clearance) || velocity_count == 0u)
                    return;
                const Vec2 sole_velocity = velocity_sum
                    / static_cast<float>(velocity_count);
                const std::size_t clearance_index = left
                    ? left_sole_clearance_observation
                    : right_sole_clearance_observation;
                const std::size_t velocity_x_index = left
                    ? left_sole_velocity_x_observation
                    : right_sole_velocity_x_observation;
                const std::size_t velocity_y_index = left
                    ? left_sole_velocity_y_observation
                    : right_sole_velocity_y_observation;
                const std::size_t maximum_clearance_index = left
                    ? left_sole_maximum_clearance_observation
                    : right_sole_maximum_clearance_observation;
                result[clearance_index] = clamp(
                    minimum_clearance / 0.25f, -2.0f, 2.0f);
                result[velocity_x_index] = clamp(
                    sole_velocity.x * facing / 6.0f, -3.0f, 3.0f);
                result[velocity_y_index] = clamp(
                    sole_velocity.y / 6.0f, -3.0f, 3.0f);
                result[maximum_clearance_index] = clamp(
                    maximum_clearance / 0.25f, -2.0f, 2.0f);
            };
            append_sole(true);
            append_sole(false);
            result[human_gait_phase_sine_observation] = std::sin(
                human_gait_coordinator_.phase);
            result[human_gait_phase_cosine_observation] = std::cos(
                human_gait_coordinator_.phase);
            result[human_gait_stage_observation] = static_cast<float>(
                human_gait_coordinator_.stage)
                / static_cast<float>(HumanGaitCoordinatorStage::count);
            result[human_gait_outgoing_side_observation] = static_cast<float>(
                human_gait_coordinator_.outgoing_side);
        }
        return result;
    }
}
