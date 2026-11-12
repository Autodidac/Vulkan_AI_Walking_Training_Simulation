#pragma once

#include "simulation.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace runner::rl
{
    struct TrainingMetrics;
}

namespace runner::diagnostics
{
    inline constexpr std::array<std::uint64_t, 6> raw_inference_replay_seeds{
        0x75100001u, 0x75101002u, 0x75102003u,
        0x75103004u, 0x75104005u, 0x75105006u
    };
    inline constexpr std::uint32_t raw_stop_replay_frames = 1200u;
    inline constexpr float raw_stop_maximum_horizontal_drift = 1.0f;
    inline constexpr float raw_stop_maximum_horizontal_speed = 0.20f;

    struct RawInferenceSeedEvidence
    {
        std::uint64_t seed{};
        float requested_direction{ 1.0f };
        float distance{};
        std::uint32_t gait_cycles{};
        std::uint32_t alternating_steps{};
        float pelvis_delta_mean{};
        float pelvis_delta_peak{};
        float center_delta_mean{};
        float center_delta_peak{};
        float torso_turn_rate_peak{};
        float support_lead_peak{};
        float stance_slip_peak{};
        float friction_work{};
        float friction_positive_work{};
        float internal_center_delta_peak{};
        float internal_positive_horizontal_energy{};
        float integration_positive_horizontal_energy{};
        std::uint32_t unsupported_forward_impulses{};
        std::uint32_t left_only_support_frames{};
        std::uint32_t right_only_support_frames{};
        std::uint32_t contact_transitions{};
        std::uint32_t raw_action_mismatches{};
        std::uint32_t command_telemetry_mismatches{};
        std::uint32_t actuator_transition_mismatches{};
        std::uint32_t motor_application_mismatches{};
        std::uint32_t helper_applications{};
        std::uint32_t root_drive_corrections{};
        std::uint32_t torso_drive_corrections{};
        std::uint32_t course_motion_corrections{};
        std::uint32_t powered_toe_commands{};
        std::uint32_t toe_constraint_applications{};
        std::uint32_t toe_rate_limit_applications{};
        std::uint32_t passive_anatomy_applications{};
        std::uint32_t action_support_release_requests{};
        std::uint32_t action_support_release_active_frames{};
        std::uint32_t traction_limit_violations{};
        std::array<float, sim::anatomy_action_count> raw_action_min{};
        std::array<float, sim::anatomy_action_count> raw_action_max{};
        std::array<float, sim::anatomy_action_count> effective_action_min{};
        std::array<float, sim::anatomy_action_count> effective_action_max{};
        std::array<float, sim::anatomy_action_count> actuator_action_min{};
        std::array<float, sim::anatomy_action_count> actuator_action_max{};
        std::array<float, sim::anatomy_action_count> motor_action_min{};
        std::array<float, sim::anatomy_action_count> motor_action_max{};
        std::array<std::uint32_t, sim::anatomy_action_count>
            command_mismatches_by_joint{};
        std::array<std::uint32_t, sim::anatomy_action_count>
            actuator_mismatches_by_joint{};
        std::array<std::uint32_t, sim::anatomy_action_count>
            motor_mismatches_by_joint{};
        sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        std::uint64_t command_trace_digest{};
        std::uint64_t contact_trace_digest{};
        std::uint64_t trajectory_fingerprint{};
        bool body_integrity{};
        bool deterministic_repeat{};
        bool stationary_course{};
        bool passed{};
    };

    struct RawStopSeedEvidence
    {
        std::uint64_t seed{};
        float requested_direction{};
        float maximum_horizontal_drift{};
        float maximum_horizontal_speed{};
        std::uint32_t completed_frames{};
        std::uint32_t gait_cycles{};
        std::uint32_t alternating_steps{};
        std::uint32_t non_foot_contact_frames{};
        std::uint32_t raw_action_mismatches{};
        std::uint32_t optional_authority_applications{};
        sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        std::uint64_t command_trace_digest{};
        std::uint64_t contact_trace_digest{};
        std::uint64_t trajectory_fingerprint{};
        bool stop_requested{};
        bool finite_motion{};
        bool body_integrity{};
        bool stationary_course{};
        bool deterministic_repeat{};
        bool passed{};
    };

    [[nodiscard]] bool raw_stop_seed_qualifies(
        const RawStopSeedEvidence& evidence) noexcept;

    struct RawCandidateEvaluationEvidence
    {
        bool present{};
        std::uint64_t update{};
        bool valid{};
        std::uint32_t invalid_runs{};
        sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        std::uint32_t rejection_mask{};
        std::string rejection_summary{};
        float distance{};
        float stride_events{};
        float survival{};
        std::uint64_t quality_key{};
    };

    [[nodiscard]] inline constexpr bool raw_candidate_evaluation_observed(
        std::uint64_t previous_evaluation_count,
        std::uint64_t current_evaluation_count,
        std::uint64_t candidate_update,
        std::uint64_t handoff_update) noexcept
    {
        return current_evaluation_count > previous_evaluation_count
            && candidate_update > handoff_update;
    }

    [[nodiscard]] RawCandidateEvaluationEvidence
        capture_raw_candidate_evaluation(
            std::uint64_t update,
            const rl::TrainingMetrics& metrics);

    struct RawInferenceReport
    {
        std::uint64_t training_seed{};
        std::uint64_t requested_updates{};
        std::uint64_t completed_updates{};
        std::uint64_t retained_update{};
        std::uint64_t handoff_update{};
        RawCandidateEvaluationEvidence first_raw_candidate{};
        RawCandidateEvaluationEvidence last_raw_candidate{};
        sim::GuidanceMode retained_guidance_mode{
            sim::GuidanceMode::assisted };
        std::array<RawInferenceSeedEvidence,
            raw_inference_replay_seeds.size()> seeds{};
        std::array<RawInferenceSeedEvidence,
            raw_inference_replay_seeds.size()> reverse_seeds{};
        std::array<RawStopSeedEvidence,
            raw_inference_replay_seeds.size()> stop_seeds{};
        std::array<float, sim::anatomy_action_count> action_variance{};
        std::array<float, sim::anatomy_action_count> lag_one_correlation{};
        float contralateral_leg_correlation{};
        float zero_control_max_distance{};
        float fixed_control_max_distance{};
        float weight_ablation_max_distance{};
        std::uint32_t zero_control_max_cycles{};
        std::uint32_t fixed_control_max_cycles{};
        std::uint32_t weight_ablation_max_cycles{};
        std::uint32_t zero_control_passes{};
        std::uint32_t fixed_control_passes{};
        std::uint32_t weight_ablation_passes{};
        std::uint64_t retained_parameter_fingerprint{};
        std::uint64_t replay_parameter_fingerprint{};
        std::uint64_t retained_parameter_digest{};
        std::uint64_t raw_receipt_digest{};
        std::uint64_t raw_receipt_optimizer_update{};
        std::uint64_t raw_receipt_sample_count{};
        bool retained_policy{};
        bool post_handoff_update{};
        bool raw_receipt_verified{};
        bool checkpoint_truthful{};
        bool raw_action_identity{};
        bool all_joint_outputs_vary{};
        bool opposing_action_phase{};
        bool opposing_support_phase{};
        bool reverse_direction_evidence{};
        bool stop_behavior_evidence{};
        bool no_optional_authority{};
        bool parameter_fingerprint_immutable{};
        bool weight_ablation_changes_trajectory{};
        bool controls_fail{};
        bool passed{};
    };

    [[nodiscard]] RawInferenceReport run_raw_inference_diagnostic(
        std::uint64_t updates = 1200u);
    void write_raw_inference_report(const RawInferenceReport& report,
        std::ostream& output);
}
