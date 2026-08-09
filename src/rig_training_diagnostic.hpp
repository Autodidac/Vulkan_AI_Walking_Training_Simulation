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
        float teacher_distance{};
        float teacher_stride_events{};
        float teacher_survival{};
        sim::InvalidMotion teacher_invalid_reason{ sim::InvalidMotion::none };
        float evaluation_distance{};
        float evaluation_stride_events{};
        std::uint32_t evaluation_rejection_mask{};
        std::uint32_t evaluation_invalid_runs{};
        sim::InvalidMotion evaluation_invalid_reason{ sim::InvalidMotion::none };
        float raw_policy_distance{};
        float raw_policy_stride_events{};
        std::uint32_t raw_policy_rejection_mask{};
        std::uint32_t raw_policy_invalid_runs{};
        sim::InvalidMotion raw_policy_invalid_reason{ sim::InvalidMotion::none };
        float teacher_authority{};
        bool retained_policy{};
        std::uint64_t retained_update{};
        float retained_distance{};
        std::uint64_t retained_quality{};
        float retained_probe_distance{};
        float retained_probe_stride_events{};
        std::uint32_t retained_probe_rejection_mask{};
        std::uint32_t retained_probe_invalid_runs{};
        sim::InvalidMotion retained_probe_invalid_reason{ sim::InvalidMotion::none };
        std::uint64_t preview_resets{};
        sim::InvalidMotion preview_reset_reason{ sim::InvalidMotion::none };
        bool rollout_course_motion_enabled{};
    };

    struct RigTrainingReport
    {
        std::array<RigTrainingResult, 5> rigs{};
        std::uint64_t updates{};
        bool passed{};
    };

    struct WalkEyeTestProof
    {
        sim::Environment environment{};
        std::uint64_t updates{};
        std::uint64_t retained_update{};
        float retained_distance{};
        float retained_stride_events{};
        std::uint32_t retained_invalid_runs{};
        std::uint32_t selected_seed{};
        float displayed_distance{};
        std::uint32_t displayed_steps{};
        std::uint32_t displayed_crossings{};
        float teacher_authority{};
        bool passed{};
    };
    [[nodiscard]] RigTrainingReport run_rig_training_diagnostic(
        std::uint64_t updates = 1200u);
    [[nodiscard]] WalkEyeTestProof run_walk_eye_test_proof(
        std::uint64_t updates = 1200u);
}
