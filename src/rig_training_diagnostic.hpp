#pragma once

#include "simulation.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace runner::diagnostics
{
    struct RigTrainingResult
    {
        std::string_view name{};
        float mean_episode_distance{};
        float evaluation_distance{};
        float evaluation_stride_events{};
        std::uint32_t evaluation_invalid_runs{};
        std::uint64_t preview_resets{};
        sim::InvalidMotion preview_reset_reason{ sim::InvalidMotion::none };
        bool rollout_course_motion_enabled{};
    };

    struct RigTrainingReport
    {
        std::array<RigTrainingResult, 4> rigs{};
        std::uint64_t updates{};
        bool passed{};
    };

    [[nodiscard]] RigTrainingReport run_rig_training_diagnostic(
        std::uint64_t updates = 100u);
}
