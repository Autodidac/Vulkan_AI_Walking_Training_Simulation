#pragma once

#include "autonomy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace runner::telemetry
{
    enum class Tone : std::uint8_t
    {
        information,
        caution,
        success,
        danger
    };

    enum class LearningState : std::uint8_t
    {
        starting,
        training_normally,
        testing_current_policy,
        valid_attempt_found,
        improving_best_result,
        retrying_after_failed_test,
        trying_fresh_policy,
        paused,
        lesson_mastered
    };

    struct LessonProgress
    {
        float updates{};
        float attempts{};
        float tests{};
        float training_work{};
        float overall{};
        float mastery{};
        bool sample_budget_complete{};
    };

    struct StatusSummary
    {
        LearningState state{ LearningState::starting };
        Tone tone{ Tone::information };
        std::string_view headline{ "STARTING" };
        std::string_view explanation{
            "The trainer is collecting its first examples. No result is expected yet." };
    };

    [[nodiscard]] constexpr float completion_ratio(std::uint64_t completed,
        std::uint64_t required) noexcept
    {
        if (required == 0u)
            return 1.0f;
        return std::clamp(static_cast<float>(completed)
            / static_cast<float>(required), 0.0f, 1.0f);
    }

    [[nodiscard]] inline LessonProgress lesson_progress(
        const rl::AutonomyStatus& status) noexcept
    {
        LessonProgress result{};
        result.updates = completion_ratio(status.stage_fresh_updates,
            status.stage_required_updates);
        result.attempts = completion_ratio(status.stage_fresh_episodes,
            status.stage_required_episodes);
        result.tests = completion_ratio(status.stage_fresh_evaluations,
            status.stage_required_evaluations);
        result.training_work = std::min({
            result.updates, result.attempts, result.tests });
        result.mastery = completion_ratio(
            static_cast<std::uint64_t>(std::max(0, status.mastery_streak)),
            static_cast<std::uint64_t>(rl::required_mastery_confirmations(status.stage)));
        result.overall = std::clamp(
            result.training_work * 0.80f + result.mastery * 0.20f,
            0.0f, 1.0f);
        if (result.mastery < 1.0f)
            result.overall = std::min(result.overall, 0.99f);
        result.sample_budget_complete = result.training_work >= 1.0f;
        return result;
    }

    [[nodiscard]] constexpr bool contains(std::string_view text,
        std::string_view token) noexcept
    {
        return text.find(token) != std::string_view::npos;
    }

    [[nodiscard]] inline LearningState learning_state(
        const rl::AutonomyStatus& status, const rl::TrainingMetrics& metrics,
        bool background_enabled, bool has_best_policy,
        std::string_view controller_state) noexcept
    {
        if (!background_enabled)
            return LearningState::paused;
        const int required = rl::required_mastery_confirmations(status.stage);
        if ((required > 0 && status.mastery_streak >= required)
            || contains(status.message, "SKILL LOCKED")
            || contains(status.message, "FULL COURSE MASTERED"))
            return LearningState::lesson_mastered;
        if (contains(status.pipeline_stage, "EVALUAT")
            || contains(status.pipeline_stage, "TEST"))
            return LearningState::testing_current_policy;
        if (contains(status.message, "FRESH POLICY")
            || contains(status.message, "NURSERY BUDGET")
            || controller_state == "FRESH")
            return LearningState::trying_fresh_policy;
        if (metrics.total_updates == 0u
            || (metrics.evaluation_count == 0u
                && status.stage_fresh_updates < 12u))
            return LearningState::starting;
        if (metrics.evaluation_valid && has_best_policy
            && metrics.best_update > 0u && metrics.update >= metrics.best_update
            && metrics.update - metrics.best_update <= 5u)
            return LearningState::improving_best_result;
        if (metrics.evaluation_valid)
            return LearningState::valid_attempt_found;
        if (metrics.evaluation_count > 0u)
            return LearningState::retrying_after_failed_test;
        return LearningState::training_normally;
    }

    [[nodiscard]] constexpr StatusSummary summarize(LearningState state) noexcept
    {
        switch (state)
        {
        case LearningState::starting:
            return { state, Tone::information, "STARTING",
                "The trainer is collecting its first examples. No result is expected yet." };
        case LearningState::training_normally:
            return { state, Tone::information, "TRAINING NORMALLY",
                "Many simulations are running. The controller is still changing and searching for a better attempt." };
        case LearningState::testing_current_policy:
            return { state, Tone::information, "TESTING CURRENT POLICY",
                "Training pauses briefly while the current controller is checked on repeatable test seeds." };
        case LearningState::valid_attempt_found:
            return { state, Tone::success, "VALID ATTEMPT FOUND",
                "The latest test passed the current safety and skill checks. Repeat confirmations are still required." };
        case LearningState::improving_best_result:
            return { state, Tone::success, "IMPROVING BEST RESULT",
                "A better valid controller was retained. Training continues from that result instead of discarding it." };
        case LearningState::retrying_after_failed_test:
            return { state, Tone::caution, "RETRYING AFTER A FAILED TEST",
                "The latest test missed a requirement. The attempt was rejected, but saved training totals were not lost." };
        case LearningState::trying_fresh_policy:
            return { state, Tone::caution, "TRYING A FRESH POLICY",
                "The previous controller used its learning budget without a safe result. A fresh controller is being tried; totals remain." };
        case LearningState::paused:
            return { state, Tone::information, "TRAINING PAUSED",
                "The current controller and all saved progress are being held. Resume training when ready." };
        case LearningState::lesson_mastered:
            return { state, Tone::success, "LESSON MASTERED",
                "The required behavior passed enough repeat tests. The trainer can advance or raise the difficulty." };
        }
        return {};
    }

    [[nodiscard]] inline StatusSummary status_summary(
        const rl::AutonomyStatus& status, const rl::TrainingMetrics& metrics,
        bool background_enabled, bool has_best_policy,
        std::string_view controller_state) noexcept
    {
        return summarize(learning_state(status, metrics, background_enabled,
            has_best_policy, controller_state));
    }

    [[nodiscard]] constexpr std::string_view latest_test_title(
        const rl::TrainingMetrics& metrics) noexcept
    {
        if (metrics.evaluation_count == 0u)
            return "LATEST TEST: WAITING FOR FIRST TEST";
        return metrics.evaluation_valid
            ? "LATEST TEST: PASSED"
            : "LATEST TEST: NOT YET PASSED";
    }

    [[nodiscard]] constexpr std::string_view missing_skill_reason(
        sim::CourseStage stage) noexcept
    {
        switch (stage)
        {
        case sim::CourseStage::balance:
            return "It needs a longer stable upright stance.";
        case sim::CourseStage::duck_press:
            return "It needs a real crouch, hold, and recovery back to standing.";
        case sim::CourseStage::uneven:
            return "It needs more real alternating steps with natural leg crossing.";
        case sim::CourseStage::crouch_walk:
            return "It needs more controlled crouched steps while staying low.";
        case sim::CourseStage::ramps:
            return "It needs a powered jump followed by a safe landing.";
        case sim::CourseStage::hurdles:
            return "It needs to clear the obstacle with a controlled jump or duck.";
        case sim::CourseStage::duck_bars:
            return "It needs a controlled flip followed by a supported landing.";
        case sim::CourseStage::moving_hazards:
            return "It needs to avoid a hazard and keep moving under control.";
        }
        return "It has not shown enough of the lesson skill yet.";
    }

    [[nodiscard]] constexpr std::string_view rejection_reason(
        std::uint32_t mask, sim::CourseStage stage) noexcept
    {
        using Failure = rl::MotionEvidenceFailure;
        if ((mask & rl::evidence_bit(Failure::body_contact)) != 0u)
            return "Its body touched the ground when only valid supports were allowed.";
        if ((mask & rl::evidence_bit(Failure::invalid_motion)) != 0u)
            return "The motion broke a safety rule, so this attempt was not kept.";
        if ((mask & rl::evidence_bit(Failure::non_neutral_posture)) != 0u)
            return "Its arms or standing posture moved outside the allowed neutral range.";
        if ((mask & rl::evidence_bit(Failure::excessive_rotation)) != 0u)
            return "It spun too much instead of holding a controlled stance.";
        if ((mask & rl::evidence_bit(Failure::invalid_crouch_posture)) != 0u)
            return "It bent at the hips instead of making a real supported crouch.";
        if ((mask & rl::evidence_bit(Failure::lateral_crab_gait)) != 0u)
            return "Its legs did not cross in a natural forward walking pattern.";
        if ((mask & rl::evidence_bit(Failure::no_stable_stance)) != 0u)
            return "It could not hold stable balance long enough.";
        if ((mask & rl::evidence_bit(Failure::missing_recovery)) != 0u)
            return "It did not recover back to stable support after the challenge.";
        if ((mask & rl::evidence_bit(Failure::missing_skill)) != 0u)
            return missing_skill_reason(stage);
        if ((mask & rl::evidence_bit(Failure::missing_progress)) != 0u)
            return "It did not travel far enough to prove useful movement.";
        if ((mask & rl::evidence_bit(Failure::unstable_joints)) != 0u)
            return "Its joint motion was too violent to count as controlled movement.";
        return "One or more repeat test runs ended before every requirement was met.";
    }

    [[nodiscard]] constexpr std::string_view latest_test_explanation(
        const rl::TrainingMetrics& metrics, sim::CourseStage stage) noexcept
    {
        if (metrics.evaluation_count == 0u)
            return "No full test has finished yet. Training can still be making normal progress.";
        if (metrics.evaluation_valid)
            return "This attempt met the current safety and skill checks.";
        return rejection_reason(metrics.evaluation_rejection_mask, stage);
    }

    [[nodiscard]] constexpr std::string_view stage_goal(
        sim::CourseStage stage) noexcept
    {
        switch (stage)
        {
        case sim::CourseStage::balance:
            return "Stay upright for 6 seconds across repeat test seeds without body contact or excessive spin.";
        case sim::CourseStage::duck_press:
            return "Crouch under pressure, hold it, then stand back up on valid supports.";
        case sim::CourseStage::uneven:
            return "Walk at least 18 m with 16 real stride events, useful speed, and controlled balance.";
        case sim::CourseStage::crouch_walk:
            return "Stay crouched for 3.5 seconds, take 8 stride events, pass 4 obstacles, and recover upright.";
        case sim::CourseStage::ramps:
            return "Complete and safely land 3 powered jumps while continuing forward movement.";
        case sim::CourseStage::hurdles:
            return "Travel 8 m, pass 3 obstacles, and clear them with controlled jumping or ducking.";
        case sim::CourseStage::duck_bars:
            return "Complete 2 powered jumps and land 2 controlled flips between 0.85 and 3 turns.";
        case sim::CourseStage::moving_hazards:
            return "Travel 11 m, make 8 stride events, pass 4 hazards, and keep collisions controlled.";
        }
        return "Complete the current lesson safely and repeatedly.";
    }

    [[nodiscard]] constexpr std::string_view sample_budget_message(
        const LessonProgress& progress) noexcept
    {
        if (progress.sample_budget_complete)
            return "Training samples are ready. The remaining 20% comes from repeat mastery tests.";
        if (progress.updates <= progress.attempts && progress.updates <= progress.tests)
            return "More controller updates are needed before this lesson can be judged fairly.";
        if (progress.attempts <= progress.tests)
            return "More completed simulation attempts are needed before this lesson can be judged fairly.";
        return "More repeat tests are needed before mastery can be confirmed.";
    }

    [[nodiscard]] constexpr Tone latest_test_tone(
        const rl::TrainingMetrics& metrics) noexcept
    {
        if (metrics.evaluation_count == 0u)
            return Tone::information;
        return metrics.evaluation_valid ? Tone::success : Tone::caution;
    }

    [[nodiscard]] inline bool raw_score_available(float score) noexcept
    {
        return std::isfinite(score);
    }

    [[nodiscard]] constexpr std::string_view total_updates_help() noexcept
    {
        return "TOTAL RIG UPDATES = completed learning cycles for the selected rig. It never resets during episode or policy retries for the same rig; selecting a different rig starts at zero.";
    }

    [[nodiscard]] constexpr std::string_view attempts_help() noexcept
    {
        return "SIMULATED RUNS are completed parallel simulated runs. PASSED STAGE CHECKS means a run proved the current skill safely; FAILED STAGE CHECKS did not.";
    }

    [[nodiscard]] constexpr std::string_view reset_help() noexcept
    {
        return "RESETS restart an episode or weak policy; ROLLBACKS restore a better retained controller. Rig totals stay for the selected rig and clear when a different rig is selected.";
    }
}
