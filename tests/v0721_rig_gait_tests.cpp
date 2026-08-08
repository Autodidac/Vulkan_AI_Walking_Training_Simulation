#include "autonomy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"
#include "ui_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{
    using runner::sim::CreatureBlueprint;

    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    bool same_geometry(const CreatureBlueprint& lhs,
        const CreatureBlueprint& rhs) noexcept
    {
        if (lhs.nodes.size() != rhs.nodes.size()
            || lhs.radii.size() != rhs.radii.size()
            || lhs.bones.size() != rhs.bones.size()
            || lhs.active_motor_count != rhs.active_motor_count
            || lhs.root_node != rhs.root_node
            || lhs.torso_node != rhs.torso_node
            || lhs.head_node != rhs.head_node
            || lhs.left_contact_node != rhs.left_contact_node
            || lhs.right_contact_node != rhs.right_contact_node
            || lhs.additional_left_contact_nodes
                != rhs.additional_left_contact_nodes
            || lhs.additional_right_contact_nodes
                != rhs.additional_right_contact_nodes)
            return false;

        for (std::size_t index = 0; index < lhs.nodes.size(); ++index)
        {
            if (lhs.nodes[index].x != rhs.nodes[index].x
                || lhs.nodes[index].y != rhs.nodes[index].y
                || lhs.radii[index] != rhs.radii[index])
                return false;
        }
        for (std::size_t index = 0; index < lhs.bones.size(); ++index)
        {
            const auto& a = lhs.bones[index];
            const auto& b = rhs.bones[index];
            if (a.a != b.a || a.b != b.b
                || a.rest_length != b.rest_length
                || a.stiffness != b.stiffness)
                return false;
        }
        for (std::size_t index = 0; index < lhs.active_motor_count; ++index)
        {
            const auto& a = lhs.motors[index];
            const auto& b = rhs.motors[index];
            if (a.a != b.a || a.pivot != b.pivot || a.c != b.c
                || a.neutral_angle != b.neutral_angle
                || a.enabled != b.enabled)
                return false;
        }
        return true;
    }

    bool direct_bone(const CreatureBlueprint& rig,
        std::uint16_t a, std::uint16_t b) noexcept
    {
        return std::ranges::any_of(rig.bones,
            [a, b](const runner::sim::DistanceConstraint& bone)
            {
                return (bone.a == a && bone.b == b)
                    || (bone.a == b && bone.b == a);
            });
    }

    bool connected(const CreatureBlueprint& rig)
    {
        if (rig.nodes.empty() || rig.root_node >= rig.nodes.size())
            return false;
        std::vector<bool> visited(rig.nodes.size(), false);
        std::vector<std::uint16_t> stack{ rig.root_node };
        visited[rig.root_node] = true;
        while (!stack.empty())
        {
            const std::uint16_t node = stack.back();
            stack.pop_back();
            for (const auto& bone : rig.bones)
            {
                std::uint16_t next = static_cast<std::uint16_t>(rig.nodes.size());
                if (bone.a == node)
                    next = bone.b;
                else if (bone.b == node)
                    next = bone.a;
                if (next < rig.nodes.size() && !visited[next])
                {
                    visited[next] = true;
                    stack.push_back(next);
                }
            }
        }
        return std::ranges::all_of(visited, [](bool value) { return value; });
    }

    bool unique_supports(const CreatureBlueprint& rig)
    {
        std::vector<std::uint16_t> supports{};
        supports.push_back(rig.left_contact_node);
        supports.push_back(rig.right_contact_node);
        supports.insert(supports.end(), rig.additional_left_contact_nodes.begin(),
            rig.additional_left_contact_nodes.end());
        supports.insert(supports.end(), rig.additional_right_contact_nodes.begin(),
            rig.additional_right_contact_nodes.end());
        std::ranges::sort(supports);
        return std::adjacent_find(supports.begin(), supports.end()) == supports.end()
            && std::ranges::all_of(supports,
                [&rig](std::uint16_t node) { return node < rig.nodes.size(); });
    }

    bool motor_chains_are_real(const CreatureBlueprint& rig)
    {
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            const auto& motor = rig.motors[index];
            if (!motor.enabled || motor.a == motor.pivot
                || motor.pivot == motor.c || motor.a == motor.c
                || !direct_bone(rig, motor.a, motor.pivot)
                || !direct_bone(rig, motor.pivot, motor.c))
                return false;
        }
        return true;
    }

    bool no_support_brace(const CreatureBlueprint& rig)
    {
        return std::ranges::none_of(rig.bones,
            [&rig](const runner::sim::DistanceConstraint& bone)
            {
                return rig.is_support_seed(bone.a)
                    && rig.is_support_seed(bone.b);
            });
    }

    bool compact_side_view_biped(const CreatureBlueprint& rig)
    {
        if (!rig.paired_leg_chains())
            return false;
        const float foot_gap = std::abs(
            rig.nodes[rig.right_contact_node].x
            - rig.nodes[rig.left_contact_node].x);
        const float knee_gap = std::abs(
            rig.nodes[rig.motors[2].c].x
            - rig.nodes[rig.motors[0].c].x);
        return foot_gap <= 0.36f && knee_gap <= 0.26f;
    }
}

int main()
{
    using namespace runner;

    const std::array<CreatureBlueprint, 8> presets{
        CreatureBlueprint::scaffold(), CreatureBlueprint::chicken(),
        CreatureBlueprint::biped(), CreatureBlueprint::humanoid(),
        CreatureBlueprint::quadruped(), CreatureBlueprint::crawler4(),
        CreatureBlueprint::hexapod(), CreatureBlueprint::monoped()
    };
    for (const CreatureBlueprint& rig : presets)
    {
        require(rig.valid(), "shipped preset is structurally invalid");
        require(connected(rig), "shipped preset is disconnected");
        require(unique_supports(rig), "shipped preset reuses a semantic support");
        require(motor_chains_are_real(rig),
            "shipped preset has a fake parent-pivot-child motor chain");
        require(std::ranges::all_of(rig.nodes, [](Vec2 node)
            { return std::isfinite(node.x) && std::isfinite(node.y); }),
            "shipped preset contains non-finite geometry");
    }

    require(compact_side_view_biped(presets[0]),
        "scaffold is still authored as a frontal split");
    require(compact_side_view_biped(presets[1]),
        "chicken legs are still authored as a frontal split");
    require(compact_side_view_biped(presets[2]),
        "biped is still authored as a frontal split");
    require(compact_side_view_biped(presets[3]),
        "humanoid is still authored as a frontal split");

    const CreatureBlueprint& quadruped = presets[4];
    const CreatureBlueprint& crawler = presets[5];
    const CreatureBlueprint& hexapod = presets[6];
    require(quadruped.support_seed_count() == 4u
            && quadruped.active_motor_count == 8u
            && no_support_brace(quadruped),
        "quadruped is not four independent articulated legs");
    require(crawler.support_seed_count() == 4u
            && crawler.active_motor_count == 8u
            && no_support_brace(crawler),
        "four-leg crawler is not four independent articulated legs");
    require(hexapod.support_seed_count() == 6u
            && hexapod.active_motor_count >= 6u
            && no_support_brace(hexapod),
        "hexapod is not six independent tripod-phase supports");
    require(!quadruped.paired_leg_chains() && !crawler.paired_leg_chains()
            && !hexapod.paired_leg_chains(),
        "multi-support rigs were misclassified as two-leg bipeds");

    bool tuning_changed = false;
    for (std::uint64_t generation = 0; generation < 36u; ++generation)
    {
        const rl::RigMutationCandidate candidate =
            rl::automatic_rig_tuning_candidate(presets[3], generation);
        require(!candidate.topology_changed && candidate.activated_motor_mask == 0u,
            "automatic tuning changed topology or activated a body part");
        require(same_geometry(presets[3], candidate.blueprint),
            "automatic tuning changed limb length, nodes, supports, or topology");
        tuning_changed = tuning_changed || candidate.changed;
    }
    require(tuning_changed,
        "automatic controller tuning produced no usable parameter candidates");

    require(!sim::completes_side_view_crossing(false, 0.20f, 0.0f, 0.12f),
        "a foot that never began behind received crossing credit");
    require(!sim::completes_side_view_crossing(true, 0.20f, 0.0f, 0.03f),
        "a dragging foot received crossing credit");
    require(!sim::completes_side_view_crossing(true, 0.01f, 0.0f, 0.12f),
        "a foot that stayed behind received crossing credit");
    require(sim::completes_side_view_crossing(true, 0.20f, 0.0f, 0.12f),
        "real behind-to-ahead side-view crossing was rejected");

    sim::Environment quadruped_environment{ quadruped, 0x7214u };
    quadruped_environment.set_course(sim::CourseStage::uneven, 0.40f);
    const auto gait = rl::walking_teacher_action(quadruped_environment);
    float gait_energy = 0.0f;
    for (std::size_t index = 0; index < quadruped.active_motor_count; ++index)
        gait_energy += std::abs(gait[index]);
    require(gait_energy > 0.20f,
        "multi-support walking bootstrap remained a stationary balance action");

    for (const auto& size : ui_layout::validation_sizes)
        require(ui_layout::rig_lab_layout_valid(size[0], size[1]),
            "Rig Lab panel and viewport do not fit a supported window");

    std::cout << "Runner v0.7.21 rig, gait, preset, and Rig Lab tests passed\n";
    return EXIT_SUCCESS;
}
