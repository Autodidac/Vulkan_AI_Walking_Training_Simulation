#include "ppo.hpp"
#include "rig_training_diagnostic.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.27 rig-training failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    void advance(runner::rl::PpoTrainer& trainer, int frames, float frame_dt)
    {
        for (int frame = 0; frame < frames; ++frame)
            trainer.step_preview(frame_dt);
    }

    void require_same_preview(const runner::rl::PpoTrainer& expected,
        const runner::rl::PpoTrainer& actual, std::string_view cadence)
    {
        require(expected.preview_reset_count() == actual.preview_reset_count(),
            "preview reset count depends on render cadence");
        require(expected.preview_last_reset_reason()
                == actual.preview_last_reset_reason(),
            "preview reset reason depends on render cadence");
        require(std::abs(expected.preview().elapsed_seconds()
                - actual.preview().elapsed_seconds()) < 1.0e-6f,
            "preview elapsed simulation time depends on render cadence");
        require(std::abs(expected.preview().distance_travelled()
                - actual.preview().distance_travelled()) < 1.0e-5f,
            "preview distance depends on render cadence");
        require(expected.preview().alternating_steps()
                == actual.preview().alternating_steps(),
            "preview gait evidence depends on render cadence");
        const auto expected_particles = expected.preview().particles();
        const auto actual_particles = actual.preview().particles();
        require(expected_particles.size() == actual_particles.size(),
            "preview particle count changed across cadence");
        for (std::size_t index = 0; index < expected_particles.size(); ++index)
        {
            const runner::sim::Particle& left = expected_particles[index];
            const runner::sim::Particle& right = actual_particles[index];
            const bool same = std::abs(left.position.x - right.position.x) < 1.0e-5f
                && std::abs(left.position.y - right.position.y) < 1.0e-5f
                && std::abs(left.previous.x - right.previous.x) < 1.0e-5f
                && std::abs(left.previous.y - right.previous.y) < 1.0e-5f;
            if (!same)
            {
                std::cerr << "cadence=" << cadence << " particle=" << index << '\n';
                require(false, "preview physics state depends on render cadence");
            }
        }
    }

    void verify_frame_independent_preview()
    {
        const runner::sim::CreatureBlueprint rig =
            runner::sim::CreatureBlueprint::quadruped();
        runner::rl::PpoTrainer at_60_hz{ rig, 1u, false };
        runner::rl::PpoTrainer at_20_hz{ rig, 1u, false };
        runner::rl::PpoTrainer at_240_hz{ rig, 1u, false };
        constexpr std::uint64_t seed = 0x727333u;
        at_60_hz.reset_preview(seed);
        at_20_hz.reset_preview(seed);
        at_240_hz.reset_preview(seed);
        advance(at_60_hz, 60, 1.0f / 60.0f);
        advance(at_20_hz, 20, 1.0f / 20.0f);
        advance(at_240_hz, 240, 1.0f / 240.0f);
        require_same_preview(at_60_hz, at_20_hz, "20 Hz");
        require_same_preview(at_60_hz, at_240_hz, "240 Hz");

        runner::rl::PpoTrainer partial{ rig, 1u, false };
        partial.reset_preview(seed);
        partial.step_preview(1.0f / 120.0f);
        require(partial.preview().elapsed_seconds() == 0.0f,
            "substep frame advanced preview physics");
        partial.reset_preview(seed);
        partial.step_preview(1.0f / 120.0f);
        require(partial.preview().elapsed_seconds() == 0.0f,
            "preview reset retained a partial fixed tick");
        partial.step_preview(1.0f / 120.0f);
        require(std::abs(partial.preview().elapsed_seconds() - 1.0f / 60.0f)
                < 1.0e-6f,
            "two half frames did not produce exactly one fixed tick");
        partial.step_preview(-1.0f);
        partial.step_preview(std::numeric_limits<float>::quiet_NaN());
        require(std::abs(partial.preview().elapsed_seconds() - 1.0f / 60.0f)
                < 1.0e-6f,
            "invalid frame delta changed preview physics");
    }
}

int main()
{
    verify_frame_independent_preview();
    const runner::diagnostics::RigTrainingReport report =
        runner::diagnostics::run_rig_training_diagnostic();
    for (const runner::diagnostics::RigTrainingResult& rig : report.rigs)
    {
        std::cout << rig.name << ": mean=" << rig.mean_episode_distance
            << " evaluation=" << rig.evaluation_distance
            << " strides=" << rig.evaluation_stride_events
            << " invalid=" << rig.evaluation_invalid_runs
            << " preview_resets=" << rig.preview_resets
            << " reason=" << runner::sim::invalid_motion_name(
                rig.preview_reset_reason) << '\n';
    }
    if (!report.passed)
    {
        std::cerr << "Runner v0.7.27 rig-training diagnostic failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Runner v0.7.27 rig-training and frame-independence checks passed\n";
    return EXIT_SUCCESS;
}
