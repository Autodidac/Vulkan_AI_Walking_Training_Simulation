#include "ppo.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static void set_elapsed(Environment& environment, float seconds) noexcept
        {
            environment.elapsed_seconds_ = seconds;
            environment.last_step_dt_ = 1.0f / 60.0f;
            environment.crouch_posture_guide_applied_this_step_ = false;
        }

        static void apply_duck_posture(Environment& environment) noexcept
        {
            environment.crouch_posture_guide_applied_this_step_ = false;
            environment.stabilize_duck_posture();
        }

        static void offset_standing_body(Environment& environment) noexcept
        {
            for (const auto node : { environment.blueprint_.root_node,
                    environment.blueprint_.torso_node,
                    environment.blueprint_.head_node })
            {
                environment.particles_[node].position += Vec2{ 0.08f, -0.12f };
                environment.particles_[node].previous += Vec2{ 0.08f, -0.12f };
            }
        }

        static void apply_standing_posture(Environment& environment) noexcept
        {
            environment.stabilize_balance_posture();
        }

        static void seed_crouch_state(Environment& environment) noexcept
        {
            environment.action_support_unload_side_ = -1;
            environment.last_action_support_unload_side_ = 1;
            environment.support_cluster_release_hold_seconds_ = { 0.12f, 0.08f };
            environment.support_toe_roll_seconds_ = { 0.06f, 0.04f };
            environment.articulated_toe_commands_ = { 0.70f, -0.50f };
        }

        static std::array<float, 8> crouch_state(
            const Environment& environment) noexcept
        {
            return {
                static_cast<float>(environment.action_support_unload_side_),
                static_cast<float>(environment.last_action_support_unload_side_),
                environment.support_cluster_release_hold_seconds_[0],
                environment.support_cluster_release_hold_seconds_[1],
                environment.support_toe_roll_seconds_[0],
                environment.support_toe_roll_seconds_[1],
                environment.articulated_toe_commands_[0],
                environment.articulated_toe_commands_[1]
            };
        }
    };
}

namespace
{
    using runner::Vec2;

    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    [[nodiscard]] bool near(float lhs, float rhs,
        float tolerance = 1.0e-6f) noexcept
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    [[nodiscard]] std::vector<Vec2> positions(
        const runner::sim::Environment& environment)
    {
        std::vector<Vec2> result{};
        result.reserve(environment.particles().size());
        for (const runner::sim::Particle& particle : environment.particles())
            result.push_back(particle.position);
        return result;
    }

    void require_same_physics(const runner::sim::Environment& lhs,
        const runner::sim::Environment& rhs, const char* message)
    {
        const auto left = lhs.particles();
        const auto right = rhs.particles();
        require(left.size() == right.size(), message);
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            require(left[index].position.x == right[index].position.x
                    && left[index].position.y == right[index].position.y
                    && left[index].previous.x == right[index].previous.x
                    && left[index].previous.y == right[index].previous.y
                    && left[index].radius == right[index].radius
                    && left[index].inverse_mass == right[index].inverse_mass
                    && left[index].grounded == right[index].grounded,
                message);
        }
        for (std::size_t index = 0;
            index < runner::sim::anatomy_action_count; ++index)
        {
            const auto left_motor = lhs.motor_diagnostic(index);
            const auto right_motor = rhs.motor_diagnostic(index);
            require(left_motor.current_angle == right_motor.current_angle
                    && left_motor.angular_velocity == right_motor.angular_velocity
                    && left_motor.applied_action == right_motor.applied_action
                    && left_motor.target_angle == right_motor.target_angle,
                message);
        }
    }

    void test_authority_scalar()
    {
        using namespace runner::sim;
        Environment environment{ CreatureBlueprint::humanoid(), 0x751C001u };
        environment.set_course(CourseStage::crouch_walk, 0.30f);

        environment.set_physical_guidance_authority(-2.0f);
        require(environment.physical_guidance_authority() == 0.0f,
            "negative Crouch authority did not clamp to zero");
        environment.set_physical_guidance_authority(2.0f);
        require(environment.physical_guidance_authority() == 1.0f,
            "Crouch authority above one did not clamp to one");
        environment.set_physical_guidance_authority(0.375f);
        require(environment.physical_guidance_authority() == 0.375f,
            "finite Crouch authority was not retained exactly");
        environment.set_physical_guidance_authority(
            std::numeric_limits<float>::quiet_NaN());
        require(environment.physical_guidance_authority() == 0.0f,
            "non-finite Crouch authority was not rejected to zero");
        environment.set_physical_guidance_authority(
            std::numeric_limits<float>::infinity());
        require(environment.physical_guidance_authority() == 0.0f,
            "infinite Crouch authority was not rejected to zero");

        environment.set_physical_guidance_authority(1.0f);
        environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
        environment.set_physical_guidance_authority(1.0f);
        require(environment.physical_guidance_authority() == 1.0f
                && environment.effective_physical_guidance_authority() == 0.0f,
            "Raw mode did not force exact zero effective Crouch authority");
    }

    void test_authority_schedules()
    {
        using namespace runner::rl;
        require(crouch_walk_teacher_authority(0u) == 1.0f
                && crouch_walk_teacher_authority(
                    crouch_walk_teacher_fade_begin_update) == 1.0f,
            "CrouchWalk full-authority interval changed");
        const std::uint64_t midpoint = (crouch_walk_teacher_fade_begin_update
            + crouch_walk_teacher_handoff_update) / 2u;
        require(near(crouch_walk_teacher_authority(midpoint), 0.5f),
            "CrouchWalk midpoint authority is not one half");
        require(crouch_walk_teacher_authority(
                    crouch_walk_teacher_handoff_update - 1u) > 0.0f
                && crouch_walk_teacher_authority(
                    crouch_walk_teacher_handoff_update) == 0.0f
                && crouch_walk_teacher_authority(
                    crouch_walk_teacher_handoff_update + 500u) == 0.0f,
            "CrouchWalk authority did not reach and retain exact zero");
        float previous = 1.0f;
        for (std::uint64_t update = 0u;
            update <= crouch_walk_teacher_handoff_update + 64u; ++update)
        {
            const float current = crouch_walk_teacher_authority(update);
            require(std::isfinite(current) && current >= 0.0f
                    && current <= previous,
                "CrouchWalk authority is not finite, bounded, and monotone");
            previous = current;
        }

        require(crouch_teacher_authority(59u) == 1.0f
                && crouch_teacher_authority(60u) == 1.0f
                && near(crouch_teacher_authority(130u), 0.5f)
                && crouch_teacher_authority(200u) == 0.0f,
            "DuckPress authority semantics changed");
    }

    void test_action_boundary()
    {
        using namespace runner;
        using namespace runner::rl;
        using namespace runner::sim;
        Environment base{ CreatureBlueprint::humanoid(), 0x751C002u };
        base.set_course(CourseStage::crouch_walk, 0.30f);
        base.set_course_motion_enabled(false);

        std::array<float, action_count> raw{};
        std::array<float, action_count> assisted_candidate{};
        for (std::size_t index = 0; index < action_count; ++index)
        {
            raw[index] = index % 2u == 0u ? -0.24f : 0.31f;
            assisted_candidate[index] = index % 3u == 0u ? 0.66f : -0.52f;
        }

        Environment full_environment = base;
        full_environment.set_physical_guidance_authority(1.0f);
        const auto full = effective_policy_action_with_unguided_baseline(
            full_environment, assisted_candidate, raw,
            CourseStage::crouch_walk, 1.0f, GuidanceMode::assisted);

        Environment half_environment = base;
        half_environment.set_physical_guidance_authority(0.5f);
        const auto half = effective_policy_action_with_unguided_baseline(
            half_environment, assisted_candidate, raw,
            CourseStage::crouch_walk, 1.0f, GuidanceMode::assisted);

        Environment zero_environment = base;
        zero_environment.set_physical_guidance_authority(0.0f);
        const auto zero = effective_policy_action_with_unguided_baseline(
            zero_environment, assisted_candidate, raw,
            CourseStage::crouch_walk, 1.0f, GuidanceMode::assisted);

        Environment raw_environment = base;
        raw_environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
        raw_environment.set_physical_guidance_authority(1.0f);
        const auto raw_mode = effective_policy_action_with_unguided_baseline(
            raw_environment, assisted_candidate, raw,
            CourseStage::crouch_walk, 1.0f,
            GuidanceMode::raw_policy_audit);

        bool full_differs{};
        for (std::size_t index = 0; index < action_count; ++index)
        {
            require(zero[index] == raw_mode[index],
                "zero Crouch authority did not equal Raw bounded/masked action");
            require(near(half[index], lerp(raw_mode[index], full[index], 0.5f)),
                "mid Crouch authority was not the single final action blend");
            full_differs = full_differs || full[index] != raw_mode[index];
        }
        require(full_differs,
            "full Crouch authority did not retain an Assisted action target");
        const auto repeated = effective_policy_action_with_unguided_baseline(
            half_environment, assisted_candidate, raw,
            CourseStage::crouch_walk, 1.0f, GuidanceMode::assisted);
        require(repeated == half,
            "Crouch final action boundary is not exactly repeatable");
    }

    void test_direct_posture_fade()
    {
        using namespace runner::sim;
        Environment base{ CreatureBlueprint::humanoid(), 0x751C003u };
        base.set_course(CourseStage::crouch_walk, 0.30f);
        base.set_course_motion_enabled(false);
        EnvironmentTestAccess::set_elapsed(base, 2.0f);
        const auto before = positions(base);

        Environment full = base;
        full.set_physical_guidance_authority(1.0f);
        EnvironmentTestAccess::apply_duck_posture(full);
        const auto full_after = positions(full);

        Environment half = base;
        half.set_physical_guidance_authority(0.5f);
        EnvironmentTestAccess::apply_duck_posture(half);
        const auto half_after = positions(half);

        Environment zero = base;
        zero.set_physical_guidance_authority(0.0f);
        EnvironmentTestAccess::apply_duck_posture(zero);
        const auto zero_after = positions(zero);

        Environment raw = base;
        raw.set_guidance_mode(GuidanceMode::raw_policy_audit);
        raw.set_physical_guidance_authority(1.0f);
        EnvironmentTestAccess::apply_duck_posture(raw);
        const auto raw_after = positions(raw);

        const std::array<std::size_t, 9> directly_guided_nodes{
            0u, 1u, 2u, 7u, 8u, 9u, 10u, 11u, 12u };
        for (const std::size_t node : directly_guided_nodes)
        {
            const float full_delta = full_after[node].y - before[node].y;
            const float half_delta = half_after[node].y - before[node].y;
            require(full_delta < -0.001f,
                "full Crouch posture guide did not move a declared body node");
            require(near(half_delta, full_delta * 0.5f),
                "Crouch root/torso/head/manipulator translation did not fade linearly");
            require(zero_after[node].x == before[node].x
                    && zero_after[node].y == before[node].y
                    && raw_after[node].x == before[node].x
                    && raw_after[node].y == before[node].y,
                "zero/Raw Crouch posture guide changed a body node");
        }
    }

    void test_state_retirement()
    {
        using namespace runner::sim;
        Environment environment{ CreatureBlueprint::humanoid(), 0x751C004u };
        environment.set_course(CourseStage::crouch_walk, 0.30f);
        EnvironmentTestAccess::seed_crouch_state(environment);
        const auto initial = EnvironmentTestAccess::crouch_state(environment);
        environment.set_physical_guidance_authority(0.5f);
        const auto half = EnvironmentTestAccess::crouch_state(environment);
        require(half[0] == initial[0] && half[1] == initial[1],
            "partial Crouch authority retired the selected side early");
        for (std::size_t index = 2u; index < half.size(); ++index)
            require(near(half[index], initial[index] * 0.5f),
                "live Crouch timer/command did not shrink with authority");

        environment.set_physical_guidance_authority(0.75f);
        require(EnvironmentTestAccess::crouch_state(environment) == half,
            "raising Crouch authority expanded already-live helper state");
        environment.set_physical_guidance_authority(0.0f);
        for (const float value : EnvironmentTestAccess::crouch_state(environment))
            require(value == 0.0f,
                "zero Crouch authority retained helper state");

        EnvironmentTestAccess::seed_crouch_state(environment);
        environment.set_physical_guidance_authority(1.0f);
        environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
        for (const float value : EnvironmentTestAccess::crouch_state(environment))
            require(value == 0.0f,
                "Raw transition retained Crouch helper state");
    }

    void test_zero_matches_raw_repeated_seed()
    {
        using namespace runner::sim;
        Environment base{ CreatureBlueprint::humanoid(), 0x751C005u };
        base.set_course(CourseStage::crouch_walk, 0.30f);
        base.set_course_motion_enabled(false);
        Environment assisted_zero = base;
        assisted_zero.set_guidance_mode(GuidanceMode::assisted);
        assisted_zero.set_physical_guidance_authority(0.0f);
        Environment assisted_repeat = assisted_zero;
        Environment raw = base;
        raw.set_guidance_mode(GuidanceMode::raw_policy_audit);
        raw.set_physical_guidance_authority(1.0f);

        std::array<float, action_count> action{};
        for (std::size_t index = 0; index < anatomy_action_count; ++index)
            action[index] = index % 2u == 0u ? 0.22f : -0.17f;
        for (int frame = 0; frame < 48; ++frame)
        {
            const StepResult zero_result = assisted_zero.step(action);
            const StepResult repeat_result = assisted_repeat.step(action);
            const StepResult raw_result = raw.step(action);
            require(zero_result.reward == raw_result.reward
                    && zero_result.forward_speed == raw_result.forward_speed
                    && zero_result.forward_odometer_progress
                        == raw_result.forward_odometer_progress
                    && zero_result.terminated == raw_result.terminated
                    && zero_result.invalid_reason == raw_result.invalid_reason
                    && repeat_result.reward == zero_result.reward,
                "zero/Raw Crouch fixed-step receipts diverged");
            require_same_physics(assisted_zero, raw,
                "zero Crouch authority did not preserve Raw physical identity");
            require_same_physics(assisted_zero, assisted_repeat,
                "same-seed zero-authority Crouch step was not exactly repeatable");
            if (zero_result.terminated)
                break;
        }
    }

    void test_stand_posture_authority()
    {
        using namespace runner;
        using namespace runner::sim;
        const std::array rigs{
            CreatureBlueprint::humanoid(), CreatureBlueprint::chicken(),
            CreatureBlueprint::crawler4(), CreatureBlueprint::hexapod()
        };
        for (const auto& rig : rigs)
        {
            for (const std::uint64_t seed : { 0x7515001ULL, 0x7515002ULL,
                    0x7515003ULL, 0x7515004ULL })
            {
                Environment base{ rig, seed };
                base.set_course(CourseStage::balance, 0.25f);
                base.set_guidance_mode(GuidanceMode::assisted);
                EnvironmentTestAccess::offset_standing_body(base);
                const auto before = positions(base);
                Environment full = base;
                full.set_physical_guidance_authority(1.0f);
                EnvironmentTestAccess::apply_standing_posture(full);
                const auto full_after = positions(full);
                require(length(full_after[rig.root_node] - before[rig.root_node]) > 0.0f,
                    "Stand posture fixture did not exercise root guidance");

                for (const float authority : { 0.0f, 0.25f, 0.5f, 1.0f })
                {
                    Environment scaled = base;
                    scaled.set_physical_guidance_authority(authority);
                    Environment repeated = scaled;
                    EnvironmentTestAccess::apply_standing_posture(scaled);
                    EnvironmentTestAccess::apply_standing_posture(repeated);
                    const auto after = positions(scaled);
                    for (std::size_t node = 0; node < before.size(); ++node)
                    {
                        const Vec2 expected = (full_after[node] - before[node]) * authority;
                        const Vec2 actual = after[node] - before[node];
                        require(near(actual.x, expected.x)
                                && near(actual.y, expected.y),
                            "Stand posture correction ignored or squared its authority");
                        const Vec2 expected_history = (full.particles()[node].previous
                            - base.particles()[node].previous) * authority;
                        const Vec2 actual_history = scaled.particles()[node].previous
                            - base.particles()[node].previous;
                        require(near(actual_history.x, expected_history.x)
                                && near(actual_history.y, expected_history.y),
                            "Stand posture history did not fade with position");
                    }
                    require_same_physics(scaled, repeated,
                        "same-seed Stand posture correction is not repeatable");
                    const auto report = rl::guidance_authority_report(scaled,
                        CourseStage::balance, 1.0f);
                    require(report.posture_guide == authority,
                        "Stand posture report disagrees with applied authority");
                    if (authority == 0.0f)
                        require_same_physics(base, scaled,
                            "zero Stand posture authority changed physics");
                }

                for (const float invalid : { -1.0f,
                        std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity() })
                {
                    Environment rejected = base;
                    rejected.set_physical_guidance_authority(invalid);
                    EnvironmentTestAccess::apply_standing_posture(rejected);
                    require_same_physics(base, rejected,
                        "invalid Stand authority still applied body support");
                }
                Environment raw = base;
                raw.set_guidance_mode(GuidanceMode::raw_policy_audit);
                raw.set_physical_guidance_authority(1.0f);
                Environment before_raw = raw;
                EnvironmentTestAccess::apply_standing_posture(raw);
                require_same_physics(before_raw, raw,
                    "Raw Stand invoked authored body support");
            }
        }
    }

    void test_duck_press_separation()
    {
        using namespace runner::sim;
        Environment base{ CreatureBlueprint::humanoid(), 0x751C006u };
        base.set_course(CourseStage::duck_press, 0.30f);
        EnvironmentTestAccess::set_elapsed(base, 2.0f);
        Environment full = base;
        full.set_physical_guidance_authority(1.0f);
        EnvironmentTestAccess::apply_duck_posture(full);
        Environment zero = base;
        zero.set_physical_guidance_authority(0.0f);
        EnvironmentTestAccess::apply_duck_posture(zero);
        require_same_physics(full, zero,
            "CrouchWalk scalar changed DuckPress physical semantics");
    }
}

int main()
{
    try
    {
        static_assert(runner::sim::observation_count == 84u);
        static_assert(runner::sim::action_count == 15u);
        test_authority_scalar();
        test_authority_schedules();
        test_action_boundary();
        test_direct_posture_fade();
        test_state_retirement();
        test_zero_matches_raw_repeated_seed();
        test_stand_posture_authority();
        test_duck_press_separation();
        std::cout << "v0.7.51 Stand posture and CrouchWalk authority tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "v0.7.51 CrouchWalk physical-authority test failed: "
            << exception.what() << '\n';
        return 1;
    }
}
