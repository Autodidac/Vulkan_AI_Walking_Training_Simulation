#include "hybrid_brain_diagnostic.hpp"

#include "locomotion_strategy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace runner::diagnostics
{
    namespace
    {
        [[nodiscard]] locomotion::Signals stable_signals() noexcept
        {
            locomotion::Signals signals{};
            signals.uprightness = 0.92f;
            signals.left_supported = true;
            signals.right_supported = true;
            signals.left_support_x = -0.28f;
            signals.right_support_x = 0.28f;
            signals.gait_cycles = 18u;
            signals.requested_direction = 1.0f;
            return signals;
        }

        [[nodiscard]] bool sustained_multi_topology() noexcept
        {
            const std::array rigs{
                sim::CreatureBlueprint::quadruped(),
                sim::CreatureBlueprint::crawler4(),
                sim::CreatureBlueprint::hexapod()
            };
            constexpr std::array<std::uint64_t, 2> seeds{
                0x7300u, 0x9e3779b9u
            };
            for (const sim::CreatureBlueprint& rig : rigs)
            {
                for (const std::uint64_t seed : seeds)
                {
                    sim::Environment environment{ rig, seed };
                    environment.set_course(sim::CourseStage::uneven, 0.30f);
                    environment.set_course_motion_enabled(false);
                    for (int step = 0; step < 1200; ++step)
                    {
                        if (environment.step(
                                rl::walking_teacher_action(environment)).terminated)
                            break;
                    }
                    if (environment.invalid_reason() != sim::InvalidMotion::none
                        || environment.elapsed_seconds() < 19.9f
                        || environment.distance_travelled()
                            < rl::multi_support_release_distance(rig)
                        || environment.gait_cycles()
                            < static_cast<std::uint32_t>(
                                rl::multi_support_release_stride_events(rig)))
                        return false;
                }
            }
            return true;
        }
    }

    HybridBrainReport run_hybrid_brain_diagnostic()
    {
        HybridBrainReport report{};

        locomotion::Signals hole = stable_signals();
        hole.left_escape_rise = 0.24f;
        hole.right_escape_rise = 0.38f;
        hole.zero_progress_seconds = 0.75f;
        const locomotion::Plan escape = locomotion::plan(hole);
        report.hole_escape = escape.intent == locomotion::Intent::escape
            && escape.direction < 0.0f && escape.step_up && !escape.brake;

        locomotion::Signals falling = stable_signals();
        falling.incoming_density = 0.90f;
        falling.incoming_time_to_impact = 0.55f;
        falling.incoming_velocity_x = 2.0f;
        falling.free_space_direction = -1.0f;
        const locomotion::Plan dodge = locomotion::plan(falling);
        report.falling_dodge = dodge.intent == locomotion::Intent::flee
            && dodge.direction < 0.0f && dodge.target_speed > 1.3f;
        falling.requested_direction = 0.0f;
        falling.turning = false;
        report.falling_dodge = report.falling_dodge
            && locomotion::plan(falling).intent == locomotion::Intent::flee;
        falling.requested_direction = 1.0f;
        falling.turning = true;
        report.falling_dodge = report.falling_dodge
            && locomotion::plan(falling).intent == locomotion::Intent::flee;

        locomotion::Signals harmless = falling;
        harmless.incoming_density = 0.08f;
        harmless.incoming_time_to_impact = 3.0f;
        report.harmless_object =
            locomotion::plan(harmless).intent != locomotion::Intent::flee;

        locomotion::Signals blocked = stable_signals();
        blocked.burial_depth = 0.22f;
        blocked.obstruction_mask = 0x3u;
        blocked.left_escape_rise = 0.45f;
        blocked.right_escape_rise = 0.45f;
        blocked.zero_progress_seconds = 0.80f;
        const locomotion::Plan blocked_plan = locomotion::plan(blocked);
        report.blocked_exit = blocked_plan.intent == locomotion::Intent::escape
            && blocked_plan.direction < 0.0f
            && std::isfinite(blocked_plan.swing_lift);

        const rl::RuntimeSafetyAuthority escape_authority =
            rl::runtime_safety_authority(escape);
        const rl::RuntimeSafetyAuthority flee_authority =
            rl::runtime_safety_authority(dodge);
        const float saturated_policy = 1.0f;
        const float bounded_escape = lerp(saturated_policy, -1.0f,
            escape_authority.support);
        report.policy_bounds = escape_authority.support >= 0.70f
            && escape_authority.body > 0.0f
            && flee_authority.support > flee_authority.body
            && bounded_escape < 0.0f && bounded_escape >= -1.0f;

        report.multi_topology = sustained_multi_topology();

        report.frame_independent = true;
        for (const float hz : std::array{ 20.0f, 60.0f, 240.0f })
        {
            locomotion::Signals cadence = hole;
            cadence.zero_progress_seconds = 0.0f;
            const float dt = 1.0f / hz;
            for (int step = 0; step < static_cast<int>(hz * 0.75f); ++step)
                cadence.zero_progress_seconds += dt;
            const locomotion::Plan cadence_plan = locomotion::plan(cadence);
            report.frame_independent = report.frame_independent
                && cadence_plan.intent == locomotion::Intent::escape
                && cadence_plan.direction < 0.0f;
        }
        return report;
    }
}
