#include "locomotion_strategy.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
    using runner::locomotion::Intent;
    using runner::locomotion::Signals;

    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    Signals stable_flat()
    {
        Signals signals{};
        signals.uprightness = 0.98f;
        signals.root_x = 0.0f;
        signals.left_support_x = -0.28f;
        signals.right_support_x = 0.28f;
        signals.left_supported = true;
        signals.right_supported = true;
        signals.incoming_time_to_impact = 10.0f;
        signals.gait_cycles = 8u;
        return signals;
    }
}

int main()
{
    {
        Signals centered = stable_flat();
        Signals off_center = centered;
        off_center.root_x = 0.72f;
        require(runner::locomotion::balance_reserve(centered) > 0.90f,
            "centered two-foot stance should have high balance reserve");
        require(runner::locomotion::balance_reserve(centered)
                > runner::locomotion::balance_reserve(off_center),
            "moving the root outside the support interval must reduce reserve");
    }

    {
        Signals step = stable_flat();
        step.near_rise = 0.55f;
        step.forward_speed = 1.10f;
        const auto plan = runner::locomotion::plan(step);
        require(plan.step_up, "reachable positive terrain must request a step-up");
        require(plan.intent == Intent::walk,
            "step-up should preserve walking rather than force crawling or running");
        require(plan.target_speed < 0.50f,
            "step-up should deliberately reduce target speed");
        require(plan.swing_lift >= 0.70f,
            "step-up must raise swing-foot clearance");
        require(plan.brake, "fast approach to a step-up should request braking");
    }

    {
        Signals immature = stable_flat();
        immature.gait_cycles = 3u;
        const auto immature_plan = runner::locomotion::plan(immature);
        require(immature_plan.intent != Intent::run,
            "running must be unavailable before gait is established");

        Signals mature = stable_flat();
        mature.gait_cycles = 14u;
        const auto mature_plan = runner::locomotion::plan(mature);
        require(mature_plan.intent == Intent::run,
            "established gait on clear terrain should progress to running");
        require(mature_plan.target_speed > 1.60f,
            "run target speed must exceed walk speed");
    }

    {
        Signals threat = stable_flat();
        threat.gait_cycles = 14u;
        threat.incoming_density = 0.75f;
        threat.incoming_time_to_impact = 0.70f;
        threat.incoming_velocity_x = 4.0f;
        threat.free_space_direction = -1.0f;
        const auto plan = runner::locomotion::plan(threat);
        require(plan.intent == Intent::flee,
            "urgent threat should select flee behavior");
        require(plan.direction < 0.0f,
            "flee behavior should honor free space away from the threat");
        require(plan.target_speed > 1.30f,
            "flee behavior should accelerate when balance reserve permits");
    }

    {
        Signals clear_prone = stable_flat();
        clear_prone.uprightness = 0.25f;
        clear_prone.left_supported = false;
        clear_prone.right_supported = false;
        clear_prone.non_foot_grounded = true;
        clear_prone.free_space_direction = 1.0f;
        require(!runner::locomotion::plan(clear_prone).emergency_crawl,
            "clear terrain must not enable crawl as a walking shortcut");

        Signals trapped = clear_prone;
        trapped.burial_depth = 0.22f;
        trapped.obstruction_mask = 0x3u;
        const auto trapped_plan = runner::locomotion::plan(trapped);
        require(trapped_plan.emergency_crawl,
            "obstructed prone rig with escape space should be allowed to crawl");
        require(trapped_plan.intent == Intent::crawl,
            "emergency crawl must be an explicit last-resort intent");
    }

    {
        Signals unstable = stable_flat();
        unstable.uprightness = 0.34f;
        unstable.right_supported = false;
        unstable.root_x = 0.55f;
        unstable.forward_speed = 1.4f;
        const float reserve = runner::locomotion::balance_reserve(unstable);
        require(reserve < 0.38f,
            "regression fixture must actually represent depleted balance reserve");
        const auto plan = runner::locomotion::plan(unstable);
        require(plan.intent == Intent::recover,
            "depleted balance reserve should select recovery before speed");
        require(plan.brake, "recovery should brake rather than push forward");
        require(plan.target_speed <= 0.11f,
            "recovery target should allow near-stationary balance correction");
    }

    {
        Signals flat = stable_flat();
        flat.gait_cycles = 14u;
        const auto plan = runner::locomotion::plan(flat);
        const float at_target = runner::locomotion::target_speed_reward(
            plan, plan.target_speed);
        const float overspeed = runner::locomotion::target_speed_reward(
            plan, plan.target_speed * 2.0f);
        require(at_target > overspeed,
            "target-speed reward must prefer controlled speed over overspeed");
    }

    std::cout << "Runner v0.7.19 general locomotion strategy tests passed\n";
    return EXIT_SUCCESS;
}
