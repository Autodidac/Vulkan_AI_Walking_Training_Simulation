#include "autonomy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <thread>

namespace runner::rl
{
    bool AutonomousTrainer::stage_mastered_locked() const noexcept
    {
        const TrainingMetrics& metrics = worker_.metrics();
        if (!metrics.evaluation_valid || metrics.evaluation_quality_key == 0u)
            return false;
        switch (stage_)
        {
        case sim::CourseStage::balance:
            return metrics.evaluation_longest_stance >= 3.0f
                && metrics.evaluation_survival >= 3.0f
                && metrics.evaluation_max_joint_speed <= 12.0f;
        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 1.0f
                && metrics.evaluation_stable_stance >= 1.0f
                && metrics.evaluation_duck_seconds >= 1.0f
                && metrics.evaluation_obstacles_passed >= 1.0f
                && metrics.evaluation_survival >= 8.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_jump_landings >= 2.0f
                && metrics.evaluation_powered_jumps >= 2.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 5.0f
                && metrics.evaluation_stride_events >= 4.0f
                && metrics.evaluation_speed >= 0.65f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_obstacles_passed >= 2.0f
                && (metrics.evaluation_jump_landings >= 1.0f
                    || metrics.evaluation_duck_seconds >= 0.75f);
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_spin_landings >= 1.0f
                && metrics.evaluation_spin_turns >= 0.85f
                && metrics.evaluation_spin_turns <= 3.05f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 9.0f
                && metrics.evaluation_obstacles_passed >= 2.0f
                && metrics.evaluation_collisions <= 4.0f;
        }
        return false;
    }

    void AutonomousTrainer::manage_curriculum_locked()
    {
        const TrainingMetrics& metrics = worker_.metrics();
        if (metrics.evaluation_count == 0 || metrics.evaluation_count == last_evaluation_count_)
            return;
        last_evaluation_count_ = metrics.evaluation_count;

        if (worker_.has_best_policy() && metrics.best_update != last_saved_best_update_)
        {
            last_saved_best_update_ = metrics.best_update;
            queue_autosave();
        }

        mastery_streak_ = stage_mastered_locked() ? mastery_streak_ + 1 : 0;
        if (worker_.has_best_policy() && metrics.evaluation_valid)
        {
            const float tolerance = std::max(0.35f, std::abs(metrics.best_evaluation_score) * 0.35f);
            degradation_streak_ = metrics.evaluation_score + tolerance < metrics.best_evaluation_score
                ? degradation_streak_ + 1 : 0;
            if (degradation_streak_ >= 2 && worker_.restore_best_policy())
            {
                ++rollback_count_;
                degradation_streak_ = 0;
                worker_message_ = "PERFORMANCE DROPPED - RESTORED BEST VALID WALKER";
            }
        }

        if (!metrics.evaluation_valid)
        {
            worker_message_ = std::format("INVALID RUN REJECTED - {} / {}",
                metrics.evaluation_invalid_runs,
                primary_motion_rejection_name(metrics.evaluation_rejection_mask));
        }
        else if (mastery_streak_ >= 3)
        {
            advance_stage_locked();
        }
        else if (stage_ != sim::CourseStage::balance && metrics.evaluation_count % 4 == 0)
        {
            attempt_rig_evolution_locked();
        }
        else
        {
            worker_message_ = std::format("{} - MASTERY {}/3", sim::course_stage_name(stage_), mastery_streak_);
        }
    }

    void AutonomousTrainer::advance_stage_locked()
    {
        mastery_streak_ = 0;
        degradation_streak_ = 0;
        if (stage_ != sim::CourseStage::moving_hazards)
        {
            stage_ = static_cast<sim::CourseStage>(static_cast<std::uint8_t>(stage_) + 1u);
            difficulty_ = 0.30f;
            worker_message_ = std::format("LESSON COMPLETE - ADVANCING TO {}", sim::course_stage_name(stage_));
        }
        else
        {
            difficulty_ = std::min(1.0f, difficulty_ + 0.10f);
            worker_message_ = std::format("FULL COURSE MASTERED - DIFFICULTY {:.0f}%", difficulty_ * 100.0f);
        }
        worker_.set_course(stage_, difficulty_, false);
        queue_autosave();
    }

    float AutonomousTrainer::evaluate_rig_locked(const sim::CreatureBlueprint& candidate) const
    {
        if (!candidate.valid())
            return -std::numeric_limits<float>::infinity();

        constexpr std::size_t agents = 4;
        const sim::CourseStage stage = stage_;
        const float difficulty = difficulty_;
        const int maximum_steps = static_cast<std::uint8_t>(stage)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 1500 : 900;
        std::array<float, agents> scores{};
        std::array<std::jthread, agents> evaluators{};

        for (std::size_t agent = 0; agent < agents; ++agent)
        {
            evaluators[agent] = std::jthread([this, &candidate, &scores, stage, difficulty, maximum_steps, agent]
            {
                const std::uint64_t seed = 0xA100u + static_cast<std::uint64_t>(agent) * 3253u;
                sim::Environment environment{ candidate, seed };
                environment.set_course(stage, difficulty);
                float reward = 0.0f;
                for (int step = 0; step < maximum_steps; ++step)
                {
                    const auto raw_action = worker_.policy().deterministic_action(
                        environment.observation());
                    const auto action = effective_policy_action(
                        environment, raw_action, stage);
                    const sim::StepResult result = environment.step(action);
                    reward += result.reward;
                    if (result.terminated)
                        break;
                }
                const StageMotionQualification qualification =
                    stage_motion_qualification(stage, environment);
                if (!qualification.valid)
                {
                    scores[agent] = -std::numeric_limits<float>::infinity();
                    return;
                }
                scores[agent] = reward + environment.distance_travelled() * 0.75f
                    + environment.elapsed_seconds() * 0.03f
                    + static_cast<float>(environment.alternating_steps()) * 0.03f
                    + environment.duck_seconds() * 0.06f
                    + static_cast<float>(environment.landed_jumps()) * 0.16f
                    + std::min(environment.maximum_spin_turns(), 3.0f) * 0.18f
                    + static_cast<float>(environment.obstacles_passed()) * 0.28f
                    - environment.collision_count() * 0.10f
                    - environment.airborne_ratio() * 0.20f
                    - environment.body_rolling_seconds() * 2.00f;
            });
        }
        for (std::jthread& evaluator : evaluators)
        {
            if (evaluator.joinable())
                evaluator.join();
        }

        float total = 0.0f;
        for (const float score : scores)
        {
            if (!std::isfinite(score))
                return -std::numeric_limits<float>::infinity();
            total += score;
        }
        return total / static_cast<float>(agents);
    }

    sim::CreatureBlueprint AutonomousTrainer::mutate_rig_locked() noexcept
    {
        sim::CreatureBlueprint candidate = worker_.blueprint();
        const float direction = (rig_generation_ & 1u) == 0u ? 1.0f : -1.0f;
        const std::uint64_t mutation = rig_generation_ % 5u;
        if (mutation == 0u)
        {
            const std::size_t pair = static_cast<std::size_t>((rig_generation_ / 2u) % 2u);
            for (const std::size_t index : { pair, pair + 2u })
            {
                if (index < candidate.motors.size())
                    candidate.motors[index].strength = clamp(candidate.motors[index].strength
                        + direction * 0.0020f, 0.025f, 0.10f);
            }
        }
        else if (mutation == 1u)
        {
            const float delta = direction * 1.25f * pi / 180.0f;
            const std::size_t pair = static_cast<std::size_t>((rig_generation_ / 2u) % 2u);
            for (const std::size_t index : { pair, pair + 2u })
            {
                if (index >= candidate.motors.size())
                    continue;
                sim::MotorConstraint& motor = candidate.motors[index];
                motor.minimum_angle -= delta;
                motor.maximum_angle += delta;
                motor.minimum_angle = std::min(motor.minimum_angle, motor.neutral_angle - 2.0f * pi / 180.0f);
                motor.maximum_angle = std::max(motor.maximum_angle, motor.neutral_angle + 2.0f * pi / 180.0f);
            }
        }
        else if (mutation == 2u)
        {
            const float delta = direction * 0.015f;
            if (candidate.left_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.left_contact_node].x -= delta;
            if (candidate.right_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.right_contact_node].x += delta;
        }
        else if (mutation == 3u)
        {
            const float delta = direction * 0.015f;
            if (candidate.torso_node < candidate.nodes.size())
                candidate.nodes[candidate.torso_node].y = clamp(candidate.nodes[candidate.torso_node].y + delta, 0.40f, 6.0f);
            if (candidate.head_node < candidate.nodes.size())
                candidate.nodes[candidate.head_node].y = clamp(candidate.nodes[candidate.head_node].y + delta, 0.45f, 6.5f);
        }
        else
        {
            const float delta = direction * 0.012f;
            for (std::size_t motor_index = 0; motor_index < candidate.active_motor_count; ++motor_index)
            {
                const sim::MotorConstraint& motor = candidate.motors[motor_index];
                if (motor.pivot < candidate.nodes.size() && motor.pivot != candidate.root_node)
                    candidate.nodes[motor.pivot].y = clamp(candidate.nodes[motor.pivot].y + delta, 0.20f, 5.5f);
            }
        }

        std::array<float, sim::action_count> negative{};
        std::array<float, sim::action_count> positive{};
        std::array<float, sim::action_count> power{};
        for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
        {
            negative[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].neutral_angle - candidate.motors[index].minimum_angle);
            positive[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].maximum_angle - candidate.motors[index].neutral_angle);
            power[index] = candidate.motors[index].strength;
        }
        candidate.rebuild_rest_lengths();
        for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
        {
            sim::MotorConstraint& motor = candidate.motors[index];
            motor.neutral_angle = candidate.rest_joint_angle(index);
            motor.minimum_angle = motor.neutral_angle - negative[index];
            motor.maximum_angle = motor.neutral_angle + positive[index];
            motor.strength = power[index];
        }
        return candidate;
    }

    void AutonomousTrainer::attempt_rig_evolution_locked()
    {
        const float baseline = evaluate_rig_locked(worker_.blueprint());
        sim::CreatureBlueprint candidate = mutate_rig_locked();
        const float candidate_score = evaluate_rig_locked(candidate);
        const float required_gain = std::max(0.025f, std::abs(baseline) * 0.01f);
        ++rig_generation_;
        if (std::isfinite(candidate_score) && candidate_score > baseline + required_gain)
        {
            worker_.set_blueprint(candidate, true);
            worker_.set_course(stage_, difficulty_, false);
            ++accepted_rig_changes_;
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            worker_message_ = std::format("RIG GENERATION {} ACCEPTED  {:+.3f} VALID SCORE",
                rig_generation_, candidate_score - baseline);
            queue_autosave();
        }
        else
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format("RIG GENERATION {} REJECTED - NO VALID IMPROVEMENT", rig_generation_);
        }
    }
}
