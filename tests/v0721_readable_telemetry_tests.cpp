#include "training_explainer.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    using runner::rl::AutonomyStatus;
    using runner::rl::MotionEvidenceFailure;
    using runner::rl::TrainingMetrics;
    using runner::sim::CourseStage;
    using runner::telemetry::LearningState;
    using runner::telemetry::Tone;

    void require(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    AutonomyStatus walking_status()
    {
        AutonomyStatus status{};
        status.enabled = true;
        status.stage = CourseStage::uneven;
        status.stage_fresh_updates = 210u;
        status.stage_required_updates = 420u;
        status.stage_fresh_episodes = 4u;
        status.stage_required_episodes = 8u;
        status.stage_fresh_evaluations = 4u;
        status.stage_required_evaluations = 8u;
        status.pipeline_stage = "ROLLOUT";
        status.message = "3. WALK / RUN - STRICT MASTERY 0/8";
        return status;
    }

    TrainingMetrics trained_metrics()
    {
        TrainingMetrics metrics{};
        metrics.update = 300u;
        metrics.total_updates = 1450u;
        metrics.evaluation_count = 12u;
        metrics.evaluation_score = -1600.0f;
        metrics.best_evaluation_score = -820.0f;
        return metrics;
    }
}

int main()
{
    {
        AutonomyStatus status = walking_status();
        status.stage_fresh_updates = 105u;
        status.stage_fresh_episodes = 6u;
        status.stage_fresh_evaluations = 8u;
        const auto progress = runner::telemetry::lesson_progress(status);
        require(progress.updates == 0.25f,
            "update progress should use the current lesson requirement");
        require(progress.attempts == 0.75f,
            "attempt progress should use completed simulated episodes");
        require(progress.tests == 1.0f,
            "test progress should clamp at one");
        require(progress.overall == 0.25f,
            "lesson progress must be the conservative minimum");
        require(!progress.sample_budget_complete,
            "partial work must not report a complete sample budget");
        require(runner::telemetry::sample_budget_message(progress).find("updates")
                != std::string_view::npos,
            "the limiting work category should be explained");
    }

    {
        AutonomyStatus status = walking_status();
        TrainingMetrics metrics{};
        const auto summary = runner::telemetry::status_summary(status, metrics,
            true, false, "FRESH");
        require(summary.state == LearningState::trying_fresh_policy,
            "fresh controller state should be explicit");
        require(summary.tone == Tone::caution,
            "fresh-policy retry is caution, not catastrophic failure");
    }

    {
        AutonomyStatus status = walking_status();
        status.stage_fresh_updates = 2u;
        TrainingMetrics metrics{};
        const auto summary = runner::telemetry::status_summary(status, metrics,
            true, false, "TRAINING");
        require(summary.state == LearningState::starting,
            "no updates or tests should report startup");
    }

    {
        AutonomyStatus status = walking_status();
        TrainingMetrics metrics = trained_metrics();
        const auto summary = runner::telemetry::status_summary(status, metrics,
            true, false, "TRAINING");
        require(summary.state == LearningState::retrying_after_failed_test,
            "ordinary rejected evaluation should report a retry");
        require(summary.tone == Tone::caution,
            "ordinary rejected evaluation must not be painted as a fatal error");
        require(summary.explanation.find("not lost") != std::string_view::npos,
            "failed-test explanation should protect user trust in saved progress");
    }

    {
        AutonomyStatus status = walking_status();
        status.pipeline_stage = "EVALUATION";
        TrainingMetrics metrics = trained_metrics();
        const auto summary = runner::telemetry::status_summary(status, metrics,
            true, false, "TRAINING");
        require(summary.state == LearningState::testing_current_policy,
            "evaluation pipeline should be described as testing");
    }

    {
        AutonomyStatus status = walking_status();
        TrainingMetrics metrics = trained_metrics();
        metrics.evaluation_valid = true;
        metrics.best_update = metrics.update;
        const auto summary = runner::telemetry::status_summary(status, metrics,
            true, true, "TRAINING");
        require(summary.state == LearningState::improving_best_result,
            "newly retained valid controller should report improvement");
        require(summary.tone == Tone::success,
            "retained improvement should be green/successful");
    }

    {
        AutonomyStatus status = walking_status();
        TrainingMetrics metrics = trained_metrics();
        metrics.evaluation_valid = true;
        metrics.best_update = 100u;
        const auto summary = runner::telemetry::status_summary(status, metrics,
            true, true, "TRAINING");
        require(summary.state == LearningState::valid_attempt_found,
            "older champion plus valid current test should report valid evidence");
    }

    {
        AutonomyStatus status = walking_status();
        TrainingMetrics metrics = trained_metrics();
        const auto paused = runner::telemetry::status_summary(status, metrics,
            false, true, "RESUMED");
        require(paused.state == LearningState::paused,
            "paused training should outrank evaluation details");
        status.mastery_streak = runner::rl::required_mastery_confirmations(status.stage);
        const auto mastered = runner::telemetry::status_summary(status, metrics,
            true, true, "RESUMED");
        require(mastered.state == LearningState::lesson_mastered,
            "required confirmation streak should report mastery");
    }

    {
        TrainingMetrics metrics{};
        require(runner::telemetry::latest_test_title(metrics)
                == "LATEST TEST: WAITING FOR FIRST TEST",
            "startup should not display a fake failed evaluation");
        require(runner::telemetry::latest_test_tone(metrics) == Tone::information,
            "no test yet is informational");
        metrics.evaluation_count = 1u;
        metrics.evaluation_valid = false;
        metrics.evaluation_rejection_mask = runner::rl::evidence_bit(
            MotionEvidenceFailure::body_contact);
        require(runner::telemetry::latest_test_title(metrics)
                == "LATEST TEST: NOT YET PASSED",
            "rejected test should use non-catastrophic wording");
        require(runner::telemetry::latest_test_explanation(metrics,
                CourseStage::uneven).find("body touched") != std::string_view::npos,
            "body-contact failure should be translated into plain English");
        require(runner::telemetry::latest_test_tone(metrics) == Tone::caution,
            "rejected test should be caution, not danger");
    }

    {
        const std::uint32_t gait_mask = runner::rl::evidence_bit(
            MotionEvidenceFailure::missing_skill);
        require(runner::telemetry::rejection_reason(gait_mask,
                CourseStage::uneven).find("alternating steps") != std::string_view::npos,
            "walking skill failure should name the missing behavior");
        require(runner::telemetry::stage_goal(CourseStage::uneven).find("18 m")
                != std::string_view::npos,
            "walking goal should expose the actual distance requirement");
        require(runner::telemetry::stage_goal(CourseStage::moving_hazards)
                .find("11 m") != std::string_view::npos,
            "mixed-course goal should expose the actual distance requirement");
    }

    {
        require(!runner::telemetry::raw_score_available(
                -std::numeric_limits<float>::infinity()),
            "uninitialized negative infinity must be shown as unavailable");
        require(runner::telemetry::raw_score_available(-1600.0f),
            "finite raw diagnostics remain available on the advanced page");
        require(runner::telemetry::total_updates_help().find("never resets")
                != std::string_view::npos,
            "total updates help must explain persistence");
        require(runner::telemetry::reset_help().find("All-time totals stay")
                != std::string_view::npos,
            "reset help must explain that all-time work survives");
    }

    std::cout << "Runner v0.7.21 readable telemetry tests passed\n";
    return EXIT_SUCCESS;
}
