#include "pixel_art.hpp"
#include "ppo.hpp"
#include "rig_training_diagnostic.hpp"
#include "simulation.hpp"
#include "species_art_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef RUNNER_SOURCE_DIR
#error RUNNER_SOURCE_DIR must be defined
#endif

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        struct ToeAuthorityEvidence
        {
            std::array<float, 2> commands{};
            Vec2 root_delta{};
            Vec2 torso_delta{};
        };

        struct RawTangentEvidence
        {
            std::array<float, 2> tangent{};
            float root_velocity{};
            float friction_work{};
        };

        struct RawCacheEvidence
        {
            float first_tangent{};
            float cleared_tangent{};
            float replanted_tangent{};
            std::uint8_t cleared_membership{};
        };

        struct OpposedPlateEvidence
        {
            std::array<float, 2> velocity_after{};
            std::array<float, 2> constraint_impulse{};
            float friction_work{};
        };

        static void set_elapsed_seconds(Environment& environment,
            float elapsed_seconds) noexcept
        {
            environment.elapsed_seconds_ = elapsed_seconds;
        }

        static void set_raw_torso_perturbation(Environment& environment,
            float angle, float angular_speed, int support_side = 0) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            const std::uint16_t root = environment.blueprint_.root_node;
            const std::uint16_t torso = environment.blueprint_.torso_node;
            const Vec2 pivot = environment.particles_[root].position;
            constexpr float dt = 1.0f / 60.0f;
            std::array<bool, 128> upper_body{};
            std::array<std::uint16_t, 128> stack{};
            std::size_t stack_size{};
            upper_body[root] = true;
            upper_body[torso] = true;
            stack[stack_size++] = torso;
            while (stack_size > 0u)
            {
                const std::uint16_t node = stack[--stack_size];
                for (const DistanceConstraint& bone : environment.blueprint_.bones)
                {
                    const std::uint16_t next = bone.a == node ? bone.b
                        : bone.b == node ? bone.a
                        : std::numeric_limits<std::uint16_t>::max();
                    if (next >= environment.particles_.size()
                        || upper_body[next])
                        continue;
                    upper_body[next] = true;
                    stack[stack_size++] = next;
                }
            }
            upper_body[root] = false;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!upper_body[index])
                    continue;
                Particle& particle = environment.particles_[index];
                const Vec2 segment = particle.position - pivot;
                particle.position = pivot + rotate(segment, angle);
                particle.previous = pivot
                    + rotate(segment, angle - angular_speed * dt);
            }
            environment.elapsed_seconds_ = 2.0f;
            environment.previous_torso_angle_ = environment.torso_roll_angle()
                - angular_speed * dt;
            environment.torso_turn_speed_ = angular_speed;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_support_seed(index))
                    continue;
                const bool left = environment.contact_cluster_contains(
                    environment.blueprint_.left_contact_node, index);
                const bool right = environment.contact_cluster_contains(
                    environment.blueprint_.right_contact_node, index);
                const bool supported = support_side == 0
                    || (support_side < 0 && left)
                    || (support_side > 0 && right);
                environment.particles_[index].grounded = supported;
                environment.support_contact_latch_[index] = supported ? 1u : 0u;
                environment.support_contact_anchor_x_[index] =
                    environment.particles_[index].position.x;
            }
        }

        static std::array<float, 2> step_raw_torso_action(
            Environment& environment,
            const std::array<float, action_count>& action) noexcept
        {
            for (std::size_t index = 0; index < anatomy_action_count; ++index)
                environment.previous_applied_actions_[index] = action[index];
            (void)environment.step(action);
            const CreatureBlueprint& rig = environment.blueprint_;
            Vec2 authored = rig.nodes[rig.torso_node] - rig.nodes[rig.root_node];
            authored.x *= environment.facing_direction();
            return {
                signed_angle(authored,
                    environment.particles_[rig.torso_node].position
                        - environment.particles_[rig.root_node].position),
                environment.torso_turn_speed_
            };
        }

        struct PlantResistanceEvidence
        {
            float displacement{};
            float speed_before{};
            float speed_after{};
        };

        static PlantResistanceEvidence displaced_human_plant_response(
            Environment& environment, float dt) noexcept
        {
            environment.set_course(CourseStage::shuttle, 0.30f);
            const std::uint16_t contact = environment.blueprint_.left_contact_node;
            Particle& foot = environment.particles_[contact];
            const float anchor = foot.position.x;
            foot.position.y = environment.ground_height_at(anchor)
                + ground_contact_offset(true, foot.radius);
            foot.previous = foot.position;
            foot.grounded = true;
            environment.support_contact_latch_[contact] = 1u;
            environment.support_contact_anchor_x_[contact] = anchor;

            foot.position.x += 0.14f;
            foot.previous.x = foot.position.x - 0.02f;
            foot.previous.y = foot.position.y + 0.60f * dt;
            const float speed_before = (foot.position.x - foot.previous.x) / dt;
            environment.solve_ground(dt);
            return {
                std::abs(foot.position.x - anchor),
                speed_before,
                (foot.position.x - foot.previous.x) / dt
            };
        }

        static float compressed_body_projection(Environment& environment,
            GuidanceMode mode) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_guidance_mode(mode);
            const CreatureBlueprint& rig = environment.blueprint_;
            const std::uint16_t left_knee = rig.motors[0].c;
            const std::uint16_t left_foot = rig.motors[1].c;
            const std::uint16_t right_knee = rig.motors[2].c;
            const std::uint16_t right_foot = rig.motors[3].c;
            for (Particle& particle : environment.particles_)
                particle.grounded = false;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (rig.is_support_seed(index))
                    environment.particles_[index].grounded = true;
            }
            const float support_y = 0.5f * (
                environment.particles_[left_foot].position.y
                + environment.particles_[right_foot].position.y);
            const float root_target_y = support_y + 0.48f;
            const float delta_y = root_target_y
                - environment.particles_[rig.root_node].position.y;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (index == left_knee || index == right_knee
                    || rig.is_support_seed(index))
                    continue;
                environment.particles_[index].position.y += delta_y;
                environment.particles_[index].previous.y += delta_y;
            }
            const Vec2 before = environment.particles_[rig.root_node].position;
            environment.project_structure_rigid(1.0f / 60.0f);
            return length(environment.particles_[rig.root_node].position - before);
        }

        static ToeAuthorityEvidence raw_toe_authority(
            Environment& environment,
            std::span<const float, anatomy_action_count> action,
            bool supported) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            environment.articulated_toe_commands_.fill(0.0f);
            for (std::size_t node = 0; node < environment.particles_.size(); ++node)
            {
                environment.particles_[node].grounded = supported
                    && environment.blueprint_.is_support_seed(node);
                environment.support_contact_latch_[node] = supported
                    && environment.blueprint_.is_support_seed(node) ? 1u : 0u;
            }
            const Vec2 root_before = environment.particles_[
                environment.blueprint_.root_node].position;
            const Vec2 torso_before = environment.particles_[
                environment.blueprint_.torso_node].position;
            environment.update_articulated_toe_commands(action, 1.0f / 60.0f);
            for (int iteration = 0; iteration < 14; ++iteration)
                environment.solve_articulated_toes();
            return {
                environment.articulated_toe_commands_,
                environment.particles_[environment.blueprint_.root_node].position
                    - root_before,
                environment.particles_[environment.blueprint_.torso_node].position
                    - torso_before
            };
        }

        static std::array<bool, 3> raw_single_toe_release(
            Environment& environment, float upward_speed = 0.60f,
            float separation = 0.0f) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            const CreatureBlueprint& rig = environment.blueprint_;
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                if (!rig.is_support_seed(index))
                    continue;
                Particle& particle = environment.particles_[index];
                particle.position.y = environment.ground_height_at(
                    particle.position.x)
                    + ground_contact_offset(true, particle.radius);
                particle.previous = particle.position;
                particle.grounded = true;
                environment.support_contact_latch_[index] = 1u;
                environment.support_contact_anchor_x_[index] =
                    particle.position.x;
            }
            Particle& toe = environment.particles_[human_left_toe_node];
            constexpr float dt = 1.0f / 60.0f;
            toe.position.y += separation;
            toe.previous = toe.position;
            toe.previous.y -= upward_speed * dt;
            environment.solve_ground(dt);
            return {
                environment.particles_[human_left_heel_node].grounded,
                environment.particles_[human_left_ball_node].grounded,
                environment.particles_[human_left_toe_node].grounded
            };
        }

        static RawTangentEvidence raw_local_tangent_response(
            Environment& environment, float left_speed, float right_speed,
            float non_support_speed) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            constexpr float dt = 1.0f / 60.0f;
            const CreatureBlueprint& rig = environment.blueprint_;
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                Particle& particle = environment.particles_[index];
                particle.previous = particle.position;
                particle.grounded = false;
                if (!rig.is_support_seed(index))
                {
                    particle.previous.x -= non_support_speed * dt;
                    continue;
                }
                const bool left = environment.contact_cluster_contains(
                    rig.left_contact_node, index);
                particle.position.y = environment.ground_height_at(
                    particle.position.x)
                    + ground_contact_offset(true, particle.radius);
                particle.previous = particle.position;
                particle.previous.x -= (left ? left_speed : right_speed) * dt;
                particle.previous.y += 0.40f * dt;
                particle.grounded = true;
                environment.support_contact_latch_[index] = 1u;
                environment.support_contact_anchor_x_[index] =
                    particle.position.x;
            }
            const std::uint16_t root = rig.root_node;
            environment.solve_ground(dt);
            return {
                environment.support_cluster_tangential_impulse_,
                (environment.particles_[root].position.x
                    - environment.particles_[root].previous.x) / dt,
                environment.support_friction_work_joules_
            };
        }

        static RawCacheEvidence raw_tangent_cache_replant(
            Environment& environment) noexcept
        {
            constexpr float dt = 1.0f / 60.0f;
            (void)raw_local_tangent_response(environment, 0.40f, 0.0f, 0.0f);
            const float first =
                environment.support_cluster_tangential_impulse_[0];
            const CreatureBlueprint& rig = environment.blueprint_;
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                if (!rig.is_support_seed(index)
                    || !environment.contact_cluster_contains(
                        rig.left_contact_node, index))
                    continue;
                Particle& particle = environment.particles_[index];
                particle.position.y += 0.10f;
                particle.previous = particle.position;
                particle.grounded = false;
                environment.support_normal_constraint_lambdas_[index] = 0.0f;
            }
            environment.solve_ground(dt);
            const float cleared =
                environment.support_cluster_tangential_impulse_[0];
            const std::uint8_t cleared_membership =
                environment.support_cluster_plate_membership_mask_[0];
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                if (!rig.is_support_seed(index)
                    || !environment.contact_cluster_contains(
                        rig.left_contact_node, index))
                    continue;
                Particle& particle = environment.particles_[index];
                particle.position.y = environment.ground_height_at(
                    particle.position.x)
                    + ground_contact_offset(true, particle.radius);
                particle.previous = particle.position;
                particle.previous.x += 0.40f * dt;
                particle.previous.y += 0.40f * dt;
                particle.grounded = true;
                environment.support_contact_latch_[index] = 1u;
                environment.support_normal_constraint_lambdas_[index] = 0.0f;
            }
            environment.solve_ground(dt);
            return { first, cleared,
                environment.support_cluster_tangential_impulse_[0],
                cleared_membership };
        }

        static OpposedPlateEvidence raw_opposed_plate_response(
            Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            constexpr float dt = 1.0f / 60.0f;
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                Particle& particle = environment.particles_[index];
                particle.previous = particle.position;
                particle.grounded = false;
                environment.support_contact_latch_[index] = 0u;
            }
            const std::array<std::uint16_t, 2> nodes{
                human_left_heel_node, human_left_ball_node };
            const std::array<float, 2> speeds{ 0.40f, -0.40f };
            for (std::size_t slot = 0; slot < nodes.size(); ++slot)
            {
                Particle& particle = environment.particles_[nodes[slot]];
                particle.position.y = environment.ground_height_at(
                    particle.position.x)
                    + ground_contact_offset(true, particle.radius);
                particle.previous = particle.position;
                particle.previous.x -= speeds[slot] * dt;
                particle.previous.y += 0.40f * dt;
                particle.grounded = true;
                environment.support_contact_latch_[nodes[slot]] = 1u;
                environment.support_contact_anchor_x_[nodes[slot]] =
                    particle.position.x;
            }
            environment.solve_ground(dt);
            OpposedPlateEvidence evidence{};
            for (std::size_t slot = 0; slot < nodes.size(); ++slot)
            {
                const Particle& particle = environment.particles_[nodes[slot]];
                evidence.velocity_after[slot] = (particle.position.x
                    - particle.previous.x) / dt;
                evidence.constraint_impulse[slot] =
                    environment.support_contact_tangent_impulses_[nodes[slot]];
            }
            evidence.friction_work = environment.support_friction_work_joules_;
            return evidence;
        }

        static std::array<float, 2> posture_projection_shifts(
            Environment& environment, GuidanceMode mode) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_guidance_mode(mode);
            const CreatureBlueprint& rig = environment.blueprint_;
            environment.particles_[rig.torso_node].position.x += 0.42f;
            environment.particles_[rig.torso_node].previous.x += 0.42f;
            environment.particles_[rig.head_node].position.x += 0.42f;
            environment.particles_[rig.head_node].previous.x += 0.42f;
            const Vec2 root_before = environment.particles_[rig.root_node].position;
            const Vec2 torso_before = environment.particles_[rig.torso_node].position;
            environment.stabilize_balance_posture();
            return {
                length(environment.particles_[rig.root_node].position - root_before),
                length(environment.particles_[rig.torso_node].position - torso_before)
            };
        }

        static std::array<float, 4> passive_appendage_conservation(
            Environment& environment) noexcept
        {
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            const CreatureBlueprint& rig = environment.blueprint_;
            environment.particles_[rig.head_node].position += Vec2{ 0.28f, -0.14f };
            environment.particles_[rig.head_node].previous += Vec2{ 0.28f, -0.14f };
            const auto weighted_state = [&]() noexcept
            {
                Vec2 center{};
                Vec2 momentum{};
                float total_mass{};
                for (const Particle& particle : environment.particles_)
                {
                    const float mass = 1.0f
                        / std::max(particle.inverse_mass, 1.0e-6f);
                    center += particle.position * mass;
                    momentum += (particle.position - particle.previous) * mass;
                    total_mass += mass;
                }
                return std::array<Vec2, 2>{
                    center / total_mass, momentum
                };
            };
            const auto before = weighted_state();
            const Vec2 root_before = environment.particles_[rig.root_node].position;
            const Vec2 torso_before = environment.particles_[rig.torso_node].position;
            const Vec2 torso_velocity_before =
                environment.particles_[rig.torso_node].position
                - environment.particles_[rig.torso_node].previous;
            environment.stabilize_passive_appendages();
            const auto after = weighted_state();
            const Vec2 torso_velocity_after =
                environment.particles_[rig.torso_node].position
                - environment.particles_[rig.torso_node].previous;
            return {
                length(environment.particles_[rig.root_node].position - root_before),
                length(environment.particles_[rig.torso_node].position - torso_before),
                length(after[0] - before[0]) + length(after[1] - before[1]),
                length(torso_velocity_after - torso_velocity_before)
            };
        }
    };
}

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.48 contact/art failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    struct PpmEvidence
    {
        int width{};
        int height{};
        std::size_t content{};
        int min_x{};
        int min_y{};
        int max_x{};
        int max_y{};
    };

    [[nodiscard]] PpmEvidence inspect_ppm(const std::filesystem::path& path)
    {
        std::ifstream input{ path };
        std::string magic{};
        int maximum{};
        PpmEvidence evidence{};
        input >> magic >> evidence.width >> evidence.height >> maximum;
        require(input.good() && magic == "P3" && maximum == 255,
            "species module is not a readable P3 image");
        evidence.min_x = evidence.width;
        evidence.min_y = evidence.height;
        evidence.max_x = -1;
        evidence.max_y = -1;
        for (int y = 0; y < evidence.height; ++y)
        {
            for (int x = 0; x < evidence.width; ++x)
            {
                int red{};
                int green{};
                int blue{};
                input >> red >> green >> blue;
                require(input.good(), "species module payload is truncated");
                if (red == 255 && green == 0 && blue == 255)
                    continue;
                ++evidence.content;
                evidence.min_x = std::min(evidence.min_x, x);
                evidence.min_y = std::min(evidence.min_y, y);
                evidence.max_x = std::max(evidence.max_x, x);
                evidence.max_y = std::max(evidence.max_y, y);
            }
        }
        return evidence;
    }

    constexpr std::size_t landing_sole_node_count = 3u;
    constexpr float landing_clearance_threshold = 0.005f;
    constexpr float landing_impulse_epsilon = 1.0e-5f;

    [[nodiscard]] constexpr std::string_view human_node_name(
        std::size_t node) noexcept
    {
        constexpr std::array<std::string_view, 19> names{
            "pelvis", "torso", "head", "left-knee", "left-ankle",
            "right-knee", "right-ankle", "left-shoulder", "left-elbow",
            "left-hand", "right-shoulder", "right-elbow", "right-hand",
            "left-heel", "left-ball", "left-toe", "right-heel",
            "right-ball", "right-toe"
        };
        return node < names.size() ? names[node] : "custom";
    }

    struct LandingEventFrame
    {
        std::array<std::array<bool, landing_sole_node_count>, 2> grounded{};
        std::array<std::array<float, landing_sole_node_count>, 2> clearance{};
        std::array<std::array<runner::Vec2, landing_sole_node_count>, 2> velocity{};
        std::array<float, 2> normal_impulse{};
        std::array<std::array<float, landing_sole_node_count>, 2>
            node_normal_impulse{};
        bool non_foot_contact{};
    };

    struct SwingLandingEvidence
    {
        bool landed{};
        std::size_t side{};
        std::size_t contact_node{};
        int unloaded_frames{};
        float minimum_clearance{};
        float maximum_speed_during_unload{};
        float landing_approach_speed{};
        float landing_approach_vertical_speed{};
        float landing_minimum_vertical_velocity{};
        float landing_maximum_vertical_velocity{};
        float landing_node_vertical_velocity{};
    };

    [[nodiscard]] bool finite_landing_frame(
        const LandingEventFrame& frame) noexcept
    {
        for (std::size_t side = 0; side < 2u; ++side)
        {
            if (!std::isfinite(frame.normal_impulse[side]))
                return false;
            for (std::size_t node = 0; node < landing_sole_node_count; ++node)
            {
                if (!std::isfinite(frame.clearance[side][node])
                    || !std::isfinite(frame.velocity[side][node].x)
                    || !std::isfinite(frame.velocity[side][node].y)
                    || !std::isfinite(frame.node_normal_impulse[side][node]))
                    return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool sole_physically_supported(
        const LandingEventFrame& frame, std::size_t side) noexcept
    {
        return std::ranges::any_of(frame.grounded[side],
            [](bool grounded) { return grounded; });
    }

    [[nodiscard]] bool sole_airborne(const LandingEventFrame& frame,
        std::size_t side) noexcept
    {
        return frame.normal_impulse[side] <= landing_impulse_epsilon
            && std::ranges::none_of(frame.grounded[side],
                [](bool grounded) { return grounded; });
    }

    [[nodiscard]] bool sole_all_nodes_clear(const LandingEventFrame& frame,
        std::size_t side) noexcept
    {
        return sole_airborne(frame, side)
            && std::ranges::all_of(frame.clearance[side], [](float clearance)
                { return clearance >= landing_clearance_threshold; });
    }

    class LandingEventDetector
    {
    public:
        [[nodiscard]] bool observe(const LandingEventFrame& frame) noexcept
        {
            const int frame_index = observed_frames_++;
            if (evidence_.landed)
                return true;

            if (!finite_landing_frame(frame))
            {
                if (clear_frames_ >= minimum_clear_frames)
                    ++armed_body_or_nonfinite_rejections_;
                ++nonfinite_frames_;
                reset_candidate();
                previous_valid_ = false;
                return false;
            }
            if (frame.non_foot_contact)
            {
                if (clear_frames_ >= minimum_clear_frames)
                    ++armed_body_or_nonfinite_rejections_;
                ++non_foot_contact_frames_;
                reset_candidate();
                previous_valid_ = false;
                return false;
            }

            const bool left_support = sole_physically_supported(frame, 0u);
            const bool right_support = sole_physically_supported(frame, 1u);
            if (pending_impact_)
            {
                const std::size_t side = pending_side_;
                const std::size_t opposite = 1u - side;
                const bool opposite_supported = sole_physically_supported(
                    frame, opposite);
                if (!opposite_supported)
                {
                    ++armed_side_change_rejections_;
                    ++unconfirmed_impact_rejections_;
                    reset_candidate();
                    previous_valid_ = false;
                    return false;
                }

                if (sole_physically_supported(frame, side))
                    ++pending_support_frames_;
                else
                    pending_support_frames_ = 0;
                ++pending_confirmation_frames_;
                if (pending_support_frames_ >= required_purchase_frames)
                {
                    evidence_.landed = true;
                    pending_impact_ = false;
                    return true;
                }
                if (pending_confirmation_frames_
                    >= maximum_confirmation_frames)
                {
                    ++unconfirmed_impact_rejections_;
                    reset_candidate();
                    previous_valid_ = false;
                }
                return false;
            }
            if (!left_support && !right_support)
            {
                // In particular, never interpret both-airborne as double support.
                if (clear_frames_ >= minimum_clear_frames)
                    ++armed_both_airborne_rejections_;
                reset_candidate();
                previous_valid_ = false;
                return false;
            }

            const int clear_side = clean_airborne_side(frame, true);
            if (clear_side >= 0)
            {
                if (active_side_ != clear_side)
                {
                    // A side change ends the old event. This frame may begin a
                    // new event, but it cannot complete or inherit from the old one.
                    if (clear_frames_ >= minimum_clear_frames)
                        ++armed_side_change_rejections_;
                    reset_candidate();
                    active_side_ = clear_side;
                }
                ++clear_frames_;
                maximum_clear_frames_observed_ = std::max(
                    maximum_clear_frames_observed_, clear_frames_);
                const std::size_t side = static_cast<std::size_t>(active_side_);
                for (std::size_t node = 0; node < landing_sole_node_count; ++node)
                {
                    minimum_clearance_ = std::min(minimum_clearance_,
                        frame.clearance[side][node]);
                    maximum_unload_speed_ = std::max(maximum_unload_speed_,
                        std::abs(frame.velocity[side][node].x));
                }
                previous_ = frame;
                previous_valid_ = true;
                return false;
            }

            if (active_side_ < 0)
            {
                previous_ = frame;
                previous_valid_ = true;
                return false;
            }

            const std::size_t side = static_cast<std::size_t>(active_side_);
            const std::size_t opposite = 1u - side;
            const bool clean_approach = sole_airborne(frame, side)
                && sole_physically_supported(frame, opposite);
            if (clean_approach)
            {
                // Once six fully-clear frames arm the event, the sole may enter
                // the final sub-5 mm approach without erasing its unload history.
                // This frame becomes the sole source of next-frame approach data.
                if (clear_frames_ < minimum_clear_frames)
                {
                    reset_candidate();
                    previous_valid_ = false;
                    return false;
                }
                previous_ = frame;
                previous_valid_ = true;
                return false;
            }

            const bool opposite_supported = sole_physically_supported(
                frame, opposite);
            if (clear_frames_ < minimum_clear_frames || !previous_valid_
                || !opposite_supported
                || frame.normal_impulse[side] <= 0.0f)
            {
                if (clear_frames_ >= minimum_clear_frames
                    && std::ranges::any_of(frame.grounded[side],
                        [](bool grounded) { return grounded; }))
                    ++armed_landing_without_impulse_rejections_;
                reset_candidate();
                previous_valid_ = false;
                return false;
            }

            std::size_t first_contact = landing_sole_node_count;
            for (std::size_t node = 0; node < landing_sole_node_count; ++node)
            {
                if (frame.node_normal_impulse[side][node] > 0.0f)
                {
                    first_contact = node;
                    break;
                }
            }
            if (first_contact == landing_sole_node_count)
            {
                if (first_node_impulse_rejection_frame_ < 0)
                {
                    first_node_impulse_rejection_frame_ = frame_index;
                    first_node_impulse_rejection_ = frame;
                    first_node_impulse_rejection_previous_ = previous_;
                }
                ++armed_landing_without_node_impulse_rejections_;
                reset_candidate();
                previous_valid_ = false;
                return false;
            }

            evidence_.landed = false;
            evidence_.side = side;
            evidence_.contact_node = first_contact;
            evidence_.unloaded_frames = clear_frames_;
            evidence_.minimum_clearance = minimum_clearance_;
            evidence_.maximum_speed_during_unload = maximum_unload_speed_;
            evidence_.landing_approach_speed = 0.0f;
            evidence_.landing_approach_vertical_speed = 0.0f;
            evidence_.landing_minimum_vertical_velocity =
                std::numeric_limits<float>::max();
            evidence_.landing_maximum_vertical_velocity =
                std::numeric_limits<float>::lowest();
            for (std::size_t node = 0; node < landing_sole_node_count; ++node)
            {
                const runner::Vec2 velocity = previous_.velocity[side][node];
                evidence_.landing_approach_speed = std::max(
                    evidence_.landing_approach_speed, std::abs(velocity.x));
                evidence_.landing_approach_vertical_speed = std::max(
                    evidence_.landing_approach_vertical_speed,
                    std::abs(velocity.y));
                evidence_.landing_minimum_vertical_velocity = std::min(
                    evidence_.landing_minimum_vertical_velocity, velocity.y);
                evidence_.landing_maximum_vertical_velocity = std::max(
                    evidence_.landing_maximum_vertical_velocity, velocity.y);
            }
            evidence_.landing_node_vertical_velocity =
                previous_.velocity[side][first_contact].y;
            const bool impact_ends_supported = sole_physically_supported(
                frame, side);
            reset_candidate();
            pending_impact_ = true;
            pending_side_ = side;
            pending_support_frames_ = impact_ends_supported ? 1 : 0;
            pending_confirmation_frames_ = 0;
            return false;
        }

        [[nodiscard]] const SwingLandingEvidence& evidence() const noexcept
        {
            return evidence_;
        }

        [[nodiscard]] std::array<int, 9> rejection_diagnostics() const noexcept
        {
            return {
                maximum_clear_frames_observed_,
                non_foot_contact_frames_,
                nonfinite_frames_,
                armed_body_or_nonfinite_rejections_,
                armed_both_airborne_rejections_,
                armed_side_change_rejections_,
                armed_landing_without_impulse_rejections_,
                armed_landing_without_node_impulse_rejections_,
                unconfirmed_impact_rejections_
            };
        }

        [[nodiscard]] int first_node_impulse_rejection_frame() const noexcept
        {
            return first_node_impulse_rejection_frame_;
        }

        [[nodiscard]] const LandingEventFrame&
        first_node_impulse_rejection() const noexcept
        {
            return first_node_impulse_rejection_;
        }

        [[nodiscard]] const LandingEventFrame&
        first_node_impulse_rejection_previous() const noexcept
        {
            return first_node_impulse_rejection_previous_;
        }

        static constexpr int minimum_clear_frames = 6;
        static constexpr int required_purchase_frames = 2;
        static constexpr int maximum_confirmation_frames = 3;

    private:
        [[nodiscard]] int clean_airborne_side(const LandingEventFrame& frame,
            bool require_full_clearance) const noexcept
        {
            int result = -1;
            for (std::size_t side = 0; side < 2u; ++side)
            {
                const std::size_t opposite = 1u - side;
                const bool clean = (require_full_clearance
                        ? sole_all_nodes_clear(frame, side)
                        : sole_airborne(frame, side))
                    && sole_physically_supported(frame, opposite);
                if (!clean)
                    continue;
                if (result >= 0)
                    return -1;
                result = static_cast<int>(side);
            }
            return result;
        }

        void reset_candidate() noexcept
        {
            active_side_ = -1;
            clear_frames_ = 0;
            minimum_clearance_ = std::numeric_limits<float>::max();
            maximum_unload_speed_ = 0.0f;
            pending_impact_ = false;
            pending_side_ = 0u;
            pending_support_frames_ = 0;
            pending_confirmation_frames_ = 0;
        }

        int active_side_{ -1 };
        int clear_frames_{};
        float minimum_clearance_{ std::numeric_limits<float>::max() };
        float maximum_unload_speed_{};
        LandingEventFrame previous_{};
        bool previous_valid_{};
        SwingLandingEvidence evidence_{};
        int maximum_clear_frames_observed_{};
        int non_foot_contact_frames_{};
        int nonfinite_frames_{};
        int armed_body_or_nonfinite_rejections_{};
        int armed_both_airborne_rejections_{};
        int armed_side_change_rejections_{};
        int armed_landing_without_impulse_rejections_{};
        int armed_landing_without_node_impulse_rejections_{};
        int unconfirmed_impact_rejections_{};
        int observed_frames_{};
        bool pending_impact_{};
        std::size_t pending_side_{};
        int pending_support_frames_{};
        int pending_confirmation_frames_{};
        int first_node_impulse_rejection_frame_{ -1 };
        LandingEventFrame first_node_impulse_rejection_{};
        LandingEventFrame first_node_impulse_rejection_previous_{};
    };

    [[nodiscard]] LandingEventFrame synthetic_airborne_frame(
        std::size_t swing_side, float horizontal_speed,
        float vertical_speed) noexcept
    {
        LandingEventFrame frame{};
        const std::size_t stance_side = 1u - swing_side;
        frame.normal_impulse[stance_side] = 0.5f;
        frame.grounded[stance_side][0] = true;
        for (std::size_t node = 0; node < landing_sole_node_count; ++node)
        {
            frame.clearance[swing_side][node] = 0.006f
                + static_cast<float>(node) * 0.001f;
            frame.velocity[swing_side][node] = {
                horizontal_speed + static_cast<float>(node) * 0.01f,
                vertical_speed - static_cast<float>(node) * 0.02f
            };
        }
        return frame;
    }

    [[nodiscard]] LandingEventFrame synthetic_landing_frame(
        std::size_t swing_side, std::size_t contact_node) noexcept
    {
        LandingEventFrame frame = synthetic_airborne_frame(
            swing_side, 0.0f, 0.0f);
        frame.grounded[swing_side][contact_node] = true;
        frame.clearance[swing_side][contact_node] = 0.0f;
        frame.normal_impulse[swing_side] = 0.25f;
        frame.node_normal_impulse[swing_side][contact_node] = 0.25f;
        return frame;
    }

    [[nodiscard]] LandingEventFrame synthetic_impact_frame(
        std::size_t swing_side, std::size_t contact_node) noexcept
    {
        LandingEventFrame frame = synthetic_airborne_frame(
            swing_side, 0.0f, 0.0f);
        frame.clearance[swing_side][contact_node] = 0.002f;
        frame.normal_impulse[swing_side] = 0.25f;
        frame.node_normal_impulse[swing_side][contact_node] = 0.25f;
        return frame;
    }

}

int main(int argc, char* argv[])
{
    using namespace runner;
    const bool support_transfer_only = argc == 2
        && std::string_view{argv[1]} == "--support-transfer-only";
    require(argc == 1 || support_transfer_only, "unknown contact/art test argument");

    const float positive_plane_correction =
        rl::finite_sole_alignment_correction({ 1.0f, 0.0f },
            rotate({ 1.0f, 0.0f }, 0.10f), { 0.0f, 0.01f });
    const float negative_plane_correction =
        rl::finite_sole_alignment_correction({ 1.0f, 0.0f },
            rotate({ 1.0f, 0.0f }, -0.10f), { 0.0f, -0.01f });
    require(positive_plane_correction > 0.0f
            && positive_plane_correction == -negative_plane_correction
            && positive_plane_correction
                == rl::finite_sole_alignment_correction({ 1.0f, 0.0f },
                    rotate({ 1.0f, 0.0f }, 0.10f), { 0.0f, 0.01f })
            && rl::finite_sole_alignment_correction({ 1.0f, 0.0f },
                { 1.0f, 0.0f }, { 0.0f, 10.0f }) == -0.12f
            && rl::finite_sole_alignment_correction({ 0.0f, 0.0f },
                { 1.0f, 0.0f }, { 0.0f, 0.0f }) == 0.0f
            && rl::finite_sole_alignment_correction(
                { std::numeric_limits<float>::quiet_NaN(), 0.0f },
                { 1.0f, 0.0f }, { 0.0f, 0.0f }) == 0.0f,
        "finite toe plane controller lost sign symmetry, repeatability, or adversarial bounds");
    {
        constexpr float ramp_start = sim::policy_control_ramp_start_seconds;
        constexpr float ramp_end = ramp_start
            + sim::policy_control_ramp_duration_seconds;
        require(sim::policy_control_ramp(-1.0f) == 0.0f
                && sim::policy_control_ramp(ramp_start) == 0.0f
                && sim::policy_control_ramp(ramp_end) == 1.0f
                && sim::policy_control_phase_seconds(ramp_start) == 0.0f
                && std::abs(sim::policy_control_phase_seconds(ramp_end)
                    - 0.625f) <= 1.0e-6f
                && std::abs(sim::policy_control_phase_seconds(20.0f)
                    - 19.025f) <= 1.0e-5f
                && sim::policy_control_phase_seconds(
                    std::numeric_limits<float>::quiet_NaN()) == 0.0f,
            "Raw actuator ramp and analytic phase integral lost their exact boundaries");
        float previous_phase{};
        for (int sample = 0; sample <= 200; ++sample)
        {
            const float time = static_cast<float>(sample) * 0.01f;
            const float phase = sim::policy_control_phase_seconds(time);
            require(phase + 1.0e-7f >= previous_phase
                    && phase <= time + 1.0e-7f,
                "Raw actuator-integrated phase clock reversed or outran physical control");
            previous_phase = phase;
        }

        require(sim::human_sole_has_flat_purchase(true, 0.0f)
                && sim::human_sole_has_flat_purchase(true,
                    sim::human_flat_sole_purchase_clearance)
                && !sim::human_sole_has_flat_purchase(true, 0.020f)
                && !sim::human_sole_has_flat_purchase(false, 0.0f)
                && !sim::human_sole_has_flat_purchase(true,
                    std::numeric_limits<float>::quiet_NaN()),
            "Human gait exchange accepted an airborne, toe-only, or invalid sole");
        const float hold_phase = pi
            - 0.5f * sim::human_gait_exchange_ease_window;
        const auto toe_only = sim::event_conditioned_human_gait_phase_step(
            hold_phase, 0.05f, true, false);
        const auto flat = sim::event_conditioned_human_gait_phase_step(
            hold_phase, 0.05f, true, true);
        require(toe_only.phase > hold_phase && toe_only.phase < flat.phase
                && toe_only.rate > 0.0f && toe_only.rate < 1.0f
                && flat.rate == 1.0f,
            "Human gait phase did not ease only for an unpurchased incoming sole");
        const auto held = sim::event_conditioned_human_gait_phase_step(
            pi - 1.0e-6f, 0.05f, true, false);
        const auto released = sim::event_conditioned_human_gait_phase_step(
            pi - 1.0e-6f, 0.05f, true, true);
        require(held.phase <= pi && held.rate < 1.0e-4f
                && released.phase > pi && released.rate == 1.0f,
            "Human gait phase crossed exchange without flat purchase or failed to release");
        float base_phase = 0.82f * pi;
        float mirrored_phase = base_phase + pi;
        float repeated_phase = base_phase;
        for (int step = 0; step < 120; ++step)
        {
            const bool right_flat = step >= 50;
            const auto base = sim::event_conditioned_human_gait_phase_step(
                base_phase, 0.018f, true, right_flat);
            const auto mirror = sim::event_conditioned_human_gait_phase_step(
                mirrored_phase, 0.018f, right_flat, true);
            const auto repeat = sim::event_conditioned_human_gait_phase_step(
                repeated_phase, 0.018f, true, right_flat);
            base_phase = base.phase;
            mirrored_phase = mirror.phase;
            repeated_phase = repeat.phase;
            const float expected_mirror = std::fmod(base_phase + pi,
                2.0f * pi);
            require(std::abs(mirrored_phase - expected_mirror) <= 1.0e-4f,
                "Human event-conditioned gait phase lost mirrored exchange timing");
            require(repeat.phase == base.phase && repeat.rate == base.rate,
                "Human event-conditioned gait phase lost repeated determinism");
        }
        const auto invalid = sim::event_conditioned_human_gait_phase_step(
            std::numeric_limits<float>::quiet_NaN(), 0.05f, true, true);
        require(invalid.phase == 0.0f && invalid.rate == 0.0f,
            "Human event-conditioned gait phase accepted a non-finite state");
        const float tracked_rate = sim::residual_paced_human_gait_rate(
            0.5f * pi, 0.02f, 0.01f, true, true);
        const float intermediate_rate = sim::residual_paced_human_gait_rate(
            0.5f * pi, 0.32f, 0.18f, true, true);
        const float saturated_rate = sim::residual_paced_human_gait_rate(
            0.5f * pi, 1.0f, 1.0f, true, true);
        const float unsupported_rate = sim::residual_paced_human_gait_rate(
            0.95f * pi, 0.02f, 0.01f, true, false);
        const float mirrored_rate = sim::residual_paced_human_gait_rate(
            1.95f * pi, 0.02f, 0.01f, false, true);
        require(tracked_rate == 1.0f
                && intermediate_rate < tracked_rate
                && intermediate_rate > saturated_rate
                && saturated_rate == sim::human_gait_minimum_residual_rate
                && unsupported_rate
                    == sim::human_gait_unsupported_exchange_rate_cap
                && mirrored_rate == unsupported_rate
                && sim::residual_paced_human_gait_rate(0.5f * pi,
                    std::numeric_limits<float>::quiet_NaN(), 0.0f, true, true)
                    == sim::human_gait_minimum_residual_rate,
            "Human residual-paced gait rate lost finite monotonic mirror bounds");
    }
    {
        sim::Environment falling_back{ sim::CreatureBlueprint::humanoid(),
            0x751A771u };
        sim::Environment falling_forward{ sim::CreatureBlueprint::humanoid(),
            0x751A771u };
        sim::Environment assisted{ sim::CreatureBlueprint::humanoid(),
            0x751A771u };
        sim::Environment backward_opposed{ sim::CreatureBlueprint::humanoid(),
            0x751A771u };
        sim::Environment forward_opposed{ sim::CreatureBlueprint::humanoid(),
            0x751A771u };
        sim::EnvironmentTestAccess::set_raw_torso_perturbation(
            falling_back, 0.12f, 0.80f, -1);
        sim::EnvironmentTestAccess::set_raw_torso_perturbation(
            falling_forward, -0.12f, -0.80f, -1);
        sim::EnvironmentTestAccess::set_raw_torso_perturbation(
            backward_opposed, 0.12f, 0.80f, -1);
        sim::EnvironmentTestAccess::set_raw_torso_perturbation(
            forward_opposed, -0.12f, -0.80f, -1);
        assisted.set_course(sim::CourseStage::uneven, 0.30f);
        const auto backward_response =
            rl::raw_human_torso_joint_balance_correction(falling_back);
        const auto forward_response =
            rl::raw_human_torso_joint_balance_correction(falling_forward);
        const auto assisted_response =
            rl::raw_human_torso_joint_balance_correction(assisted);
        const bool left_stance =
            backward_response[sim::human_left_ankle_action] != 0.0f;
        const std::size_t selected_stance_ankle = left_stance
            ? sim::human_left_ankle_action : sim::human_right_ankle_action;
        const std::size_t selected_swing_ankle = left_stance
            ? sim::human_right_ankle_action : sim::human_left_ankle_action;
        const float selected_support_sign = left_stance ? -1.0f : 1.0f;
        require(selected_support_sign
                    * backward_response[selected_stance_ankle] > 0.0f
                && selected_support_sign
                    * forward_response[selected_stance_ankle] < 0.0f
                && backward_response[selected_swing_ankle] == 0.0f
                && forward_response[selected_swing_ankle] == 0.0f,
            "signed Raw torso perturbations did not request the selected stance ankle");
        bool only_stance_ankle_changed = true;
        for (std::size_t index = 0;
            index < sim::anatomy_action_count; ++index)
        {
            if (index == selected_stance_ankle)
                continue;
            only_stance_ankle_changed = only_stance_ankle_changed
                && backward_response[index] == 0.0f
                && forward_response[index] == 0.0f;
        }
        const std::array<float, sim::action_count> zero_action{};
        const auto anatomy = [](const auto& action)
        {
            return std::span<const float, sim::anatomy_action_count>{
                action.data(), sim::anatomy_action_count };
        };
        const auto& raw_rig = falling_back.blueprint();
        const Vec2 endpoint_before = sim::requested_leg_endpoint(raw_rig,
            left_stance, anatomy(zero_action), falling_back.facing_direction());
        const Vec2 backward_endpoint = sim::requested_leg_endpoint(raw_rig,
            left_stance, anatomy(backward_response),
            falling_back.facing_direction());
        const Vec2 forward_endpoint = sim::requested_leg_endpoint(raw_rig,
            left_stance, anatomy(forward_response),
            falling_forward.facing_direction());
        require(std::ranges::all_of(assisted_response,
                    [](float value) { return value == 0.0f; })
                && only_stance_ankle_changed
                && length(backward_endpoint - endpoint_before) <= 1.0e-7f
                && length(forward_endpoint - endpoint_before) <= 1.0e-7f,
            "Raw torso response changed a hip/knee endpoint, swing ankle, or assisted action");
        const auto physical_balance_action = [](const sim::Environment& environment,
            bool oppose)
        {
            auto parameters = rl::anatomy_scaled_foundational_gait(
                environment.blueprint(), environment.gait_task(),
                environment.elapsed_seconds());
            rl::configure_raw_human_foundational_walk(parameters);
            parameters.direction = environment.locomotion_direction()
                * environment.facing_direction();
            const auto base = rl::biped_gait_teacher_action(
                environment, parameters);
            const auto balanced = rl::apply_raw_human_support_balance(
                environment, parameters, base);
            auto action = balanced.action;
            if (oppose && balanced.support_side != 0)
            {
                const std::size_t ankle = balanced.support_side < 0
                    ? sim::human_left_ankle_action
                    : sim::human_right_ankle_action;
                action[ankle] = clamp(base[ankle]
                    - (balanced.action[ankle] - base[ankle]), -1.0f, 1.0f);
            }
            return action;
        };
        const auto backward_action = physical_balance_action(
            falling_back, false);
        const auto backward_wrong = physical_balance_action(
            backward_opposed, true);
        const auto forward_action = physical_balance_action(
            falling_forward, false);
        const auto forward_wrong = physical_balance_action(
            forward_opposed, true);
        const auto backward_physical =
            sim::EnvironmentTestAccess::step_raw_torso_action(
                falling_back, backward_action);
        const auto backward_wrong_physical =
            sim::EnvironmentTestAccess::step_raw_torso_action(
                backward_opposed, backward_wrong);
        const auto forward_physical =
            sim::EnvironmentTestAccess::step_raw_torso_action(
                falling_forward, forward_action);
        const auto forward_wrong_physical =
            sim::EnvironmentTestAccess::step_raw_torso_action(
                forward_opposed, forward_wrong);
        require(std::abs(backward_physical[0])
                    < std::abs(backward_wrong_physical[0])
                && std::abs(forward_physical[0])
                    < std::abs(forward_wrong_physical[0])
                && backward_physical[1] < backward_wrong_physical[1]
                && forward_physical[1] > forward_wrong_physical[1],
            "signed Raw stance-ankle correction increased a coherent one-step physical torso fall");

    }
    {
        const auto seeds = diagnostics::walk_eye_candidate_seeds;
        require(seeds.front() == 0x00C0FFEEu && seeds.size() == 4u,
            "walk-eye candidate set changed outside its release contract");
        for (std::size_t left = 0; left < seeds.size(); ++left)
            for (std::size_t right = left + 1u; right < seeds.size(); ++right)
                require(seeds[left] != seeds[right],
                    "walk-eye candidate search contains a duplicate seed");
    }

    const std::filesystem::path root{ RUNNER_SOURCE_DIR };
    constexpr std::array species{ "chicken", "dog", "hexapod" };
    constexpr std::array parts{
        "body", "head", "tail", "upper_leg", "lower_leg", "foot"
    };
    for (const std::string_view animal : species)
    {
        for (const std::string_view part : parts)
        {
            const PpmEvidence evidence = inspect_ppm(root / "assets" / "optional"
                / "species_runtime" / (std::string{ animal } + "_"
                    + std::string{ part } + "_side.ppm"));
            const bool hex_body = animal == "hexapod" && part == "body";
            require(evidence.content >= static_cast<std::size_t>(
                    evidence.width * evidence.height / 12),
                "normalized species module is visually empty");
            require(evidence.min_y <= 2 && evidence.max_y >= evidence.height - 3
                    && evidence.max_x >= evidence.width - 3
                    && evidence.min_x <= (hex_body ? evidence.width / 6 : 2),
                "source-cell whitespace still shortens a species module");
        }
    }

    const art::SpeciesArtTopology chicken = art::species_art_topology(
        sim::CreatureSpecies::chicken);
    const art::SpeciesArtTopology dog = art::species_art_topology(
        sim::CreatureSpecies::dog);
    const art::SpeciesArtTopology hexapod = art::species_art_topology(
        sim::CreatureSpecies::hexapod);
    require(chicken.body_front == chicken.head_attachment
            && chicken.physical_tail && chicken.body_rear == 0u
            && chicken.tail_tip == 5u,
        "Chicken body/head/tail do not share physical attachment landmarks");
    require(dog.body_front == dog.head_attachment && !dog.physical_tail,
        "Dog head does not begin at the physical shoulder attachment");
    require(hexapod.body_front == hexapod.head_attachment,
        "Hexapod head does not begin at its physical body attachment");

    constexpr float amplitude = 0.08f;
    const float left_strike = rl::human_contralateral_arm_swing_offset(
        0.0f, amplitude, 1.0f);
    const float right_strike = rl::human_contralateral_arm_swing_offset(
        pi, amplitude, 1.0f);
    require(left_strike < 0.0f && right_strike > 0.0f
            && std::abs(left_strike + right_strike) < 1.0e-6f,
        "Human arms are not exactly opposed at contralateral heel strikes");
    require(std::abs(rl::human_contralateral_arm_swing_offset(
                0.0f, amplitude, -1.0f) + left_strike) < 1.0e-6f,
        "Human arm opposition does not mirror with travel direction");
    require(rl::human_contralateral_arm_swing_offset(
                std::numeric_limits<float>::quiet_NaN(), amplitude) == 0.0f,
        "non-finite arm phase produced a gait command");

    const sim::CreatureBlueprint persisted_human =
        sim::CreatureBlueprint::humanoid();
    require(persisted_human.human_paired_limb_topology()
            && persisted_human.nodes.size() == 19u
            && persisted_human.bones.size() == 23u
            && persisted_human.active_motor_count == 12u,
        "canonical Human does not own the complete 19-node/12-motor physical topology");
    const sim::Environment articulated_human{ persisted_human, 0x748F00u };
    const std::uint64_t authored_human_signature = persisted_human.signature();
    const std::uint64_t environment_human_signature =
        articulated_human.blueprint().signature();
    require(environment_human_signature
            == authored_human_signature
            && articulated_human.blueprint().nodes.size() == 19u
            && articulated_human.blueprint().bones.size() == 23u
            && articulated_human.blueprint().additional_left_contact_nodes.size() == 2u
            && articulated_human.blueprint().additional_right_contact_nodes.size() == 2u
            && articulated_human.blueprint().human_casual_gait_plan()
            && !articulated_human.blueprint().horizontal_multi_support_plan(),
        "Environment changed the canonical authored Human topology or signature: authored="
            + std::to_string(authored_human_signature) + " environment="
            + std::to_string(environment_human_signature));
    const sim::CreatureBlueprint& physical_human = articulated_human.blueprint();
    const auto solver_bottom = [&](std::uint16_t node)
    {
        return physical_human.nodes[node].y
            - sim::ground_contact_offset(true, physical_human.radii[node]);
    };
    require(std::abs(solver_bottom(sim::human_left_heel_node)
                - solver_bottom(sim::human_left_ball_node)) <= 1.0e-6f
            && std::abs(solver_bottom(sim::human_left_heel_node)
                - solver_bottom(sim::human_left_toe_node)) <= 1.0e-6f
            && std::abs(solver_bottom(sim::human_right_heel_node)
                - solver_bottom(sim::human_right_ball_node)) <= 1.0e-6f
            && std::abs(solver_bottom(sim::human_right_heel_node)
                - solver_bottom(sim::human_right_toe_node)) <= 1.0e-6f,
        "Human neutral soles do not share one solver-space contact plane");
    const auto authored_knee_cross = [&](std::size_t hip_index,
        std::size_t knee_index)
    {
        const sim::MotorConstraint& hip = physical_human.motors[hip_index];
        const sim::MotorConstraint& knee = physical_human.motors[knee_index];
        const Vec2 upper = physical_human.nodes[hip.c]
            - physical_human.nodes[hip.pivot];
        const Vec2 lower = physical_human.nodes[knee.c]
            - physical_human.nodes[knee.pivot];
        return upper.x * lower.y - upper.y * lower.x;
    };
    require(authored_knee_cross(0u, 1u) < -1.0e-4f
            && authored_knee_cross(2u, 3u) < -1.0e-4f,
        "Human neutral pose puts a knee on the backward-facing flexion branch");
    const auto require_rest_chain_round_trip = [&](bool left)
    {
        const std::size_t hip_index = left ? 0u : 2u;
        const std::size_t knee_index = left ? 1u : 3u;
        const std::size_t ankle_index = left
            ? sim::human_left_ankle_action
            : sim::human_right_ankle_action;
        const sim::MotorConstraint& hip = physical_human.motors[hip_index];
        const sim::MotorConstraint& knee = physical_human.motors[knee_index];
        const sim::MotorConstraint& ankle = physical_human.motors[ankle_index];
        const Vec2 authored_upper = physical_human.nodes[hip.c]
            - physical_human.nodes[hip.pivot];
        const Vec2 authored_lower = physical_human.nodes[knee.c]
            - physical_human.nodes[knee.pivot];
        const Vec2 authored_endpoint = physical_human.nodes[knee.c]
            - physical_human.nodes[hip.pivot];
        const float authored_cross = authored_upper.x * authored_lower.y
            - authored_upper.y * authored_lower.x;
        const rl::TwoLinkSagittalSolution solution =
            rl::solve_two_link_sagittal(length(authored_upper),
                length(authored_lower), authored_endpoint,
                authored_cross >= 0.0f ? -1.0f : 1.0f);
        require(solution.valid
                && length(solution.upper - authored_upper) <= 1.0e-4f
                && length(solution.lower - authored_lower) <= 1.0e-4f,
            "Human gait IK does not continuously reproduce its authored rest branch");
        const Vec2 hip_reference = physical_human.nodes[hip.a]
            - physical_human.nodes[hip.pivot];
        const Vec2 authored_foot = normalized(
            physical_human.nodes[ankle.c]
                - physical_human.nodes[ankle.pivot],
            { 1.0f, 0.0f });
        const std::array<float, 3> rest_actions{
            rl::motor_action_for_target_angle(hip,
                signed_angle(hip_reference, solution.upper)),
            rl::motor_action_for_target_angle(knee,
                signed_angle(-1.0f * solution.upper, solution.lower)),
            rl::motor_action_for_target_angle(ankle,
                signed_angle(-1.0f * solution.lower, authored_foot))
        };
        require(std::ranges::all_of(rest_actions, [](float action)
                { return std::abs(action) <= 1.0e-4f; }),
            "Human gait IK rest branch does not map to neutral joint authority");
    };
    require_rest_chain_round_trip(true);
    require_rest_chain_round_trip(false);
    const rl::BipedGaitParameters neutral_gait =
        rl::anatomy_scaled_foundational_gait(physical_human);
    require(std::abs(neutral_gait.stance_center_x) <= 1.0e-6f
            && rl::authored_gait_startup_blend(
                0.0f, neutral_gait.startup_seconds) == 0.0f
            && rl::authored_gait_startup_blend(
                neutral_gait.startup_seconds,
                neutral_gait.startup_seconds) == 1.0f
            && rl::authored_gait_startup_blend(
                1.0e-5f, neutral_gait.startup_seconds) < 1.0e-6f,
        "Human gait phase origin is not continuous in its controlled ankle frame");
    {
        const auto phase_at = [&](float elapsed_seconds)
        {
            return sim::policy_control_phase_seconds(elapsed_seconds)
                    * 2.0f * pi * neutral_gait.cadence_hz
                + neutral_gait.phase_offset;
        };
        float lower = sim::policy_control_ramp_start_seconds;
        float upper = lower + sim::policy_control_ramp_duration_seconds + 1.0f;
        for (int iteration = 0; iteration < 40; ++iteration)
        {
            const float middle = 0.5f * (lower + upper);
            if (phase_at(middle) < pi)
                lower = middle;
            else
                upper = middle;
        }
        sim::Environment before_boundary{ physical_human, 0x751A701u };
        sim::Environment after_boundary{ physical_human, 0x751A701u };
        for (sim::Environment* environment : {
                &before_boundary, &after_boundary })
        {
            environment->set_course(sim::CourseStage::uneven, 0.30f);
            environment->set_course_motion_enabled(false);
            environment->set_guidance_mode(
                sim::GuidanceMode::raw_policy_audit);
        }
        constexpr float boundary_epsilon = 1.0e-5f;
        sim::EnvironmentTestAccess::set_elapsed_seconds(before_boundary,
            lower - boundary_epsilon);
        sim::EnvironmentTestAccess::set_elapsed_seconds(after_boundary,
            upper + boundary_epsilon);
        const auto before_action =
            rl::raw_walking_teacher_action(before_boundary);
        const auto after_action = rl::raw_walking_teacher_action(after_boundary);
        for (std::size_t index = 0; index < sim::anatomy_action_count; ++index)
        {
            require(std::isfinite(before_action[index])
                    && std::isfinite(after_action[index])
                    && std::abs(before_action[index] - after_action[index])
                        <= 2.0e-3f,
                "Raw Human teacher has a joint-target jump at stance/swing exchange");
        }

        sim::Environment cycle_a{ physical_human, 0x751A702u };
        sim::Environment cycle_b{ physical_human, 0x751A702u };
        for (sim::Environment* environment : { &cycle_a, &cycle_b })
        {
            environment->set_course(sim::CourseStage::uneven, 0.30f);
            environment->set_course_motion_enabled(false);
            environment->set_guidance_mode(
                sim::GuidanceMode::raw_policy_audit);
        }
        rl::BipedGaitParameters raw_periodic_gait = neutral_gait;
        rl::configure_raw_human_foundational_walk(raw_periodic_gait);
        constexpr float periodic_sample_seconds = 3.0f;
        sim::EnvironmentTestAccess::set_elapsed_seconds(cycle_a,
            periodic_sample_seconds);
        sim::EnvironmentTestAccess::set_elapsed_seconds(cycle_b,
            periodic_sample_seconds + 1.0f / raw_periodic_gait.cadence_hz);
        const auto cycle_action_a = rl::raw_walking_teacher_action(cycle_a);
        const auto cycle_action_b = rl::raw_walking_teacher_action(cycle_b);
        for (std::size_t index = 0; index < sim::anatomy_action_count; ++index)
        {
            require(std::abs(cycle_action_a[index] - cycle_action_b[index])
                    <= 2.0e-4f,
                "Raw Human teacher joint targets are not one-cycle periodic");
        }
    }
    const auto has_bone = [&](std::uint16_t a, std::uint16_t b)
    {
        return std::ranges::any_of(physical_human.bones,
            [=](const sim::DistanceConstraint& bone)
            {
                return (bone.a == a && bone.b == b)
                    || (bone.a == b && bone.b == a);
            });
    };
    require(physical_human.additional_left_contact_nodes
                == std::vector<std::uint16_t>{ 14u, 15u }
            && physical_human.additional_right_contact_nodes
                == std::vector<std::uint16_t>{ 17u, 18u }
            && physical_human.left_contact_node == 13u
            && physical_human.right_contact_node == 16u
            && has_bone(3u, 4u) && has_bone(4u, 13u)
            && has_bone(4u, 14u) && has_bone(13u, 14u)
            && has_bone(14u, 15u)
            && !has_bone(3u, 13u) && !has_bone(3u, 14u)
            && has_bone(5u, 6u) && has_bone(6u, 16u)
            && has_bone(6u, 17u) && has_bone(16u, 17u)
            && has_bone(17u, 18u)
            && !has_bone(5u, 16u) && !has_bone(5u, 17u)
            && physical_human.node_support_mask(13u) == 0x1u
            && physical_human.node_support_mask(14u) == 0x1u
            && physical_human.node_support_mask(15u) == 0x1u
            && physical_human.node_support_mask(16u) == 0x2u
            && physical_human.node_support_mask(17u) == 0x2u
            && physical_human.node_support_mask(18u) == 0x2u
            && physical_human.motors[8].a == 3u
            && physical_human.motors[8].pivot == 4u
            && physical_human.motors[8].c == 14u
            && physical_human.motors[9].a == 5u
            && physical_human.motors[9].pivot == 6u
            && physical_human.motors[9].c == 17u,
        "Human physical feet lost ankle/rear-foot/toe anatomy, gained a knee shortcut, or crossed side/action ownership");
    const auto& physical_particles = articulated_human.particles();
    require(physical_particles.size() == physical_human.nodes.size()
            && physical_particles[13u].position.x
                < physical_particles[14u].position.x
            && physical_particles[14u].position.x
                < physical_particles[15u].position.x
            && physical_particles[16u].position.x
                < physical_particles[17u].position.x
            && physical_particles[17u].position.x
                < physical_particles[18u].position.x,
        "Human production particles no longer preserve both complete heel-ball-toe chains");
    require(articulated_human.equipment_mount_node_index() == 12u
            && length(articulated_human.equipment_display_position()
                - physical_particles[12u].position) <= 1.0e-6f,
        "Human weapon renderer mount detached from the physical wrist/hand endpoint");
    {
        sim::Environment raw_release{ physical_human, 0x751A703u };
        sim::Environment raw_release_repeat{ physical_human, 0x751A703u };
        sim::Environment raw_downward{ physical_human, 0x751A704u };
        sim::Environment raw_jitter{ physical_human, 0x751A705u };
        sim::Environment raw_penetrating{ physical_human, 0x751A706u };
        const auto contacts =
            sim::EnvironmentTestAccess::raw_single_toe_release(raw_release);
        const auto repeated =
            sim::EnvironmentTestAccess::raw_single_toe_release(
                raw_release_repeat);
        const auto downward =
            sim::EnvironmentTestAccess::raw_single_toe_release(
                raw_downward, -0.60f);
        const auto jitter =
            sim::EnvironmentTestAccess::raw_single_toe_release(
                raw_jitter, sim::moving_contact_release_speed_mps * 0.5f);
        const auto penetrating =
            sim::EnvironmentTestAccess::raw_single_toe_release(
                raw_penetrating, 0.60f, -2.0e-5f);
        require(contacts == repeated
                && contacts[0] && contacts[1] && !contacts[2],
            "one rising Raw toe released the entire load-bearing sole");
        require(downward[0] && downward[1] && downward[2]
                && jitter[0] && jitter[1] && jitter[2]
                && penetrating[0] && penetrating[1] && penetrating[2],
            "downward, jittering, or penetrating unilateral support escaped contact");
    }
    {
        sim::Environment moving_body{ physical_human, 0x751A710u };
        sim::Environment opposed_slip{ physical_human, 0x751A711u };
        sim::Environment opposed_repeat{ physical_human, 0x751A711u };
        sim::Environment cache_replant{ physical_human, 0x751A712u };
        sim::Environment rolling_plate{ physical_human, 0x751A713u };
        sim::Environment rolling_plate_repeat{ physical_human, 0x751A713u };
        const auto body =
            sim::EnvironmentTestAccess::raw_local_tangent_response(
                moving_body, 0.0f, 0.0f, 0.40f);
        const auto opposed =
            sim::EnvironmentTestAccess::raw_local_tangent_response(
                opposed_slip, 0.40f, -0.40f, 0.0f);
        const auto opposed_again =
            sim::EnvironmentTestAccess::raw_local_tangent_response(
                opposed_repeat, 0.40f, -0.40f, 0.0f);
        const auto cache =
            sim::EnvironmentTestAccess::raw_tangent_cache_replant(
                cache_replant);
        const auto rolling =
            sim::EnvironmentTestAccess::raw_opposed_plate_response(
                rolling_plate);
        const auto rolling_again =
            sim::EnvironmentTestAccess::raw_opposed_plate_response(
                rolling_plate_repeat);
        require(std::abs(body.tangent[0]) <= 1.0e-6f
                && std::abs(body.tangent[1]) <= 1.0e-6f
                && std::abs(body.root_velocity - 0.40f) <= 1.0e-6f,
            "stationary contacts globally braked unrelated body momentum");
        require(opposed.tangent == opposed_again.tangent
                && opposed.tangent[0] < -1.0e-5f
                && opposed.tangent[1] > 1.0e-5f
                && opposed.friction_work <= 1.0e-6f,
            "opposed local support slip did not receive deterministic dissipative friction");
        require(cache.first_tangent < -1.0e-5f
                && std::abs(cache.cleared_tangent) <= 1.0e-6f
                && cache.cleared_membership == 0u
                && cache.replanted_tangent > 1.0e-5f,
            "released/replanted support reused a stale tangent sign or membership");
        require(rolling.velocity_after == rolling_again.velocity_after
                && rolling.constraint_impulse
                    == rolling_again.constraint_impulse
                && rolling.constraint_impulse[0] > 1.0e-5f
                && rolling.constraint_impulse[1] < -1.0e-5f
                && std::abs(rolling.velocity_after[0]) < 0.40f
                && std::abs(rolling.velocity_after[1]) < 0.40f
                && rolling.friction_work <= 1.0e-6f,
            "opposed heel/ball slips were averaged instead of dissipated per contact");
    }

    std::array<float, sim::anatomy_action_count> first_swing{};
    std::array<float, sim::anatomy_action_count> second_swing{};
    for (std::size_t motor = 0; motor < sim::anatomy_action_count; ++motor)
    {
        first_swing[motor] = sim::rig_test_motor_input(
            sim::RigTestPattern::gait, motor, pi * 0.5f, 0.0f);
        second_swing[motor] = sim::rig_test_motor_input(
            sim::RigTestPattern::gait, motor, pi * 1.5f, 0.0f);
    }
    const int first_side = sim::action_requested_swing_side(
        articulated_human.blueprint(), first_swing);
    const int second_side = sim::action_requested_swing_side(
        articulated_human.blueprint(), second_swing);
    if (first_side == 0 || second_side != -first_side)
        std::cerr << "Action contact evidence: sides=" << first_side << ','
            << second_side << " heights="
            << sim::requested_leg_endpoint_height(articulated_human.blueprint(), true, first_swing)
            << ',' << sim::requested_leg_endpoint_height(articulated_human.blueprint(), false, first_swing)
            << " second=" << sim::requested_leg_endpoint_height(articulated_human.blueprint(), true, second_swing)
            << ',' << sim::requested_leg_endpoint_height(articulated_human.blueprint(), false, second_swing)
            << '\n';
    require(first_side != 0 && second_side == -first_side,
        "opposed motor geometry did not request alternating physical feet");
    std::array<float, sim::anatomy_action_count> neutral_actions{};
    require(sim::action_requested_swing_side(articulated_human.blueprint(),
                neutral_actions) == 0,
        "neutral motor geometry manufactured a timed swing-foot selection");
    require(sim::rearm_support_unload_side(-1, 0, true) == 0
            && sim::rearm_support_unload_side(1, 0, true) == 0,
        "neutral geometry did not rearm a completed physical unload");
    require(sim::rearm_support_unload_side(-1, 0, false) == -1
            && sim::rearm_support_unload_side(1, -1, true) == 1,
        "active release or a continuously requested side bypassed unload latching");
    require(sim::rearm_support_unload_side(-1, 1, true) == -1
            && sim::rearm_support_unload_side(1, -1, true) == 1,
        "opposed policy geometry rewrote history instead of earning its release");

    const art::HandArtDimensions fitted_hand = art::hand_art_dimensions(
        32.0f, 40, 28);
    require(fitted_hand.thickness <= 20.0f && fitted_hand.length <= 29.0f,
        "Human hand still scales larger than its fitted forearm");

    const art::OrientedArtTransform planted = art::support_boot_transform(
        { 0.0f, -1.0f }, { 0.2f, 0.0f }, 32.0f, 18.0f, 1.0f, true);
    require(std::abs(planted.beginning.y - planted.ending.y) < 1.0e-6f,
        "a grounded Human boot does not retain a horizontal sole");
    const art::OrientedArtTransform mirrored = art::support_boot_transform(
        { 0.0f, -1.0f }, { 0.2f, 0.0f }, 32.0f, 18.0f, -1.0f, true);
    require(planted.ending.x > planted.beginning.x
            && mirrored.ending.x < mirrored.beginning.x,
        "grounded boot orientation does not mirror with facing");
    const art::OrientedArtTransform physical = art::articulated_boot_transform(
        { 0.0f, 0.0f }, { 24.0f, -6.0f }, 30.0f, 14.0f, 1.0f);
    require(physical.ending.x > physical.beginning.x
            && physical.ending.y < physical.beginning.y,
        "Human boot art ignored the physical heel-to-toe pitch");
    const art::OrientedArtTransform physical_mirror = art::articulated_boot_transform(
        { 0.0f, 0.0f }, { -24.0f, -6.0f }, 30.0f, 14.0f, -1.0f);
    require(physical_mirror.ending.x < physical_mirror.beginning.x,
        "physical Human boot axis did not preserve reverse-facing orientation");

    for (const float hz : std::array{ 20.0f, 60.0f, 240.0f })
    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            static_cast<std::uint64_t>(0x7480u + static_cast<unsigned>(hz)) };
        const auto response =
            sim::EnvironmentTestAccess::displaced_human_plant_response(
                environment, 1.0f / hz);
        require(std::abs(response.displacement - 0.14f) < 1.0e-5f,
            "physical Human traction teleported a planted foot to an anchor");
        require(std::abs(response.speed_after)
                < std::abs(response.speed_before) - 1.0e-5f,
            "a normally loaded Human foot received no dissipative traction");
    }

    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            0x748A11u };
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        environment.set_course_motion_enabled(false);
        std::array<float, sim::action_count> zero_policy{};
        const auto assisted = rl::effective_policy_action(environment,
            zero_policy, sim::CourseStage::uneven, 0.0f,
            sim::GuidanceMode::assisted);
        const auto raw_zero = rl::effective_policy_action(environment,
            zero_policy, sim::CourseStage::uneven, 0.0f,
            sim::GuidanceMode::raw_policy_audit);
        require(std::all_of(assisted.begin(), assisted.end(), [](float value)
            {
                return value == 0.0f;
            }),
            "teacher-zero Assisted replay manufactured movement from a zero policy");
        require(assisted == raw_zero,
            "teacher-zero Assisted replay differs from Raw policy execution");

        const rl::GuidanceAuthorityReport assisted_report =
            rl::guidance_authority_report(environment,
                sim::CourseStage::uneven, 0.0f);
        require(assisted_report.lesson_teacher == 0.0f
                && assisted_report.topology_support == 0.0f
                && assisted_report.topology_body == 0.0f
                && assisted_report.posture_guide == 0.0f
                && !assisted_report.optional_guidance_active(),
            "teacher-zero Assisted replay retained hidden optional guidance");

        environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        const rl::GuidanceAuthorityReport raw_report =
            rl::guidance_authority_report(environment,
                sim::CourseStage::uneven, 1.0f);
        require(raw_report.mode == sim::GuidanceMode::raw_policy_audit
                && raw_report.mandatory_joint_cluster == 1.0f
                && raw_report.physical_contact == 1.0f
                && !raw_report.optional_guidance_active(),
            "raw policy audit retained optional teacher or reflex authority");

        std::array<float, sim::action_count> saturated_policy{};
        saturated_policy.fill(2.0f);
        const auto raw_saturated = rl::effective_policy_action(environment,
            saturated_policy, sim::CourseStage::uneven, 1.0f,
            sim::GuidanceMode::raw_policy_audit);
        const rl::ActionMask raw_mask = rl::action_mask_for(
            environment, sim::CourseStage::uneven);
        for (std::size_t index = 0; index < sim::action_count; ++index)
        {
            require(raw_saturated[index] == (raw_mask[index] ? 1.0f : 0.0f),
                "raw policy audit changed a policy output beyond clamp/topology-task masking");
        }
    }

    for (const std::uint64_t seed : std::array<std::uint64_t, 3>{
            0x748B10u, 0x748B11u, 0x748B12u })
    {
        sim::Environment grounded{ sim::CreatureBlueprint::humanoid(), seed };
        sim::Environment airborne{ sim::CreatureBlueprint::humanoid(), seed };
        sim::Environment repeated{ sim::CreatureBlueprint::humanoid(), seed };
        const auto grounded_toe = sim::EnvironmentTestAccess::raw_toe_authority(
            grounded, first_swing, true);
        const auto airborne_toe = sim::EnvironmentTestAccess::raw_toe_authority(
            airborne, first_swing, false);
        const auto repeated_toe = sim::EnvironmentTestAccess::raw_toe_authority(
            repeated, first_swing, true);
        require(grounded_toe.commands[0] == 0.0f
                && grounded_toe.commands[1] == 0.0f
                && grounded_toe.commands == airborne_toe.commands
                && grounded_toe.commands == repeated_toe.commands
                && length(grounded_toe.root_delta - airborne_toe.root_delta)
                    <= 1.0e-7f
                && length(grounded_toe.torso_delta - airborne_toe.torso_delta)
                    <= 1.0e-7f
                && length(grounded_toe.root_delta - repeated_toe.root_delta)
                    <= 1.0e-7f
                && length(grounded_toe.torso_delta - repeated_toe.torso_delta)
                    <= 1.0e-7f,
            "policy geometry, contact state, or repeated-seed order powered the raw passive toe hinge");

        std::array<float, sim::anatomy_action_count> zero_action{};
        sim::Environment neutral{ sim::CreatureBlueprint::humanoid(), seed };
        const auto neutral_toe = sim::EnvironmentTestAccess::raw_toe_authority(
            neutral, zero_action, true);
        require(neutral_toe.commands[0] == 0.0f
                && neutral_toe.commands[1] == 0.0f
                && length(neutral_toe.root_delta) <= 1.0e-7f
                && length(neutral_toe.torso_delta) <= 1.0e-7f,
            "zero policy output manufactured raw toe-powered body motion");
    }

    {
        sim::HumanSupportFeedback capture{};
        capture.valid = true;
        capture.total_normal_impulse = 12.0f;
        capture.capture_error_x = 0.20f;
        capture.load_asymmetry = 0.50f;
        const float forward_shift = rl::human_capture_endpoint_shift(
            capture, 1.0f, 2.0f, 0.40f);
        const float repeated_shift = rl::human_capture_endpoint_shift(
            capture, 1.0f, 2.0f, 0.40f);
        capture.capture_error_x = -0.20f;
        const float reverse_shift = rl::human_capture_endpoint_shift(
            capture, 1.0f, 2.0f, 0.40f);
        capture.capture_error_x = -0.10f;
        const float mirrored_shift = rl::human_capture_endpoint_shift(
            capture, -1.0f, 2.0f, 0.40f);
        capture.capture_error_x = 0.10f;
        const float canonical_shift = rl::human_capture_endpoint_shift(
            capture, 1.0f, 2.0f, 0.40f);
        require(std::abs(forward_shift - 0.18f) <= 1.0e-7f
                && repeated_shift == forward_shift
                && reverse_shift == -forward_shift
                && mirrored_shift == canonical_shift
                && canonical_shift > 0.0f,
            "Human capture endpoint correction lost sign, facing mirror, bound, or repeatability");

        const rl::HumanCaptureSupportWeights bilateral =
            rl::human_capture_support_weights(capture, true, true);
        const rl::HumanCaptureSupportWeights left_only =
            rl::human_capture_support_weights(capture, true, false);
        const rl::HumanCaptureSupportWeights right_only =
            rl::human_capture_support_weights(capture, false, true);
        const rl::HumanCaptureSupportWeights exclude_left =
            rl::human_capture_support_weights(capture, true, true, true, false);
        const rl::HumanCaptureSupportWeights excluded =
            rl::human_capture_support_weights(capture, true, true, true, true);
        const rl::HumanCaptureSupportWeights repeated_bilateral =
            rl::human_capture_support_weights(capture, true, true);
        sim::HumanSupportFeedback stale_asymmetry = capture;
        stale_asymmetry.load_asymmetry = 1.0f;
        const rl::HumanCaptureSupportWeights fallback_left =
            rl::human_capture_support_weights(
                stale_asymmetry, true, true, false, true);
        require(std::abs(bilateral.left - 0.25f) <= 1.0e-7f
                && std::abs(bilateral.right - 0.75f) <= 1.0e-7f
                && left_only.left == 1.0f && left_only.right == 0.0f
                && right_only.left == 0.0f && right_only.right == 1.0f
                && exclude_left.left == 0.0f && exclude_left.right == 1.0f
                && excluded.left == 0.0f && excluded.right == 0.0f
                && fallback_left.left == 1.0f
                && fallback_left.right == 0.0f
                && repeated_bilateral.left == bilateral.left
                && repeated_bilateral.right == bilateral.right,
            "Human capture endpoint correction ignored physical load/support ownership");

        sim::HumanSupportFeedback invalid = capture;
        invalid.valid = false;
        sim::HumanSupportFeedback nonfinite = capture;
        nonfinite.capture_error_x = std::numeric_limits<float>::quiet_NaN();
        require(rl::human_capture_endpoint_shift(
                    invalid, 1.0f, 2.0f, 0.40f) == 0.0f
                && rl::human_capture_endpoint_shift(
                    nonfinite, 1.0f, 2.0f, 0.40f) == 0.0f
                && rl::human_capture_endpoint_shift(
                    capture, 0.0f, 2.0f, 0.40f) == 0.0f
                && rl::human_capture_endpoint_shift(
                    capture, 1.0f, 0.0f, 0.40f) == 0.0f
                && rl::human_capture_endpoint_shift(
                    capture, 1.0f, 2.0f, 0.0f) == 0.0f
                && rl::human_capture_support_weights(
                    invalid, true, true).left == 0.0f,
            "Human capture endpoint correction invented authority from invalid evidence");

    }

    {
        const float overspeed_lift = rl::human_flare_vertical_lift(
            0.10f, -0.72f, 0.005f, -0.30f, 1.0f);
        const float repeated_lift = rl::human_flare_vertical_lift(
            0.10f, -0.72f, 0.005f, -0.30f, 1.0f);
        require(std::abs(overspeed_lift - 0.1176f) <= 1.0e-6f
                && repeated_lift == overspeed_lift,
            "physical flare velocity braking was cancelled or non-repeatable");
        require(rl::human_flare_vertical_lift(
                    0.10f, -0.20f, 0.005f, -0.30f, 1.0f) == 0.0f
                && rl::human_flare_vertical_lift(
                    0.10f, 0.20f, 0.005f, -0.30f, 1.0f) == 0.0f
                && rl::human_flare_vertical_lift(
                    0.10f, -0.72f, 0.005f, -0.30f, 0.0f) == 0.0f
                && rl::human_flare_vertical_lift(
                    std::numeric_limits<float>::quiet_NaN(), -0.72f,
                    0.005f, -0.30f, 1.0f) == 0.0f,
            "physical flare braking invented lift without finite descent/phase evidence");
        const float forward_brake = rl::human_flare_horizontal_correction(
            0.80f, 1.0f);
        const float repeated_brake = rl::human_flare_horizontal_correction(
            0.80f, 1.0f);
        const float mirrored_brake = rl::human_flare_horizontal_correction(
            -0.80f, 1.0f);
        require(std::abs(forward_brake - 0.18f) <= 1.0e-7f
                && repeated_brake == forward_brake
                && mirrored_brake == -forward_brake,
            "physical flare horizontal braking was not repeatable or mirrored");
        require(rl::human_flare_horizontal_correction(
                    0.80f, 0.0f) == 0.0f
                && rl::human_flare_horizontal_correction(
                    std::numeric_limits<float>::quiet_NaN(), 1.0f) == 0.0f
                && rl::human_flare_target_clearance >= 0.010f
                && rl::human_flare_target_velocity_y >= -0.35f
                && rl::human_flare_target_velocity_y <= -0.02f,
            "physical flare target escaped the observable landing contract");

        sim::HumanGaitCoordinatorState guard_state{};
        guard_state.active = true;
        guard_state.stage = sim::HumanGaitCoordinatorStage::descent;
        guard_state.outgoing_side = -1;
        guard_state.phase = pi + 0.80f * pi;
        sim::HumanGaitCoordinatorInput guard_input{};
        guard_input.moving = true;
        guard_input.facing_local_direction = 1.0f;
        guard_input.left_minimum_clearance = 0.0545f;
        guard_input.left_maximum_clearance = 0.0800f;
        guard_input.left_average_velocity_y = -0.207f;
        guard_input.left_terminal_alignment_distance = 0.1833f;
        const sim::HumanGaitCoordinatorState guarded =
            sim::step_human_gait_coordinator(guard_state, guard_input,
                0.10f, 1.0f / 60.0f);
        const sim::HumanGaitCoordinatorState repeated_guard =
            sim::step_human_gait_coordinator(guard_state, guard_input,
                0.10f, 1.0f / 60.0f);
        require(guarded.stage == sim::HumanGaitCoordinatorStage::flare
                && guarded.flare_clearance_guard > 0.080f
                && guarded.flare_clearance_guard < 0.090f
                && repeated_guard.flare_clearance_guard
                    == guarded.flare_clearance_guard
                && rl::human_flare_clearance_target(guarded)
                    == guarded.flare_clearance_guard,
            "terminal foot alignment did not retain a deterministic physical stopping clearance");
        sim::HumanGaitCoordinatorState mirrored_guard_state = guard_state;
        mirrored_guard_state.outgoing_side = 1;
        mirrored_guard_state.phase = 0.80f * pi;
        sim::HumanGaitCoordinatorInput mirrored_guard_input{};
        mirrored_guard_input.moving = true;
        mirrored_guard_input.facing_local_direction = -1.0f;
        mirrored_guard_input.right_minimum_clearance =
            guard_input.left_minimum_clearance;
        mirrored_guard_input.right_maximum_clearance =
            guard_input.left_maximum_clearance;
        mirrored_guard_input.right_average_velocity_y =
            guard_input.left_average_velocity_y;
        mirrored_guard_input.right_terminal_alignment_distance =
            guard_input.left_terminal_alignment_distance;
        const sim::HumanGaitCoordinatorState mirrored_guard =
            sim::step_human_gait_coordinator(mirrored_guard_state,
                mirrored_guard_input, 0.10f, 1.0f / 60.0f);
        require(mirrored_guard.flare_clearance_guard
                    == guarded.flare_clearance_guard,
            "terminal foot stopping clearance was not mirror invariant");
        guard_input.left_minimum_clearance = 0.070f;
        guard_input.left_maximum_clearance = 0.070f;
        guard_input.left_average_velocity_y = 0.10f;
        guard_input.left_maximum_abs_velocity_y = 0.10f;
        guard_input.left_maximum_velocity_y_deviation = 0.0f;
        guard_input.left_terminal_alignment_distance = 0.0f;
        const sim::HumanGaitCoordinatorState minimum_guard =
            sim::step_human_gait_coordinator(guard_state, guard_input,
                0.10f, 1.0f / 60.0f);
        guard_input.left_minimum_clearance = 0.0f;
        guard_input.left_maximum_clearance = 0.40f;
        guard_input.left_average_velocity_y = -0.20f;
        guard_input.left_maximum_abs_velocity_y = 0.25f;
        guard_input.left_maximum_velocity_y_deviation = 0.02f;
        guard_input.left_terminal_alignment_distance = 1.0f;
        const sim::HumanGaitCoordinatorState maximum_guard =
            sim::step_human_gait_coordinator(guard_state, guard_input,
                0.10f, 1.0f / 60.0f);
        require(minimum_guard.flare_clearance_guard
                    == rl::human_flare_target_clearance
                && maximum_guard.flare_clearance_guard == 0.100f,
            "terminal foot stopping clearance escaped its finite morphology band");
        guard_input.left_terminal_alignment_distance =
            std::numeric_limits<float>::quiet_NaN();
        const sim::HumanGaitCoordinatorState invalid_guard =
            sim::step_human_gait_coordinator(guard_state, guard_input,
                0.10f, 1.0f / 60.0f);
        sim::HumanGaitCoordinatorState invalid_target = guarded;
        invalid_target.flare_clearance_guard =
            std::numeric_limits<float>::quiet_NaN();
        require(!invalid_guard.active
                && rl::human_flare_clearance_target(invalid_target)
                    == rl::human_flare_target_clearance,
            "non-finite terminal alignment evidence reached a motor target");

        sim::HumanGaitCoordinatorState departure{};
        departure.active = true;
        departure.stage = sim::HumanGaitCoordinatorStage::departure;
        departure.outgoing_side = -1;
        departure.phase = pi;
        sim::HumanGaitCoordinatorInput departure_input{};
        departure_input.moving = true;
        departure_input.facing_local_direction = 1.0f;
        departure_input.right_supported = true;
        departure_input.right_maximum_clearance = 0.0f;
        departure_input.right_normal_impulse = 1.0f;
        departure_input.left_minimum_clearance = 0.0f;
        departure_input.left_maximum_clearance = 0.060f;
        departure_input.left_average_velocity_y = 0.20f;
        sim::HumanGaitCoordinatorState false_rocker = departure;
        for (int frame = 0; frame < 4; ++frame)
        {
            false_rocker = sim::step_human_gait_coordinator(false_rocker,
                departure_input, 0.10f, 1.0f / 60.0f);
        }
        require(false_rocker.stage
                    == sim::HumanGaitCoordinatorStage::departure
                && false_rocker.event_frames == 0u,
            "a heel-planted rocker was promoted to an all-node departure");

        departure_input.left_minimum_clearance = 0.006f;
        sim::HumanGaitCoordinatorState released = departure;
        sim::HumanGaitCoordinatorState released_repeat = departure;
        for (int frame = 0; frame < 3; ++frame)
        {
            released = sim::step_human_gait_coordinator(released,
                departure_input, 0.10f, 1.0f / 60.0f);
            released_repeat = sim::step_human_gait_coordinator(
                released_repeat, departure_input, 0.10f, 1.0f / 60.0f);
        }
        require(released.stage == sim::HumanGaitCoordinatorStage::swing
                && released.phase == pi
                && released_repeat.stage == released.stage
                && released_repeat.phase == released.phase
                && released_repeat.event_frames == released.event_frames,
            "three-frame all-node rising departure was not deterministic");

        departure_input.left_minimum_clearance = 0.0f;
        const sim::HumanGaitCoordinatorState replanted_heel =
            sim::step_human_gait_coordinator(released, departure_input,
                0.10f, 1.0f / 60.0f);
        require(replanted_heel.stage
                    == sim::HumanGaitCoordinatorStage::departure
                && replanted_heel.phase == pi
                && replanted_heel.event_frames == 0u
                && rl::human_departure_leg_shortening >= 0.030f
                && rl::human_departure_leg_shortening <= 0.060f,
            "a pre-apex heel recontact advanced instead of re-arming departure");

        sim::HumanGaitCoordinatorState apex{};
        apex.active = true;
        apex.stage = sim::HumanGaitCoordinatorStage::apex;
        apex.outgoing_side = -1;
        apex.phase = pi + 0.50f * pi;
        sim::HumanGaitCoordinatorInput apex_input{};
        apex_input.moving = true;
        apex_input.facing_local_direction = 1.0f;
        apex_input.left_minimum_clearance = 0.080f;
        apex_input.left_maximum_clearance = 0.085f;
        apex_input.right_supported = true;
        apex_input.right_normal_impulse = 1.0f;
        apex_input.left_average_velocity_y = -0.70f;
        apex_input.left_maximum_abs_velocity_y = 0.72f;
        apex_input.left_maximum_velocity_y_deviation = 0.02f;
        const sim::HumanGaitCoordinatorState overspeed_apex =
            sim::step_human_gait_coordinator(apex, apex_input,
                0.10f, 1.0f / 60.0f);
        require(overspeed_apex.stage == sim::HumanGaitCoordinatorStage::apex
                && overspeed_apex.event_frames == 0u
                && overspeed_apex.phase == apex.phase,
            "an overspeed ballistic apex was allowed to command more descent");
        apex_input.left_average_velocity_y = -0.18f;
        apex_input.left_maximum_abs_velocity_y = 0.20f;
        sim::HumanGaitCoordinatorState controlled_apex = apex;
        sim::HumanGaitCoordinatorState controlled_repeat = apex;
        for (int frame = 0; frame < 2; ++frame)
        {
            controlled_apex = sim::step_human_gait_coordinator(
                controlled_apex, apex_input, 0.10f, 1.0f / 60.0f);
            controlled_repeat = sim::step_human_gait_coordinator(
                controlled_repeat, apex_input, 0.10f, 1.0f / 60.0f);
        }
        require(controlled_apex.stage
                    == sim::HumanGaitCoordinatorStage::descent
                && controlled_repeat.stage == controlled_apex.stage
                && controlled_repeat.phase == controlled_apex.phase,
            "a controlled repeated-seed apex did not enter finite descent");

        sim::HumanGaitCoordinatorState left_descent{};
        left_descent.active = true;
        left_descent.outgoing_side = -1;
        left_descent.stage = sim::HumanGaitCoordinatorStage::descent;
        left_descent.phase = pi + 0.65f * pi;
        sim::HumanGaitCoordinatorState right_descent = left_descent;
        right_descent.outgoing_side = 1;
        right_descent.phase = 0.65f * pi;
        const float left_alignment = rl::human_landing_alignment_blend(
            left_descent, true);
        require(std::abs(left_alignment - 0.5f) <= 1.0e-6f
                && rl::human_landing_alignment_blend(
                    left_descent, true) == left_alignment
                && std::abs(rl::human_landing_alignment_blend(
                    right_descent, false) - left_alignment) <= 1.0e-6f
                && rl::human_landing_alignment_blend(
                    left_descent, false) == 0.0f,
            "descent ankle/toe alignment was non-repeatable, asymmetric, or applied to the stance sole");
        sim::HumanGaitCoordinatorState left_swing = left_descent;
        left_swing.stage = sim::HumanGaitCoordinatorStage::swing;
        left_swing.phase = pi + 0.25f * pi;
        sim::HumanGaitCoordinatorState right_swing = left_swing;
        right_swing.outgoing_side = 1;
        right_swing.phase = 0.25f * pi;
        const float left_terminal_alignment =
            rl::human_terminal_sole_alignment_blend(left_swing, true);
        require(std::abs(left_terminal_alignment - 0.5f) <= 1.0e-5f
                && rl::human_terminal_sole_alignment_blend(
                    left_swing, true) == left_terminal_alignment
                && std::abs(rl::human_terminal_sole_alignment_blend(
                    right_swing, false) - left_terminal_alignment) <= 1.0e-5f
                && rl::human_terminal_sole_alignment_blend(
                    left_swing, false) == 0.0f,
            "released Human sole did not begin deterministic mirrored alignment while safely airborne: left="
                + std::to_string(left_terminal_alignment)
                + " repeat=" + std::to_string(
                    rl::human_terminal_sole_alignment_blend(
                        left_swing, true))
                + " mirrored=" + std::to_string(
                    rl::human_terminal_sole_alignment_blend(
                        right_swing, false))
                + " wrong-side=" + std::to_string(
                    rl::human_terminal_sole_alignment_blend(
                        left_swing, false)));
        left_swing.phase = pi;
        require(rl::human_terminal_sole_alignment_blend(
                    left_swing, true) == 0.0f,
            "terminal sole alignment bypassed the physical departure event");
        left_swing.stage = sim::HumanGaitCoordinatorStage::apex;
        left_swing.phase = pi + 0.50f * pi;
        require(rl::human_terminal_sole_alignment_blend(
                    left_swing, true) == 1.0f,
            "terminal sole alignment did not converge before descent");
        left_descent.stage = sim::HumanGaitCoordinatorStage::flare;
        require(rl::human_landing_alignment_blend(
                    left_descent, true) == 1.0f,
            "physical flare did not retain full terrain alignment");

        sim::HumanGaitCoordinatorState left_landing = left_descent;
        left_landing.stage = sim::HumanGaitCoordinatorStage::landing;
        left_landing.phase = pi + 0.90f * pi;
        sim::HumanGaitCoordinatorState right_landing = left_landing;
        right_landing.outgoing_side = 1;
        right_landing.phase = 0.90f * pi;
        const float left_completion = rl::human_landing_completion_blend(
            left_landing, true);
        require(std::abs(left_completion - 0.5f) <= 1.0e-6f
                && rl::human_landing_completion_blend(
                    left_landing, true) == left_completion
                && std::abs(rl::human_landing_completion_blend(
                    right_landing, false) - left_completion) <= 1.0e-6f
                && rl::human_landing_completion_blend(
                    left_landing, false) == 0.0f,
            "final landing descent was non-repeatable, asymmetric, or applied to the stance sole");

        sim::HumanGaitCoordinatorState recontact{};
        recontact.active = true;
        recontact.stage = sim::HumanGaitCoordinatorStage::flare;
        recontact.outgoing_side = -1;
        recontact.phase = pi + 0.80f * pi;
        sim::HumanGaitCoordinatorInput purchase_input{};
        purchase_input.moving = true;
        purchase_input.facing_local_direction = 1.0f;
        purchase_input.left_supported = true;
        purchase_input.right_supported = true;
        purchase_input.left_minimum_clearance = 0.0f;
        purchase_input.right_minimum_clearance = 0.0f;
        purchase_input.left_maximum_clearance = 0.005f;
        purchase_input.right_maximum_clearance = 0.005f;
        purchase_input.left_normal_impulse = 1.0f;
        purchase_input.right_normal_impulse = 1.0f;
        purchase_input.left_heel_normal_impulse = 0.25f;
        purchase_input.right_heel_normal_impulse = 0.25f;
        const sim::HumanGaitCoordinatorState landed =
            sim::step_human_gait_coordinator(recontact, purchase_input,
                0.10f, 1.0f / 60.0f);
        require(landed.stage == sim::HumanGaitCoordinatorStage::landing
                && landed.outgoing_side == recontact.outgoing_side
                && landed.event_frames == 0u,
            "physical bilateral recontact remained stranded in flare");

        auto light_landing_input = purchase_input;
        light_landing_input.left_normal_impulse = 0.49f;
        light_landing_input.right_normal_impulse = 4.60f;
        const auto light_landing = sim::step_human_gait_coordinator(landed,
            light_landing_input, pi, 1.0f / 60.0f);
        require(light_landing.stage == sim::HumanGaitCoordinatorStage::purchase
                && light_landing.event_frames == 0u,
            "first unloaded landing was counted as a load-transfer receipt");
        auto invalid_load = purchase_input;
        invalid_load.left_normal_impulse = std::numeric_limits<float>::quiet_NaN();
        require(!sim::step_human_gait_coordinator(light_landing,
                invalid_load, 0.10f, 1.0f / 60.0f).active,
            "nonfinite landing load authorized support transfer");

        sim::HumanGaitCoordinatorState purchasing =
            sim::step_human_gait_coordinator(landed, purchase_input,
                pi, 1.0f / 60.0f);
        sim::HumanGaitCoordinatorState purchasing_repeat =
            sim::step_human_gait_coordinator(landed, purchase_input,
                pi, 1.0f / 60.0f);
        require(purchasing.stage == sim::HumanGaitCoordinatorStage::purchase
                && purchasing.outgoing_side == -1
                && purchasing.event_frames == 1u
                && purchasing_repeat.stage == purchasing.stage
                && purchasing_repeat.phase == purchasing.phase
                && purchasing_repeat.event_frames == purchasing.event_frames,
            "first flat landing receipt skipped or failed to enter purchase");
        sim::HumanGaitCoordinatorState interrupted = purchasing;
        for (const int side : {-1, 1})
        {
            auto unloaded_purchase = purchasing;
            unloaded_purchase.outgoing_side = side;
            auto unequal_load = purchase_input;
            unequal_load.left_normal_impulse = side < 0 ? 0.49f : 4.60f;
            unequal_load.right_normal_impulse = side < 0 ? 4.60f : 0.49f;
            for (int receipt = 0; receipt < 8; ++receipt)
            {
                const auto repeated = sim::step_human_gait_coordinator(
                    unloaded_purchase, unequal_load, 0.10f, 1.0f / 60.0f);
                unloaded_purchase = sim::step_human_gait_coordinator(
                    unloaded_purchase, unequal_load, 0.10f, 1.0f / 60.0f);
                require(unloaded_purchase.stage == sim::HumanGaitCoordinatorStage::purchase
                        && unloaded_purchase.outgoing_side == side
                        && unloaded_purchase.event_frames == 0u
                        && repeated.phase == unloaded_purchase.phase
                        && repeated.event_frames == unloaded_purchase.event_frames,
                    "lightly touching new sole released the loaded support foot");
            }
        }
        purchase_input.non_foot_contact = true;
        interrupted = sim::step_human_gait_coordinator(interrupted,
            purchase_input, 0.10f, 1.0f / 60.0f);
        require(interrupted.stage == sim::HumanGaitCoordinatorStage::purchase
                && interrupted.event_frames == 0u
                && interrupted.outgoing_side == -1,
            "body-contaminated purchase changed support ownership");
        purchase_input.non_foot_contact = false;
        purchasing = interrupted;
        purchasing = sim::step_human_gait_coordinator(purchasing,
            purchase_input, 0.10f, 1.0f / 60.0f);
        require(purchasing.stage == sim::HumanGaitCoordinatorStage::purchase
                && purchasing.event_frames == 1u
                && purchasing.outgoing_side == -1,
            "purchase swapped ownership before two clean bilateral receipts");
        purchasing = sim::step_human_gait_coordinator(purchasing,
            purchase_input, 0.10f, 1.0f / 60.0f);
        require(purchasing.stage == sim::HumanGaitCoordinatorStage::departure
                && purchasing.outgoing_side == 1
                && purchasing.phase == 0.0f
                && purchasing.event_frames == 0u,
            "sustained physical purchase did not hand off the next departure");
        auto loaded_roll = purchase_input;
        loaded_roll.left_maximum_clearance = 0.070f;
        loaded_roll.left_normal_impulse = 3.0f;
        loaded_roll.right_supported = false;
        loaded_roll.right_normal_impulse = 0.0f;
        loaded_roll.right_heel_normal_impulse = 0.0f;
        loaded_roll.right_minimum_clearance = 0.030f;
        loaded_roll.right_maximum_clearance = 0.055f;
        loaded_roll.right_average_velocity_y = 0.10f;
        const auto next_swing = [](auto state, const auto& input)
        {
            for (int tick = 0; tick < 3; ++tick)
                state = sim::step_human_gait_coordinator(state, input, 0.10f, 1.0f / 60.0f);
            return state;
        };
        const auto swinging = next_swing(purchasing, loaded_roll);
        auto cleared_departure = purchasing;
        cleared_departure.event_frames = 2u;
        auto already_clear = loaded_roll;
        already_clear.right_average_velocity_y = 0.0f;
        const auto clear_departure = [&](auto state, const auto& input)
        {
            return sim::step_human_gait_coordinator(state, input, 0.10f, 1.0f / 60.0f);
        };
        auto shallow_clearance = already_clear;
        shallow_clearance.right_minimum_clearance = 0.019f;
        auto contacting = already_clear;
        contacting.right_supported = true;
        auto uncleared_first = cleared_departure;
        uncleared_first.completed_handoffs = 0u;
        require(clear_departure(cleared_departure, already_clear).stage
                    == sim::HumanGaitCoordinatorStage::swing
                && clear_departure(cleared_departure, already_clear).completed_handoffs == 1u
                && clear_departure(cleared_departure, shallow_clearance).event_frames == 0u
                && clear_departure(cleared_departure, contacting).event_frames == 0u
                && clear_departure(uncleared_first, already_clear).event_frames == 0u,
            "an already airborne later sole must finish release without treating a brush as swing");
        auto delayed_departure = purchasing;
        delayed_departure.stage_seconds = 0.22f;
        sim::HumanSupportFeedback loaded_stance{};
        loaded_stance.valid = true;
        loaded_stance.total_normal_impulse = 4.5f;
        loaded_stance.load_asymmetry = -0.75f;
        const float shortening = rl::human_walk_loaded_departure_shortening(
            delayed_departure, loaded_stance, true, 2.0f);
        require(shortening > rl::human_departure_leg_shortening + 0.070f
                && shortening <= rl::human_departure_leg_shortening + 0.10f
                && rl::human_walk_loaded_departure_shortening(
                    delayed_departure, loaded_stance, true, 2.0f) == shortening,
            "delayed loaded departure must retract the complete swing leg deterministically");
        auto mirrored_departure = delayed_departure;
        mirrored_departure.outgoing_side = -delayed_departure.outgoing_side;
        auto mirrored_stance = loaded_stance;
        mirrored_stance.load_asymmetry = -loaded_stance.load_asymmetry;
        auto no_transfer = delayed_departure;
        no_transfer.completed_handoffs = 0u;
        require(rl::human_walk_loaded_departure_shortening(
                    mirrored_departure, mirrored_stance, true, 2.0f) == shortening
                && rl::human_walk_loaded_departure_shortening(
                    delayed_departure, mirrored_stance, true, 2.0f)
                    == rl::human_departure_leg_shortening
                && rl::human_walk_loaded_departure_shortening(
                    delayed_departure, loaded_stance, false, 2.0f)
                    == rl::human_departure_leg_shortening
                && rl::human_walk_loaded_departure_shortening(
                    no_transfer, loaded_stance, true, 2.0f)
                    == rl::human_departure_leg_shortening,
            "whole-sole release recovery must be mirrored, loaded and preserve the first plant");
        const auto hip = [&](auto gait, auto support, int side, float angle, float rate)
        {
            return rl::human_walk_stance_hip_trim(gait, support, side, angle, rate);
        };
        const float forward_trim = hip(delayed_departure, loaded_stance, -1, 0.21f, 0.6f);
        require(forward_trim < -0.13f
                && forward_trim == hip(mirrored_departure, mirrored_stance, 1, 0.21f, 0.6f)
                && hip(delayed_departure, loaded_stance, -1, -0.14f, -0.6f) > 0.17f
                && hip(delayed_departure, loaded_stance, -1, 0.21f, -0.6f) > forward_trim
                && hip(delayed_departure, loaded_stance, 0, 0.21f, 0.6f) == 0.0f
                && hip(delayed_departure, mirrored_stance, -1, 0.21f, 0.6f) == 0.0f
                && hip(no_transfer, loaded_stance, -1, 0.21f, 0.6f) == 0.0f,
            "stance hip feedback must damp measured pitch on either loaded leg only after a transfer");
        auto split_load = loaded_stance;
        split_load.load_asymmetry = -0.12f;
        require(rl::human_walk_dominant_support_side(loaded_stance, true, true) == -1
                && rl::human_walk_dominant_support_side(mirrored_stance, true, true) == 1
                && rl::human_walk_dominant_support_side(split_load, true, true) == 0
                && rl::human_walk_dominant_support_side(loaded_stance, false, false) == 0,
            "a trailing toe may not conceal the load-bearing leg or invent a stance");
        require(swinging.stage == sim::HumanGaitCoordinatorStage::swing
                && swinging.completed_handoffs == 1u
                && next_swing(purchasing, loaded_roll).stage == swinging.stage,
            "physically loaded toe roll erased the already-earned stance during second swing");
        auto unloaded_roll = loaded_roll;
        unloaded_roll.left_normal_impulse = 0.0f;
        require(next_swing(purchasing, unloaded_roll).stage == sim::HumanGaitCoordinatorStage::departure,
            "unloaded trailing geometry incorrectly supported a swing");
        auto before_first = purchasing;
        before_first.completed_handoffs = 0u;
        require(next_swing(before_first, loaded_roll).stage == sim::HumanGaitCoordinatorStage::departure,
            "a roll before any verified flat plant was accepted as an established stance");
        loaded_roll.right_average_velocity_y = -0.10f;
        const auto sampled_after_apex = sim::step_human_gait_coordinator(swinging,
            loaded_roll, pi, 1.0f / 60.0f);
        require(sampled_after_apex.stage == sim::HumanGaitCoordinatorStage::apex
                && sampled_after_apex.established_swing_clearance,
            "a verified swing missed its apex just because the next sample was descending");
        auto raised_heel_landing = purchase_input;
        raised_heel_landing.left_maximum_clearance = 0.070f;
        raised_heel_landing.left_normal_impulse = 2.0f;
        raised_heel_landing.right_normal_impulse = 3.0f;
        raised_heel_landing.right_heel_normal_impulse = 1.0f;
        const auto sample_landing = [](const auto& state, const auto& contact)
        {
            return sim::step_human_gait_coordinator(state, contact, 0.10f, 1.0f / 60.0f);
        };
        const auto heel_received = sample_landing(sampled_after_apex, raised_heel_landing);
        const auto heel_repeat = sample_landing(sampled_after_apex, raised_heel_landing);
        require(heel_received.stage == sim::HumanGaitCoordinatorStage::purchase
                && heel_received.completed_handoffs == 1u
                && heel_received.event_frames == 1u
                && heel_repeat.phase == heel_received.phase
                && sample_landing(heel_received, raised_heel_landing).completed_handoffs == 2u,
            "raised terrain skipped the independently measured two-frame heel purchase");
        auto heel_with_raised_toe = raised_heel_landing;
        heel_with_raised_toe.right_maximum_clearance = 0.110f;
        const auto finish_landing = sample_landing(sampled_after_apex, heel_with_raised_toe);
        require(finish_landing.stage == sim::HumanGaitCoordinatorStage::landing
                && finish_landing.completed_handoffs == 1u
                && finish_landing.event_frames == 0u
                && sample_landing(finish_landing, heel_with_raised_toe).completed_handoffs == 1u,
            "heel-only early landing failed to request sole alignment or acquired a false transfer");
        auto toe_only = raised_heel_landing;
        toe_only.right_heel_normal_impulse = 0.0f;
        auto no_clear_swing = sampled_after_apex;
        no_clear_swing.established_swing_clearance = false;
        auto body_contact = raised_heel_landing;
        body_contact.non_foot_contact = true;
        const auto interrupted_landing = sample_landing(sampled_after_apex, body_contact);
        require(sample_landing(sampled_after_apex, toe_only).stage
                    != sim::HumanGaitCoordinatorStage::purchase
                && sample_landing(no_clear_swing, raised_heel_landing).stage
                    != sim::HumanGaitCoordinatorStage::purchase
                && sample_landing(interrupted_landing, raised_heel_landing).stage
                    != sim::HumanGaitCoordinatorStage::purchase,
            "toe-only, unverified swing, or body contact qualified as an early heel purchase");

        const auto dry_traction = sim::contact_traction(1.0f, 0.0f, 0.0f,
            0.11f, 1.0f, 1.0f / 60.0f);
        const auto wet_traction = sim::contact_traction(0.4f, 0.3f, 0.25f,
            0.11f, 1.0f, 1.0f / 60.0f);
        for (float sign : { -1.0f, 1.0f })
        {
            require(sim::sliding_support_can_reacquire_static_anchor(
                    dry_traction, sign * 0.09f, sign * 0.015f, 2.0f, sign * 1.0f)
                && !sim::sliding_support_can_reacquire_static_anchor(
                    dry_traction, sign * 0.09f, sign * 0.15f, 2.0f, sign * 1.0f)
                && !sim::sliding_support_can_reacquire_static_anchor(
                    dry_traction, sign * 0.09f, sign * 0.015f, 0.0f, 0.0f)
                && !sim::sliding_support_can_reacquire_static_anchor(
                    dry_traction, sign * 0.002f, sign * 0.015f, 2.0f, sign * 1.0f)
                && !sim::sliding_support_can_reacquire_static_anchor(
                    wet_traction, sign * 0.09f, sign * 0.015f, 2.0f, sign * 1.0f),
                "re-stick eligibility must respect residual speed, load and local material");
        }
    }

    {
        if (support_transfer_only)
        {
            std::cout << "Physical support-transfer contracts passed (not gait acceptance)\n";
            return EXIT_SUCCESS;
        }
        std::vector<LandingEventFrame> clean_landing{};
        for (int frame = 0; frame < LandingEventDetector::minimum_clear_frames;
            ++frame)
        {
            clean_landing.push_back(synthetic_airborne_frame(0u,
                frame == LandingEventDetector::minimum_clear_frames - 1
                    ? 0.31f : 0.02f,
                frame == LandingEventDetector::minimum_clear_frames - 1
                    ? -0.41f : -1.20f));
        }
        clean_landing.push_back(synthetic_impact_frame(0u, 1u));
        clean_landing.push_back(synthetic_landing_frame(0u, 1u));
        clean_landing.push_back(synthetic_landing_frame(0u, 1u));

        LandingEventDetector positive{};
        LandingEventDetector repeated{};
        for (const LandingEventFrame& frame : clean_landing)
        {
            (void)positive.observe(frame);
            (void)repeated.observe(frame);
        }
        const SwingLandingEvidence positive_evidence = positive.evidence();
        const SwingLandingEvidence repeated_evidence = repeated.evidence();
        require(positive_evidence.landed
                && positive_evidence.side == 0u
                && positive_evidence.contact_node == 1u
                && positive_evidence.unloaded_frames == 6
                && std::abs(positive_evidence.minimum_clearance - 0.006f)
                    <= 1.0e-7f
                && std::abs(positive_evidence.landing_approach_speed - 0.33f)
                    <= 1.0e-7f
                && std::abs(positive_evidence.landing_approach_vertical_speed
                    - 0.45f) <= 1.0e-7f
                && std::abs(positive_evidence.landing_node_vertical_velocity
                    + 0.43f) <= 1.0e-7f,
            "landing detector did not bind a clean same-sole event to its immediate approach frame");
        require(repeated_evidence.landed == positive_evidence.landed
                && repeated_evidence.side == positive_evidence.side
                && repeated_evidence.contact_node == positive_evidence.contact_node
                && repeated_evidence.unloaded_frames
                    == positive_evidence.unloaded_frames
                && repeated_evidence.minimum_clearance
                    == positive_evidence.minimum_clearance
                && repeated_evidence.maximum_speed_during_unload
                    == positive_evidence.maximum_speed_during_unload
                && repeated_evidence.landing_approach_speed
                    == positive_evidence.landing_approach_speed
                && repeated_evidence.landing_approach_vertical_speed
                    == positive_evidence.landing_approach_vertical_speed
                && repeated_evidence.landing_minimum_vertical_velocity
                    == positive_evidence.landing_minimum_vertical_velocity
                && repeated_evidence.landing_maximum_vertical_velocity
                    == positive_evidence.landing_maximum_vertical_velocity
                && repeated_evidence.landing_node_vertical_velocity
                    == positive_evidence.landing_node_vertical_velocity,
            "identical landing-event frames did not reproduce exact evidence");

        const auto observe_clear_frames = [](LandingEventDetector& detector,
            std::size_t side, int count, float horizontal_speed,
            float vertical_speed)
        {
            for (int frame = 0; frame < count; ++frame)
            {
                (void)detector.observe(synthetic_airborne_frame(
                    side, horizontal_speed, vertical_speed));
            }
        };

        LandingEventDetector both_airborne{};
        observe_clear_frames(both_airborne, 0u, 6, 0.10f, -0.20f);
        LandingEventFrame airborne_gap = synthetic_airborne_frame(
            0u, 0.10f, -0.20f);
        airborne_gap.normal_impulse[1] = 0.0f;
        airborne_gap.grounded[1].fill(false);
        (void)both_airborne.observe(airborne_gap);
        (void)both_airborne.observe(synthetic_landing_frame(0u, 0u));
        require(!both_airborne.evidence().landed,
            "both-airborne interval was mistaken for supported landing history");

        LandingEventDetector side_change{};
        observe_clear_frames(side_change, 0u, 6, 0.10f, -0.20f);
        (void)side_change.observe(synthetic_airborne_frame(
            1u, 0.10f, -0.20f));
        (void)side_change.observe(synthetic_landing_frame(0u, 0u));
        require(!side_change.evidence().landed,
            "a side change inherited another sole's unload history");

        LandingEventDetector short_unload{};
        observe_clear_frames(short_unload, 0u, 5, 0.10f, -0.20f);
        (void)short_unload.observe(synthetic_landing_frame(0u, 0u));
        require(!short_unload.evidence().landed,
            "a short unload qualified as a landing event");

        LandingEventDetector body_contact{};
        observe_clear_frames(body_contact, 0u, 6, 0.10f, -0.20f);
        LandingEventFrame contaminated_landing = synthetic_landing_frame(
            0u, 0u);
        contaminated_landing.non_foot_contact = true;
        (void)body_contact.observe(contaminated_landing);
        (void)body_contact.observe(synthetic_landing_frame(0u, 0u));
        require(!body_contact.evidence().landed,
            "non-foot body contact did not invalidate the landing event");

        LandingEventDetector impulse_only_bounce{};
        observe_clear_frames(impulse_only_bounce, 0u, 6, 0.10f, -0.20f);
        (void)impulse_only_bounce.observe(synthetic_impact_frame(0u, 1u));
        for (int frame = 0;
            frame < LandingEventDetector::maximum_confirmation_frames;
            ++frame)
        {
            (void)impulse_only_bounce.observe(synthetic_airborne_frame(
                0u, 0.05f, -0.05f));
        }
        require(!impulse_only_bounce.evidence().landed,
            "an impulse-only bounce was mistaken for a sustained landing");

        LandingEventDetector one_frame_touch{};
        observe_clear_frames(one_frame_touch, 0u, 6, 0.10f, -0.20f);
        (void)one_frame_touch.observe(synthetic_impact_frame(0u, 1u));
        (void)one_frame_touch.observe(synthetic_landing_frame(0u, 1u));
        (void)one_frame_touch.observe(synthetic_airborne_frame(
            0u, 0.05f, -0.05f));
        (void)one_frame_touch.observe(synthetic_landing_frame(0u, 1u));
        require(!one_frame_touch.evidence().landed,
            "a one-frame touch was mistaken for a sustained landing");

        LandingEventDetector pending_body_contact{};
        observe_clear_frames(pending_body_contact, 0u, 6, 0.10f, -0.20f);
        (void)pending_body_contact.observe(synthetic_impact_frame(0u, 1u));
        LandingEventFrame contaminated_confirmation =
            synthetic_landing_frame(0u, 1u);
        contaminated_confirmation.non_foot_contact = true;
        (void)pending_body_contact.observe(contaminated_confirmation);
        (void)pending_body_contact.observe(synthetic_landing_frame(0u, 1u));
        require(!pending_body_contact.evidence().landed,
            "body contact during replant confirmation did not reject the event");

        LandingEventDetector unmixed{};
        observe_clear_frames(unmixed, 0u, 5, 0.01f, -0.02f);
        (void)unmixed.observe(synthetic_landing_frame(0u, 0u));
        observe_clear_frames(unmixed, 1u, 6, 0.31f, -0.41f);
        (void)unmixed.observe(synthetic_impact_frame(1u, 2u));
        (void)unmixed.observe(synthetic_landing_frame(1u, 2u));
        (void)unmixed.observe(synthetic_landing_frame(1u, 2u));
        const SwingLandingEvidence unmixed_evidence = unmixed.evidence();
        require(unmixed_evidence.landed && unmixed_evidence.side == 1u
                && unmixed_evidence.unloaded_frames == 6
                && std::abs(unmixed_evidence.landing_approach_speed - 0.33f)
                    <= 1.0e-7f
                && std::abs(unmixed_evidence.landing_approach_vertical_speed
                    - 0.45f) <= 1.0e-7f
                && std::abs(unmixed_evidence.landing_node_vertical_velocity
                    + 0.45f) <= 1.0e-7f,
            "landing evidence mixed minima or contact velocity across events");
    }

    {
        sim::Environment physical_swing{ sim::CreatureBlueprint::humanoid(),
            0x751A771u };
        physical_swing.set_course(sim::CourseStage::uneven, 0.30f);
        physical_swing.set_course_motion_enabled(false);
        physical_swing.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        physical_swing.set_raw_authority_telemetry_enabled(true);
        LandingEventDetector landing_detector{};
        constexpr float dt = 1.0f / 60.0f;
        const std::array<std::array<std::uint16_t, 3>, 2> sole_nodes{
            std::array<std::uint16_t, 3>{ sim::human_left_heel_node,
                sim::human_left_ball_node, sim::human_left_toe_node },
            std::array<std::uint16_t, 3>{ sim::human_right_heel_node,
                sim::human_right_ball_node, sim::human_right_toe_node }
        };
        const sim::CreatureBlueprint& swing_rig = physical_swing.blueprint();
        const auto non_support_clearance = [&](const sim::Particle& particle)
        {
            const float firmness = physical_swing.terrain_firmness_at(
                particle.position.x);
            const float looseness = physical_swing.terrain_looseness_at(
                particle.position.x);
            const float burial_allowance = sim::stage_uses_deformable_terrain(
                    physical_swing.course_stage())
                ? std::min(particle.radius * 0.78f,
                    (1.0f - firmness + looseness * 0.45f) * 0.18f)
                : 0.0f;
            const float contact_plane = physical_swing.ground_height_at(
                particle.position.x) + sim::ground_contact_offset(
                    false, particle.radius) - burial_allowance;
            return particle.position.y - contact_plane;
        };
        float initial_minimum_nonfoot_clearance =
            std::numeric_limits<float>::infinity();
        for (std::size_t node = 0; node < physical_swing.particles().size(); ++node)
        {
            if (swing_rig.is_support_seed(node))
                continue;
            initial_minimum_nonfoot_clearance = std::min(
                initial_minimum_nonfoot_clearance,
                non_support_clearance(physical_swing.particles()[node]));
        }
        int first_nonfoot_frame = -1;
        std::string first_nonfoot_detail{ "none" };
        std::array<int, 19> nonfoot_frames_by_node{};
        std::ostringstream coordinator_trace{};
        std::ostringstream foot_trace{};
        auto prior_stage = sim::HumanGaitCoordinatorStage::count;
        int prior_outgoing{};
        unsigned prior_support_mask = 4u;
        int coordinator_trace_count{};
        for (int frame = 0; frame < 240 && physical_swing.valid_motion(); ++frame)
        {
            const auto action = rl::raw_walking_teacher_action(physical_swing);
            (void)physical_swing.step(action);
            const auto body = physical_swing.particles();
            const sim::CreatureBlueprint& rig = physical_swing.blueprint();
            const sim::RawAuthorityTelemetry& telemetry =
                physical_swing.raw_authority_telemetry();
            LandingEventFrame landing_frame{};
            landing_frame.normal_impulse = telemetry.support_normal_impulse;
            landing_frame.non_foot_contact =
                physical_swing.non_foot_grounded();
            for (std::size_t side = 0; side < 2u; ++side)
            {
                for (std::size_t slot = 0; slot < landing_sole_node_count;
                    ++slot)
                {
                    const std::uint16_t node = sole_nodes[side][slot];
                    const sim::Particle& particle = body[node];
                    const float plane = physical_swing.ground_height_at(
                        particle.position.x) + sim::ground_contact_offset(
                            true, particle.radius);
                    landing_frame.grounded[side][slot] = particle.grounded;
                    landing_frame.clearance[side][slot] =
                        particle.position.y - plane;
                    landing_frame.velocity[side][slot] =
                        (particle.position - particle.previous) / dt;
                    landing_frame.node_normal_impulse[side][slot] = telemetry
                        .support_contact_normal_impulse[side * 3u + slot];
                }
            }
                const sim::HumanGaitCoordinatorState& coordinator =
                    physical_swing.human_gait_coordinator();
            const sim::HumanSupportFeedback support_feedback =
                physical_swing.human_support_feedback();
            if (frame == 95 || frame == 106 || frame == 120
                || frame == 128 || frame == 136 || frame == 144)
            {
                const sim::MotorDiagnostic ankle =
                    physical_swing.motor_diagnostic(
                        sim::human_left_ankle_action);
                const sim::MotorDiagnostic toe =
                    physical_swing.motor_diagnostic(
                        sim::human_left_toe_action);
                const sim::Particle& heel = body[sim::human_left_heel_node];
                const sim::Particle& ball = body[sim::human_left_ball_node];
                const sim::Particle& toe_node = body[sim::human_left_toe_node];
                const Vec2 heel_to_ball = ball.position - heel.position;
                const Vec2 ball_to_toe = toe_node.position - ball.position;
                const Vec2 desired_heel_to_ball{
                    heel_to_ball.x,
                    physical_swing.ground_height_at(ball.position.x)
                        - physical_swing.ground_height_at(heel.position.x)
                        + sim::ground_contact_offset(true, ball.radius)
                        - sim::ground_contact_offset(true, heel.radius) };
                const Vec2 desired_ball_to_toe{
                    ball_to_toe.x,
                    physical_swing.ground_height_at(toe_node.position.x)
                        - physical_swing.ground_height_at(ball.position.x)
                        + sim::ground_contact_offset(true, toe_node.radius)
                        - sim::ground_contact_offset(true, ball.radius) };
                if (!foot_trace.str().empty())
                    foot_trace << ';';
                foot_trace << 'f' << frame
                    << ":rear=" << std::atan2(heel_to_ball.y,
                        heel_to_ball.x) << '/'
                    << std::atan2(desired_heel_to_ball.y,
                        desired_heel_to_ball.x)
                    << ":ankle=" << ankle.current_angle << '/'
                    << ankle.target_angle << '/' << ankle.applied_action
                    << '/' << action[sim::human_left_ankle_action]
                    << '/' << ankle.angular_velocity
                    << ":distal=" << std::atan2(ball_to_toe.y,
                        ball_to_toe.x) << '/'
                    << std::atan2(desired_ball_to_toe.y,
                        desired_ball_to_toe.x)
                    << ":toe=" << toe.current_angle << '/'
                    << toe.target_angle << '/' << toe.applied_action
                    << '/' << action[sim::human_left_toe_action]
                    << '/' << toe.angular_velocity;
            }
            const unsigned support_mask =
                (physical_swing.left_supported() ? 1u : 0u)
                | (physical_swing.right_supported() ? 2u : 0u);
            if (coordinator_trace_count < 24
                && (coordinator.stage != prior_stage
                    || coordinator.outgoing_side != prior_outgoing
                    || support_mask != prior_support_mask
                    || (coordinator.stage
                            == sim::HumanGaitCoordinatorStage::flare
                        && frame % 8 == 0)))
            {
                if (coordinator_trace_count++ != 0)
                    coordinator_trace << ';';
                coordinator_trace << 'f' << frame << ":s"
                    << static_cast<unsigned>(coordinator.stage)
                    << ":o" << coordinator.outgoing_side
                    << ":p" << coordinator.phase
                    << ":c" << support_mask
                    << ":q" << landing_frame.normal_impulse[0] << ','
                    << landing_frame.normal_impulse[1]
                    << ":b" << body[rig.root_node].position.y << ','
                    << (body[rig.root_node].position.y
                            - body[rig.root_node].previous.y) / dt
                    << '|' << body[rig.torso_node].position.y << ','
                    << (body[rig.torso_node].position.y
                            - body[rig.torso_node].previous.y) / dt
                    << ":cp" << support_feedback.center_of_mass.x << ','
                    << support_feedback.center_of_mass_velocity.x << ','
                    << support_feedback.aggregate_cop_x << ','
                    << support_feedback.capture_error_x
                    << ":h" << landing_frame.clearance[0][0] << ','
                    << landing_frame.clearance[0][1] << ','
                    << landing_frame.clearance[0][2] << '|'
                    << landing_frame.clearance[1][0] << ','
                    << landing_frame.clearance[1][1] << ','
                    << landing_frame.clearance[1][2]
                    << ":v" << landing_frame.velocity[0][0].x << ','
                    << landing_frame.velocity[0][1].x << ','
                    << landing_frame.velocity[0][2].x << '|'
                    << landing_frame.velocity[0][0].y << ','
                    << landing_frame.velocity[0][1].y << ','
                    << landing_frame.velocity[0][2].y
                    << ":a" << action[0] << ',' << action[1] << ','
                    << action[2] << ',' << action[3] << ','
                    << action[sim::human_left_ankle_action] << ','
                    << action[sim::human_right_ankle_action] << ','
                    << action[sim::human_left_toe_action] << ','
                    << action[sim::human_right_toe_action]
                    << ":t" << telemetry.motor_torque[0] << ','
                    << telemetry.motor_torque[1] << '|'
                    << telemetry.motor_torque[2] << ','
                    << telemetry.motor_torque[3];
                prior_stage = coordinator.stage;
                prior_outgoing = coordinator.outgoing_side;
                prior_support_mask = support_mask;
            }
            std::ostringstream contact_detail;
            bool found_nonfoot{};
            bool first_detail_node = true;
            for (std::size_t node = 0; node < body.size(); ++node)
            {
                if (!body[node].grounded || rig.is_support_seed(node))
                    continue;
                found_nonfoot = true;
                if (node < nonfoot_frames_by_node.size())
                    ++nonfoot_frames_by_node[node];
                if (first_nonfoot_frame >= 0)
                    continue;
                const sim::Particle& particle = body[node];
                const Vec2 velocity = (particle.position - particle.previous)
                    / dt;
                if (!first_detail_node)
                    contact_detail << ';';
                first_detail_node = false;
                contact_detail << "node=" << node << '(' << human_node_name(node)
                    << ") clearance=" << non_support_clearance(particle)
                    << " velocity=" << velocity.x << ',' << velocity.y
                    << " position=" << particle.position.x << ','
                    << particle.position.y;
            }
            if (found_nonfoot && first_nonfoot_frame < 0)
            {
                first_nonfoot_frame = frame;
                contact_detail << " upright=" << physical_swing.uprightness()
                    << " root=" << body[rig.root_node].position.x << ','
                    << body[rig.root_node].position.y
                    << " torso=" << body[rig.torso_node].position.x << ','
                    << body[rig.torso_node].position.y
                    << " supported=" << physical_swing.left_supported() << ','
                    << physical_swing.right_supported() << " sole_grounded=";
                for (std::size_t side = 0; side < 2u; ++side)
                    for (std::size_t slot = 0; slot < landing_sole_node_count;
                        ++slot)
                        contact_detail << landing_frame.grounded[side][slot];
                contact_detail << " sole_impulse="
                    << landing_frame.normal_impulse[0] << ','
                    << landing_frame.normal_impulse[1] << " action=";
                for (std::size_t index = 0; index < action.size(); ++index)
                {
                    if (index != 0u)
                        contact_detail << ',';
                    contact_detail << action[index];
                }
                first_nonfoot_detail = contact_detail.str();
            }
            require(found_nonfoot == landing_frame.non_foot_contact,
                "test-local non-foot scan disagrees with production contact classification");
            if (landing_detector.observe(landing_frame))
                break;
        }
        const SwingLandingEvidence evidence = landing_detector.evidence();
        const auto rejection_diagnostics =
            landing_detector.rejection_diagnostics();
        std::ostringstream node_impulse_rejection_detail{};
        const int node_impulse_rejection_frame =
            landing_detector.first_node_impulse_rejection_frame();
        if (node_impulse_rejection_frame >= 0)
        {
            const LandingEventFrame& rejected =
                landing_detector.first_node_impulse_rejection();
            const LandingEventFrame& preceding =
                landing_detector.first_node_impulse_rejection_previous();
            node_impulse_rejection_detail << "frame="
                << node_impulse_rejection_frame << " aggregate="
                << rejected.normal_impulse[0] << ','
                << rejected.normal_impulse[1] << " grounded=";
            for (std::size_t side = 0; side < 2u; ++side)
            {
                if (side != 0u)
                    node_impulse_rejection_detail << '|';
                for (std::size_t node = 0; node < landing_sole_node_count;
                    ++node)
                {
                    node_impulse_rejection_detail
                        << rejected.grounded[side][node];
                }
            }
            node_impulse_rejection_detail << " node_impulse=";
            for (std::size_t side = 0; side < 2u; ++side)
            {
                if (side != 0u)
                    node_impulse_rejection_detail << '|';
                for (std::size_t node = 0; node < landing_sole_node_count;
                    ++node)
                {
                    if (node != 0u)
                        node_impulse_rejection_detail << ',';
                    node_impulse_rejection_detail
                        << rejected.node_normal_impulse[side][node];
                }
            }
            node_impulse_rejection_detail << " clearance=";
            for (std::size_t side = 0; side < 2u; ++side)
            {
                if (side != 0u)
                    node_impulse_rejection_detail << '|';
                for (std::size_t node = 0; node < landing_sole_node_count;
                    ++node)
                {
                    if (node != 0u)
                        node_impulse_rejection_detail << ',';
                    node_impulse_rejection_detail
                        << rejected.clearance[side][node];
                }
            }
            node_impulse_rejection_detail << " previous_velocity=";
            for (std::size_t side = 0; side < 2u; ++side)
            {
                if (side != 0u)
                    node_impulse_rejection_detail << '|';
                for (std::size_t node = 0; node < landing_sole_node_count;
                    ++node)
                {
                    if (node != 0u)
                        node_impulse_rejection_detail << ',';
                    node_impulse_rejection_detail
                        << preceding.velocity[side][node].x << ':'
                        << preceding.velocity[side][node].y;
                }
            }
        }
        else
            node_impulse_rejection_detail << "none";
        const std::string physical_failure =
            "finite Human swing did not produce multi-frame all-node unload "
            "and a bounded ground-relative landing: frames="
            + std::to_string(evidence.unloaded_frames)
            + " clearance="
            + std::to_string(evidence.minimum_clearance)
            + " swing_vx="
            + std::to_string(evidence.maximum_speed_during_unload)
            + " landing_vx=" + std::to_string(evidence.landing_approach_speed)
            + " landing_vy="
            + std::to_string(evidence.landing_approach_vertical_speed)
            + " signed_vy=["
            + std::to_string(evidence.landing_minimum_vertical_velocity)
            + ',' + std::to_string(evidence.landing_maximum_vertical_velocity)
            + "] contact_vy="
            + std::to_string(evidence.landing_node_vertical_velocity)
            + " side=" + std::to_string(evidence.side)
            + " node=" + std::to_string(evidence.contact_node)
            + " observed_clear=" + std::to_string(rejection_diagnostics[0])
            + " nonfoot_frames=" + std::to_string(rejection_diagnostics[1])
            + " nonfinite_frames=" + std::to_string(rejection_diagnostics[2])
            + " reject_body=" + std::to_string(rejection_diagnostics[3])
            + " reject_both_air=" + std::to_string(rejection_diagnostics[4])
            + " reject_side=" + std::to_string(rejection_diagnostics[5])
            + " reject_landing_impulse="
            + std::to_string(rejection_diagnostics[6])
            + " reject_node_impulse="
            + std::to_string(rejection_diagnostics[7])
            + " reject_unconfirmed_impact="
            + std::to_string(rejection_diagnostics[8])
            + " node_impulse_rejection={"
            + node_impulse_rejection_detail.str() + "}"
            + " initial_min_nonfoot_clearance="
            + std::to_string(initial_minimum_nonfoot_clearance)
            + " first_nonfoot_frame=" + std::to_string(first_nonfoot_frame)
            + " first_nonfoot={" + first_nonfoot_detail + "}"
            + " coordinator={" + coordinator_trace.str() + "}"
            + " foot={" + foot_trace.str() + "}"
            + " nonfoot_node_frames=" + [&]()
            {
                std::string summary{};
                for (std::size_t node = 0; node < nonfoot_frames_by_node.size();
                    ++node)
                {
                    if (nonfoot_frames_by_node[node] == 0)
                        continue;
                    if (!summary.empty())
                        summary += ',';
                    summary += std::to_string(node) + '('
                        + std::string(human_node_name(node)) + ")="
                        + std::to_string(nonfoot_frames_by_node[node]);
                }
                return summary.empty() ? std::string{ "none" } : summary;
            }();
        require(evidence.landed
                && evidence.unloaded_frames >= 6
                && evidence.minimum_clearance >= 0.005f
                && evidence.landing_approach_speed <= 0.40f
                && evidence.landing_approach_vertical_speed <= 0.65f
                && evidence.landing_node_vertical_velocity < -0.01f
                && evidence.landing_node_vertical_velocity >= -0.65f,
            physical_failure);
    }

    {
        sim::Environment raw_teacher{ sim::CreatureBlueprint::humanoid(),
            0x748A12u };
        sim::Environment raw_repeat{ sim::CreatureBlueprint::humanoid(),
            0x748A12u };
        raw_teacher.set_course(sim::CourseStage::uneven, 0.30f);
        raw_repeat.set_course(sim::CourseStage::uneven, 0.30f);
        raw_teacher.set_course_motion_enabled(false);
        raw_repeat.set_course_motion_enabled(false);
        raw_teacher.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        raw_repeat.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        raw_teacher.set_raw_authority_telemetry_enabled(true);
        const auto weighted_center = [](const sim::Environment& environment)
        {
            Vec2 center{};
            float total_mass{};
            for (const sim::Particle& particle : environment.particles())
            {
                const float mass = 1.0f
                    / std::max(particle.inverse_mass, 1.0e-6f);
                center += particle.position * mass;
                total_mass += mass;
            }
            return center / std::max(total_mass, 1.0e-6f);
        };
        const Vec2 initial_center = weighted_center(raw_teacher);
        Vec2 previous_center = initial_center;
        float previous_center_velocity{};
        float previous_pelvis_x = raw_teacher.particles()[
            raw_teacher.blueprint().root_node].position.x;
        float previous_pelvis_velocity{};
        float pelvis_delta_sum{};
        float maximum_pelvis_delta{};
        float center_delta_sum{};
        float maximum_center_delta{};
        float maximum_torso_turn_rate{};
        float maximum_support_lead{};
        float maximum_slip_speed{};
        std::uint32_t unsupported_com_impulses{};
        std::uint32_t measured_frames{};
        std::uint32_t command_telemetry_mismatches{};
        std::uint32_t actuator_transition_mismatches{};
        std::uint32_t motor_application_mismatches{};
        std::array<float, sim::anatomy_action_count> previous_actuator{};
        std::ostringstream raw_coordinator_trace{};
        auto raw_prior_stage = sim::HumanGaitCoordinatorStage::count;
        int raw_prior_outgoing{};
        unsigned raw_prior_support_mask = 4u;
        int raw_trace_count{};
        for (int frame = 0; frame < 1200; ++frame)
        {
            const auto action = rl::raw_walking_teacher_action(raw_teacher);
            const auto repeated_action = rl::raw_walking_teacher_action(raw_repeat);
            require(action == repeated_action,
                "same-seed raw action demonstration diverged before physics");
            std::array<float, sim::anatomy_action_count> expected_actuator{};
            const float control_ramp = sim::policy_control_ramp(
                raw_teacher.elapsed_seconds());
            for (std::size_t index = 0;
                index < sim::anatomy_action_count; ++index)
            {
                const float desired = clamp(action[index], -1.0f, 1.0f)
                    * control_ramp;
                expected_actuator[index] = previous_actuator[index]
                    + clamp(desired - previous_actuator[index],
                        -1.0f / 60.0f, 1.0f / 60.0f);
            }
            const sim::StepResult result = raw_teacher.step(action);
            const sim::StepResult repeated_result = raw_repeat.step(repeated_action);
            const sim::CreatureBlueprint& rig = raw_teacher.blueprint();
            const auto particles = raw_teacher.particles();
            const float pelvis_velocity = (particles[rig.root_node].position.x
                - previous_pelvis_x) * 60.0f;
            const float pelvis_delta = std::abs(
                pelvis_velocity - previous_pelvis_velocity);
            pelvis_delta_sum += pelvis_delta;
            maximum_pelvis_delta = std::max(maximum_pelvis_delta, pelvis_delta);
            previous_pelvis_velocity = pelvis_velocity;
            previous_pelvis_x = particles[rig.root_node].position.x;
            const Vec2 center = weighted_center(raw_teacher);
            const float center_velocity = (center.x - previous_center.x) * 60.0f;
            const float center_delta = std::abs(
                center_velocity - previous_center_velocity);
            center_delta_sum += center_delta;
            maximum_center_delta = std::max(maximum_center_delta, center_delta);
            const sim::RawAuthorityTelemetry& authority =
                raw_teacher.raw_authority_telemetry();
                const sim::HumanGaitCoordinatorState& coordinator =
                    raw_teacher.human_gait_coordinator();
                const sim::HumanSupportFeedback support_feedback =
                    raw_teacher.human_support_feedback();
            const unsigned support_mask =
                (raw_teacher.left_supported() ? 1u : 0u)
                | (raw_teacher.right_supported() ? 2u : 0u);
            if (raw_trace_count < 96
                && (coordinator.stage != raw_prior_stage
                    || coordinator.outgoing_side != raw_prior_outgoing
                    || support_mask != raw_prior_support_mask
                    || frame % 30 == 0 || result.terminated))
            {
                const auto current_observation = raw_teacher.observation();
                const auto anatomy = std::span<const float,
                    sim::anatomy_action_count>{ action.data(),
                        sim::anatomy_action_count };
                const Vec2 requested_left = sim::requested_leg_endpoint(
                    raw_teacher.blueprint(), true, anatomy,
                    raw_teacher.facing_direction());
                const Vec2 requested_right = sim::requested_leg_endpoint(
                    raw_teacher.blueprint(), false, anatomy,
                    raw_teacher.facing_direction());
                const auto actuator_anatomy = std::span<const float,
                    sim::anatomy_action_count>{
                        authority.actuator_actions.data(),
                        sim::anatomy_action_count };
                const Vec2 actuated_left = sim::requested_leg_endpoint(
                    raw_teacher.blueprint(), true, actuator_anatomy,
                    raw_teacher.facing_direction());
                const Vec2 actuated_right = sim::requested_leg_endpoint(
                    raw_teacher.blueprint(), false, actuator_anatomy,
                    raw_teacher.facing_direction());
                const auto trace_particles = raw_teacher.particles();
                const auto& trace_rig = raw_teacher.blueprint();
                const Vec2 actual_left = trace_particles[
                    trace_rig.motors[1].c].position
                    - trace_particles[trace_rig.motors[0].pivot].position;
                const Vec2 actual_right = trace_particles[
                    trace_rig.motors[3].c].position
                    - trace_particles[trace_rig.motors[2].pivot].position;
                const sim::MotorDiagnostic left_hip_motor =
                    raw_teacher.motor_diagnostic(0u);
                const sim::MotorDiagnostic left_knee_motor =
                    raw_teacher.motor_diagnostic(1u);
                const sim::MotorDiagnostic right_hip_motor =
                    raw_teacher.motor_diagnostic(2u);
                const sim::MotorDiagnostic right_knee_motor =
                    raw_teacher.motor_diagnostic(3u);
                if (raw_trace_count++ != 0)
                    raw_coordinator_trace << ';';
                raw_coordinator_trace << 'f' << frame << ":s"
                    << static_cast<unsigned>(coordinator.stage)
                    << ":o" << coordinator.outgoing_side
                    << ":p" << coordinator.phase
                    << ":c" << support_mask
                    << ":u" << raw_teacher.uprightness()
                    << ":x" << raw_teacher.distance_travelled()
                    << ":v" << raw_teacher.forward_speed()
                    << ":cp" << support_feedback.capture_error_x << ','
                    << support_feedback.load_asymmetry << ','
                    << support_feedback.total_normal_impulse
                    << ":h" << current_observation[
                        sim::left_sole_clearance_observation] * 0.25f
                    << ',' << current_observation[
                        sim::right_sole_clearance_observation] * 0.25f
                    << ":sv" << current_observation[
                        sim::left_sole_velocity_x_observation] * 6.0f
                    << ',' << current_observation[
                        sim::left_sole_velocity_y_observation] * 6.0f
                    << '|' << current_observation[
                        sim::right_sole_velocity_x_observation] * 6.0f
                    << ',' << current_observation[
                        sim::right_sole_velocity_y_observation] * 6.0f
                    << ":q" << authority.support_normal_impulse[0]
                    << ',' << authority.support_normal_impulse[1]
                    << ":lr" << requested_left.x << ',' << actual_left.x
                    << '|' << requested_right.x << ',' << actual_right.x
                    << ":ar" << actuated_left.x << ',' << actuated_left.y
                    << '|' << actuated_right.x << ',' << actuated_right.y
                    << ":e" << (left_hip_motor.target_angle
                            - left_hip_motor.current_angle)
                    << ',' << (left_knee_motor.target_angle
                            - left_knee_motor.current_angle)
                    << '|' << (right_hip_motor.target_angle
                            - right_hip_motor.current_angle)
                    << ',' << (right_knee_motor.target_angle
                            - right_knee_motor.current_angle)
                    << ":t" << authority.motor_torque[0] << ','
                    << authority.motor_torque[1] << '|'
                    << authority.motor_torque[2] << ','
                    << authority.motor_torque[3]
                    << ":sl" << raw_teacher.stance_slip_speed()
                    << '/' << raw_teacher.available_traction()
                    << '/' << raw_teacher.used_traction()
                    << ":a" << action[0] << ',' << action[1] << ','
                    << action[2] << ',' << action[3] << ','
                    << action[sim::human_left_ankle_action] << ','
                    << action[sim::human_right_ankle_action] << ','
                    << action[sim::human_left_toe_action] << ','
                    << action[sim::human_right_toe_action];
                raw_prior_stage = coordinator.stage;
                raw_prior_outgoing = coordinator.outgoing_side;
                raw_prior_support_mask = support_mask;
            }
            for (std::size_t index = 0;
                index < sim::anatomy_action_count; ++index)
            {
                command_telemetry_mismatches +=
                    authority.commanded_actions[index] == action[index]
                    ? 0u : 1u;
                actuator_transition_mismatches += std::abs(
                    authority.actuator_actions[index]
                        - expected_actuator[index]) <= 1.0e-7f ? 0u : 1u;
                const sim::MotorDiagnostic motor =
                    raw_teacher.motor_diagnostic(index);
                motor_application_mismatches += motor.available
                        && motor.enabled
                        && motor.applied_action
                            == authority.actuator_actions[index]
                    ? 0u : 1u;
                previous_actuator[index] = authority.actuator_actions[index];
            }
            const float ground_impulse = authority.sources[
                static_cast<std::size_t>(
                    sim::AuthoritySource::ground_contact)]
                        .center_velocity_delta_x;
            const float course_impulse = authority.sources[
                static_cast<std::size_t>(
                    sim::AuthoritySource::course_collision)]
                        .center_velocity_delta_x;
            const bool contact_causal = std::abs(ground_impulse) > 1.0e-5f
                || std::abs(course_impulse) > 1.0e-5f;
            const bool forward_energy_increased = center_velocity > 0.0f
                && center_velocity
                    > std::max(0.0f, previous_center_velocity) + 0.025f;
            if (center_delta > 0.025f)
            {
                if (!contact_causal && forward_energy_increased)
                    ++unsupported_com_impulses;
            }
            previous_center_velocity = center_velocity;
            previous_center = center;

            maximum_torso_turn_rate = std::max(maximum_torso_turn_rate,
                std::abs(raw_teacher.observation()[63]) * 6.0f);
            maximum_slip_speed = std::max(maximum_slip_speed,
                raw_teacher.stance_slip_speed());
            float support_x{};
            std::uint32_t support_count{};
            for (std::size_t node = 0; node < particles.size(); ++node)
            {
                if (!rig.is_support_seed(node) || !particles[node].grounded)
                    continue;
                support_x += particles[node].position.x;
                ++support_count;
            }
            if (support_count > 0u)
            {
                const float lead = std::abs(center.x
                    - support_x / static_cast<float>(support_count));
                maximum_support_lead = std::max(maximum_support_lead, lead);
            }
            ++measured_frames;
            const auto first_particles = raw_teacher.particles();
            const auto repeated_particles = raw_repeat.particles();
            require(result.terminated == repeated_result.terminated
                    && result.invalid_reason == repeated_result.invalid_reason
                    && first_particles.size() == repeated_particles.size(),
                "same-seed raw trial produced different terminal state");
            for (std::size_t node = 0; node < first_particles.size(); ++node)
            {
                require(first_particles[node].position.x
                            == repeated_particles[node].position.x
                        && first_particles[node].position.y
                            == repeated_particles[node].position.y
                        && first_particles[node].previous.x
                            == repeated_particles[node].previous.x
                        && first_particles[node].previous.y
                            == repeated_particles[node].previous.y,
                    "same-seed raw articulated trajectory is not deterministic");
            }
            if (result.terminated)
                break;
        }
        if (!raw_teacher.valid_motion()
            || !raw_teacher.body_integrity_valid()
            || raw_teacher.alternating_steps() < 12u
            || raw_teacher.distance_travelled() < 10.0f
            || raw_teacher.gait_cycles() < static_cast<std::uint32_t>(
                rl::casual_walk_teacher_stride_events))
        {
            std::cerr << "raw teacher distance=" << raw_teacher.distance_travelled()
                << " cycles=" << raw_teacher.gait_cycles()
                << " elapsed=" << raw_teacher.elapsed_seconds()
                << " alternating=" << raw_teacher.alternating_steps()
                << " invalid=" << static_cast<int>(raw_teacher.invalid_reason())
                << " upright=" << raw_teacher.uprightness()
                << " max_slip=" << maximum_slip_speed
                << " max_support_lead=" << maximum_support_lead
                << " coordinator={" << raw_coordinator_trace.str()
                << "}\n";
            const auto& raw_rig = raw_teacher.blueprint();
            const auto& raw_particles = raw_teacher.particles();
            const Vec2 raw_root = raw_particles[raw_rig.root_node].position;
            for (std::size_t node = 0; node < raw_particles.size(); ++node)
            {
                const float rest_radius = length(raw_rig.nodes[node]
                    - raw_rig.nodes[raw_rig.root_node]);
                const float current_radius = length(
                    raw_particles[node].position - raw_root);
                if (current_radius > std::max(1.80f,
                    rest_radius * 3.00f + 0.80f))
                    std::cerr << "raw integrity node=" << node
                        << " radius=" << current_radius << '\n';
            }
            for (std::size_t bone_index = 0;
                bone_index < raw_rig.bones.size(); ++bone_index)
            {
                const auto& bone = raw_rig.bones[bone_index];
                const float ratio = length(raw_particles[bone.b].position
                    - raw_particles[bone.a].position) / bone.rest_length;
                if (ratio < 0.20f || ratio > 2.50f)
                    std::cerr << "raw integrity bone=" << bone_index
                        << " ratio=" << ratio << '\n';
            }
            const Vec2 torso_segment = raw_particles[raw_rig.torso_node].position
                - raw_root;
            const Vec2 head_segment = raw_particles[raw_rig.head_node].position
                - raw_particles[raw_rig.torso_node].position;
            const Vec2 rest_torso = raw_rig.nodes[raw_rig.torso_node]
                - raw_rig.nodes[raw_rig.root_node];
            const Vec2 rest_head = raw_rig.nodes[raw_rig.head_node]
                - raw_rig.nodes[raw_rig.torso_node];
            std::cerr << "raw torso_ratio=" << length(torso_segment) / length(rest_torso)
                << " head_ratio=" << length(head_segment) / length(rest_head)
                << " head_dot=" << dot(normalized(torso_segment),
                    normalized(head_segment)) << '\n';
        }
        require(raw_teacher.valid_motion()
                && raw_teacher.body_integrity_valid()
                && raw_teacher.distance_travelled()
                    >= 10.0f
                && raw_teacher.alternating_steps() >= 12u
                && raw_teacher.gait_cycles()
                    >= static_cast<std::uint32_t>(
                        rl::casual_walk_teacher_stride_events),
            "the raw action-space gait demonstration is not physically learnable "
            "without optional assistance");
        require(pelvis_delta_sum / static_cast<float>(measured_frames) <= 0.18f
                && maximum_pelvis_delta <= 1.0f
                && center_delta_sum / static_cast<float>(measured_frames) <= 0.12f
                && maximum_center_delta <= 0.75f
                && maximum_torso_turn_rate <= 2.5f
                && maximum_support_lead <= 0.55f
                && maximum_slip_speed <= 1.5f
                && unsupported_com_impulses == 0u
                && command_telemetry_mismatches == 0u
                && actuator_transition_mismatches == 0u
                && motor_application_mismatches == 0u,
            "the raw action-space gait passed distance by lunging, slipping, "
            "rolling, or receiving a non-contact forward impulse");
    }

    {
        sim::Environment assisted{ sim::CreatureBlueprint::humanoid(), 0x748A13u };
        sim::Environment raw{ sim::CreatureBlueprint::humanoid(), 0x748A13u };
        const float assisted_shift =
            sim::EnvironmentTestAccess::compressed_body_projection(
                assisted, sim::GuidanceMode::assisted);
        const float raw_shift = sim::EnvironmentTestAccess::compressed_body_projection(
            raw, sim::GuidanceMode::raw_policy_audit);
        require(assisted_shift > 0.01f && raw_shift == 0.0f,
            "raw learned Human replay still inherits upper-body rail projection");
    }

    {
        sim::Environment assisted{ sim::CreatureBlueprint::humanoid(), 0x748A14u };
        sim::Environment raw{ sim::CreatureBlueprint::humanoid(), 0x748A14u };
        const auto assisted_shifts =
            sim::EnvironmentTestAccess::posture_projection_shifts(
                assisted, sim::GuidanceMode::assisted);
        const auto raw_shifts = sim::EnvironmentTestAccess::posture_projection_shifts(
            raw, sim::GuidanceMode::raw_policy_audit);
        require(assisted_shifts[1] > 0.001f
                && raw_shifts[0] == 0.0f && raw_shifts[1] == 0.0f,
            "raw Human posture helper still injects root or torso displacement");

        sim::Environment passive{ sim::CreatureBlueprint::humanoid(), 0x748A15u };
        const auto conservation =
            sim::EnvironmentTestAccess::passive_appendage_conservation(passive);
        require(conservation[0] == 0.0f
                && conservation[1] == 0.0f
                && conservation[2] == 0.0f
                && conservation[3] == 0.0f,
            "raw passive anatomy still inherits an authored head/torso pose servo");
    }

    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            0x74848u };
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        environment.set_course_motion_enabled(false);
        double torso_pitch_sum{};
        float maximum_torso_pitch{};
        float maximum_hand_fore_aft{};
        std::size_t observed_frames{};
        std::size_t opposed_arm_frames{};
        std::array<float, 2> maximum_supported_extension{};
        std::array<std::size_t, 2> supported_samples{};
        std::array<std::size_t, 2> straight_support_samples{};
        for (int frame = 0; frame < 1200; ++frame)
        {
            std::array<float, sim::action_count> residual{};
            const auto action = rl::effective_policy_action(environment,
                residual, sim::CourseStage::uneven, 0.0f);
            const sim::StepResult result = environment.step(action);
            require(!result.terminated,
                "contact-led Human teacher terminated during the live gait soak");
            if (frame < 120)
                continue;

            const sim::CreatureBlueprint& rig = environment.blueprint();
            const auto particles = environment.particles();
            Vec2 authored_body = rig.nodes[rig.torso_node]
                - rig.nodes[rig.root_node];
            authored_body.x *= environment.facing_direction();
            const Vec2 current_body = particles[rig.torso_node].position
                - particles[rig.root_node].position;
            const float torso_pitch = std::abs(
                signed_angle(authored_body, current_body));
            torso_pitch_sum += torso_pitch;
            maximum_torso_pitch = std::max(maximum_torso_pitch, torso_pitch);

            const float facing = environment.facing_direction();
            const float left_hand = (particles[9].position.x
                - particles[7].position.x) * facing;
            const float right_hand = (particles[12].position.x
                - particles[10].position.x) * facing;
            maximum_hand_fore_aft = std::max({ maximum_hand_fore_aft,
                std::abs(left_hand), std::abs(right_hand) });
            if (left_hand * right_hand <= 0.0f)
                ++opposed_arm_frames;
            ++observed_frames;

            for (std::size_t side = 0; side < 2u; ++side)
            {
                const bool left = side == 0u;
                if (left ? !environment.left_supported()
                    : !environment.right_supported())
                    continue;
                const std::size_t hip_index = left ? 0u : 2u;
                const std::size_t knee_index = hip_index + 1u;
                const sim::MotorConstraint& hip = rig.motors[hip_index];
                const sim::MotorConstraint& knee = rig.motors[knee_index];
                const float chain_length = length(rig.nodes[hip.c]
                    - rig.nodes[hip.pivot]) + length(rig.nodes[knee.c]
                    - rig.nodes[knee.pivot]);
                const float extension = length(particles[knee.c].position
                    - particles[hip.pivot].position)
                    / std::max(chain_length, 1.0e-5f);
                maximum_supported_extension[side] = std::max(
                    maximum_supported_extension[side], extension);
                ++supported_samples[side];
                if (extension >= 0.90f)
                    ++straight_support_samples[side];
            }
        }
        const float mean_pitch = static_cast<float>(torso_pitch_sum
            / static_cast<double>(std::max<std::size_t>(observed_frames, 1u)));
        if (mean_pitch > 0.12f || maximum_torso_pitch > 0.34f
            || maximum_hand_fore_aft > 0.52f
            || opposed_arm_frames * 4u < observed_frames * 3u)
            std::cerr << "Human live gait evidence: mean_pitch=" << mean_pitch
                << " max_pitch=" << maximum_torso_pitch
                << " max_hand_fore_aft=" << maximum_hand_fore_aft
                << " opposed=" << opposed_arm_frames << '/' << observed_frames
                << " support_extension=" << maximum_supported_extension[0]
                << ',' << maximum_supported_extension[1]
                << " heel/toe=" << environment.heel_strikes()
                << '/' << environment.toe_offs() << '\n';
        require(mean_pitch <= 0.12f && maximum_torso_pitch <= 0.34f,
            "live Human trunk retained a backward rail-slide pitch");
        require(maximum_hand_fore_aft <= 0.52f
                && opposed_arm_frames * 4u >= observed_frames * 3u,
            "live Human arms reached forward together or escaped side-rest");
        for (std::size_t side = 0; side < 2u; ++side)
            require(maximum_supported_extension[side] >= 0.94f
                    && straight_support_samples[side] * 2u
                        >= supported_samples[side],
                "live Human support leg remained chronically flexed");
        require(environment.distance_travelled() >= 8.0f
                && environment.gait_cycles() >= 8u,
            "live Human gait soak advanced without real support transfers");
        require(environment.maximum_lower_leg_scissor_seconds()
                <= sim::sustained_scissor_limit_seconds,
            "articulated heel/toe rollover was misclassified as whole-leg scissor motion");
        require(environment.valid_motion(),
            "articulated Human teacher probe became invalid");
        require(environment.heel_strikes() >= 4u
                && environment.toe_offs() >= 4u,
            "live Human gait never produced physical heel/toe contact phases");
    }

    require(std::abs(art::human_limb_layer_opacity(true) - 0.98f)
                <= 1.0e-6f
            && std::abs(art::human_limb_layer_opacity(false) - 0.92f)
                <= 1.0e-6f,
        "Human rear limbs are translucent ghosts or duplicate foreground art");
    const float visible_upper_arm = art::human_limb_thickness_ratio(false, true)
        * art::human_upper_arm_opaque_transverse_fraction;
    const float visible_forearm = art::human_limb_thickness_ratio(false, false)
        * art::human_forearm_opaque_transverse_fraction;
    const float visible_thigh = art::human_limb_thickness_ratio(true, true)
        * (11.0f / 20.0f);
    const float visible_shin = art::human_limb_thickness_ratio(true, false)
        * (8.0f / 20.0f);
    require(std::abs(visible_upper_arm - visible_forearm) <= 1.0e-6f
            && visible_upper_arm > visible_shin
            && visible_upper_arm < visible_thigh,
        "source padding no longer yields coherent arm and support-limb visible envelopes");
    const art::HandArtDimensions compact_hand = art::hand_art_dimensions(
        28.0f, 18, 12, 1.0f);
    require(compact_hand.thickness <= 17.0f && compact_hand.length <= 25.0f,
        "Human hand art escaped the compact forearm-owned envelope");

    // Runner physics is authored for the fixed 60 Hz simulation contract;
    // 30 Hz exercises the supported slow-frame clamp without inventing a
    // variable 240 Hz integration mode that the application never runs.
    for (const float hz : std::array{ 30.0f, 60.0f })
    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            static_cast<std::uint64_t>(0x748C0u + static_cast<unsigned>(hz)) };
        environment.set_course(sim::CourseStage::crouch_walk, 0.30f);
        environment.set_course_motion_enabled(false);
        environment.set_physical_guidance_authority(1.0f);
        float maximum_pelvis_drop{};
        float minimum_head_clearance = std::numeric_limits<float>::infinity();
        const float requested_dt = 1.0f / hz;
        const float physical_dt = clamp(requested_dt, 1.0f / 240.0f, 1.0f / 30.0f);
        const int frames = static_cast<int>(std::ceil(20.0f / physical_dt));
        for (int frame = 0; frame < frames; ++frame)
        {
            std::array<float, sim::action_count> residual{};
            const auto action = rl::effective_policy_action(environment,
                residual, sim::CourseStage::crouch_walk, 1.0f);
            const sim::StepResult result = environment.step(action, requested_dt);
            if (result.terminated)
                std::cerr << "Crouch terminated " << hz << " Hz frame=" << frame
                    << " reason=" << sim::invalid_motion_name(result.invalid_reason)
                    << " root="
                    << environment.particles()[sim::CreatureBlueprint::humanoid()
                        .root_node].position.x
                    << ','
                    << environment.particles()[sim::CreatureBlueprint::humanoid()
                        .root_node].position.y
                    << " max_kmh=" << environment.maximum_speed_kmh()
                    << " distance=" << environment.crouch_walk_distance()
                    << " cycles=" << environment.gait_cycles()
                    << " duck=" << environment.duck_seconds()
                    << " torso_pitch=" << environment.current_crouch_posture().torso_pitch
                    << " pelvis_drop=" << environment.current_crouch_posture().pelvis_drop
                    << " support_margin=" << environment.current_crouch_posture().support_margin
                    << " feet_supported=" << environment.current_crouch_posture().feet_supported
                    << " landing=" << environment.last_landing_side()
                    << ':' << environment.last_landing_air_seconds()
                    << ':' << environment.last_landing_clearance()
                    << ':' << environment.last_landing_displacement()
                    << ':' << environment.last_landing_qualified()
                    << " nonfoot=" << environment.current_crouch_posture().non_foot_grounded
                    << " rolling_s=" << environment.body_rolling_seconds() << '\n';
            require(!result.terminated,
                "translating Human crouch walk terminated during its physical soak");
            const sim::CrouchPostureEvidence posture =
                environment.current_crouch_posture();
            maximum_pelvis_drop = std::max(maximum_pelvis_drop,
                posture.pelvis_drop);
            minimum_head_clearance = std::min(minimum_head_clearance,
                environment.duck_clearance_margin());
        }
        if (environment.crouch_walk_distance() < 1.50f
            || environment.gait_cycles() < 8u
            || environment.duck_seconds() < 3.5f
            || maximum_pelvis_drop < 0.30f)
            std::cerr << "Crouch walk evidence " << hz << " Hz: distance="
                << environment.crouch_walk_distance() << " cycles="
                << environment.gait_cycles() << " duck="
                << environment.duck_seconds() << " drop=" << maximum_pelvis_drop
                << " clearance=" << minimum_head_clearance << '\n';
        require(environment.crouch_walk_distance() >= 1.50f
                && environment.gait_cycles() >= 8u
                && environment.duck_seconds() >= 3.5f
                && maximum_pelvis_drop >= 0.30f,
            "Human crouch walk did not stay low while alternating and translating");
    }

    std::cout << "Runner v0.7.48 contact-led gait and species art checks passed\n";
    return 0;
}
