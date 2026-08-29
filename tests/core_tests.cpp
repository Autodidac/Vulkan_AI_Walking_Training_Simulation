#include "autonomy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"
#include "ui_layout.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static void solve_motor(Environment& environment,
            const MotorConstraint& motor, float action) noexcept
        {
            environment.solve_motor(motor, action);
        }

        static bool articulated_toes_move(Environment& environment) noexcept
        {
            MotorConstraint left{};
            MotorConstraint right{};
            if (!environment.articulated_toe_motor(true, left)
                || !environment.articulated_toe_motor(false, right))
                return false;
            const float left_before = environment.joint_angle(left);
            const float right_before = environment.joint_angle(right);
            std::array<float, anatomy_action_count> crouch{};
            crouch[0] = -0.45f;
            crouch[1] = 0.65f;
            crouch[2] = 0.45f;
            crouch[3] = -0.65f;
            for (int frame = 0; frame < 48; ++frame)
            {
                environment.update_articulated_toe_commands(crouch, 1.0f / 60.0f);
                for (int iteration = 0; iteration < 14; ++iteration)
                    environment.solve_articulated_toes();
                environment.limit_articulated_toe_rates(1.0f / 60.0f);
            }
            return std::abs(wrap_angle(environment.joint_angle(left) - left_before)) > 0.01f
                && std::abs(wrap_angle(environment.joint_angle(right) - right_before)) > 0.01f;
        }

        static bool articulated_toe_rate_is_bounded(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.60f);
            MotorConstraint left{};
            if (!environment.articulated_toe_motor(true, left))
                return false;
            std::array<float, anatomy_action_count> action{};
            float previous = environment.joint_angle(left);
            constexpr float dt = 1.0f / 60.0f;
            for (int frame = 0; frame < 180; ++frame)
            {
                const float sign = (frame & 1) == 0 ? 1.0f : -1.0f;
                action[0] = sign;
                action[1] = -sign;
                environment.update_articulated_toe_commands(action, dt);
                for (int iteration = 0; iteration < 14; ++iteration)
                    environment.solve_articulated_toes();
                environment.limit_articulated_toe_rates(dt);
                const float current = environment.joint_angle(left);
                const float delta = std::abs(wrap_angle(current - previous));
                const bool supported = environment.contact_supported(
                    environment.blueprint_.left_contact_node);
                if (delta > toe_angular_rate_limit(supported,
                        environment.course_stage_) * dt + 0.0002f)
                    return false;
                previous = current;
            }
            return true;
        }

        static void collapse_upper_body(Environment& environment) noexcept
        {
            if (environment.blueprint_.root_node >= environment.particles_.size()
                || environment.blueprint_.torso_node >= environment.particles_.size()
                || environment.blueprint_.head_node >= environment.particles_.size())
                return;
            const Vec2 root = environment.particles_[environment.blueprint_.root_node].position;
            environment.particles_[environment.blueprint_.torso_node].position = root + Vec2{ 0.05f, 0.20f };
            environment.particles_[environment.blueprint_.head_node].position = root + Vec2{ 0.12f, 0.28f };
            environment.particles_[environment.blueprint_.torso_node].previous =
                environment.particles_[environment.blueprint_.torso_node].position;
            environment.particles_[environment.blueprint_.head_node].previous =
                environment.particles_[environment.blueprint_.head_node].position;
        }

        static void detach_left_support_cluster(Environment& environment) noexcept
        {
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_left_support_seed(index))
                    continue;
                environment.particles_[index].position.x += 4.0f;
                environment.particles_[index].previous = environment.particles_[index].position;
            }
        }

        static void set_duck_pressure(Environment& environment, float pressure) noexcept
        {
            environment.duck_obstacle_weight_ = pressure;
        }

        static void set_shuttle_state(Environment& environment,
            ShuttleState state) noexcept
        {
            environment.shuttle_state_ = state;
        }

        static bool hip_hinge_is_rejected(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            auto pin = [&](std::size_t node)
            {
                if (node >= environment.particles_.size())
                    return;
                Particle& particle = environment.particles_[node];
                particle.position.y = environment.ground_height_at(particle.position.x)
                    + ground_contact_offset(true, particle.radius);
                particle.previous = particle.position;
                particle.grounded = true;
            };
            pin(environment.blueprint_.left_contact_node);
            pin(environment.blueprint_.right_contact_node);
            for (const std::uint16_t node : environment.blueprint_.additional_left_contact_nodes)
                pin(node);
            for (const std::uint16_t node : environment.blueprint_.additional_right_contact_nodes)
                pin(node);
            const Vec2 root = environment.particles_[environment.blueprint_.root_node].position;
            environment.particles_[environment.blueprint_.torso_node].position =
                root + Vec2{ 1.05f, 0.42f };
            environment.particles_[environment.blueprint_.head_node].position =
                root + Vec2{ 1.72f, 0.58f };
            environment.particles_[environment.blueprint_.torso_node].previous =
                environment.particles_[environment.blueprint_.torso_node].position;
            environment.particles_[environment.blueprint_.head_node].previous =
                environment.particles_[environment.blueprint_.head_node].position;
            return !environment.crouch_posture_valid();
        }

        static bool guided_squat_is_valid(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            for (int frame = 0; frame < 900; ++frame)
            {
                const auto action = rl::duck_teacher_action(environment);
                const StepResult step = environment.step(action);
                const CrouchPostureEvidence evidence =
                    environment.current_crouch_posture();
                if (crouch_posture_qualified(evidence)
                    && evidence.pelvis_drop >= 0.22f
                    && evidence.left_knee_flex >= 0.12f
                    && evidence.right_knee_flex >= 0.12f
                    && evidence.torso_pitch <= 0.65f)
                    return true;
                if (step.terminated)
                    return false;
            }
            return false;
        }

        static bool crouch_guide_preserves_support_dynamics(
            Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            environment.elapsed_seconds_ = 6.0f;
            environment.duck_press_contact_seen_ = true;
            if (!environment.valid_node(environment.blueprint_.left_contact_node))
                return false;
            Particle& support = environment.particles_[
                environment.blueprint_.left_contact_node];
            support.position.x += 0.093f;
            support.position.y += 0.017f;
            support.previous.x = support.position.x - 0.041f;
            support.previous.y = support.position.y + 0.006f;
            support.grounded = false;
            const Vec2 position_before = support.position;
            const Vec2 previous_before = support.previous;
            const bool grounded_before = support.grounded;
            environment.stabilize_duck_posture();
            return length(support.position - position_before) < 1.0e-7f
                && length(support.previous - previous_before) < 1.0e-7f
                && support.grounded == grounded_before;
        }

        static bool static_friction_anchor_is_physical(
            Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.25f);
            if (!environment.valid_node(environment.blueprint_.left_contact_node))
                return false;
            constexpr float dt = 1.0f / 60.0f;
            const std::size_t node = environment.blueprint_.left_contact_node;
            Particle& support = environment.particles_[node];
            const float ground = environment.ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.position.y = ground;
            support.previous = support.position;
            support.grounded = true;
            environment.solve_ground(dt);
            const float anchor_x = support.position.x;

            support.position += Vec2{ 0.14f, 0.55f };
            support.previous = support.position - Vec2{ 0.05f, 0.20f };
            support.grounded = false;
            environment.solve_ground(dt);
            const bool static_held = support.grounded
                && std::abs(support.position.x - anchor_x) < 0.05f
                && std::abs(support.position.y
                    - (environment.ground_height_at(support.position.x)
                        + ground_contact_offset(true, support.radius))) < 1.0e-6f
                && length(support.position - support.previous) < 1.0e-7f;

            environment.set_course(CourseStage::uneven, 0.25f);
            const float moving_ground = environment.ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.position.y = moving_ground;
            support.previous = support.position;
            support.grounded = true;
            environment.solve_ground(dt);
            support.position += Vec2{ 0.0f, 0.040f };
            support.previous = support.position - Vec2{ 0.0f, 0.42f * dt };
            support.grounded = false;
            environment.solve_ground(dt);
            return static_held && !support.grounded;
        }

        static bool press_collision_resolves_below(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.head_node))
                return false;
            Particle& head = environment.particles_[environment.blueprint_.head_node];
            head.previous = head.position - Vec2{ 0.07f, -0.11f };
            const Vec2 velocity_before = head.position - head.previous;
            const float x_before = head.position.x;
            const float bottom = head.position.y + head.radius * 0.45f;
            environment.course_features_.clear();
            environment.course_features_.push_back({
                CourseFeatureKind::duck_press,
                { head.position.x, bottom + 0.16f }, { 1.5f, 0.16f }, 0.0f,
                { 0.0f, -0.5f }, -2
            });
            environment.duck_press_contact_this_step_ = false;
            environment.duck_press_max_penetration_ = 0.0f;
            environment.solve_course();
            const Vec2 velocity_after = head.position - head.previous;
            return environment.duck_press_contact_this_step_
                && head.position.y + head.radius <= bottom + 0.0001f
                && std::abs(head.position.x - x_before) < 0.000001f
                && length(velocity_after - velocity_before) < 0.000001f;
        }

        static bool press_anchor_remains_fixed(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            if (!environment.valid_node(environment.blueprint_.root_node))
                return false;
            const float expected = environment.blueprint_.nodes[
                environment.blueprint_.root_node].x;
            Particle& root = environment.particles_[environment.blueprint_.root_node];
            root.position.x += 1.75f;
            root.previous.x += 1.75f;
            environment.elapsed_seconds_ = 3.5f;
            environment.duck_press_completed_ = false;
            environment.rebuild_course_features();
            return environment.course_features_.size() == 1u
                && environment.course_features_.front().kind == CourseFeatureKind::duck_press
                && std::abs(environment.course_features_.front().center.x - expected) < 0.000001f;
        }

        static void force_fused_supports(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return;
            const float left_anchor = environment.particles_[
                environment.blueprint_.left_contact_node].position.x;
            const float right_anchor = environment.particles_[
                environment.blueprint_.right_contact_node].position.x;
            const float center = 0.5f * (left_anchor + right_anchor);
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                const bool left = environment.blueprint_.is_left_support_seed(index);
                const bool right = environment.blueprint_.is_right_support_seed(index);
                if (!left && !right)
                    continue;
                // Fuse the two feet by translating each complete cluster. Do
                // not collapse heel, ball, and toe into one impossible point.
                const float anchor = left ? left_anchor : right_anchor;
                const float offset = environment.particles_[index].position.x - anchor;
                environment.particles_[index].position.x = center + offset;
                environment.particles_[index].previous.x = center + offset;
            }
        }

        static void separate_supports(Environment& environment) noexcept
        {
            environment.separate_support_clusters();
        }

        static bool moving_stage_allows_leg_crossing(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.45f);
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return false;
            const float left_before = environment.particles_[
                environment.blueprint_.left_contact_node].position.x;
            const float right_before = environment.particles_[
                environment.blueprint_.right_contact_node].position.x;
            const float left_shift = right_before - left_before + 0.36f;
            const float right_shift = left_before - right_before - 0.36f;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_left_support_seed(index))
                {
                    environment.particles_[index].position.x += left_shift;
                    environment.particles_[index].previous.x += left_shift;
                }
                if (environment.blueprint_.is_right_support_seed(index))
                {
                    environment.particles_[index].position.x += right_shift;
                    environment.particles_[index].previous.x += right_shift;
                }
            }
            const float crossed_gap = primary_support_gap(environment);
            environment.separate_support_clusters();
            return crossed_gap < 0.0f
                && primary_support_gap(environment) < 0.0f;
        }

        static float primary_support_gap(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.right_contact_node].position.x
                - environment.particles_[environment.blueprint_.left_contact_node].position.x;
        }

        static float semantic_support_cluster_gap(
            const Environment& environment) noexcept
        {
            float left = 0.0f;
            float right = 0.0f;
            std::size_t left_count = 0u;
            std::size_t right_count = 0u;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_left_support_seed(index))
                {
                    left += environment.particles_[index].position.x;
                    ++left_count;
                }
                if (environment.blueprint_.is_right_support_seed(index))
                {
                    right += environment.particles_[index].position.x;
                    ++right_count;
                }
            }
            if (left_count == 0u || right_count == 0u)
                return 0.0f;
            return std::abs(right / static_cast<float>(right_count)
                - left / static_cast<float>(left_count));
        }

        static float minimum_semantic_support_clearance(
            const Environment& environment) noexcept
        {
            std::array<std::uint16_t, 32> supports{};
            std::size_t count = 0;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_support_seed(index))
                    supports[count++] = static_cast<std::uint16_t>(index);
            }
            float minimum = 1000.0f;
            for (std::size_t first = 0; first < count; ++first)
            {
                for (std::size_t second = first + 1; second < count; ++second)
                {
                    const std::uint16_t lhs_index = supports[first];
                    const std::uint16_t rhs_index = supports[second];
                    const bool same_foot =
                        (environment.blueprint_.is_left_support_seed(lhs_index)
                            && environment.blueprint_.is_left_support_seed(rhs_index))
                        || (environment.blueprint_.is_right_support_seed(lhs_index)
                            && environment.blueprint_.is_right_support_seed(rhs_index));
                    if (same_foot)
                        continue;
                    const Particle& lhs = environment.particles_[lhs_index];
                    const Particle& rhs = environment.particles_[rhs_index];
                    const float clearance = std::abs(rhs.position.x - lhs.position.x)
                        - lhs.radius - rhs.radius;
                    minimum = std::min(minimum, clearance);
                }
            }
            return minimum;
        }

        static void complete_duck_press(Environment& environment) noexcept
        {
            environment.duck_press_completed_ = true;
            environment.duck_walk_started_seconds_ = 9.0f;
            environment.elapsed_seconds_ = 10.0f;
            environment.rebuild_course_features();
        }

        static void qualify_crouch_walk(Environment& environment) noexcept
        {
            qualify_stable_stance(environment);
            environment.duck_press_completed_ = true;
            environment.duck_walk_started_seconds_ = 1.0f;
            environment.duck_active_ = true;
            environment.duck_recovery_count_ = 1u;
            environment.duck_seconds_ = 3.0f;
            environment.crouch_walk_seconds_ = 2.5f;
            environment.crouch_walk_distance_ = 1.2f;
            environment.alternating_steps_ = 5u;
            environment.obstacles_passed_ = 4u;
        }

        static void force_non_foot_contact(Environment& environment) noexcept
        {
            environment.non_foot_grounded_ = true;
        }

        static void qualify_stable_stance(Environment& environment) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.elapsed_seconds_ = 6.5f;
            environment.stable_stance_seconds_ = 6.5f;
            environment.longest_stable_stance_seconds_ = 6.5f;
            environment.maximum_joint_speed_ = 0.5f;
            environment.uncontrolled_spin_turns_ = 0.0f;
        }

        static void qualify_walk_evidence(Environment& environment,
            float longest_stance, std::uint32_t steps,
            std::uint32_t crossings, float distance, float elapsed) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.elapsed_seconds_ = elapsed;
            environment.stable_stance_seconds_ = 0.0f;
            environment.longest_stable_stance_seconds_ = longest_stance;
            environment.alternating_steps_ = steps;
            environment.limb_crossings_ = crossings;
            environment.distance_travelled_ = distance;
        }

        static void force_standing_spin(Environment& environment, float turns) noexcept
        {
            environment.uncontrolled_spin_turns_ = turns;
        }

        static void force_sustained_leg_scissor(Environment& environment) noexcept
        {
            environment.maximum_lower_leg_scissor_seconds_ =
                sustained_scissor_limit_seconds + 0.01f;
        }

        static void force_arms_overhead(Environment& environment) noexcept
        {
            if (environment.particles_.size() < 13u)
                return;
            const Vec2 left = environment.particles_[7].position;
            const Vec2 right = environment.particles_[10].position;
            environment.particles_[8].position = left + Vec2{ -0.08f, 0.70f };
            environment.particles_[9].position = left + Vec2{ -0.02f, 1.34f };
            environment.particles_[11].position = right + Vec2{ 0.08f, 0.70f };
            environment.particles_[12].position = right + Vec2{ 0.02f, 1.34f };
            for (const std::size_t index : { 8u, 9u, 11u, 12u })
                environment.particles_[index].previous = environment.particles_[index].position;
        }

        struct StanceFrame
        {
            bool supported{};
            bool body_clear{};
            bool upright{};
            bool head_high{};
            bool low_slip{};
            bool low_torso_turn{};
            bool low_joint_speed{};
            bool low_vertical_speed{};
        };

        static StanceFrame stance_frame(const Environment& environment) noexcept
        {
            float joint_speed = 0.0f;
            for (std::size_t index = 0;
                index < environment.blueprint_.active_motor_count; ++index)
            {
                joint_speed = std::max(joint_speed,
                    std::abs(environment.angular_velocities_[index]));
            }
            const std::uint16_t root = environment.blueprint_.root_node;
            const float vertical_speed = root < environment.particles_.size()
                ? (environment.particles_[root].position.y
                    - environment.particles_[root].previous.y) * 60.0f
                : 0.0f;
            const std::uint16_t head = environment.blueprint_.head_node;
            const float head_clearance = head < environment.particles_.size()
                ? environment.particles_[head].position.y
                    - environment.ground_height_at(
                        environment.particles_[head].position.x)
                : 0.0f;
            const float rest_head_clearance = head < environment.blueprint_.nodes.size()
                ? environment.blueprint_.nodes[head].y : 0.0f;
            const float head_ratio = rest_head_clearance > 1.0e-5f
                ? head_clearance / rest_head_clearance : 0.0f;
            return {
                environment.contact_supported(environment.blueprint_.left_contact_node)
                    || environment.contact_supported(environment.blueprint_.right_contact_node),
                !environment.non_foot_grounded_,
                environment.torso_uprightness() >= 0.84f,
                head_ratio >= 0.62f,
                environment.stance_slip_speed_ <= 0.10f,
                std::abs(environment.torso_turn_speed_) <= 2.00f,
                joint_speed <= 12.0f,
                std::abs(vertical_speed) <= 1.50f
            };
        }
    };
}

namespace
{
    template <typename T>
    bool write_v0727_value(std::ofstream& output, const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        output.write(reinterpret_cast<const char*>(&value), sizeof(T));
        return static_cast<bool>(output);
    }

    bool write_v0727_vector(std::ofstream& output, const std::vector<float>& values)
    {
        const std::uint64_t count = values.size();
        if (!write_v0727_value(output, count))
            return false;
        if (values.empty())
            return true;
        output.write(reinterpret_cast<const char*>(values.data()),
            static_cast<std::streamsize>(values.size() * sizeof(float)));
        return static_cast<bool>(output);
    }

    bool write_v0727_metrics(std::ofstream& output,
        const runner::rl::TrainingMetrics& value)
    {
        return write_v0727_value(output, value.update)
            && write_v0727_value(output, value.environment_steps)
            && write_v0727_value(output, value.total_updates)
            && write_v0727_value(output, value.total_environment_steps)
            && write_v0727_value(output, value.total_episodes)
            && write_v0727_value(output, value.total_valid_episodes)
            && write_v0727_value(output, value.total_invalid_episodes)
            && write_v0727_value(output, value.total_resets)
            && write_v0727_value(output, value.total_alternating_steps)
            && write_v0727_value(output, value.total_falls)
            && write_v0727_value(output, value.total_collisions)
            && write_v0727_value(output, value.total_powered_jumps)
            && write_v0727_value(output, value.total_landed_jumps)
            && write_v0727_value(output, value.total_landed_flips)
            && write_v0727_value(output, value.total_obstacles_passed)
            && write_v0727_value(output, value.total_distance)
            && write_v0727_value(output, value.total_training_seconds)
            && write_v0727_value(output, value.mean_reward)
            && write_v0727_value(output, value.mean_episode_distance)
            && write_v0727_value(output, value.mean_speed)
            && write_v0727_value(output, value.policy_loss)
            && write_v0727_value(output, value.value_loss)
            && write_v0727_value(output, value.entropy)
            && write_v0727_value(output, value.learning_rate)
            && write_v0727_value(output, value.evaluation_reward)
            && write_v0727_value(output, value.evaluation_distance)
            && write_v0727_value(output, value.evaluation_speed)
            && write_v0727_value(output, value.evaluation_score)
            && write_v0727_value(output, value.evaluation_survival)
            && write_v0727_value(output, value.evaluation_collisions)
            && write_v0727_value(output, value.evaluation_airborne_ratio)
            && write_v0727_value(output, value.evaluation_stride_events)
            && write_v0727_value(output, value.evaluation_duck_seconds)
            && write_v0727_value(output, value.evaluation_powered_jumps)
            && write_v0727_value(output, value.evaluation_jump_landings)
            && write_v0727_value(output, value.evaluation_spin_turns)
            && write_v0727_value(output, value.evaluation_spin_landings)
            && write_v0727_value(output, value.evaluation_obstacles_passed)
            && write_v0727_value(output, value.evaluation_stable_stance)
            && write_v0727_value(output, value.evaluation_longest_stance)
            && write_v0727_value(output, value.evaluation_duck_recoveries)
            && write_v0727_value(output, value.evaluation_max_joint_speed)
            && write_v0727_value(output, value.evaluation_quality_key)
            && write_v0727_value(output, value.evaluation_rejection_mask)
            && write_v0727_value(output, value.evaluation_invalid_runs)
            && write_v0727_value(output, value.evaluation_valid)
            && write_v0727_value(output, value.best_evaluation_distance)
            && write_v0727_value(output, value.best_evaluation_score)
            && write_v0727_value(output, value.best_quality_key)
            && write_v0727_value(output, value.best_update)
            && write_v0727_value(output, value.evaluation_count)
            && write_v0727_value(output, value.imitation_samples)
            && write_v0727_value(output, value.imitation_weight)
            && write_v0727_value(output, value.imitation_source_score);
    }
    bool write_v0727_checkpoint(const std::filesystem::path& path,
        const runner::rl::PpoTrainer::CheckpointData& data)
    {
        constexpr std::array<char, 8> magic{
            'E', 'P', 'P', 'O', '2', '8', '\0', '\1'
        };
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        const auto stage = static_cast<std::uint8_t>(data.stage);
        output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        return output
            && write_v0727_value(output, data.training_semantics)
            && write_v0727_value(output, data.rig_signature)
            && write_v0727_value(output, data.optimizer_step)
            && write_v0727_value(output, data.random_state)
            && write_v0727_value(output, stage)
            && write_v0727_value(output, data.difficulty)
            && write_v0727_metrics(output, data.metrics)
            && write_v0727_vector(output, data.parameters)
            && write_v0727_vector(output, data.first_moment)
            && write_v0727_vector(output, data.second_moment)
            && write_v0727_vector(output, data.best_parameters)
            && write_v0727_vector(output, data.reward_history)
            && write_v0727_vector(output, data.speed_history);
    }
    bool rewrite_checkpoint_magic(const std::filesystem::path& path,
        const std::array<char, 8>& magic)
    {
        std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!stream)
            return false;
        stream.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        return static_cast<bool>(stream);
    }
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner core test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace runner;

    const sim::CreatureBlueprint scaffold = sim::CreatureBlueprint::scaffold();
    require(scaffold.valid() && scaffold.active_motor_count == 4u
            && scaffold.paired_leg_chains(),
        "minimal scaffold is not a valid two-joint-per-side training rig");
    bool topology_mutation_seen = false;
    bool parametric_mutation_seen = false;
    for (std::uint64_t generation = 0; generation < 22u; ++generation)
    {
        const rl::RigMutationCandidate mutation = rl::evolve_rig_candidate(
            scaffold, generation);
        if (!mutation.changed)
            continue;
        require(mutation.blueprint.valid(),
            "rig evolution published a structurally invalid candidate");
        const auto articulated_foot_intact = [](const sim::CreatureBlueprint& rig,
            bool left)
        {
            const auto& support_nodes = left
                ? rig.additional_left_contact_nodes
                : rig.additional_right_contact_nodes;
            if (support_nodes.size() < 2u)
                return true;
            const std::uint16_t ball = support_nodes[0];
            const std::uint16_t toe = support_nodes[1];
            return std::ranges::any_of(rig.bones,
                [ball, toe](const sim::DistanceConstraint& bone)
                {
                    return (bone.a == ball && bone.b == toe)
                        || (bone.a == toe && bone.b == ball);
                });
        };
        require(mutation.blueprint.support_seed_count() >= scaffold.support_seed_count(),
            "topology evolution removed a semantic foot contact");
        require(articulated_foot_intact(mutation.blueprint, true)
                && articulated_foot_intact(mutation.blueprint, false),
            "topology evolution split or detached an articulated toe edge");
        topology_mutation_seen = topology_mutation_seen || mutation.topology_changed;
        parametric_mutation_seen = parametric_mutation_seen || !mutation.topology_changed;
    }
    require(topology_mutation_seen && parametric_mutation_seen,
        "rig evolution does not produce both topology and parameter candidates");
    const rl::RigMutationCandidate articulated_growth =
        rl::evolve_rig_candidate(scaffold, 5u);
    require(articulated_growth.changed && articulated_growth.topology_changed
            && articulated_growth.blueprint.active_motor_count
                == scaffold.active_motor_count + 1u
            && articulated_growth.activated_motor_mask
                == static_cast<std::uint8_t>(1u << scaffold.active_motor_count),
        "bone split does not activate one neutral trainable joint slot");
    const std::size_t grown_slot = scaffold.active_motor_count;
    require(articulated_growth.blueprint.motors[grown_slot].enabled
            && articulated_growth.blueprint.motors[grown_slot].a
                < articulated_growth.blueprint.nodes.size()
            && articulated_growth.blueprint.motors[grown_slot].pivot
                < articulated_growth.blueprint.nodes.size()
            && articulated_growth.blueprint.motors[grown_slot].c
                < articulated_growth.blueprint.nodes.size(),
        "newly activated topology motor is not structurally valid");

    rl::PolicyNetwork neutral_policy{ 0xA4710u };
    std::array<float, sim::observation_count> neutral_observation{};
    neutral_observation.fill(0.5f);
    neutral_policy.neutralize_action_slot(grown_slot);
    require(std::abs(neutral_policy.evaluate(neutral_observation).mean[grown_slot])
            < 1.0e-7f,
        "new topology action slot retains stale actor motion after neutralization");

    const sim::DuckPressProfile press_clear = sim::duck_press_profile(1.0f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_descend = sim::duck_press_profile(3.5f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_hold = sim::duck_press_profile(5.5f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_retract = sim::duck_press_profile(8.0f, 0.5f, 5.0f);
    require(press_clear.bottom_y > 6.0f && press_descend.descending
            && press_descend.vertical_velocity < 0.0f,
        "duck press does not begin clear and descend gradually");
    require(press_hold.holding && press_hold.bottom_y < 4.2f,
        "duck press does not hold a meaningful crouch target");
    require(press_retract.retracting && press_retract.vertical_velocity > 0.0f,
        "duck press does not retract after the hold");
    const std::array stub_rigs{
        sim::CreatureBlueprint::scaffold(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid()
    };
    for (const sim::CreatureBlueprint& rig : stub_rigs)
    {
        require(rig.support_seed_count() == 2u
                && rig.additional_left_contact_nodes.empty()
                && rig.additional_right_contact_nodes.empty()
                && rig.left_contact_node < rig.radii.size()
                && rig.right_contact_node < rig.radii.size()
                && rig.radii[rig.left_contact_node] >= 0.104f
                && rig.radii[rig.right_contact_node] >= 0.104f
                && rig.radii[rig.left_contact_node] <= 0.1121f
                && rig.radii[rig.right_contact_node] <= 0.1121f,
            "paired rig does not use one compact loaded support stub per leg");
    }
    const sim::CreatureBlueprint chicken_topology =
        sim::CreatureBlueprint::chicken();
    require(chicken_topology.paired_leg_chains()
            && !chicken_topology.horizontal_multi_support_plan(),
        "chicken paired-leg balance topology is not isolated from multi-support press logic");
    require(chicken_topology.radii[chicken_topology.left_contact_node] >= 0.06f
            && chicken_topology.radii[chicken_topology.left_contact_node] <= 0.08f
            && chicken_topology.radii[chicken_topology.right_contact_node] >= 0.06f
            && chicken_topology.radii[chicken_topology.right_contact_node] <= 0.08f,
        "chicken talons inherited the Human passive-boot contact radius");

    const sim::CreatureBlueprint quadruped_topology =
        sim::CreatureBlueprint::quadruped();
    require(quadruped_topology.support_seed_count() >= 4u
            && !quadruped_topology.monopedal_gait()
            && quadruped_topology.horizontal_multi_support_plan(),
        "quadruped semantic supports do not select multi-support press topology");
    const sim::Environment discovery_environment(sim::CreatureBlueprint::biped(), 83u);
    const std::size_t crouch_lane = 2u
        * discovery_environment.blueprint().active_motor_count + 6u;
    const rl::MotorDiscoveryProbe crouch_probe = rl::motor_discovery_probe(
        discovery_environment, crouch_lane, 120u, 0u);
    require(crouch_probe.action[0] < 0.0f && crouch_probe.action[1] > 0.0f
            && crouch_probe.action[2] > 0.0f && crouch_probe.action[3] < 0.0f,
        "motor discovery does not explore simultaneous bilateral hip-knee flexion");
    sim::Environment press_collision_environment(sim::CreatureBlueprint::humanoid(), 71u);
    require(sim::EnvironmentTestAccess::press_collision_resolves_below(
            press_collision_environment),
        "duck press collision injects velocity or horizontal drag");
    sim::Environment press_anchor_environment(sim::CreatureBlueprint::humanoid(), 73u);
    require(sim::EnvironmentTestAccess::press_anchor_remains_fixed(
            press_anchor_environment),
        "duck press follows a sliding rig instead of staying fixed over the station");

    require(ui_layout::top_bar_box(1970.0f).width == 1970.0f,
        "top GUI background does not span the full drawable width");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::metric) - 10.0f) < 0.001f,
        "metric near-course markers are not 10 metres apart");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::imperial) - 15.24f) < 0.001f,
        "imperial near-course markers are not 50 feet apart");
    require(ui_layout::lifetime_delta(120u, 20u) == 100u
            && ui_layout::lifetime_delta(20u, 120u) == 0u,
        "rig lifetime counters can underflow");
    const ui_layout::TrainingTotals first_rig_current{
        120u, 1'200u, 900u, 300u, 1'200u, 480u, 12u, 7u, 9u, 3u,
        1'200.0, 123.5
    };
    const ui_layout::TrainingTotals first_rig_start{
        20u, 200u, 150u, 50u, 200u, 80u, 2u, 1u, 4u, 1u,
        200.0, 23.5
    };
    const ui_layout::TrainingTotals first_rig_delta =
        ui_layout::training_totals_delta(first_rig_current, first_rig_start);
    require(first_rig_delta.optimizer_updates == 100u
            && first_rig_delta.completed_agent_runs == 1'000u
            && first_rig_delta.support_gait_cycles == 400u
            && first_rig_delta.episode_restarts == 1'000u
            && first_rig_delta.passed_stage_checks == 750u
            && first_rig_delta.failed_stage_checks == 250u
            && std::abs(first_rig_delta.agent_sim_seconds - 1'000.0) < 0.001
            && std::abs(first_rig_delta.agent_distance - 100.0) < 0.001
            && ui_layout::training_totals_consistent(first_rig_delta),
        "selected-rig session delta mixes scopes, result classes, or display units");
    const ui_layout::TrainingTotals reset_current{
        2u, 3u, 1u, 2u, 3u, 4u, 0u, 0u, 0u, 0u, 5.0, 6.0
    };
    const ui_layout::TrainingTotals reset_delta =
        ui_layout::training_totals_delta(reset_current, first_rig_current);
    require(reset_delta.optimizer_updates == 0u
            && reset_delta.completed_agent_runs == 0u
            && reset_delta.agent_sim_seconds == 0.0
            && reset_delta.agent_distance == 0.0,
        "fresh-controller counter reset inflated app-session totals");
    ui_layout::TrainingTotals session_totals{};
    ui_layout::accumulate_training_totals(session_totals, first_rig_delta);
    ui_layout::accumulate_training_totals(session_totals,
        ui_layout::TrainingTotals{ 5u, 50u, 40u, 10u, 50u, 20u, 1u, 2u, 3u, 1u,
            50.0, 8.0 });
    require(session_totals.optimizer_updates == 105u
            && session_totals.completed_agent_runs == 1'050u
            && session_totals.support_gait_cycles == 420u
            && session_totals.passed_stage_checks == 790u
            && session_totals.failed_stage_checks == 260u
            && std::abs(session_totals.agent_sim_seconds - 1'050.0) < 0.001
            && std::abs(session_totals.agent_distance - 108.0) < 0.001
            && ui_layout::training_totals_consistent(session_totals)
            && !ui_layout::training_totals_consistent(
                ui_layout::TrainingTotals{ 0u, 2u, 1u, 0u }),
        "all-rig app-session totals overwrite, double-count, or hide result classes");
    require(ui_layout::training_totals_scope_headings[0]
                == "SELECTED RIG - THIS SELECTION"
            && ui_layout::training_totals_scope_headings[1]
                == "THIS APP SESSION - ALL RIGS"
            && ui_layout::training_totals_scope_headings[2]
                == "SELECTED RIG - LIFETIME"
            && ui_layout::totals_wall_time_label == "WALL TIME"
            && ui_layout::totals_agent_sim_time_label == "AGENT-SIM TIME"
            && ui_layout::totals_support_cycles_label == "SUPPORT GAIT CYCLES",
        "default totals page scope headings or unit labels are ambiguous");
    require(std::abs(sim::accepted_forward_odometer_progress(0.10f, 1.0f / 60.0f)
                - 0.10f) < 0.0001f
            && sim::accepted_forward_odometer_progress(-0.01f, 1.0f / 60.0f) == 0.0f
            && sim::accepted_forward_odometer_progress(1.0f, 1.0f / 60.0f) == 0.0f
            && sim::accepted_forward_odometer_progress(
                std::numeric_limits<float>::infinity(), 1.0f / 60.0f) == 0.0f,
        "odometer accepts backward, teleport, or non-finite displacement");
    rl::TrainingMetrics cumulative{};
    cumulative.total_episodes = 12u;
    cumulative.total_valid_episodes = 9u;
    cumulative.total_invalid_episodes = 3u;
    cumulative.total_resets = 14u;
    cumulative.total_alternating_steps = 48u;
    cumulative.total_falls = 2u;
    cumulative.total_collisions = 7u;
    cumulative.total_powered_jumps = 5u;
    cumulative.total_landed_jumps = 4u;
    cumulative.total_landed_flips = 1u;
    cumulative.total_obstacles_passed = 11u;
    cumulative.total_distance = 123.5;
    require(cumulative.total_valid_episodes + cumulative.total_invalid_episodes
            == cumulative.total_episodes
            && cumulative.total_landed_jumps <= cumulative.total_powered_jumps,
        "cumulative runtime statistics are internally inconsistent");
    require(ui_layout::live_layout_valid(1280.0f, 820.0f),
        "supported minimum live layout overlaps its panel, telemetry, or PIP");
    require(!ui_layout::supported_window(1279.0f, 820.0f)
            && !ui_layout::supported_window(1280.0f, 819.0f),
        "undersized windows are incorrectly treated as fully supported");
    const ui_layout::Box minimum_content = ui_layout::content_box(1280.0f, 820.0f);
    const ui_layout::Box minimum_world = ui_layout::live_world_box(minimum_content);
    const ui_layout::Box minimum_pip = ui_layout::training_pip_box(minimum_world);
    require(ui_layout::contains(minimum_world, minimum_pip),
        "training PIP escapes the world viewport");
    require(!ui_layout::overlaps(minimum_pip,
                ui_layout::primary_telemetry_box(minimum_world))
            && !ui_layout::overlaps(minimum_pip,
                ui_layout::bottom_telemetry_box(minimum_world)),
        "training PIP overlaps primary telemetry at the supported minimum window");

    for (const auto& size : ui_layout::validation_sizes)
    {
        const ui_layout::Box content = ui_layout::content_box(size[0], size[1]);
        require(ui_layout::rig_lab_layout_valid(size[0], size[1]),
            "Rig Lab responsive layout overlaps or underflows");
        const ui_layout::Box rig_live = ui_layout::rig_lab_live_box(content);
        const ui_layout::Box rig_trainer =
            ui_layout::rig_lab_trainer_panel_box(content);
        const ui_layout::Box rig_panel = ui_layout::rig_lab_panel_box(content);
        const ui_layout::Box rig_world = ui_layout::rig_lab_world_box(content);
        require(rig_trainer.width >= 400.0f
                && !ui_layout::overlaps(rig_trainer, rig_panel)
                && !ui_layout::overlaps(rig_trainer, rig_world),
            "Rig Lab does not preserve the complete trainer beside editing controls");
        if (ui_layout::rig_lab_shows_live(content))
        {
            require(rig_live.width >= 620.0f
                    && !ui_layout::overlaps(rig_live, rig_trainer),
                "wide Rig Lab does not preserve its fourth Live viewport pane");
        }
        const ui_layout::BlueprintFit fit = ui_layout::fit_blueprint(
            rig_world, -0.9f, 1.1f, 0.0f, 4.9f);
        const float left = ui_layout::blueprint_screen_x(fit, rig_world, -0.9f);
        const float right = ui_layout::blueprint_screen_x(fit, rig_world, 1.1f);
        const float top = ui_layout::blueprint_screen_y(fit, 4.9f);
        const float ground = ui_layout::blueprint_screen_y(fit, 0.0f);
        require(left >= rig_world.x + 47.0f
                && right <= rig_world.x + rig_world.width - 47.0f
                && top >= rig_world.y + 69.0f
                && ground <= rig_world.y + rig_world.height - 47.0f,
            "Rig Lab active graph escapes its bounded world framing");
    }
    {
        const ui_layout::Box viewport{ 572.0f, 78.0f, 1360.0f, 1080.0f };
        const ui_layout::BlueprintFit malformed = ui_layout::fit_blueprint(
            viewport, -500.0f, 500.0f, -100.0f, 700.0f);
        require(ui_layout::blueprint_screen_x(malformed, viewport, -500.0f)
                    >= viewport.x + 47.0f
                && ui_layout::blueprint_screen_x(malformed, viewport, 500.0f)
                    <= viewport.x + viewport.width - 47.0f
                && ui_layout::blueprint_screen_y(malformed, 700.0f)
                    >= viewport.y + 69.0f
                && ui_layout::blueprint_screen_y(malformed, -100.0f)
                    <= viewport.y + viewport.height - 47.0f,
            "large edited Rig Lab graph is clipped by a minimum zoom floor");
        const ui_layout::Box test_card{ 20.0f, 200.0f, 520.0f, 225.0f };
        const ui_layout::RigLabTestLayout rows =
            ui_layout::rig_lab_test_layout(test_card);
        require(ui_layout::contains(test_card, rows.selection_row)
                && ui_layout::contains(test_card, rows.range_row)
                && ui_layout::contains(test_card, rows.pattern_row)
                && ui_layout::contains(test_card, rows.status_row)
                && ui_layout::contains(test_card, rows.manual_slider)
                && !ui_layout::overlaps(rows.selection_row, rows.range_row)
                && !ui_layout::overlaps(rows.range_row, rows.pattern_row)
                && !ui_layout::overlaps(rows.pattern_row, rows.status_row)
                && !ui_layout::overlaps(rows.status_row, rows.manual_slider),
            "Rig Lab Test controls or status rows overlap");
    }
    require(sim::classify_motion_gate(1.0f, 50.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::overspeed, "50 km/h hard gate missing");
    require(sim::classify_motion_gate(-0.2f, 0.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::flipped, "flip hard gate missing outside flip lessons");
    require(sim::classify_motion_gate(-0.2f, 0.0f, { 0.0f, 4.0f }, 0.4f, 2.7f, 0.0f,
            false, sim::CourseStage::duck_bars, 0.5f)
        == sim::InvalidMotion::none, "controlled flip lesson still rejects an airborne flip");
    require(sim::classify_motion_gate(0.4f, 0.0f, { 0.0f, 4.0f }, 1.2f, 2.7f, 0.0f,
            false, sim::CourseStage::duck_bars, 3.21f)
        == sim::InvalidMotion::excessive_spins, "more than three spins is not rejected");
    require(rl::balance_mastery_lock_confirmations == 3
            && rl::mastery_lock_confirmations >= 8
            && rl::required_mastery_confirmations(sim::CourseStage::balance) == 3
            && rl::required_mastery_confirmations(sim::CourseStage::duck_press) >= 8,
        "standing and later-stage mastery confirmation counts are incorrect");
    require(rl::should_confirm_retained_mastery(
                sim::CourseStage::duck_press, 1, true)
            && rl::should_confirm_retained_mastery(
                sim::CourseStage::duck_press, 7, true)
            && !rl::should_confirm_retained_mastery(
                sim::CourseStage::duck_press, 0, true)
            && !rl::should_confirm_retained_mastery(
                sim::CourseStage::duck_press, 8, true)
            && !rl::should_confirm_retained_mastery(
                sim::CourseStage::duck_press, 1, false),
        "retained mastery confirmation boundaries are incorrect");
    require(rl::evaluation_seed(2u, 7u) == rl::evaluation_seed(2u, 7u)
            && rl::evaluation_seed(2u, 7u) != rl::evaluation_seed(2u, 8u)
            && rl::evaluation_seed(2u, 7u) != rl::evaluation_seed(3u, 7u),
        "independent mastery evaluation seeds are not deterministic and distinct");

    rl::TrainingMetrics standing_mastery{};
    standing_mastery.evaluation_valid = true;
    standing_mastery.evaluation_invalid_runs = 0u;
    standing_mastery.evaluation_longest_stance = rl::standing_mastery_seconds;
    standing_mastery.evaluation_survival = rl::standing_mastery_seconds;
    standing_mastery.evaluation_spin_turns = rl::standing_mastery_spin_limit;
    standing_mastery.evaluation_max_joint_speed =
        rl::standing_mastery_joint_speed_limit;
    require(rl::strict_balance_mastery(standing_mastery),
        "all-six-seed standing evidence cannot satisfy the mastery gate");
    standing_mastery.evaluation_max_joint_speed += 0.01f;
    require(!rl::strict_balance_mastery(standing_mastery),
        "standing mastery accepts joint speed above its visible limit");
    require(sim::controlled_somersault_allowed(
            sim::CourseStage::duck_bars, 2.75f, 1.2f, true),
        "controlled somersault is rejected without a powered-launch flag");
    require(!sim::controlled_somersault_allowed(
            sim::CourseStage::uneven, 2.0f, 1.2f, true)
            && !sim::controlled_somersault_allowed(
                sim::CourseStage::duck_bars, 3.01f, 1.2f, true)
            && !sim::controlled_somersault_allowed(
                sim::CourseStage::duck_bars, 2.0f, 0.10f, true),
        "wrong-stage, over-three-turn, or non-rotating tumbling is accepted");
    require(sim::forward_prone_allowed(sim::CourseStage::uneven,
            true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::duck_press,
                true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::crouch_walk,
                true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::uneven,
                true, false, 0.25f, 0.10f),
        "forward-prone recovery rules do not preserve crouch foot-only contact");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 301.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::out_of_bounds, "course bounds gate missing");
    require(!sim::friction_driven_shuffle(0.42f, true, false, 0.30f, 0u, 0.0f)
            && !sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 1u, 0.0f)
            && !sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 0u, 0.10f),
        "normal single-support, established-gait, or foot-repositioning slide is penalized");
    require(sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 0u, 0.0f),
        "friction-driven double-support shuffling is not recognized");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.8f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::sustained_flight, "unpowered flight hard gate missing");
    require(!sim::duck_ground_contact_allowed(true, true)
            && sim::duck_ground_contact_allowed(true, false)
            && sim::duck_ground_contact_allowed(false, true),
        "foot-only duck contact rule is not strict");
    sim::CrouchPostureEvidence hinge{};
    hinge.paired_leg_chains = true;
    hinge.feet_supported = true;
    hinge.pelvis_drop = 0.08f;
    hinge.left_knee_flex = 0.03f;
    hinge.right_knee_flex = 0.02f;
    hinge.torso_pitch = 1.10f;
    hinge.support_margin = 0.12f;
    require(!sim::crouch_posture_qualified(hinge),
        "forward hip hinge is accepted as a crouch");
    sim::CrouchPostureEvidence squat{};
    squat.paired_leg_chains = true;
    squat.feet_supported = true;
    squat.pelvis_drop = 0.44f;
    squat.left_knee_flex = 0.32f;
    squat.right_knee_flex = 0.31f;
    squat.torso_pitch = 0.20f;
    squat.support_margin = 0.14f;
    require(sim::crouch_posture_qualified(squat),
        "bilateral pelvis-down squat cannot satisfy crouch evidence");
    require(sim::stage_skill_evidence(sim::CourseStage::balance,
            0u, 0.0f, 0u, 0.0f, 0u, 0u),
        "standing incorrectly requires movement");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_press,
            0u, 0.80f, 0u, 0.0f, 0u, 1u),
        "static crouch incorrectly requires walking or running");
    require(!sim::stage_skill_evidence(sim::CourseStage::uneven,
            0u, 0.0f, 0u, 0.0f, 0u, 0u)
            && sim::stage_skill_evidence(sim::CourseStage::uneven,
                10u, 0.0f, 0u, 0.0f, 0u, 0u),
        "walking/running stage uses the wrong movement evidence");
    require(sim::CreatureBlueprint::monoped().monopedal_gait()
            && !sim::CreatureBlueprint::humanoid().monopedal_gait(),
        "monoped gait is not distinguished from alternating biped gait");
    require(!sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
            3u, 3.0f, 0u, 0.0f, 0u, 4u)
            && !sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
                5u, 1.5f, 0u, 0.0f, 0u, 4u)
            && sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
                8u, 3.0f, 0u, 0.0f, 0u, 4u),
        "duck stage can qualify without sustained crouch walking and obstacles");
    require(sim::powered_joint_launch(sim::CourseStage::ramps, 1.0f, 0.08f),
        "joint-powered jump is not recognized");
    require(!sim::powered_joint_launch(sim::CourseStage::duck_press, 1.0f, 0.08f),
        "duck lesson incorrectly enables flight");
    require(sim::allowed_airtime_for_stage(sim::CourseStage::duck_bars, true) > 2.0f,
        "controlled flip lesson does not allow bounded powered airtime");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 3.0f, false)
        == sim::InvalidMotion::micro_motion, "micro-motion gate missing");
    require(!sim::recovery_should_start(true, 0.95f, false, false),
        "harmless upright obstacle contact created a rewardable recovery event");
    require(!sim::recovery_should_start(true, 0.80f, false, false),
        "ordinary obstacle contact still opens a rewardable recovery event");
    require(sim::recovery_should_start(false, 0.68f, false, false),
        "major balance loss did not start recovery without a collision");
    require(!sim::recovery_should_start(true, 0.40f, true, true),
        "hard ground impact incorrectly opened a recovery window");

    require(!sim::recovery_terminal_fall(true, false, true),
        "recoverable near-fall terminated during its recovery window");
    require(sim::recovery_terminal_fall(true, false, false),
        "unrecovered geometric fall was not terminal");
    require(sim::recovery_terminal_fall(true, true, true),
        "hard ground impact incorrectly received recovery grace");

    require(!sim::advanced_material_pressure_ready(7.9f, 8.0f, 2u, 2u, true),
        "material pressure unlocked before protected distance");
    require(!sim::advanced_material_pressure_ready(8.0f, 8.0f, 1u, 2u, true),
        "material pressure unlocked before repeated gait evidence");
    require(!sim::advanced_material_pressure_ready(8.0f, 8.0f, 2u, 1u, true),
        "paired skating events unlocked falling material without crossings");
    require(sim::advanced_material_pressure_ready(8.0f, 8.0f, 2u, 2u, true),
        "paired physical gait did not unlock advanced material pressure");
    require(sim::advanced_material_pressure_ready(8.0f, 8.0f, 2u, 0u, false),
        "multi-support physical cycles incorrectly require biped crossings");
    require(!sim::qualifies_alternating_step(-1, 0, 0.30f, 0.10f),
        "simultaneous two-foot landing counted as a step");
    require(!sim::qualifies_alternating_step(-1, 1, 0.05f, 0.10f),
        "rapid hopping counted as an alternating step");
    require(!sim::qualifies_alternating_step(-1, 1, 0.30f, 0.005f),
        "in-place foot twitch counted as walking");
    require(sim::qualifies_alternating_step(-1, 1, 0.30f, 0.08f),
        "real spaced alternating step was rejected");
    require(!sim::qualifies_supported_step(-1, 1, 0.30f, 0.04f, 0.08f, 0.02f)
            && sim::qualifies_supported_step(
                -1, 1, 0.30f, 0.045f, 0.08f, 0.02f),
        "tiny contact wiggle still counts as a supported walking step");
    require(!sim::qualifies_supported_step(-1, 1, 0.30f, 0.08f, 0.059f, 0.12f)
            && sim::qualifies_supported_step(
                -1, 1, 0.30f, 0.08f, 0.06f, 0.12f),
        "airborne support-transfer boundary is not enforced as a physical step");
    require(!sim::qualifies_supported_step(-1, 1, 0.30f, 0.08f, 0.08f, 0.014f)
            && sim::qualifies_supported_step(
                -1, 1, 0.30f, 0.08f, 0.08f, 0.015f),
        "authored support-clearance boundary rejects real steps or accepts foot drag");
    require(sim::qualifies_monoped_support_transfer(
                -1, 1, 0.30f, 0.05f, 0.06f, 0.05f),
        "real monoped heel-toe airborne transfer was rejected");
    require(!sim::qualifies_monoped_support_transfer(
                -1, 1, 0.30f, 0.02f, 0.06f, 0.05f)
            && !sim::qualifies_monoped_support_transfer(
                -1, 1, 0.30f, 0.05f, 0.02f, 0.05f)
            && !sim::qualifies_monoped_support_transfer(
                -1, 1, 0.30f, 0.05f, 0.06f, 0.01f)
            && !sim::qualifies_monoped_support_transfer(
                -1, -1, 0.30f, 0.05f, 0.06f, 0.05f),
        "monoped transfer accepts twitch, planted, low-clearance, or same-edge contact");
    require(sim::qualifies_topology_support_transfer(
                true, true, 0.05f, 0.015f, 0.08f, 0.018f, 0u, 1u),
        "authored branch-level dog/hexapod transfer boundary was rejected");
    require(!sim::qualifies_topology_support_transfer(
                false, true, 0.05f, 0.015f, 0.08f, 0.018f, 0u, 1u)
            && !sim::qualifies_topology_support_transfer(
                true, false, 0.05f, 0.015f, 0.08f, 0.018f, 0u, 1u)
            && !sim::qualifies_topology_support_transfer(
                true, true, 0.049f, 0.015f, 0.08f, 0.018f, 0u, 1u)
            && !sim::qualifies_topology_support_transfer(
                true, true, 0.05f, 0.014f, 0.08f, 0.018f, 0u, 1u)
            && !sim::qualifies_topology_support_transfer(
                true, true, 0.05f, 0.015f, 0.079f, 0.018f, 0u, 1u)
            && !sim::qualifies_topology_support_transfer(
                true, true, 0.05f, 0.015f, 0.08f, 0.017f, 0u, 1u)
            && !sim::qualifies_topology_support_transfer(
                true, true, 0.05f, 0.015f, 0.08f, 0.018f, 1u, 1u)
            && !sim::qualifies_topology_support_transfer(
                true, true, 0.05f, 0.015f, 0.08f, 0.018f,
                std::numeric_limits<std::size_t>::max(), 1u),
        "topology transfer accepts non-topology, planted, low-air, low-clearance, "
        "twitch, low-displacement, repeated-phase, or uninitialized evidence");
    require(!sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
            0.16f, 0.12f, false, true)
            && sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
                0.16f, 0.12f, true, true)
            && sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
                0.16f, 0.12f, false, false),
        "paired gait crossing is either optional or incorrectly forced on nonpaired rigs");
    require(sim::classify_foot_contact_phase(false, false, false)
                == sim::FootContactPhase::airborne
            && sim::classify_foot_contact_phase(true, false, false)
                == sim::FootContactPhase::heel_strike
            && sim::classify_foot_contact_phase(true, true, true)
                == sim::FootContactPhase::flat
            && sim::classify_foot_contact_phase(false, true, true)
                == sim::FootContactPhase::toe_off,
        "heel, flat-foot, toe-off, and airborne phases are not distinct");
    require(sim::foot_friction_retention(0.04f, 1.0f, 0.0f, false, false) == 0.0f,
        "loaded low-speed foot does not enter static friction");
    require(sim::foot_friction_retention(0.45f, 1.0f, 0.0f, false, false)
            < sim::foot_friction_retention(0.45f, 0.25f, 0.75f, false, false),
        "firm ground does not provide more dynamic traction than loose ground");
    require(sim::foot_friction_retention(0.45f, 1.0f, 0.0f, true, false)
            < sim::foot_friction_retention(0.45f, 1.0f, 0.0f, false, false),
        "static lessons do not apply stronger planted-foot friction");
    require(sim::rig_test_motor_input(sim::RigTestPattern::crouch,
                0u, 0.0f, 0.0f) < 0.0f
            && sim::rig_test_motor_input(sim::RigTestPattern::crouch,
                1u, 0.0f, 0.0f) > 0.0f
            && sim::rig_test_motor_input(sim::RigTestPattern::gait,
                0u, pi * 0.5f, 0.0f)
                * sim::rig_test_motor_input(sim::RigTestPattern::gait,
                    2u, pi * 0.5f, 0.0f) < 0.0f,
        "rig lab crouch and alternating gait test patterns are incorrect");

    const sim::CourseFeature rock_feature{
        sim::CourseFeatureKind::rock, {}, {}, 0.27f, {}
    };
    const sim::CourseFeature projectile_feature{
        sim::CourseFeatureKind::projectile, {}, {}, 0.19f, { -4.0f, 1.0f }
    };
    const sim::CourseFeature hurdle_feature{
        sim::CourseFeatureKind::hurdle, {}, { 0.14f, 0.42f }, 0.0f, {}
    };
    require(std::abs(sim::course_feature_observation_size(rock_feature) - 0.27f) < 0.0001f,
        "rock radius is absent from policy observations");
    require(std::abs(sim::course_feature_observation_size(projectile_feature) - 0.19f) < 0.0001f,
        "projectile radius is absent from policy observations");
    require(std::abs(sim::course_feature_observation_size(hurdle_feature) - 0.42f) < 0.0001f,
        "rectangular obstacle extent is incorrect in policy observations");

    const sim::CreatureBlueprint chicken = sim::CreatureBlueprint::chicken();
    require(chicken.head_node < chicken.nodes.size()
            && chicken.nodes[chicken.head_node].x > chicken.nodes[chicken.torso_node].x
            && chicken.nodes[chicken.head_node].y > chicken.nodes[chicken.torso_node].y,
        "chicken preset does not have a raised forward bird head");
    require(chicken.nodes[5].x < chicken.nodes[chicken.root_node].x - 0.50f
            && chicken.nodes[4].x > chicken.nodes[chicken.head_node].x,
        "chicken preset lacks a distinct tail and beak");
    require(chicken.nodes[chicken.torso_node].x
            > chicken.nodes[chicken.root_node].x + 0.25f
            && std::abs(chicken.nodes[chicken.torso_node].y
                - chicken.nodes[chicken.root_node].y) < 0.30f,
        "chicken semantic torso axis is not a compact horizontal bird body");
    require(std::ranges::any_of(chicken.bones, [&](const sim::DistanceConstraint& bone)
        {
            return (bone.a == chicken.root_node && bone.b == chicken.torso_node)
                || (bone.b == chicken.root_node && bone.a == chicken.torso_node);
        }) && std::ranges::any_of(chicken.bones, [&](const sim::DistanceConstraint& bone)
        {
            return (bone.a == chicken.torso_node && bone.b == chicken.head_node)
                || (bone.b == chicken.torso_node && bone.a == chicken.head_node);
        }),
        "chicken root, torso, and head do not form an intact semantic spine");

    {
        constexpr std::size_t chicken_seed_count = 6u;
        std::uint32_t valid_chicken_seeds = 0u;
        for (std::size_t seed_index = 0; seed_index < chicken_seed_count; ++seed_index)
        {
            const std::uint64_t seed = 0xC11C000u
                + static_cast<std::uint64_t>(seed_index) * 4099u;
            sim::Environment environment{ chicken, seed };
            environment.set_course(sim::CourseStage::balance, 0.25f);
            const std::array<float, sim::action_count> raw_action{};
            for (int frame = 0; frame < 1200; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::balance);
                const sim::StepResult step = environment.step(action);
                if (environment.valid_motion()
                    && environment.longest_stable_stance_seconds()
                        >= rl::standing_mastery_seconds)
                    break;
                if (step.terminated)
                    break;
            }
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            const bool accepted = qualification.valid
                && environment.body_integrity_valid()
                && environment.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds
                && environment.uncontrolled_spin_turns() <= 0.55f;
            valid_chicken_seeds += accepted ? 1u : 0u;
            if (!accepted)
            {
                std::cerr << "chicken balance seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " stance=" << environment.longest_stable_stance_seconds()
                    << " spin=" << environment.uncontrolled_spin_turns()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }
        }
        require(valid_chicken_seeds == chicken_seed_count,
            "chicken balance still reproduces the live 0/6 valid-seed regression");
    }

    sim::Environment crossing_feet(sim::CreatureBlueprint::humanoid(), 18);
    require(sim::EnvironmentTestAccess::moving_stage_allows_leg_crossing(crossing_feet),
        "support separation prevents one side-view leg from passing the other");

    sim::Environment fused_feet(sim::CreatureBlueprint::humanoid(), 19);
    sim::EnvironmentTestAccess::force_fused_supports(fused_feet);
    sim::EnvironmentTestAccess::separate_supports(fused_feet);
    require(sim::EnvironmentTestAccess::primary_support_gap(fused_feet) > 0.18f,
        "left and right feet can remain fused into one support blob");

    const std::array<sim::CreatureBlueprint, 4> support_presets{
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod()
    };
    for (std::size_t preset = 0; preset < support_presets.size(); ++preset)
    {
        sim::Environment environment(support_presets[preset], 100u + preset);
        sim::EnvironmentTestAccess::force_fused_supports(environment);
        for (int iteration = 0; iteration < 64; ++iteration)
            sim::EnvironmentTestAccess::separate_supports(environment);
        const float support_gap =
            sim::EnvironmentTestAccess::semantic_support_cluster_gap(environment);
        const std::string support_message =
            "production rig index " + std::to_string(preset)
            + " retained fused support clusters; gap=" + std::to_string(support_gap);
        require(support_gap > 0.040f, support_message);
    }

    sim::CreatureBlueprint editor_bone_rig = sim::CreatureBlueprint::scaffold();
    require(editor_bone_rig.valid(), "editor scaffold starts invalid");
    editor_bone_rig.bones.front().stiffness = 0.42f;
    require(editor_bone_rig.valid()
            && std::abs(editor_bone_rig.bones.front().stiffness - 0.42f) < 0.0001f,
        "editor bone stiffness control cannot preserve a valid rig");

    const sim::CreatureBlueprint humanoid_rig = sim::CreatureBlueprint::humanoid();
    const sim::CreatureBlueprint quad_rig = sim::CreatureBlueprint::quadruped();
    require(humanoid_rig.paired_leg_chains() && !quad_rig.paired_leg_chains(),
        "biped-only control path is not separated from quadruped control");

    sim::Environment neutral_humanoid(humanoid_rig, 131);
    neutral_humanoid.set_course(sim::CourseStage::balance, 0.25f);
    const auto standing_teacher = rl::balance_teacher_action(neutral_humanoid);
    require(std::abs(standing_teacher[0]) < 0.20f
            && std::abs(standing_teacher[2]) < 0.20f,
        "standing teacher still forces a jumping-jack hip pose");
    std::array<float, sim::action_count> exploratory{};
    exploratory.fill(0.80f);
    const auto residual_standing = rl::effective_policy_action(
        neutral_humanoid, exploratory, sim::CourseStage::balance);
    require(std::abs(residual_standing[0] - standing_teacher[0]) > 0.08f,
        "standing teacher still erases nearly all PPO exploration");

    sim::Environment neutral_quad(quad_rig, 137);
    neutral_quad.set_course(sim::CourseStage::balance, 0.25f);
    const auto quad_teacher = rl::balance_teacher_action(neutral_quad);
    require(std::ranges::all_of(quad_teacher, [](float value)
            {
                return std::abs(value) < 0.30f;
            }),
        "quadruped standing receives biped-like forced leg motion");

    sim::Environment hinge_humanoid(humanoid_rig, 139);
    require(sim::EnvironmentTestAccess::hip_hinge_is_rejected(hinge_humanoid),
        "live humanoid forward bow passes the physical crouch gate");
    sim::Environment guided_squat(humanoid_rig, 140);
    require(sim::EnvironmentTestAccess::guided_squat_is_valid(guided_squat),
        "authored crouch guide cannot produce a pelvis-down bilateral squat");

    require(sim::planted_contact_persists(
            true, true, true, 0.55f, 2.0f, false),
        "static support manifold did not retain a measured ground contact");
    require(sim::planted_contact_persists(
            true, true, false, 0.020f, 0.030f, false),
        "measured moving support contact released before a deliberate lift");
    require(!sim::planted_contact_persists(
            true, true, false, 0.020f, 0.040f, false)
            && !sim::planted_contact_persists(
                true, true, false, 0.040f, 0.030f, false),
        "moving foot remained magnetically planted after lift speed or separation");
    require(!sim::planted_contact_persists(
            true, true, true, 0.018f, 0.08f, true),
        "explicit powered release was ignored");
    require(!sim::support_contact_release_requested(
                true, false, false, sim::moving_contact_release_speed_mps)
            && sim::support_contact_release_requested(
                true, false, false,
                sim::moving_contact_release_speed_mps + 0.001f)
            && !sim::support_contact_release_requested(
                true, true, false,
                sim::moving_contact_release_speed_mps + 0.001f)
            && sim::support_contact_release_requested(
                true, true, true, 0.001f)
            && !sim::support_contact_release_requested(
                false, false, true, 1.0f),
        "support release accepts planted jitter or rejects deliberate dynamic lift");


    sim::Environment unpinned_squat(humanoid_rig, 1401);
    require(sim::EnvironmentTestAccess::crouch_guide_preserves_support_dynamics(
            unpinned_squat),
        "crouch curriculum directly pins semantic foot coordinates or support state");
    sim::Environment static_anchor(humanoid_rig, 1402);
    require(sim::EnvironmentTestAccess::static_friction_anchor_is_physical(
            static_anchor),
        "ground solver failed measured static-friction anchoring or moving release");

    sim::Environment crouch_humanoid(humanoid_rig, 141);
    crouch_humanoid.set_course(sim::CourseStage::duck_press, 0.30f);
    sim::EnvironmentTestAccess::set_duck_pressure(crouch_humanoid, 1.0f);
    const auto walk_teacher = rl::walking_teacher_action(neutral_humanoid);
    require(walk_teacher[0] * walk_teacher[2] < 0.0f
            || walk_teacher[1] * walk_teacher[3] < 0.0f,
        "walking teacher does not alternate the near and far leg chains");

    locomotion::Plan stable_multi_support{};
    stable_multi_support.intent = locomotion::Intent::walk;
    stable_multi_support.balance_reserve = 1.0f;
    require(rl::multi_support_gait_authority(stable_multi_support) == 1.0f,
        "stable multi-support gait lost full authored authority");
    stable_multi_support.brake = true;
    require(rl::multi_support_gait_authority(stable_multi_support) == 0.55f,
        "multi-support brake does not reduce gait authority");
    stable_multi_support.intent = locomotion::Intent::recover;
    require(rl::multi_support_gait_authority(stable_multi_support) == 0.18f,
        "multi-support recovery continues full gait drive");
    stable_multi_support.intent = locomotion::Intent::crawl;
    require(rl::multi_support_gait_authority(stable_multi_support) == 0.30f,
        "multi-support crawl authority escaped its recovery bound");

    const auto crouch_teacher = rl::duck_teacher_action(crouch_humanoid);
    require(std::abs(crouch_teacher[0]) < std::abs(crouch_teacher[1])
            && std::abs(crouch_teacher[2]) < std::abs(crouch_teacher[3]),
        "static crouch teacher still spreads hips before bending knees");

    const rl::MotorDiscoveryProbe positive_probe = rl::motor_discovery_probe(
        neutral_humanoid, 0u, 0u, 0u);
    const rl::MotorDiscoveryProbe negative_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count, 0u, 0u);
    const rl::MotorDiscoveryProbe synchronized_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count * 2u, 0u, 0u);
    const rl::MotorDiscoveryProbe alternating_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count * 2u + 3u, 0u, 0u);
    require(positive_probe.weight > 0.80f && positive_probe.action[0] > 0.0f
            && negative_probe.action[0] < 0.0f,
        "motor discovery does not test one joint in both directions");
    require(std::ranges::all_of(
            std::span(synchronized_probe.action).first(humanoid_rig.active_motor_count),
            [](float value) { return value > 0.0f; }),
        "motor discovery does not test synchronized joint motion");
    require(alternating_probe.action[0] * alternating_probe.action[1] < 0.0f,
        "motor discovery does not test alternating joint motion");
    require(rl::motor_discovery_probe(neutral_humanoid, 0u, 480u, 0u).weight == 0.0f,
        "motor discovery never yields control back to PPO");

    sim::Environment press_environment(sim::CreatureBlueprint::humanoid(), 17);
    press_environment.set_course(sim::CourseStage::duck_press, 0.5f);
    sim::EnvironmentTestAccess::set_duck_pressure(press_environment, 1.0f);
    const auto press_teacher = rl::duck_teacher_action(press_environment);
    require(std::abs(press_teacher[4]) < 0.0001f
            && std::abs(press_teacher[5]) < 0.0001f
            && std::abs(press_teacher[6]) < 0.0001f
            && std::abs(press_teacher[7]) < 0.0001f,
        "duck teacher still prefers shoulder or arm swing over leg compression");
    require(std::abs(press_teacher[1]) + std::abs(press_teacher[3]) > 0.60f,
        "duck teacher does not apply meaningful leg compression");
    require(sim::EnvironmentTestAccess::press_collision_resolves_below(press_environment),
        "duck press clips through the model instead of resolving below the platen");
    sim::EnvironmentTestAccess::complete_duck_press(press_environment);
    require(std::abs(press_environment.ground_height_at(0.0f)
            - press_environment.ground_height_at(1.25f)) < 0.0001f,
        "static crouch lesson incorrectly requires uneven-ground movement");
    require(std::ranges::none_of(press_environment.course_features(),
            [](const sim::CourseFeature& feature)
            {
                return feature.kind == sim::CourseFeatureKind::overhead_bar;
            }),
        "static crouch lesson incorrectly contains the moving low-bar course");

    sim::Environment crouch_environment(sim::CreatureBlueprint::humanoid(), 23);
    crouch_environment.set_course(sim::CourseStage::crouch_walk, 0.5f);
    require(std::abs(crouch_environment.ground_height_at(0.0f)
            - crouch_environment.ground_height_at(1.25f)) > 0.005f,
        "crouch-walk lesson ground remains flat and stable");
    const auto later_bar_iterator = std::ranges::find_if(
        crouch_environment.course_features(), [](const sim::CourseFeature& feature)
        {
            return feature.kind == sim::CourseFeatureKind::overhead_bar;
        });
    require(later_bar_iterator != crouch_environment.course_features().end(),
        "crouch-walk lesson has no later moving low bar");
    const sim::CourseFeature& later_bar = *later_bar_iterator;
    require(later_bar.center.x
            - crouch_environment.particles()[crouch_environment.blueprint().root_node].position.x >= 5.5f,
        "crouch-walk low bar starts too close for a meaningful response");
    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "crouch-walk low bar is not horizontal or is effectively a wall");
    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
            8u, 3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk evidence is rejected");

    require(sim::ground_velocity_retention(true, 0.0f)
        < sim::ground_velocity_retention(false, 0.0f),
        "feet do not receive more ground traction than head, tail, or body nodes");
    require(sim::ground_velocity_retention(true, 0.0f) == 0.0f,
        "grounded orange foot nodes still retain wheel-like horizontal velocity");
    require(std::abs(sim::ground_contact_offset(true, 0.20f) - 0.065f) < 0.0001f,
        "semantic feet still collide with the ground as rolling circles");
    require(sim::ground_velocity_retention(false, 0.0f) >= 0.95f,
        "non-foot body contact can still pin the creature to the ground");
    const int anchored_sequence = sim::first_course_feature_sequence(1.0f, 3.0f);
    const float anchored_x = sim::course_feature_world_x(anchored_sequence, 3.0f);
    const float advanced_x = sim::course_feature_world_x(anchored_sequence, 4.0f);
    require(std::abs((advanced_x - anchored_x) + 1.0f) < 0.0001f,
        "course debris does not advance in world space solely from course progress");
    require(std::abs(sim::course_marker_distance_m(4) - 32.0f) < 0.0001f,
        "course mile-marker spacing is not shared with obstacle scheduling");
    require(sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
        && sim::course_stage_name(sim::CourseStage::duck_press)
            == "2. STATIC CROUCH / HOLD / RECOVER"
        && sim::course_stage_name(sim::CourseStage::uneven) == "3. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::shuttle)
            == "4. BACK / TURN / RETURN"
        && sim::course_stage_name(sim::CourseStage::crouch_walk)
            == "5. CROUCH WALK / UNEVEN AVOID"
        && sim::course_stage_name(sim::CourseStage::ramps) == "6. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::hurdles)
            == "7. MOVING LOW BAR / HURDLE"
        && sim::course_stage_name(sim::CourseStage::duck_bars)
            == "8. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards)
            == "9. MIXED GOAL COURSE"
        && sim::course_stage_curriculum_index(sim::CourseStage::balance)
            < sim::course_stage_curriculum_index(sim::CourseStage::duck_press)
        && sim::course_stage_curriculum_index(sim::CourseStage::duck_press)
            < sim::course_stage_curriculum_index(sim::CourseStage::uneven)
        && sim::course_stage_curriculum_index(sim::CourseStage::uneven)
            < sim::course_stage_curriculum_index(sim::CourseStage::shuttle)
        && sim::course_stage_curriculum_index(sim::CourseStage::shuttle)
            < sim::course_stage_curriculum_index(sim::CourseStage::crouch_walk),
        "stand, crouch, walk, shuttle, and crouch-walk curriculum is misordered");
    require(!sim::stage_skill_evidence(sim::CourseStage::duck_press, 0u, 0.6f, 0u, 0.0f, 0u, 0u),
        "duck lesson completes without moving crouch evidence");
    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk, 8u, 3.0f, 0u, 0.0f, 0u, 4u),
        "foot-only sustained crouch walk and obstacle evidence cannot complete the duck lesson");
    require(sim::stage_skill_evidence(sim::CourseStage::ramps, 0u, 0.0f, 1u, 0.0f, 0u, 0u),
        "landed jump cannot complete the jump lesson");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_bars, 0u, 0.0f, 1u, 1.0f, 1u, 0u),
        "controlled landed flip cannot complete the flip lesson");
    require(!sim::stage_skill_evidence(sim::CourseStage::moving_hazards, 2u, 0.0f, 0u, 0.0f, 0u, 0u),
        "mixed goal lesson can complete without passing an obstacle");
    require(sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 5)
            == sim::CourseFeatureKind::rock
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 6)
            == sim::CourseFeatureKind::hurdle
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 7)
            == sim::CourseFeatureKind::overhead_bar
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 8)
            == sim::CourseFeatureKind::moving_hazard
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 9)
            == sim::CourseFeatureKind::projectile,
        "moving-hazard lesson does not schedule every obstacle class on consecutive markers");
    require(sim::course_marker_distance_m(sim::course_safe_runway_markers) >= 40.0f,
        "course does not provide the requested longer learning runway");
    sim::CourseFeature rock_order{};
    rock_order.kind = sim::CourseFeatureKind::rock;
    rock_order.center = { 1.0f, 0.25f };
    rock_order.radius = 0.25f;
    require(!sim::measure_forward_gait_faults(sim::ShuttlePhase::traverse, 0.49f)
            && sim::measure_forward_gait_faults(sim::ShuttlePhase::traverse, 0.50f)
            && !sim::measure_forward_gait_faults(sim::ShuttlePhase::backing, 2.0f)
            && !sim::measure_forward_gait_faults(sim::ShuttlePhase::braking, 2.0f)
            && !sim::measure_forward_gait_faults(sim::ShuttlePhase::turning, 2.0f)
            && !sim::measure_forward_gait_faults(sim::ShuttlePhase::traverse,
                std::numeric_limits<float>::quiet_NaN()),
        "reverse/turn transition poses leak into forward gait quality telemetry");

    require(!sim::pathological_lower_leg_crossing(true, true, false, false)
            && !sim::pathological_lower_leg_crossing(true, false, true, false)
            && sim::pathological_lower_leg_crossing(true, false, false, false)
            && sim::pathological_lower_leg_crossing(true, true, true, false)
            && sim::pathological_lower_leg_crossing(false, true, false, true),
        "normal single-support pass-through and pathological crossed stance are conflated");
    require(std::abs(sim::contiguous_condition_seconds(true, 0.10f, 0.05f) - 0.15f) < 1.0e-6f
            && sim::contiguous_condition_seconds(false, 0.30f, 0.05f) == 0.0f
            && sim::contiguous_condition_seconds(true, -0.10f, 0.05f) == 0.0f
            && sim::contiguous_condition_seconds(true, 0.10f,
                std::numeric_limits<float>::quiet_NaN()) == 0.0f,
        "contiguous gait fault timer carries separated or invalid samples");
    require(!sim::knee_crosses_before_foot(1.12f, 0.92f, 0.34f, rock_order),
        "normal bent-knee lead is still over-constrained");
    require(sim::knee_crosses_before_foot(1.42f, 0.82f, 0.20f, rock_order),
        "egregious low-foot body-first rock shove is not detected");
    require(!sim::knee_crosses_before_foot(1.42f, 1.32f, 0.34f, rock_order),
        "foot-first rock traversal is incorrectly penalized");
    require(!sim::knee_crosses_before_foot(1.42f, 0.82f, 0.58f, rock_order),
        "useful foot clearance is incorrectly rejected because the knee leads");
    require(sim::gait_progress_multiplier(0, false, 0.0f) == 0.0f,
        "sliding without a real step still receives walking progress credit");
    require(sim::gait_progress_multiplier(2, true, 0.18f)
            > sim::gait_progress_multiplier(0, true, 0.18f),
        "alternating lifted-foot gait does not receive stronger progress credit");
    require(sim::friction_driven_shuffle(0.45f, true, true, 0.50f, 0u, 0.0f),
        "double-supported wheel-like sliding is not detected");
    require(!sim::friction_driven_shuffle(0.45f, true, false, 0.50f, 0u, 0.0f),
        "single-support walking is incorrectly classified as wheel sliding");
    require(!sim::rolling_gate_active(1.0f)
            && sim::rolling_gate_active(sim::rolling_gate_activation_seconds),
        "rolling hard gate does not provide a bounded startup settle window");
    require(sim::body_rolling_limit(sim::CourseStage::duck_press, 1.8f)
            > sim::body_rolling_limit(sim::CourseStage::duck_press, 4.0f),
        "rolling gate does not become strict after startup");
    require(sim::foot_pivot_rolling_limit(1.8f) > sim::foot_pivot_rolling_limit(4.0f),
        "orange-foot rolling gate does not become strict after startup");
    require(sim::zero_progress_window(0.0f, 0u, 0.0f, false),
        "zero movement is not classified for reset");
    require(!sim::zero_progress_window(0.08f, 0u, 0.0f, false),
        "meaningful translation is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 1u, 0.0f, false),
        "a new gait step is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 0u, 0.18f, false),
        "useful leg lift is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 0u, 0.0f, true),
        "active recovery is incorrectly reset as idle");
    require(sim::update_zero_progress_seconds(1.0f, true, 1.0f)
            >= sim::zero_progress_reset_seconds,
        "two idle windows do not reach the reset threshold");
    require(sim::update_zero_progress_seconds(1.0f, false, 1.0f) == 0.0f,
        "useful motion does not rapidly clear the idle-reset accumulator");
    require(sim::micro_motion_window(
                0.11f, 0.04f, 0.04f, 0u, true, false)
            && sim::micro_motion_window(
                0.17f, 0.10f, 0.30f, 0u, true, false),
        "high-energy no-gait vibration bypasses the micro-motion gate");
    require(!sim::micro_motion_window(
                0.17f, 0.02f, 0.30f, 1u, true, false)
            && !sim::micro_motion_window(
                0.17f, 0.13f, 0.40f, 0u, true, false)
            && !sim::micro_motion_window(
                0.17f, 0.02f, 0.30f, 0u, true, true),
        "real gait, useful progress, or active recovery is micro-motion");
    require(rl::self_imitation_prior_weight(0, 128) > rl::self_imitation_prior_weight(500, 128)
            && rl::self_imitation_prior_weight(500, 128) > 0.0f,
        "best-result imitation guide does not decay into a light prior");
    require(rl::self_imitation_prior_weight(0, 0) == 0.0f,
        "empty imitation memory still changes PPO gradients");

    const sim::CreatureBlueprint biped_walk = sim::CreatureBlueprint::biped();
    const sim::CreatureBlueprint humanoid_walk = sim::CreatureBlueprint::humanoid();
    const sim::CreatureBlueprint quadruped_walk = sim::CreatureBlueprint::quadruped();
    const rl::BipedGaitParameters biped_foundational_gait =
        rl::anatomy_scaled_foundational_gait(biped_walk);
    const rl::BipedGaitParameters humanoid_foundational_gait =
        rl::anatomy_scaled_foundational_gait(humanoid_walk);
    sim::CreatureBlueprint humanoid_with_shifted_arms = humanoid_walk;
    for (std::size_t node = 7u; node < humanoid_with_shifted_arms.nodes.size(); ++node)
        humanoid_with_shifted_arms.nodes[node].x += 4.0f;
    const rl::BipedGaitParameters shifted_arm_gait =
        rl::anatomy_scaled_foundational_gait(humanoid_with_shifted_arms);
    const auto minimum_biped_leg_length = [](const sim::CreatureBlueprint& rig)
    {
        float minimum = std::numeric_limits<float>::infinity();
        for (const std::size_t hip_index : { 0u, 2u })
        {
            const std::size_t knee_index = hip_index + 1u;
            if (knee_index >= rig.active_motor_count)
                continue;
            const sim::MotorConstraint& hip = rig.motors[hip_index];
            const sim::MotorConstraint& knee = rig.motors[knee_index];
            minimum = std::min(minimum,
                length(rig.nodes[hip.c] - rig.nodes[hip.pivot])
                    + length(rig.nodes[knee.c] - rig.nodes[knee.pivot]));
        }
        return minimum;
    };
    const float biped_leg_length = minimum_biped_leg_length(biped_walk);
    const float humanoid_leg_length = minimum_biped_leg_length(humanoid_walk);
    require(std::isfinite(biped_foundational_gait.step_length)
            && std::isfinite(humanoid_foundational_gait.swing_lift)
            && biped_foundational_gait.step_length >= 0.50f
            && biped_foundational_gait.step_length <= 0.82f
            && humanoid_foundational_gait.step_length >= 0.96f
            && humanoid_foundational_gait.step_length <= 1.10f
            && humanoid_foundational_gait.swing_lift >= 0.38f
            && humanoid_foundational_gait.swing_lift <= 0.42f
            && std::abs(humanoid_foundational_gait.cadence_hz - 1.04f) < 1.0e-6f
            && humanoid_foundational_gait.transition_flex >= 0.14f
            && humanoid_foundational_gait.transition_flex <= 0.18f,
        "foundational biped gait is not finite and anatomy-bounded");
    require(humanoid_foundational_gait.step_length > biped_foundational_gait.step_length
            && humanoid_foundational_gait.step_length <= 1.10f
            && humanoid_foundational_gait.swing_lift < 0.82f
            && std::abs(shifted_arm_gait.step_length
                - humanoid_foundational_gait.step_length) < 1.0e-6f
            && std::abs(shifted_arm_gait.swing_lift
                - humanoid_foundational_gait.swing_lift) < 1.0e-6f,
        "foundational stride still depends on arm presence or geometry");
    require(rl::authored_gait_startup_blend(-1.0f) == 0.0f
            && rl::authored_gait_startup_blend(0.0f) == 0.0f
            && std::abs(rl::authored_gait_startup_blend(0.075f) - 0.5f) < 1.0e-6f
            && rl::authored_gait_startup_blend(0.15f) == 1.0f
            && rl::authored_gait_startup_blend(2.0f) == 1.0f
            && rl::authored_gait_startup_blend(
                std::numeric_limits<float>::quiet_NaN()) == 0.0f,
        "authored gait startup does not blend deterministically from the saved pose");
    const float authored_left_foot_offset = humanoid_walk.nodes[
        humanoid_walk.left_contact_node].x
        - humanoid_walk.nodes[humanoid_walk.motors[0].pivot].x;
    const float authored_right_foot_offset = humanoid_walk.nodes[
        humanoid_walk.right_contact_node].x
        - humanoid_walk.nodes[humanoid_walk.motors[2].pivot].x;
    const float authored_gait_progress = humanoid_foundational_gait.phase_offset / pi;
    const float gait_left_foot_offset = humanoid_foundational_gait.stance_center_x
        + rl::sagittal_step_x(humanoid_foundational_gait.step_length,
            authored_gait_progress, false);
    const float gait_right_foot_offset = humanoid_foundational_gait.stance_center_x
        + rl::sagittal_step_x(humanoid_foundational_gait.step_length,
            authored_gait_progress, true);
    require(humanoid_foundational_gait.phase_offset > 0.0f
            && humanoid_foundational_gait.phase_offset < pi
            && std::abs(gait_left_foot_offset - authored_left_foot_offset) < 1.0e-5f
            && std::abs(gait_right_foot_offset - authored_right_foot_offset) < 1.0e-5f,
        "foundational Human gait does not begin from the saved authored foot stance");
    const auto bounded_foundational_endpoint = [](
        const rl::BipedGaitParameters& parameters, float chain_length)
    {
        return rl::bounded_biped_leg_target({
            parameters.stance_center_x + 0.5f * parameters.step_length,
            -parameters.leg_height + parameters.transition_flex }, chain_length);
    };
    const Vec2 biped_endpoint = bounded_foundational_endpoint(
        biped_foundational_gait, biped_leg_length);
    const Vec2 humanoid_endpoint = bounded_foundational_endpoint(
        humanoid_foundational_gait, humanoid_leg_length);
    const Vec2 shifted_arm_endpoint = bounded_foundational_endpoint(
        shifted_arm_gait, humanoid_leg_length);
    const float humanoid_mid_vertical_reach = rl::biped_stance_vertical_reach(
        humanoid_foundational_gait.stance_center_x, humanoid_leg_length);
    const float humanoid_endpoint_vertical_reach =
        rl::biped_stance_vertical_reach(
            humanoid_foundational_gait.stance_center_x
                + 0.5f * humanoid_foundational_gait.step_length,
            humanoid_leg_length);
    const Vec2 humanoid_mid_stance = rl::bounded_biped_leg_target({
        humanoid_foundational_gait.stance_center_x,
        -humanoid_mid_vertical_reach }, humanoid_leg_length);
    require(rl::biped_leg_target_within_reach(biped_endpoint,
                biped_leg_length)
            && rl::biped_leg_target_within_reach(humanoid_endpoint,
                humanoid_leg_length)
            && rl::biped_leg_target_within_reach(shifted_arm_endpoint,
                humanoid_leg_length)
            && rl::biped_leg_target_within_reach(humanoid_mid_stance,
                humanoid_leg_length),
        "phase-local gait envelope emitted a near-locked or unreachable target");
    require(std::abs(humanoid_mid_stance.y) + 1.0e-5f
                >= humanoid_endpoint_vertical_reach
            && length(humanoid_mid_stance) + 1.0e-5f
                >= humanoid_leg_length
                    * (1.0f - rl::biped_leg_reach_reserve)
            && humanoid_foundational_gait.leg_height
                >= humanoid_leg_length * 0.95f
            && std::abs(humanoid_mid_stance.y)
                > humanoid_endpoint_vertical_reach
                    + 0.01f * humanoid_leg_length
            && rl::biped_stance_vertical_reach(
                std::numeric_limits<float>::quiet_NaN(),
                humanoid_leg_length) == 0.0f
            && rl::biped_stance_vertical_reach(0.0f, -1.0f) == 0.0f,
        "phase-local gait envelope failed to straighten Human mid-stance support");
    rl::BipedGaitParameters excessive_reach = humanoid_foundational_gait;
    excessive_reach.step_length = humanoid_leg_length * 0.80f;
    excessive_reach.leg_height = humanoid_leg_length * 1.20f;
    require(!rl::biped_gait_target_within_reach(
                excessive_reach, humanoid_leg_length),
        "adversarial unreachable gait was accepted as an authored target");
    const Vec2 excessive_desired{
        excessive_reach.stance_center_x + 0.5f * excessive_reach.step_length,
        -excessive_reach.leg_height };
    const Vec2 excessive_bounded = rl::bounded_biped_leg_target(
        excessive_desired, humanoid_leg_length);
    const Vec2 invalid_bounded = rl::bounded_biped_leg_target({
        std::numeric_limits<float>::quiet_NaN(), -1.0f },
        humanoid_leg_length);
    require(rl::biped_leg_target_within_reach(excessive_bounded,
                humanoid_leg_length)
            && std::abs(excessive_desired.x * excessive_bounded.y
                - excessive_desired.y * excessive_bounded.x) < 1.0e-4f
            && length(invalid_bounded) == 0.0f,
        "phase-local gait envelope did not safely bound adversarial input");
    require(std::abs(rl::sagittal_step_x(0.60f, 0.0f, false) - 0.30f) < 1.0e-6f
            && std::abs(rl::sagittal_step_x(0.60f, 0.5f, false)) < 1.0e-6f
            && std::abs(rl::sagittal_step_x(0.60f, 1.0f, false) + 0.30f) < 1.0e-6f
            && std::abs(rl::sagittal_step_x(0.60f, 0.0f, true) + 0.30f) < 1.0e-6f
            && rl::sagittal_step_x(std::numeric_limits<float>::quiet_NaN(),
                0.5f, false) == 0.0f,
        "sagittal cosine step path changed endpoints, crossing, or finite bounds");
    const rl::TwoLinkSagittalSolution left_knee =
        rl::solve_two_link_sagittal(1.0f, 1.0f, { -0.20f, -1.6f }, 1.0f);
    const rl::TwoLinkSagittalSolution right_knee =
        rl::solve_two_link_sagittal(1.0f, 1.0f, { 0.20f, -1.6f }, 1.0f);
    const rl::TwoLinkSagittalSolution mirrored_left_knee =
        rl::solve_two_link_sagittal(1.0f, 1.0f, { 0.20f, -1.6f }, -1.0f);
    const rl::TwoLinkSagittalSolution mirrored_right_knee =
        rl::solve_two_link_sagittal(1.0f, 1.0f, { -0.20f, -1.6f }, -1.0f);
    const Vec2 authored_hand { 0.12f, -1.45f };
    const Vec2 arm_forward = rl::authored_opposed_swing_target(
        authored_hand, pi * 0.5f, 0.405f, 0.0f, 1.0f);
    const Vec2 arm_backward = rl::authored_opposed_swing_target(
        authored_hand, pi * 1.5f, 0.405f, 0.0f, 1.0f);
    const Vec2 casual_arm_rest = rl::human_casual_arm_rest_target(1.70f);
    const Vec2 casual_arm_forward = rl::authored_opposed_swing_target(
        casual_arm_rest, pi * 0.5f, 0.17f, 0.0f, 1.0f);
    const Vec2 casual_arm_backward = rl::authored_opposed_swing_target(
        casual_arm_rest, pi * 1.5f, 0.17f, 0.0f, 1.0f);

    require(left_knee.valid && right_knee.valid
            && mirrored_left_knee.valid && mirrored_right_knee.valid
            && left_knee.upper.x > 0.0f && right_knee.upper.x > 0.0f
            && mirrored_left_knee.upper.x < 0.0f
            && mirrored_right_knee.upper.x < 0.0f,
        "paired knees do not share and mirror one facing-relative bend side");
    require(arm_forward.x > authored_hand.x + 0.38f
            && arm_backward.x < authored_hand.x - 0.38f
            && std::abs(arm_forward.y - arm_backward.y) < 1.0e-5f
            && arm_forward.y < -1.20f
            && std::abs(casual_arm_rest.x) < 1.0e-6f
            && std::abs(casual_arm_rest.y + 1.428f) < 1.0e-5f
            && casual_arm_forward.x > 0.16f
            && casual_arm_backward.x < -0.16f
            && std::abs(casual_arm_forward.y - casual_arm_backward.y) < 1.0e-5f
            && length(rl::human_casual_arm_rest_target(
                std::numeric_limits<float>::quiet_NaN())) == 0.0f
            && length(rl::human_casual_arm_rest_target(-1.0f)) == 0.0f
            && !rl::solve_two_link_sagittal(0.0f, 1.0f,
                { 0.0f, -1.0f }, 1.0f).valid,
        "arm teacher is not a bounded fore/aft sagittal chain target");
    require(sim::foundational_gait_cadence_hz == 1.24f
            && sim::authored_foundational_gait_cadence_hz(biped_walk) == 1.24f
            && sim::authored_foundational_gait_cadence_hz(humanoid_walk) == 1.24f
            && sim::authored_foundational_gait_cadence_hz(quadruped_walk) == 1.30f
            && sim::authored_foundational_gait_cadence_hz(
                sim::CreatureBlueprint::crawler4()) == 1.44f
            && sim::authored_foundational_gait_cadence_hz(
                sim::CreatureBlueprint::hexapod()) == 1.50f,
        "foundational teacher and observed topology clocks diverged");
    require(rl::walk_mastery_distance == 18.0f
            && rl::walk_mastery_stride_events == 14.0f,
        "cross-platform Walk mastery aggregate drifted");
    rl::TrainingMetrics turn_mastery{};
    turn_mastery.evaluation_valid = true;
    turn_mastery.evaluation_quality_key = 1u;
    turn_mastery.evaluation_distance = rl::walk_mastery_distance;
    turn_mastery.evaluation_stride_events = rl::walk_mastery_stride_events;
    turn_mastery.evaluation_survival = 18.0f;
    turn_mastery.evaluation_collisions = 1.0f;
    for (const float signed_round_trip_speed : { -8.0f, 0.0f, 8.0f })
    {
        turn_mastery.evaluation_speed = signed_round_trip_speed;
        require(rl::shuttle_mastery_evidence(turn_mastery),
            "valid round-trip turn evidence was rejected by signed-speed cancellation");
    }
    turn_mastery.evaluation_distance = rl::walk_mastery_distance - 0.01f;
    require(!rl::shuttle_mastery_evidence(turn_mastery),
        "turn mastery accepted insufficient round-trip distance");
    turn_mastery.evaluation_distance = rl::walk_mastery_distance;
    turn_mastery.evaluation_valid = false;
    require(!rl::shuttle_mastery_evidence(turn_mastery),
        "turn mastery accepted an invalid evaluation");
    require(rl::foundational_walk_teacher_handoff_update(biped_walk) == 500u
            && rl::foundational_walk_teacher_handoff_update(humanoid_walk) == 900u
            && rl::foundational_walk_teacher_handoff_update(quadruped_walk) == 900u,
        "foundational teacher handoff ignores support and manipulator topology");
    require(rl::foundational_walk_teacher_authority(299u, biped_walk) == 1.0f
            && rl::foundational_walk_teacher_authority(500u, biped_walk) == 0.0f
            && rl::foundational_walk_teacher_authority(599u, humanoid_walk) == 1.0f
            && rl::foundational_walk_teacher_authority(900u, humanoid_walk) == 0.0f
            && rl::foundational_walk_teacher_authority(599u, quadruped_walk) == 1.0f
            && rl::foundational_walk_teacher_authority(900u, quadruped_walk) == 0.0f,
        "foundational teacher authority does not reach exact scoped boundaries");
    const float biped_mid_authority =
        rl::foundational_walk_teacher_authority(400u, biped_walk);
    const float quadruped_mid_authority =
        rl::foundational_walk_teacher_authority(750u, quadruped_walk);
    require(std::abs(biped_mid_authority - 0.5f) < 1.0e-6f
            && std::abs(quadruped_mid_authority - 0.5f) < 1.0e-6f,
        "foundational teacher authority does not decay linearly");
    require(rl::guided_rollout_imitation_weight(
                0u, sim::CourseStage::uneven) == 64.0f
            && rl::guided_rollout_imitation_weight(
                7200u, sim::CourseStage::uneven) == 0.0f
            && rl::guided_rollout_imitation_weight(
                0u, sim::CourseStage::balance) == 0.0f,
        "guided gait imitation is not scoped and bounded");
    require(rl::guided_rollout_imitation_weight(
                599u, sim::CourseStage::uneven, &quadruped_walk) == 64.0f
            && std::abs(rl::guided_rollout_imitation_weight(
                750u, sim::CourseStage::uneven, &quadruped_walk) - 32.0f)
                < 1.0e-6f
            && rl::guided_rollout_imitation_weight(
                900u, sim::CourseStage::uneven, &quadruped_walk) == 16.0f
            && rl::guided_rollout_imitation_weight(
                1050u, sim::CourseStage::uneven, &quadruped_walk) == 8.0f
            && rl::guided_rollout_imitation_weight(
                1200u, sim::CourseStage::uneven, &quadruped_walk) == 0.0f
            && rl::lesson_teacher_authority(
                900u, sim::CourseStage::uneven, quadruped_walk) == 0.0f,
        "multi-support consolidation is not finite or survives as action authority");
    const sim::CreatureBlueprint monoped_walk = sim::CreatureBlueprint::monoped();
    const sim::CreatureBlueprint chicken_walk = sim::CreatureBlueprint::chicken();
    const auto monoped_reflex = rl::topology_runtime_reflex_authority(
        monoped_walk, sim::CourseStage::uneven);
    const auto chicken_reflex = rl::topology_runtime_reflex_authority(
        chicken_walk, sim::CourseStage::shuttle);
    const auto ordinary_reflex = rl::topology_runtime_reflex_authority(
        humanoid_walk, sim::CourseStage::uneven);
    const auto out_of_scope_reflex = rl::topology_runtime_reflex_authority(
        monoped_walk, sim::CourseStage::balance);
    const auto human_shuttle_reflex = rl::topology_runtime_reflex_authority(
        humanoid_walk, sim::CourseStage::shuttle);
    const auto quadruped_reflex = rl::topology_runtime_reflex_authority(
        quadruped_walk, sim::CourseStage::uneven);
    const auto hexapod_reflex = rl::topology_runtime_reflex_authority(
        sim::CreatureBlueprint::hexapod(), sim::CourseStage::uneven);
    require(monoped_reflex.support == 0.92f && monoped_reflex.body == 0.82f
            && chicken_reflex.support == 0.88f && chicken_reflex.body == 0.50f
            && ordinary_reflex.support == 1.0f && ordinary_reflex.body == 0.84f
            && out_of_scope_reflex.support == 0.0f
            && out_of_scope_reflex.body == 0.0f
            && human_shuttle_reflex.support == 1.0f
            && human_shuttle_reflex.body == 1.0f
            && quadruped_reflex.support == 0.94f
            && quadruped_reflex.body == 0.70f
            && hexapod_reflex.support == 0.98f
            && hexapod_reflex.body == 0.84f,
        "fragile topology code brain is not bounded to its rig and walking stages");
    {
        sim::Environment human_reflex{ humanoid_walk, 0x447u };
        human_reflex.set_course(sim::CourseStage::uneven, 0.30f);
        const auto raw_support_teacher = rl::raw_walking_teacher_action(human_reflex);
        std::array<float, sim::action_count> positive{};
        std::array<float, sim::action_count> negative{};
        positive.fill(1.0f);
        negative.fill(-1.0f);
        const auto positive_effective = rl::effective_policy_action(
            human_reflex, positive, sim::CourseStage::uneven, 0.0f);
        const auto negative_effective = rl::effective_policy_action(
            human_reflex, negative, sim::CourseStage::uneven, 0.0f);
        bool bounded_body_residual_visible = false;
        float maximum_body_action{};
        for (std::size_t index = 0; index < humanoid_walk.active_motor_count; ++index)
        {
            if (index < 4u)
            {
                require(std::abs(positive_effective[index] - raw_support_teacher[index]) < 1.0e-6f
                        && std::abs(negative_effective[index] - raw_support_teacher[index]) < 1.0e-6f,
                    "post-handoff Human policy can override the opposed leg-transfer clock");
                continue;
            }
            require(std::abs(positive_effective[index]) <= 0.320001f
                    && std::abs(negative_effective[index]) <= 0.320001f,
                "post-handoff Human upper-body residual exceeds its gait bound");
            maximum_body_action = std::max(maximum_body_action,
                std::abs(positive_effective[index]));
            bounded_body_residual_visible = bounded_body_residual_visible
                || std::abs(positive_effective[index] - negative_effective[index]) > 0.02f;
        }
        require(maximum_body_action > 0.10f,
            "startup stability damping erased the Human code-brain body control");
        require(bounded_body_residual_visible,
            "post-handoff Human gait erased the learned upper-body residual");
    }

    require(rl::guided_rollout_imitation_weight(
                900u, sim::CourseStage::uneven, &monoped_walk) == 48.0f
            && rl::guided_rollout_imitation_weight(
                1050u, sim::CourseStage::uneven, &chicken_walk) == 28.0f
            && rl::guided_rollout_imitation_weight(
                1200u, sim::CourseStage::uneven, &monoped_walk) == 0.0f
            && rl::lesson_teacher_authority(
                900u, sim::CourseStage::uneven, chicken_walk) == 0.0f,
        "fragile support topology does not consolidate without teacher authority");
    {
        rl::PpoTrainer monoped_prior{ monoped_walk, 8u, false };
        monoped_prior.set_course(sim::CourseStage::uneven, 0.30f, false);
        rl::PpoTrainer chicken_prior{ chicken_walk, 8u, false };
        chicken_prior.set_course(sim::CourseStage::uneven, 0.30f, false);
        rl::PpoTrainer biped_prior{ humanoid_walk, 8u, false };
        biped_prior.set_course(sim::CourseStage::uneven, 0.30f, false);
        const std::size_t monoped_samples =
            monoped_prior.foundational_teacher_sample_count();
        const std::size_t chicken_samples =
            chicken_prior.foundational_teacher_sample_count();
        require(monoped_samples >= 512u,
            "monoped compatibility rig cannot build a clean planted teacher-trajectory prior");
        require(chicken_samples >= 512u,
            "chicken cannot build a clean planted teacher-trajectory prior");
        require(biped_prior.foundational_teacher_sample_count() == 0u,
            "clean fragile-topology prior is applied to ordinary paired rigs");
        chicken_prior.set_course(sim::CourseStage::uneven, 0.30f, false);
        require(chicken_prior.foundational_teacher_sample_count()
                == chicken_samples,
            "repeated-seed teacher-prior construction is not deterministic");
    }

    require(rl::crouch_teacher_authority(
                rl::crouch_teacher_fade_begin_update - 1u) == 1.0f
            && rl::crouch_teacher_authority(
                rl::crouch_teacher_handoff_update) == 0.0f
            && std::abs(rl::crouch_teacher_authority(
                (rl::crouch_teacher_fade_begin_update
                    + rl::crouch_teacher_handoff_update) / 2u) - 0.5f) < 1.0e-6f,
        "crouch teacher authority does not fade to an exact finite handoff");
    require(rl::guided_rollout_imitation_weight(
                0u, sim::CourseStage::duck_press) == 64.0f
            && rl::guided_rollout_imitation_weight(
                rl::crouch_teacher_handoff_update,
                sim::CourseStage::duck_press) == 0.0f
            && rl::lesson_teacher_authority(
                rl::crouch_teacher_handoff_update,
                sim::CourseStage::duck_press, humanoid_walk) == 0.0f,
        "crouch demonstration or action authority survives the handoff");
    sim::Environment raw_crouch_environment{ humanoid_walk, 0xC001C0DEu };
    raw_crouch_environment.set_course(sim::CourseStage::duck_press, 0.30f);
    std::array<float, sim::action_count> raw_crouch_action{};
    for (std::size_t index = 0; index < humanoid_walk.active_motor_count; ++index)
        raw_crouch_action[index] = 0.07f * static_cast<float>(index + 1u) - 0.24f;
    const auto zero_authority_crouch = rl::effective_policy_action(
        raw_crouch_environment, raw_crouch_action,
        sim::CourseStage::duck_press, 0.0f);
    bool zero_authority_safety_shaped = false;
    for (std::size_t index = 0; index < humanoid_walk.active_motor_count; ++index)
    {
        if (!rl::motor_drives_support_branch(
                humanoid_walk, humanoid_walk.motors[index])
            && std::abs(zero_authority_crouch[index])
                < std::abs(raw_crouch_action[index]))
            zero_authority_safety_shaped = true;
    }
    require(zero_authority_safety_shaped,
        "zero-authority policy bypasses topology-neutral startup safety shaping");

    require(!rl::nursery_policy_reset_allowed(
                sim::CourseStage::uneven, 100000u, 120u)
            && rl::nursery_policy_reset_allowed(sim::CourseStage::balance,
                rl::stage_minimum_fresh_updates(sim::CourseStage::balance) + 120u,
                12u),
        "nursery reset can erase a partial walker or cannot recycle a stale stander");
    require(rl::incremental_locomotion_candidate(sim::CourseStage::uneven,
                true, true, false, true, 4u, 2u, 2.0f, 3.0f, 1.0f)
            && !rl::incremental_locomotion_candidate(sim::CourseStage::uneven,
                true, true, false, true, 4u, 0u, 2.0f, 3.0f, 1.0f)
            && !rl::incremental_locomotion_candidate(sim::CourseStage::uneven,
                true, true, true, true, 4u, 2u, 2.0f, 3.0f, 1.0f),
        "incremental retention accepts crab/body motion or rejects real partial gait");

    rl::PolicyNetwork imitation_probe{ 0x1A117A7Eu };
    std::array<float, sim::observation_count> imitation_observation{};
    imitation_observation[0] = 0.25f;
    imitation_observation[1] = 0.95f;
    imitation_observation[38] = 0.75f;
    imitation_observation[39] = -0.25f;
    std::array<float, sim::action_count> imitation_target{};
    imitation_target[0] = 0.65f;
    imitation_target[1] = -0.45f;
    imitation_target[2] = -0.55f;
    imitation_target[3] = 0.40f;
    const auto imitation_error = [&](const rl::PolicyNetwork& policy)
    {
        const auto action = policy.deterministic_action(imitation_observation);
        float error{};
        for (std::size_t index = 0; index < sim::anatomy_action_count; ++index)
        {
            const float delta = action[index] - imitation_target[index];
            error += delta * delta;
        }
        return error;
    };
    const float imitation_before = imitation_error(imitation_probe);
    for (int iteration = 0; iteration < 160; ++iteration)
    {
        imitation_probe.zero_gradients();
        float ignored_loss{};
        imitation_probe.accumulate_imitation_gradient(imitation_observation,
            imitation_target, 1.0f, ignored_loss);
        std::vector<float>& parameters = imitation_probe.parameters();
        const std::vector<float>& gradients = imitation_probe.gradients();
        for (std::size_t index = 0; index < parameters.size(); ++index)
            parameters[index] -= gradients[index] * 0.02f;
    }
    require(imitation_error(imitation_probe) < imitation_before * 0.10f,
        "dedicated supervised actor gradient does not learn its gait target");
    require(rl::policy_regression_guard(10.0f, 8.5f, true),
        "large valid-policy degradation does not restore the champion");
    require(rl::policy_regression_guard(10.0f, 10.5f, false),
        "invalid policy does not restore the champion");
    require(!rl::policy_regression_guard(10.0f, 9.4f, true),
        "small exploration change triggers an unnecessary champion rollback");
    require(rl::elite_motion_eligible(sim::CourseStage::uneven, true, 10, 1.2f, 4.0f),
        "valid sustained stepped best result cannot seed self-imitation");
    require(!rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 0, 0.0f, 4.0f, 0.8f),
        "ducking without clearing a low bar can still seed self-imitation");
    require(rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 8, 1.2f, 12.0f,
            3.0f, 0u, 0.0f, 0u, 4u),
        "valid sustained foot-only crouch-walk result cannot seed self-imitation");
    require(!rl::elite_motion_eligible(sim::CourseStage::uneven, false, 8, 12.0f, 20.0f),
        "invalid rolling result can seed self-imitation");
    require(sim::hazard_approach_weight(0.40f) == 1.0f,
        "near obstacle does not activate full leg-lift training");
    require(sim::hazard_quiver_motion(0.50f, 0.02f, 0.03f, 0.40f, 0.20f),
        "high-energy no-lift obstacle quiver is not detected");
    require(!sim::hazard_quiver_motion(0.50f, 0.02f, 0.35f, 0.40f, 0.20f),
        "useful obstacle leg lift is incorrectly classified as quivering");
    require(sim::rolling_body_motion(0.20f, 0.70f, 0.35f, false, true),
        "head, tail, or body rolling is not detected");
    require(!sim::rolling_body_motion(0.20f, 0.70f, 0.95f, true, false),
        "normal foot-supported walking is incorrectly classified as rolling");
    require(sim::foot_pivot_rolling_motion(0.24f, true, true, 0.01f, 0.02f, 0.50f),
        "double-supported rolling around stationary orange foot nodes is not detected");
    require(!sim::foot_pivot_rolling_motion(0.24f, true, false, 0.01f, 0.18f, 0.50f),
        "single-support lifted-foot walking is incorrectly rejected as foot-node rolling");
    require(sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f, 0.02f),
        "straight double-supported skating around planted feet is not rejected");
    require(!sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f,
            0.02f, 4u, 1u),
        "a multi-support rig with an authored leg lifted is rejected as skating");
    require(sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f,
            0.02f, 6u, 0u),
        "a planted multi-support rig can evade the anti-skating rejection");
    require(!sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f,
            0.02f, 2u, 0u, true),
        "a recent physical paired-foot transfer is rejected during its planted phase");
    require(!sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f,
            0.02f, 6u, 0u, true),
        "a recent physical multi-support transfer is rejected during its planted phase");
    require(!sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f,
            0.02f, 2u, 0u, false, 1u),
        "one physical monoped foot is misclassified as two-foot skating");
    require(sim::unsupported_locomotion_penalty(true, false, 0.80f) > 0.10f
            && sim::unsupported_locomotion_penalty(true, false, 0.10f) == 0.0f
            && sim::unsupported_locomotion_penalty(true, true, 2.0f) == 0.0f
            && sim::unsupported_locomotion_penalty(false, false, 2.0f) == 0.0f,
        "sustained flight shaping rejects normal transfer or powered airtime");
    require(sim::course_zone_is_flat(24.0f) && sim::course_zone_is_flat(48.0f),
        "long flat sand-sim patrol zones are missing");
    require(!sim::course_zone_is_flat(32.0f) && !sim::course_zone_is_flat(40.0f),
        "sand mounds are not separated from flat patrol zones");
    require(sim::obstacles_require_flat_zone(sim::CourseStage::hurdles, 1.0f),
        "early debris training can place obstacles on hills");
    require(!sim::obstacles_require_flat_zone(sim::CourseStage::moving_hazards, 0.75f),
        "advanced combat traversal never combines hazards with terrain");
    require(sim::first_course_feature_sequence(0.0f, 29.9f) <= 3,
        "a contacted obstacle is culled like a pickup before it passes behind the actor");

    const std::array<sim::CreatureBlueprint, 8> presets{
        sim::CreatureBlueprint::scaffold(),
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod(),
        sim::CreatureBlueprint::monoped()
    };
    for (const sim::CreatureBlueprint& preset : presets)
    {
        require(preset.valid(), "preset is structurally invalid");
        require(preset.signature() != 0, "preset signature is empty");
        for (std::size_t motor_index = 0; motor_index < preset.active_motor_count; ++motor_index)
        {
            const sim::MotorConstraint& motor = preset.motors[motor_index];
            require(motor.minimum_angle <= motor.neutral_angle && motor.neutral_angle <= motor.maximum_angle,
                "preset motor rest angle is outside limits");
            require(std::abs(wrap_angle(preset.rest_joint_angle(motor_index) - motor.neutral_angle)) < 0.001f,
                "preset motor is not calibrated to rest geometry");
            require(motor.strength <= 0.060f, "default joint speed remains too strong");
            require((motor.maximum_angle - motor.minimum_angle) * 180.0f / pi >= 60.0f,
                "preset cannot articulate enough to lift a leg over debris");
        }
    }

    sim::CreatureBlueprint disconnected = sim::CreatureBlueprint::humanoid();
    disconnected.motors[0].c = disconnected.head_node;
    disconnected.motors[0].enabled = true;
    require(!disconnected.valid(), "enabled motor without direct A-pivot-C bones was accepted");

    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.torso_node < humanoid.nodes.size()
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[7].y + 0.10f
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[10].y + 0.10f,
        "humanoid central shoulder pivot is not above both lateral shoulder pivots");
    require(humanoid.nodes[8].y < humanoid.nodes[7].y
            && humanoid.nodes[11].y < humanoid.nodes[10].y,
        "humanoid rest arms do not hang below the shoulder pivots");
    require(std::ranges::any_of(humanoid.bones, [](const sim::DistanceConstraint& bone)
            { return (bone.a == 2u && bone.b == 7u) || (bone.a == 7u && bone.b == 2u); })
            && std::ranges::any_of(humanoid.bones, [](const sim::DistanceConstraint& bone)
            { return (bone.a == 2u && bone.b == 10u) || (bone.a == 10u && bone.b == 2u); }),
        "raised humanoid shoulder girdle can still invert through the upper spine");
    require(humanoid.nodes.size() == 13u,
        "human-calibrated rig does not retain the compact articulated body and arms");
    require(std::abs(humanoid.nodes[0].x - (-0.179952502f)) < 0.00001f
            && std::abs(humanoid.nodes[0].y - 2.61523819f) < 0.00001f
            && std::abs(humanoid.nodes[1].y - 3.76404762f) < 0.00001f
            && std::abs(humanoid.nodes[2].y - 4.13904762f) < 0.00001f
            && humanoid.nodes[9].y < humanoid.nodes[0].y
            && humanoid.nodes[9].y > humanoid.nodes[3].y
            && humanoid.nodes[12].y < humanoid.nodes[0].y
            && humanoid.nodes[12].y > humanoid.nodes[5].y,
        "current authored Human calibration not applied");
    require(humanoid.bones.size() == 15u,
        "humanoid legs or articulated arms are not structurally connected");
    require(humanoid.active_motor_count == sim::anatomy_action_count,
        "humanoid does not expose independent shoulder and elbow motors");
    require(humanoid.left_contact_node == humanoid.motors[1].c
            && humanoid.right_contact_node == humanoid.motors[3].c,
        "terminal lower-leg joints are not the physical support stubs");
    require(humanoid.additional_left_contact_nodes.empty()
            && humanoid.additional_right_contact_nodes.empty(),
        "terrain-hostile heel/ball/toe collision contacts remain on the humanoid");
    require(humanoid.left_contact_node < humanoid.radii.size()
            && humanoid.right_contact_node < humanoid.radii.size()
            && humanoid.radii[humanoid.left_contact_node] >= 0.104f
            && humanoid.radii[humanoid.right_contact_node] >= 0.104f
            && humanoid.radii[humanoid.left_contact_node] <= 0.1121f
            && humanoid.radii[humanoid.right_contact_node] <= 0.1121f,
        "humanoid terminal support stubs are not compact and terrain-conforming");
    for (std::size_t motor_index = 0; motor_index < humanoid.active_motor_count; ++motor_index)
    {
        const sim::MotorConstraint& motor = humanoid.motors[motor_index];
        require(motor.enabled, "humanoid active motor is disabled");
        if (motor_index < 4u)
        {
            const float driven_arm = length(humanoid.nodes[motor.c] - humanoid.nodes[motor.pivot]);
            const float expected_linear_gain = (motor_index % 2u) == 0u ? 0.045f : 0.051f;
            const float expected_strength = clamp(
                expected_linear_gain / std::max(0.75f, driven_arm), 0.032f, 0.056f);
            const float expected_travel = (motor_index % 2u) == 0u ? 36.0f : 58.0f;
            require(std::abs(motor.strength - expected_strength) < 0.0001f,
                "humanoid leg motor does not use the bounded obstacle-capable effective gain");
            require(std::abs((motor.neutral_angle - motor.minimum_angle) * 180.0f / pi
                - expected_travel) < 0.05f, "obstacle-capable backward leg travel was not applied");
            require(std::abs((motor.maximum_angle - motor.neutral_angle) * 180.0f / pi
                - expected_travel) < 0.05f, "obstacle-capable forward leg travel was not applied");
        }
        else
        {
            const float arm_travel = (motor.maximum_angle - motor.minimum_angle)
                * 180.0f / pi;
            require(arm_travel >= 110.0f && arm_travel <= 155.0f,
                "humanoid arm motor does not preserve bounded natural counter-swing travel");
            require(motor.strength <= 0.025f,
                "humanoid arm motor is too strong for natural gait stabilization");
        }
    }


    {
        sim::Environment motor_reaction{ humanoid, 0xC8357u };
        const sim::MotorConstraint& shoulder = humanoid.motors[4];
        const auto center_of_mass = [](std::span<const sim::Particle> particles)
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (const sim::Particle& particle : particles)
            {
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                weighted_y += static_cast<double>(particle.position.y) * mass;
                total_mass += mass;
            }
            return Vec2{
                static_cast<float>(weighted_x / total_mass),
                static_cast<float>(weighted_y / total_mass)
            };
        };
        const Vec2 chest_before = motor_reaction.particles()[humanoid.torso_node].position;
        const Vec2 pivot_before = motor_reaction.particles()[shoulder.pivot].position;
        const Vec2 driven_before = motor_reaction.particles()[shoulder.c].position;
        const Vec2 center_before = center_of_mass(motor_reaction.particles());
        sim::EnvironmentTestAccess::solve_motor(motor_reaction, shoulder, 1.0f);
        const Vec2 chest_delta =
            motor_reaction.particles()[humanoid.torso_node].position - chest_before;
        const Vec2 pivot_delta =
            motor_reaction.particles()[shoulder.pivot].position - pivot_before;
        const Vec2 driven_delta =
            motor_reaction.particles()[shoulder.c].position - driven_before;
        const Vec2 center_delta = center_of_mass(motor_reaction.particles()) - center_before;
        require(length(chest_delta) > 1.0e-7f,
            "humanoid shoulder still pins the parent chest in world space");
        require(length(pivot_delta) > 1.0e-7f,
            "humanoid shoulder pivot is still a world-space anchor");
        require(length(driven_delta) > length(chest_delta),
            "parent body receives more correction than the driven arm");
        require(length(center_delta) < 2.0e-5f,
            "internal shoulder correction injects center-of-mass translation");
    }

    {
        sim::Environment stable_humanoid{ humanoid, 0x57A8u };
        stable_humanoid.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> neutral{};
        sim::EnvironmentTestAccess::qualify_stable_stance(stable_humanoid);
        const rl::StageMotionQualification stable =
            rl::stage_motion_qualification(sim::CourseStage::balance, stable_humanoid);
        require(stable.valid,
            "neutral humanoid cannot produce a sustained stage-valid standing baseline");
        require(stable_humanoid.stable_stance_seconds() >= 3.0f,
            "standing baseline never accumulates sustained stance evidence");

        sim::Environment collapsed{ humanoid, 0xC011A9u };
        collapsed.set_course(sim::CourseStage::balance, 0.25f);
        sim::EnvironmentTestAccess::collapse_upper_body(collapsed);
        for (int frame = 0; frame < 180 && collapsed.valid_motion(); ++frame)
        {
            sim::EnvironmentTestAccess::collapse_upper_body(collapsed);
            (void)collapsed.step(neutral);
        }
        const rl::StageMotionQualification rejected =
            rl::stage_motion_qualification(sim::CourseStage::balance, collapsed);
        require(!rejected.valid,
            "collapsed humanoid can still qualify as a standing best result");
        require((rejected.rejection_mask
                & rl::evidence_bit(rl::MotionEvidenceFailure::no_stable_stance)) != 0u
            || !collapsed.valid_motion(),
            "collapsed standing rejection does not expose posture evidence");
    }

    {
        sim::Environment duck_lesson{ humanoid, 0xD0C7u };
        duck_lesson.set_course(sim::CourseStage::duck_press, 0.35f);
        require(std::ranges::any_of(duck_lesson.course_features(),
                [](const sim::CourseFeature& feature)
                {
                    return feature.kind == sim::CourseFeatureKind::duck_press;
                }),
            "duck lesson has no explicit compression platen");
        std::array<float, sim::action_count> unrelated{
            0.9f, -0.8f, 0.2f, 0.7f, -0.9f, 0.6f, 0.1f, -0.5f
        };
        const auto coordinated = rl::bilateral_joint_synergy_action(
            duck_lesson, unrelated, sim::CourseStage::duck_press);
        const auto& duck_rig = duck_lesson.blueprint();
        const float left_hip_direction =
            rl::authored_joint_flexion_direction(duck_rig.motors[0]);
        const float left_knee_direction =
            rl::authored_joint_flexion_direction(duck_rig.motors[1]);
        const float right_hip_direction =
            rl::authored_joint_flexion_direction(duck_rig.motors[2]);
        const float right_knee_direction =
            rl::authored_joint_flexion_direction(duck_rig.motors[3]);
        require(std::abs(coordinated[0] * left_hip_direction
                    - coordinated[2] * right_hip_direction)
                < std::abs(unrelated[0] * left_hip_direction
                    - unrelated[2] * right_hip_direction)
            && std::abs(coordinated[1] * left_knee_direction
                    - coordinated[3] * right_knee_direction)
                < std::abs(unrelated[1] * left_knee_direction
                    - unrelated[3] * right_knee_direction),
            "AI outputs are still eight unrelated joint commands");
        sim::EnvironmentTestAccess::set_duck_pressure(duck_lesson, 1.0f);
        const std::array<float, sim::action_count> neutral{};
        const auto duck = rl::effective_policy_action(
            duck_lesson, neutral, sim::CourseStage::duck_press);
        require(duck[0] * left_hip_direction > 0.05f
                && duck[1] * left_knee_direction > 0.10f
                && duck[2] * right_hip_direction > 0.05f
                && duck[3] * right_knee_direction > 0.10f,
            "compression pressure does not trigger a coordinated leg-driven duck primitive");
        require(std::abs(duck[4]) < 0.01f && std::abs(duck[5]) < 0.01f
                && std::abs(duck[6]) < 0.01f && std::abs(duck[7]) < 0.01f,
            "compression lesson still drives shoulders or elbows");
    }

    {
        sim::Environment observation_environment{ humanoid, 0x0B5E7u };
        const auto observation = observation_environment.observation();
        static_assert(sim::observation_count == 62);
        require(observation.size() == 62u,
            "anatomy, shuttle, material, water, and equipment observation layout is not sixty-two floats");
        require(observation[20] == 0.0f && observation[21] == 0.0f,
            "contact channels overlap motor channels at reset");
        require(std::isfinite(observation[18]) && std::isfinite(observation[19]),
            "right-arm angular velocity channels are missing");

        observation_environment.set_course(sim::CourseStage::shuttle, 0.30f);
        sim::EnvironmentTestAccess::set_shuttle_state(observation_environment,
            sim::ShuttleState{ sim::ShuttlePhase::backing, 1.0f, -1.0f });
        const auto backing_observation = observation_environment.observation();
        require(backing_observation[43] == -1.0f
                && backing_observation[44] == 1.0f
                && std::abs(backing_observation[45] - 2.0f / 3.0f) < 0.0001f,
            "policy cannot distinguish backing from forward traversal by facing and shuttle phase");
        sim::EnvironmentTestAccess::set_shuttle_state(observation_environment,
            sim::ShuttleState{ sim::ShuttlePhase::traverse, -1.0f, -1.0f });
        const auto returned_observation = observation_environment.observation();
        require(returned_observation[43] == 1.0f
                && returned_observation[44] == -1.0f
                && returned_observation[45] == 0.0f,
            "post-turn traversal is observationally aliased with pre-turn backing");
    }

    {
        sim::Environment assisted_stance{ humanoid, 0xBA1A9CEu };
        assisted_stance.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> raw_action{};
        std::array<std::uint32_t, 8> stance_failures{};
        for (int frame = 0; frame < 720; ++frame)
        {
            const auto action = rl::effective_policy_action(
                assisted_stance, raw_action, sim::CourseStage::balance);
            const sim::StepResult result = assisted_stance.step(action);
            const bool lesson_complete = assisted_stance.valid_motion()
                && assisted_stance.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds;
            const auto diagnostics =
                sim::EnvironmentTestAccess::stance_frame(assisted_stance);
            const std::array<bool, 8> passed{
                diagnostics.supported,
                diagnostics.body_clear,
                diagnostics.upright,
                diagnostics.head_high,
                diagnostics.low_slip,
                diagnostics.low_torso_turn,
                diagnostics.low_joint_speed,
                diagnostics.low_vertical_speed
            };
            for (std::size_t index = 0; index < passed.size(); ++index)
                stance_failures[index] += passed[index] ? 0u : 1u;
            if (lesson_complete || result.terminated)
                break;
        }
        const rl::StageMotionQualification qualification =
            rl::stage_motion_qualification(sim::CourseStage::balance, assisted_stance);
        if (!qualification.valid)
        {
            std::cerr << "balance controller diagnostics: rejection="
                << qualification.rejection_mask
                << " invalid=" << static_cast<int>(assisted_stance.invalid_reason())
                << " stance=" << assisted_stance.stable_stance_seconds()
                << " longest=" << assisted_stance.longest_stable_stance_seconds()
                << " max_joint=" << assisted_stance.maximum_joint_speed()
                << " survival=" << assisted_stance.elapsed_seconds()
                << " failures[support,body,upright,head,slip,turn,joint,vertical]=";
            for (const std::uint32_t failures : stance_failures)
                std::cerr << failures << ',';
            std::cerr << std::endl;
        }
        require(qualification.valid
                && assisted_stance.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds,
            "shared balance controller cannot sustain a strict neutral physics stance");
        require(rl::training_preview_priority(
                sim::CourseStage::balance, assisted_stance) > 0,
            "stage-qualified standing environment disappears from the training PIP");
        sim::EnvironmentTestAccess::collapse_upper_body(assisted_stance);
        require(!assisted_stance.current_display_posture_valid(),
            "fresh geometric posture check accepts a collapsed current body");
        require(!rl::stage_display_sample_eligible(
                sim::CourseStage::balance, assisted_stance),
            "collapsed current frame is still published as a valid sample");
    }

    {
        constexpr std::size_t evaluation_agents = 6;
        std::uint32_t valid_agents = 0;
        for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
        {
            const std::uint64_t seed = 0xE000u
                + static_cast<std::uint64_t>(agent) * 4099u;
            sim::Environment environment{ humanoid, seed };
            environment.set_course(sim::CourseStage::balance, 0.25f);
            const std::array<float, sim::action_count> raw_action{};
            for (int frame = 0; frame < 1200; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::balance);
                const sim::StepResult result = environment.step(action);
                if (environment.valid_motion()
                    && environment.longest_stable_stance_seconds()
                        >= rl::standing_mastery_seconds)
                    break;
                if (result.terminated)
                    break;
            }
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            const bool integrity = environment.body_integrity_valid();
            valid_agents += qualification.valid && integrity ? 1u : 0u;
            if (!qualification.valid || !integrity)
            {
                std::cerr << "evaluation seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " integrity=" << integrity
                    << " stance=" << environment.stable_stance_seconds()
                    << " longest=" << environment.longest_stable_stance_seconds()
                    << " max_joint=" << environment.maximum_joint_speed()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }
        }
        require(valid_agents == evaluation_agents,
            "shared balance controller fails the strict six-of-six PPO seed gate");
    }

    {
            sim::Environment intact{ humanoid, 0x1A7E6u };
        require(intact.body_integrity_valid(),
            "fresh humanoid body fails the full skeleton integrity gate");
        sim::EnvironmentTestAccess::qualify_stable_stance(intact);
        sim::EnvironmentTestAccess::detach_left_support_cluster(intact);
        require(!intact.body_integrity_valid(),
            "detached foot cluster passes the full skeleton integrity gate");
        require(!rl::stage_display_sample_eligible(sim::CourseStage::balance, intact),
            "detached feet can still publish as a qualified training preview");
        require(rl::training_preview_frame_renderable(intact)
                && rl::training_preview_priority(sim::CourseStage::balance, intact) == 1,
            "finite rejected training attempts disappear instead of remaining diagnosable in PIP");
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
        sim::Environment dynamic_walk{ humanoid, 0xD1A61Cu };
        dynamic_walk.set_course(sim::CourseStage::uneven, 0.30f);
        sim::EnvironmentTestAccess::qualify_walk_evidence(
            dynamic_walk, 0.20f, 8u, 4u, 4.0f, 5.0f);
        require(rl::stage_motion_qualification(
                sim::CourseStage::uneven, dynamic_walk).valid,
            "sustained alternating gait is rejected for lacking a standing hold");
        sim::EnvironmentTestAccess::force_sustained_leg_scissor(dynamic_walk);
        const auto scissored = rl::stage_motion_qualification(
            sim::CourseStage::uneven, dynamic_walk);
        require(!scissored.valid
                && (scissored.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::lower_leg_scissor)) != 0u,
            "persistent lower-leg X gait remains eligible for retention");

        sim::Environment transient_walk{ humanoid, 0x7A451Eu };
        transient_walk.set_course(sim::CourseStage::uneven, 0.30f);
        sim::EnvironmentTestAccess::qualify_walk_evidence(
            transient_walk, 0.20f, 3u, 1u, 4.0f, 5.0f);
        const auto transient = rl::stage_motion_qualification(
            sim::CourseStage::uneven, transient_walk);
        require(!transient.valid
                && (transient.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::no_stable_stance)) != 0u,
            "short transient gait bypasses both static and dynamic support proof");

        sim::Environment crab_walk{ humanoid, 0xC2ABu };
        crab_walk.set_course(sim::CourseStage::uneven, 0.30f);
        sim::EnvironmentTestAccess::qualify_walk_evidence(
            crab_walk, 0.20f, 8u, 0u, 4.0f, 5.0f);
        const auto crab = rl::stage_motion_qualification(
            sim::CourseStage::uneven, crab_walk);
        require(!crab.valid
                && (crab.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::no_stable_stance)) != 0u
                && (crab.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::missing_skill)) != 0u,
            "high-count no-crossing gait fabricates dynamic support evidence");
    }

    {
        sim::Environment neutral_stance{ humanoid, 0x576A6Eu };
        sim::EnvironmentTestAccess::qualify_stable_stance(neutral_stance);
        require(rl::stage_motion_qualification(
                sim::CourseStage::balance, neutral_stance).valid,
            "neutral strict standing evidence is rejected");
        require(rl::stage_display_sample_eligible(
                sim::CourseStage::balance, neutral_stance),
            "current neutral strict stance is not eligible for top-priority PIP display");
        sim::EnvironmentTestAccess::force_arms_overhead(neutral_stance);
        const auto raised = rl::stage_motion_qualification(
            sim::CourseStage::balance, neutral_stance);
        require(!raised.valid
                && (raised.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::non_neutral_posture)) != 0u,
            "arms-up standing exploit is still stage-valid");

        sim::Environment spinning_stance{ humanoid, 0x5A1E7u };
        sim::EnvironmentTestAccess::qualify_stable_stance(spinning_stance);
        sim::EnvironmentTestAccess::force_standing_spin(
            spinning_stance, rl::standing_qualification_spin_limit + 0.01f);
        const auto spinning = rl::stage_motion_qualification(
            sim::CourseStage::balance, spinning_stance);
        require(!spinning.valid
                && (spinning.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::excessive_rotation)) != 0u,
            "rotating standing exploit is still stage-valid");

        rl::TrainingMetrics strict{};
        strict.evaluation_valid = true;
        strict.evaluation_invalid_runs = 0u;
        strict.evaluation_longest_stance = rl::standing_mastery_seconds;
        strict.evaluation_survival = rl::standing_mastery_seconds;
        strict.evaluation_spin_turns = rl::standing_mastery_spin_limit;
        strict.evaluation_max_joint_speed = 7.5f;
        require(rl::strict_balance_mastery(strict),
            "strict standing values cannot advance mastery");
        strict.evaluation_invalid_runs = 1u;
        require(rl::strict_balance_mastery(strict),
            "five-of-six strict standing seeds cannot advance robust mastery");
        strict.evaluation_invalid_runs = 2u;
        require(!rl::strict_balance_mastery(strict),
            "four-of-six standing seeds incorrectly advance mastery");
    }

    {
        const std::array<sim::CreatureBlueprint, 4> passive_rigs{
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::humanoid(),
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
                if (!environment.body_integrity_valid())
                {
                    std::cerr << "passive integrity failure rig=" << index
                        << " frame=" << frame
                        << " invalid=" << static_cast<int>(environment.invalid_reason())
                        << std::endl;
                }
                require(environment.body_integrity_valid(),
                    "head or passive tail escaped the articulated body");
                if (!environment.valid_motion())
                    environment.reset(0x7100u + index * 257u + static_cast<std::size_t>(frame));
            }
        }
    }

    {
        sim::Environment flip_semantics{ humanoid, 0xF11Fu };
        require(flip_semantics.maximum_flip_turns() == 0.0f
                && flip_semantics.uncontrolled_spin_turns() == 0.0f,
            "fresh rig does not separate flip and spin counters");
        sim::EnvironmentTestAccess::force_standing_spin(flip_semantics, 0.25f);
        require(flip_semantics.uncontrolled_spin_turns() == 0.25f,
            "grounded standing rotation cannot be represented by the strict gate");
        flip_semantics.reset(0xF120u);
        require(flip_semantics.uncontrolled_spin_turns() == 0.0f,
            "standing spin evidence leaks across episode resets");
    }

    require(rl::policy_candidate_better(2u, 1.0f, 1u, 1000.0f, true),
        "higher stage-valid evidence loses to scalar reward");
    require(!rl::policy_candidate_better(1u, 1000.0f, 2u, 1.0f, true),
        "high-reward lower-quality exploit can replace a valid controller");
    const std::uint64_t strict_quality =
        rl::strict_evaluation_quality_bit | 1u;
    require(rl::strict_evaluation_quality(strict_quality)
            && rl::policy_candidate_retainable(
                sim::CourseStage::uneven, strict_quality)
            && !rl::policy_candidate_retainable(
                sim::CourseStage::uneven, 60'000u)
            && rl::policy_candidate_retainable(
                sim::CourseStage::balance, 60'000u)
            && rl::policy_candidate_better(strict_quality, 1.0f,
                10'000u, 1000.0f, true)
            && !rl::policy_candidate_better(60'000u, 1000.0f,
                strict_quality, 1.0f, true),
        "partial invalid gait can overwrite a strict-valid retained controller");

    const std::uint64_t farther_shuttle =
        rl::shuttle_motion_quality(1u, 19.0f, 40u, 30.0f);
    const std::uint64_t busy_short_shuttle =
        rl::shuttle_motion_quality(1u, 17.0f, 96u, 40.0f);
    require(farther_shuttle > busy_short_shuttle,
        "busy support cycling can outrank farther physical shuttle traversal");
    require(rl::shuttle_motion_quality(2u, 12.0f, 30u, 28.0f)
            > farther_shuttle,
        "an additional completed shuttle turn does not dominate one-turn evidence");
    require(rl::shuttle_motion_quality(1u, -10.0f, 65535u, 60.0f)
            < rl::shuttle_motion_quality(1u, 1.0f, 1u, 1.0f),
        "negative displacement or inflated cadence can defeat real shuttle progress");
    require(rl::shuttle_motion_quality(1u, 19.0f, 40u, 30.0f)
            == farther_shuttle,
        "shuttle quality ordering is not deterministic across repeated inputs");

    const std::uint64_t quadruped_handoff =
        rl::foundational_walk_teacher_handoff_update(quadruped_walk);
    require(!rl::policy_candidate_retainable(sim::CourseStage::uneven,
                60'000u, quadruped_handoff - 1u, quadruped_walk)
            && rl::policy_candidate_retainable(sim::CourseStage::uneven,
                60'000u, quadruped_handoff, quadruped_walk)
            && !rl::policy_candidate_retainable(sim::CourseStage::uneven,
                0u, quadruped_handoff, quadruped_walk)
            && !rl::policy_candidate_retainable(sim::CourseStage::hurdles,
                60'000u, quadruped_handoff, quadruped_walk)
            && rl::policy_candidate_retainable(sim::CourseStage::uneven,
                strict_quality, quadruped_handoff - 1u, quadruped_walk)
            && !rl::policy_candidate_retainable(sim::CourseStage::shuttle,
                60'000u, quadruped_handoff, quadruped_walk)
            && !rl::policy_candidate_retainable(sim::CourseStage::uneven,
                60'000u, rl::foundational_walk_teacher_handoff_update(monoped_walk),
                monoped_walk)
            && !rl::policy_candidate_retainable(sim::CourseStage::uneven,
                60'000u, rl::foundational_walk_teacher_handoff_update(chicken_walk),
                chicken_walk)
            && rl::policy_candidate_retainable(sim::CourseStage::shuttle,
                strict_quality, quadruped_handoff - 1u, quadruped_walk),
        "partial shuttle or fragile-rig evidence can replace a strict retained controller");

    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();
    const sim::CreatureBlueprint crawler4 = sim::CreatureBlueprint::crawler4();
    const sim::CreatureBlueprint hexapod = sim::CreatureBlueprint::hexapod();
    require(quadruped.support_seed_count() == 4,
        "quadruped is still semantically a two-foot biped");
    require(quadruped.additional_left_contact_nodes.size() == 1
            && quadruped.additional_right_contact_nodes.size() == 1,
        "quadruped diagonal support pairs are missing");
    require(std::abs(quadruped.nodes[4].x - quadruped.nodes[5].x) > 0.30f
            && std::abs(quadruped.nodes[6].x - quadruped.nodes[7].x) > 0.30f,
        "quadruped near/far legs overlap in the side-view geometry");
    require(crawler4.nodes.size() >= 9 && crawler4.bones.size() >= 10
            && crawler4.support_seed_count() == 4,
        "four-legged crawler geometry or support semantics are incomplete");
    require(hexapod.nodes.size() >= 10 && hexapod.bones.size() >= 10
        && hexapod.active_motor_count == 8
        && hexapod.support_seed_count() == 6
        && hexapod.horizontal_multi_support_plan(),
    "six-legged hexapod geometry, tripod phases, or support semantics are incomplete");

    {
        sim::Environment biped_support{ humanoid, 0xFEE7u };
        biped_support.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> zero_actions{};
        bool support_observed = false;
        for (int frame = 0; frame < 60; ++frame)
        {
            const sim::StepResult result = biped_support.step(zero_actions);
            support_observed = support_observed
                || biped_support.left_supported() || biped_support.right_supported();
            if (result.terminated)
                break;
        }
        require(support_observed,
            "terminal biped support stubs never became valid support contacts");
        require(biped_support.invalid_reason() != sim::InvalidMotion::sustained_flight,
            "grounded passive biped feet were still classified as flying");
    }

    for (std::size_t stage_index = 0; stage_index < sim::course_stage_count; ++stage_index)
    {
        sim::Environment environment{ humanoid, 42u + stage_index };
        const auto stage = static_cast<sim::CourseStage>(stage_index);
        environment.set_course(stage, 0.45f);
        if (stage == sim::CourseStage::hurdles
            || stage == sim::CourseStage::moving_hazards)
            require(!environment.course_features().empty(),
                "moving obstacle curriculum stage has no course features");
        const std::array<float, sim::action_count> zero_actions{};
        for (int frame = 0; frame < 120; ++frame)
        {
            const sim::StepResult result = environment.step(zero_actions);
            require(std::isfinite(result.reward), "course reward is not finite");
            require(std::isfinite(result.forward_speed), "course speed is not finite");
            const auto observation = environment.observation();
            for (const float value : observation)
                require(std::isfinite(value), "course observation is not finite");
            if (result.terminated)
                environment.reset(42u + stage_index + static_cast<std::size_t>(frame));
        }
    }


    {
        sim::Environment procedural{ humanoid, 0xC0A57u };
        procedural.set_course(sim::CourseStage::moving_hazards, 0.75f);
        const float initial_progress = procedural.course_progress();
        const float initial_height = procedural.ground_height_at(29.0f);
        const std::array<float, sim::action_count> zero_actions{};
        for (int frame = 0; frame < 90; ++frame)
        {
            const sim::StepResult result = procedural.step(zero_actions);
            require(std::isfinite(result.reward), "procedural obstacle reward is not finite");
            (void)result.terminated;
        }
        require(procedural.course_progress() > initial_progress,
            "procedural course does not advance when the creature is stationary");
        require(std::abs(procedural.ground_height_at(29.0f) - initial_height) > 0.001f,
            "procedural inclines and hills do not move through the training lane");

        std::array<bool, static_cast<std::size_t>(
            sim::CourseFeatureKind::projectile) + 1u> found{};
        for (const sim::CourseFeature& feature : procedural.course_features())
            found[static_cast<std::size_t>(feature.kind)] = true;
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::hurdle)],
            "procedural course omitted hurdles");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::overhead_bar)],
            "procedural course omitted overhead bars");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::moving_hazard)],
            "procedural course omitted moving hazards");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::rock)],
            "procedural course omitted rocks");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::projectile)],
            "procedural course omitted thrown objects");
    }

    {
        sim::Environment flat_obstacles{ humanoid, 0xF1A7u };
        flat_obstacles.set_course(sim::CourseStage::hurdles, 0.45f);
        require(!flat_obstacles.course_features().empty(),
            "flat debris lesson has no obstacles");
        for (const sim::CourseFeature& feature : flat_obstacles.course_features())
        {
            require(sim::course_zone_is_flat(sim::course_marker_distance_m(feature.marker_sequence)),
                "early obstacle curriculum placed debris on a hill or slope");
        }
    }


    {
        rl::PpoTrainer stance_trainer{ humanoid, 8 };
        stance_trainer.set_cpu_mode(1);
        constexpr int standing_update_budget = 40;
        for (int update = 0; update < standing_update_budget
            && !stance_trainer.has_best_policy(); ++update)
            stance_trainer.train_one_update();
        if (!stance_trainer.has_best_policy())
        {
            const rl::TrainingMetrics& metrics = stance_trainer.metrics();
            std::cerr << "standing acceptance diagnostics: updates=" << metrics.update
                << " evaluations=" << metrics.evaluation_count
                << " valid=" << metrics.evaluation_valid
                << " rejection=" << metrics.evaluation_rejection_mask
                << " invalid_runs=" << metrics.evaluation_invalid_runs
                << " stance=" << metrics.evaluation_stable_stance
                << " longest=" << metrics.evaluation_longest_stance
                << " max_joint=" << metrics.evaluation_max_joint_speed
                << " survival=" << metrics.evaluation_survival << '\n';
        }
        require(stance_trainer.metrics().evaluation_count >= 1u,
            "bounded standing training never ran deterministic evaluation");
        require(stance_trainer.metrics().evaluation_valid,
            "bounded standing training did not produce a valid standing candidate");
        require(stance_trainer.metrics().evaluation_quality_key != 0u,
            "valid standing candidate has no lexicographic quality evidence");
        require(stance_trainer.has_best_policy(),
            "first valid standing candidate was not retained as the best controller");
    }

    rl::PpoTrainer trainer{ humanoid, 16 };
    require(trainer.rollout_worker_count() >= 1, "parallel rollout worker count is invalid");
    trainer.set_cpu_mode(1);
    const std::size_t normal_workers = trainer.rollout_worker_count();
    trainer.set_cpu_mode(2);
    const std::size_t faster_workers = trainer.rollout_worker_count();
    trainer.set_cpu_mode(4);
    const std::size_t maximum_workers = trainer.rollout_worker_count();
    require(normal_workers <= faster_workers && faster_workers <= maximum_workers,
        "speed modes do not increase persistent worker budget");
    require(maximum_workers == trainer.maximum_worker_count(),
        "MAX CPU does not enable the full persistent worker pool");
    require(trainer.exploration() < 0.20f, "fresh exploration is too aggressive");
    require(trainer.exploration() <= 0.081f, "fresh policy still applies an aggressive spawn impulse");
    trainer.set_course(sim::CourseStage::balance, 0.25f, false);
    for (int update = 0; update < 2; ++update)
    {
        trainer.train_one_update();
        const rl::TrainingMetrics& metrics = trainer.metrics();
        require(metrics.update == static_cast<std::uint64_t>(update + 1), "PPO update counter mismatch");
        require(std::isfinite(metrics.mean_reward), "PPO mean reward is not finite");
        require(std::isfinite(metrics.mean_speed), "PPO mean speed is not finite");
        require(std::isfinite(metrics.policy_loss), "PPO policy loss is not finite");
        require(std::isfinite(metrics.value_loss), "PPO value loss is not finite");
        require(metrics.total_updates == metrics.update,
            "fresh cumulative update count does not track policy updates");
        require(metrics.total_environment_steps == metrics.environment_steps,
            "fresh cumulative environment count does not track policy steps");
        require(metrics.total_training_seconds > 0.0,
            "cumulative training time did not advance");
        require(metrics.total_distance >= 0.0
                && metrics.total_distance <= metrics.total_training_seconds
                    * static_cast<double>(sim::odometer_speed_limit_mps + 0.001f),
            "parallel workers multiply or discontinuously inflate the odometer");
    }

    require(trainer.lesson_update() == 2u,
        "lesson-local update clock did not advance with optimizer work");
    trainer.set_course(sim::CourseStage::balance, 0.35f, true);
    require(trainer.lesson_update() == 2u,
        "same-lesson difficulty adjustment reset the lesson clock");
    trainer.set_course(sim::CourseStage::duck_press, 0.25f, true);
    require(trainer.lesson_update() == 0u && trainer.metrics().update == 2u,
        "Stand to Crouch did not reset lesson age while preserving rig work");
    trainer.train_one_update();
    require(trainer.lesson_update() == 1u && trainer.metrics().update == 3u,
        "Crouch lesson age is coupled to lifetime policy age");

    const std::filesystem::path temporary =
        std::filesystem::temp_directory_path() / "runner-v061-core-test.eppo";
    std::string error{};
    require(trainer.save_checkpoint(temporary, error), "failed to save checkpoint: " + error);
    rl::PpoTrainer resumed{ humanoid, 16 };
    require(resumed.load_checkpoint(temporary, error, false), "failed to resume checkpoint: " + error);
    require(resumed.policy().parameters() == trainer.policy().parameters(), "checkpoint policy mismatch");
    require(resumed.metrics().update == trainer.metrics().update, "checkpoint update count was not restored");
    require(resumed.lesson_update() == trainer.lesson_update()
            && resumed.checkpoint_data().lesson_update == trainer.lesson_update(),
        "checkpoint did not restore the stage-local lesson clock");
    require(resumed.metrics().total_updates == trainer.metrics().total_updates
            && resumed.metrics().total_environment_steps
                == trainer.metrics().total_environment_steps,
        "checkpoint cumulative update/environment totals were not restored");
    require(resumed.metrics().total_training_seconds == trainer.metrics().total_training_seconds,
        "checkpoint cumulative training time was not restored");
    require(resumed.metrics().total_distance == trainer.metrics().total_distance,
        "checkpoint cumulative odometer was not restored");
    require(resumed.optimizer_step() == trainer.optimizer_step(), "checkpoint optimizer state was not restored");
    require(trainer.checkpoint_data().training_semantics == rl::training_semantics_version,
        "checkpoint does not persist the current training-semantics signature");
    require(resumed.course_stage() == trainer.course_stage(), "checkpoint curriculum stage was not restored");
    const rl::TrainingMetrics before_rig_switch = resumed.metrics();
    resumed.set_blueprint(sim::CreatureBlueprint::biped(), false);
    require(resumed.metrics().update == 0u
            && resumed.metrics().total_updates == before_rig_switch.total_updates
            && resumed.metrics().total_environment_steps
                == before_rig_switch.total_environment_steps
            && resumed.metrics().total_distance == before_rig_switch.total_distance,
        "canonical rig switch erased the all-time training ledger");

    rl::PpoTrainer::CheckpointData legacy = trainer.checkpoint_data();
    legacy.training_semantics = 0x0007'3501u;
    legacy.first_moment.clear();
    legacy.second_moment.clear();
    legacy.best_parameters.clear();
    rl::PpoTrainer blocked_legacy{ humanoid, 16 };
    require(!blocked_legacy.apply_checkpoint_data(legacy, error, false),
        "legacy semantics resumed as valid mastery instead of requiring transfer");
    require(blocked_legacy.import_lifetime_ledger(legacy.metrics, error)
            && blocked_legacy.metrics().update == 0u
            && blocked_legacy.optimizer_step() == 0u
            && blocked_legacy.metrics().total_updates == trainer.metrics().total_updates
            && blocked_legacy.metrics().total_environment_steps
                == trainer.metrics().total_environment_steps
            && blocked_legacy.metrics().total_distance == trainer.metrics().total_distance,
        "incompatible checkpoint did not preserve only its all-time ledger");
    rl::TrainingMetrics invalid_lifetime = legacy.metrics;
    invalid_lifetime.total_distance = std::numeric_limits<double>::infinity();
    require(!blocked_legacy.import_lifetime_ledger(invalid_lifetime, error)
            && blocked_legacy.metrics().total_distance == trainer.metrics().total_distance,
        "non-finite legacy odometer was imported");

    const std::filesystem::path lifetime_import_directory =
        std::filesystem::temp_directory_path() / "runner-v0742-lifetime-import-test";
    std::filesystem::remove_all(lifetime_import_directory);
    std::filesystem::create_directories(lifetime_import_directory);
    const sim::CreatureSpeciesPaths human_paths =
        sim::creature_species_paths(sim::CreatureSpecies::human);
    const std::filesystem::path current_autosave = lifetime_import_directory
        / human_paths.autosave_checkpoint;
    const std::filesystem::path current_rig = lifetime_import_directory
        / human_paths.evolved_rig;
    const std::filesystem::path current_state = lifetime_import_directory
        / human_paths.autonomy_state;
    const std::filesystem::path v0741_autosave = lifetime_import_directory
        / "runner-v0741-natural-gait-autosave.eppo";
    require(rl::PpoTrainer::write_checkpoint_data(legacy, v0741_autosave, error),
        "failed to write legacy lifetime import fixture: " + error);
    constexpr std::array<char, 8> v0741_magic{
        'E', 'P', 'P', 'O', '4', '1', '\0', '\1' };
    require(rewrite_checkpoint_magic(v0741_autosave, v0741_magic),
        "failed to mark the fallback fixture as an EPPO41 checkpoint");
    {
        rl::AutonomousTrainer importing{ humanoid, 16 };
        importing.set_autosave_paths(current_autosave, current_rig, current_state);
        importing.set_background_enabled(false);
        std::string import_message{};
        require(importing.load_autosave(import_message)
                && import_message.find("V0.7.41 LIFETIME LEDGER")
                    != std::string::npos,
            "v0.7.41 fallback autosave was not selected before a new save");
        for (int attempt = 0; attempt < 400
            && importing.metrics().total_updates != trainer.metrics().total_updates;
            ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            importing.synchronize();
        }
        require(importing.metrics().update == 0u
                && importing.metrics().total_updates == trainer.metrics().total_updates
                && importing.metrics().total_environment_steps
                    == trainer.metrics().total_environment_steps
                && importing.metrics().total_distance == trainer.metrics().total_distance
                && !std::filesystem::exists(current_autosave),
            "legacy autosave did not import only lifetime totals before current save");
    }
    std::filesystem::remove_all(lifetime_import_directory);

    const std::filesystem::path isolation_directory =
        std::filesystem::temp_directory_path() / "runner-v0742-species-isolation-test";
    std::filesystem::remove_all(isolation_directory);
    std::filesystem::create_directories(isolation_directory);
    const std::filesystem::path isolated_checkpoint = isolation_directory
        / "runner-v0742-human-autosave.eppo";
    const std::filesystem::path wrong_species_rig = isolation_directory
        / "runner-v0742-human-evolved.rig";
    const std::filesystem::path isolated_state = isolation_directory
        / "runner-v0742-human-autonomy.state";
    require(rl::PpoTrainer::write_checkpoint_data(trainer.checkpoint_data(),
            isolated_checkpoint, error),
        "failed to write species-isolation checkpoint: " + error);
    require(sim::CreatureBlueprint::crawler4().save(wrong_species_rig, error),
        "failed to write cross-species autosave rig: " + error);
    {
        rl::AutonomousTrainer isolated{ humanoid, 16 };
        isolated.set_autosave_paths(isolated_checkpoint, wrong_species_rig,
            isolated_state);
        isolated.set_background_enabled(false);
        isolated.synchronize();
        const std::uint64_t original_signature = isolated.rig_signature();
        const rl::TrainingMetrics original_metrics = isolated.metrics();
        std::string isolation_message{};
        require(isolated.load_autosave(isolation_message),
            "cross-species autosave fixture was not queued");
        for (int attempt = 0; attempt < 400
            && isolated.autonomy_status().message.find("Rig topology is dog")
                == std::string::npos;
            ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            isolated.synchronize();
        }
        require(isolated.autonomy_status().message.find("Rig topology is dog")
                != std::string::npos,
            "cross-species autosave was not rejected before application");
        require(isolated.rig_signature() == original_signature,
            "cross-species autosave changed the active Human rig");
        require(isolated.metrics().update == original_metrics.update
                && isolated.metrics().total_updates == original_metrics.total_updates,
            std::format("cross-species autosave changed controller counters: update {} -> {}, "
                        "total {} -> {}",
                original_metrics.update, isolated.metrics().update,
                original_metrics.total_updates, isolated.metrics().total_updates));
    }
    std::filesystem::remove_all(isolation_directory);

    rl::PpoTrainer transferred_legacy{ humanoid, 16 };
    require(transferred_legacy.apply_checkpoint_data(legacy, error, true),
        "explicit dimension-compatible legacy weight transfer failed: " + error);
    require(transferred_legacy.policy().parameters() == trainer.policy().parameters()
            && transferred_legacy.metrics().update == 0u
            && transferred_legacy.lesson_update() == 0u
            && transferred_legacy.optimizer_step() == 0u
            && transferred_legacy.controller_state() == rl::ControllerState::transferred,
        "legacy transfer retained optimizer, mastery, or non-transfer controller state");
    const std::filesystem::path legacy_path =
        std::filesystem::temp_directory_path() / "runner-v0715-legacy-transfer-test.eppo";
    require(rl::PpoTrainer::write_checkpoint_data(legacy, legacy_path, error),
        "failed to write legacy transfer fixture: " + error);
    rl::PpoTrainer loaded_legacy{ humanoid, 16 };
    require(!loaded_legacy.load_checkpoint(legacy_path, error, false)
            && loaded_legacy.load_checkpoint(legacy_path, error, true),
        "file-based legacy checkpoint is not resume-blocked and transfer-enabled");
    std::filesystem::remove(legacy_path);
    constexpr std::size_t v0727_parameter_count = 8'017u;
    rl::PpoTrainer::CheckpointData v0727 = trainer.checkpoint_data();
    v0727.training_semantics = rl::training_semantics_version - 1u;
    v0727.parameters.resize(v0727_parameter_count);
    for (std::size_t index = 0; index < v0727.parameters.size(); ++index)
    {
        const int centered = static_cast<int>(index % 257u) - 128;
        v0727.parameters[index] = static_cast<float>(centered) * 0.001f;
    }
    v0727.first_moment.clear();
    v0727.second_moment.clear();
    v0727.best_parameters.clear();
    v0727.reward_history.clear();
    v0727.speed_history.clear();

    rl::PpoTrainer migrated_v0727{ humanoid, 16 };
    require(migrated_v0727.apply_checkpoint_data(v0727, error, true),
        "v0.7.27 dimension migration failed: " + error);
    const auto& migrated_parameters = migrated_v0727.policy().parameters();
    constexpr std::size_t hidden = rl::PolicyNetwork::hidden_size;
    constexpr std::size_t old_input = 50u;
    constexpr std::size_t old_output = 8u;
    constexpr std::size_t old_b1 = hidden * old_input;
    constexpr std::size_t old_w2 = old_b1 + hidden;
    constexpr std::size_t old_b2 = old_w2 + hidden * hidden;
    constexpr std::size_t old_actor_w = old_b2 + hidden;
    constexpr std::size_t old_actor_b = old_actor_w + old_output * hidden;
    constexpr std::size_t old_value_w = old_actor_b + old_output;
    constexpr std::size_t old_value_b = old_value_w + hidden;
    constexpr std::size_t old_log_std = old_value_b + 1u;
    constexpr std::size_t new_input = rl::PolicyNetwork::input_size;
    constexpr std::size_t new_output = rl::PolicyNetwork::output_size;
    constexpr std::size_t new_b1 = hidden * new_input;
    constexpr std::size_t new_w2 = new_b1 + hidden;
    constexpr std::size_t new_b2 = new_w2 + hidden * hidden;
    constexpr std::size_t new_actor_w = new_b2 + hidden;
    constexpr std::size_t new_actor_b = new_actor_w + new_output * hidden;
    constexpr std::size_t new_value_w = new_actor_b + new_output;
    constexpr std::size_t new_value_b = new_value_w + hidden;
    constexpr std::size_t new_log_std = new_value_b + 1u;
    for (std::size_t row = 0; row < hidden; ++row)
    {
        for (std::size_t column = 0; column < old_input; ++column)
        {
            require(migrated_parameters[row * new_input + column]
                    == v0727.parameters[row * old_input + column],
                "v0.7.27 first-layer anatomy weight changed during migration");
        }
        for (std::size_t column = old_input; column < new_input; ++column)
        {
            require(migrated_parameters[row * new_input + column] == 0.0f,
                "new material/equipment observation channel is not neutral after migration");
        }
    }
    require(std::equal(v0727.parameters.begin() + static_cast<std::ptrdiff_t>(old_b1),
            v0727.parameters.begin() + static_cast<std::ptrdiff_t>(old_actor_b),
            migrated_parameters.begin() + static_cast<std::ptrdiff_t>(new_b1)),
        "v0.7.27 hidden or anatomy actor weights changed during migration");
    require(std::equal(v0727.parameters.begin() + static_cast<std::ptrdiff_t>(old_actor_b),
            v0727.parameters.begin() + static_cast<std::ptrdiff_t>(old_actor_b + old_output),
            migrated_parameters.begin() + static_cast<std::ptrdiff_t>(new_actor_b)),
        "v0.7.27 anatomy actor bias changed during migration");
    require(std::equal(v0727.parameters.begin() + static_cast<std::ptrdiff_t>(old_value_w),
            v0727.parameters.begin() + static_cast<std::ptrdiff_t>(old_log_std + old_output),
            migrated_parameters.begin() + static_cast<std::ptrdiff_t>(new_value_w)),
        "v0.7.27 value or exploration weights changed during migration");
    for (std::size_t output = old_output; output < new_output; ++output)
    {
        const std::size_t actor_row = new_actor_w + output * hidden;
        require(std::all_of(migrated_parameters.begin() + static_cast<std::ptrdiff_t>(actor_row),
                migrated_parameters.begin() + static_cast<std::ptrdiff_t>(actor_row + hidden),
                [](float value) { return value == 0.0f; })
                && migrated_parameters[new_actor_b + output] == 0.0f
                && migrated_parameters[new_log_std + output] == std::log(0.08f),
            "new equipment actor output is not neutral after migration");
    }
    require(migrated_v0727.metrics().update == 0u
            && migrated_v0727.optimizer_step() == 0u,
        "v0.7.27 migration retained incompatible optimizer or mastery state");

    rl::PpoTrainer::CheckpointData truncated_v0727 = v0727;
    truncated_v0727.parameters.pop_back();
    rl::PpoTrainer rejected_v0727{ humanoid, 16 };
    require(!rejected_v0727.apply_checkpoint_data(truncated_v0727, error, true),
        "truncated v0.7.27 network migrated silently");

    const std::filesystem::path v0727_path =
        std::filesystem::temp_directory_path() / "runner-v0727-migration-test.eppo";
    require(write_v0727_checkpoint(v0727_path, v0727),
        "failed to create authentic EPPO28 migration fixture");
    rl::PpoTrainer file_v0727{ humanoid, 16 };
    require(!file_v0727.load_checkpoint(v0727_path, error, false),
        "v0.7.27 checkpoint resumed optimizer/mastery instead of requiring transfer");
    require(file_v0727.load_checkpoint(v0727_path, error, true),
        "authentic EPPO28 file did not migrate: " + error);
    require(file_v0727.policy().parameters() == migrated_parameters,
        "EPPO28 file migration differs from in-memory deterministic migration");
    std::filesystem::remove(v0727_path);

    rl::PpoTrainer wrong_rig{ sim::CreatureBlueprint::quadruped(), 16 };
    require(!wrong_rig.load_checkpoint(temporary, error, false), "mismatched rig checkpoint resumed silently");
    require(wrong_rig.load_checkpoint(temporary, error, true), "intentional v0.4 transfer failed: " + error);
    require(wrong_rig.metrics().update == 0, "transfer retained incompatible progress metrics");
    require(wrong_rig.optimizer_step() == 0, "transfer retained incompatible optimizer state");
    std::filesystem::remove(temporary);

    const std::filesystem::path autonomy_checkpoint =
        std::filesystem::temp_directory_path() / "runner-core-autonomy.eppo";
    const std::filesystem::path autonomy_rig =
        std::filesystem::temp_directory_path() / "runner-core-autonomy.rig";
    const std::filesystem::path autonomy_state =
        std::filesystem::temp_directory_path() / "runner-core-autonomy.state";
    {
        rl::AutonomousTrainer autonomous{ humanoid, 16 };
        autonomous.set_autosave_paths(
            autonomy_checkpoint, autonomy_rig, autonomy_state);
        autonomous.set_background_enabled(false);
        autonomous.synchronize();
        require(autonomous.autonomy_status().stage == sim::CourseStage::balance,
            "autonomous trainer did not start with balance curriculum");
        require(autonomous.autonomy_status().environment_count == 16,
            "autonomous trainer environment count mismatch");
        autonomous.train_one_update();
        const auto update_deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(30);
        while (autonomous.metrics().update == 0
            && std::chrono::steady_clock::now() < update_deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            autonomous.synchronize();
        }
        require(autonomous.metrics().update >= 1, "coroutine background worker did not process requested update");

        autonomous.set_updates_per_cycle(4);
        autonomous.set_background_enabled(true);
        sim::CreatureBlueprint edited = humanoid;
        edited.nodes[1].x += 0.01f;
        edited.rebuild_rest_lengths();
        const auto command_started = std::chrono::steady_clock::now();
        autonomous.set_blueprint(edited, true);
        const auto command_elapsed = std::chrono::steady_clock::now() - command_started;
        require(command_elapsed < std::chrono::milliseconds(20),
            "hip edit blocked the caller on active training work");
        for (int attempt = 0; attempt < 1200 && autonomous.rig_signature() != edited.signature(); ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            autonomous.synchronize();
        }
        require(autonomous.rig_signature() == edited.signature(),
            "queued hip edit was not eventually published");
        autonomous.set_updates_per_cycle(1);
        require(autonomous.updates_per_cycle() == 1, "NORMAL speed mode did not latch");
        autonomous.set_updates_per_cycle(2);
        require(autonomous.updates_per_cycle() == 2, "FASTER speed mode did not latch");
        autonomous.set_updates_per_cycle(4);
        require(autonomous.updates_per_cycle() == 4, "MAX CPU speed mode did not latch");
    }
    std::filesystem::remove(autonomy_checkpoint);
    std::filesystem::remove(autonomy_rig);
    std::filesystem::remove(autonomy_state);

    std::cout << "Runner core standing, PIP, obstacle, integrity, telemetry, concurrency, gait, and rig-edit tests passed\n";
    return EXIT_SUCCESS;
}
