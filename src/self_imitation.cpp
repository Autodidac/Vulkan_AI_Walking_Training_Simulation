#include "ppo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace runner::rl
{
    void PpoTrainer::clear_self_imitation_prior() noexcept
    {
        self_imitation_prior_.clear();
        self_imitation_source_score_ = -std::numeric_limits<float>::infinity();
        metrics_.imitation_samples = 0;
        metrics_.imitation_weight = 0.0f;
        metrics_.imitation_source_score = -std::numeric_limits<float>::infinity();
    }

    void PpoTrainer::clear_foundational_teacher_prior() noexcept
    {
        foundational_teacher_prior_.clear();
    }

    void PpoTrainer::refresh_foundational_teacher_prior()
    {
        clear_foundational_teacher_prior();
        const bool fragile_support_topology = blueprint_.monopedal_gait()
            || blueprint_.avian_gait();
        if (course_stage_ != sim::CourseStage::uneven
            || !fragile_support_topology)
            return;

        constexpr std::size_t candidate_agents = 6u;
        constexpr std::size_t samples_per_agent = 256u;
        constexpr int maximum_steps = 1200;
        const float required_distance = blueprint_.monopedal_gait() ? 5.0f : 10.0f;
        const std::uint32_t required_cycles = blueprint_.monopedal_gait() ? 8u : 14u;
        foundational_teacher_prior_.reserve(candidate_agents * samples_per_agent);

        for (std::size_t agent = 0; agent < candidate_agents; ++agent)
        {
            sim::Environment environment{ blueprint_, 0x7380u + agent * 4099u };
            environment.set_course(course_stage_, course_difficulty_);
            environment.set_course_motion_enabled(false);
            std::vector<ImitationSample> trajectory{};
            trajectory.reserve(static_cast<std::size_t>(maximum_steps));
            for (int step = 0; step < maximum_steps; ++step)
            {
                ImitationSample sample{};
                sample.observation = environment.observation();
                sample.action = walking_teacher_action(environment);
                const sim::StepResult result = environment.step(sample.action);
                const bool clean_demonstration_frame = environment.valid_motion()
                    && !environment.non_foot_grounded()
                    && environment.uprightness() > 0.62f
                    && environment.body_rolling_seconds() < 0.08f
                    && environment.foot_pivot_rolling_seconds() < 0.08f;
                if (clean_demonstration_frame)
                    trajectory.push_back(sample);
                if (result.terminated)
                    break;
            }

            const bool complete_clean_teacher = environment.valid_motion()
                && environment.elapsed_seconds() >= 19.9f
                && environment.distance_travelled() >= required_distance
                && environment.gait_cycles() >= required_cycles;
            if (!complete_clean_teacher || trajectory.empty())
                continue;

            const std::size_t stride = std::max<std::size_t>(1u,
                (trajectory.size() + samples_per_agent - 1u) / samples_per_agent);
            std::size_t retained{};
            for (std::size_t index = 0;
                index < trajectory.size() && retained < samples_per_agent;
                index += stride, ++retained)
            {
                foundational_teacher_prior_.push_back(trajectory[index]);
            }
        }
    }

    void PpoTrainer::refresh_self_imitation_prior()
    {
        if (best_parameters_.size() != policy_.parameter_count())
        {
            clear_self_imitation_prior();
            return;
        }

        constexpr std::size_t candidate_agents = 6;
        constexpr std::size_t maximum_prior_samples = 512;
        const int maximum_steps = static_cast<std::uint8_t>(course_stage_)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 2400 : 1200;
        PolicyNetwork teacher{ 0x51E17Eu };
        teacher.parameters() = best_parameters_;
        std::vector<ImitationSample> best_trajectory{};
        float best_score = -std::numeric_limits<float>::infinity();
        std::uint64_t best_quality = 0u;
        const bool strict_source =
            strict_evaluation_quality(metrics_.best_quality_key);

        for (std::size_t agent = 0; agent < candidate_agents; ++agent)
        {
            sim::Environment environment{ blueprint_, 0xB500u + agent * 4099u };
            environment.set_course(course_stage_, course_difficulty_);
            std::vector<ImitationSample> trajectory{};
            trajectory.reserve(static_cast<std::size_t>(maximum_steps));
            float reward = 0.0f;
            for (int step = 0; step < maximum_steps; ++step)
            {
                ImitationSample sample{};
                sample.observation = environment.observation();
                const auto raw_action = teacher.deterministic_action(sample.observation);
                sample.action = effective_policy_action(
                    environment, raw_action, course_stage_,
                    lesson_teacher_authority(
                        lesson_update_, course_stage_, environment.blueprint()));
                const sim::StepResult result = environment.step(sample.action);
                reward += result.reward;
                const bool clean_demonstration_frame = environment.valid_motion()
                    && !environment.non_foot_grounded()
                    && environment.uprightness() > 0.70f
                    && environment.body_rolling_seconds() < 0.08f
                    && environment.foot_pivot_rolling_seconds() < 0.08f;
                if (clean_demonstration_frame)
                    trajectory.push_back(sample);
                if (result.terminated)
                    break;
            }

            const StageMotionQualification qualification =
                stage_motion_qualification(course_stage_, environment);
            const bool incremental_source = !strict_source
                && incremental_locomotion_candidate(course_stage_, environment);
            if (!qualification.valid && !incremental_source)
                continue;

            const std::uint64_t trajectory_quality = qualification.valid
                ? strict_evaluation_quality_bit | qualification.quality_key
                : pack_quality(
                    quality_bucket(environment.distance_travelled()),
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.gait_cycles(), 65535u)),
                    quality_bucket(environment.elapsed_seconds()),
                    quality_bucket(environment.primary_support_span_ratio()));

            const float score = reward + environment.distance_travelled() * 0.75f
                + environment.elapsed_seconds() * 0.025f
                + static_cast<float>(environment.alternating_steps()) * 0.03f
                + environment.duck_seconds() * 0.08f
                + static_cast<float>(environment.landed_jumps()) * 0.20f
                + std::min(environment.maximum_spin_turns(), 3.0f) * 0.25f
                + static_cast<float>(environment.obstacles_passed()) * 0.35f
                - environment.collision_count() * 0.10f
                - environment.airborne_ratio() * 0.20f;
            if (!trajectory.empty()
                && (trajectory_quality > best_quality
                    || (trajectory_quality == best_quality && score > best_score)))
            {
                best_quality = trajectory_quality;
                best_score = score;
                best_trajectory = std::move(trajectory);
            }
        }

        if (best_trajectory.empty())
        {
            clear_self_imitation_prior();
            return;
        }

        self_imitation_prior_.clear();
        const std::size_t stride = std::max<std::size_t>(1,
            (best_trajectory.size() + maximum_prior_samples - 1u) / maximum_prior_samples);
        for (std::size_t index = 0; index < best_trajectory.size(); index += stride)
            self_imitation_prior_.push_back(best_trajectory[index]);
        if (self_imitation_prior_.size() > maximum_prior_samples)
            self_imitation_prior_.resize(maximum_prior_samples);

        self_imitation_source_score_ = best_score;
        metrics_.imitation_samples = static_cast<std::uint32_t>(self_imitation_prior_.size());
        metrics_.imitation_source_score = best_score;
        metrics_.imitation_weight = self_imitation_prior_weight(0, self_imitation_prior_.size());
    }

    void PpoTrainer::apply_self_imitation_prior()
    {
        if (self_imitation_prior_.empty())
        {
            metrics_.imitation_weight = 0.0f;
            return;
        }

        constexpr std::size_t samples_per_batch = 32;
        const std::uint64_t age = metrics_.update >= metrics_.best_update
            ? metrics_.update - metrics_.best_update : 0u;
        const float weight = self_imitation_prior_weight(age, self_imitation_prior_.size());
        const std::size_t count = std::min(samples_per_batch, self_imitation_prior_.size());
        const std::size_t offset = static_cast<std::size_t>(
            (metrics_.update * 131u + adam_.step * 17u) % self_imitation_prior_.size());
        const std::size_t sample_stride = std::max<std::size_t>(1,
            self_imitation_prior_.size() / count);

        for (std::size_t index = 0; index < count; ++index)
        {
            const ImitationSample& sample = self_imitation_prior_[
                (offset + index * sample_stride) % self_imitation_prior_.size()];
            const PolicyNetwork::Evaluation evaluation = policy_.evaluate(sample.observation);
            const float old_log_probability = policy_.log_probability(sample.action, evaluation);
            float ignored_policy_loss = 0.0f;
            float ignored_value_loss = 0.0f;
            float ignored_entropy = 0.0f;
            policy_.accumulate_gradient(sample.observation, sample.action,
                old_log_probability, weight, evaluation.value,
                0.20f, 0.0f, 0.0f,
                ignored_policy_loss, ignored_value_loss, ignored_entropy);
        }

        metrics_.imitation_weight = weight;
        metrics_.imitation_samples = static_cast<std::uint32_t>(self_imitation_prior_.size());
        metrics_.imitation_source_score = self_imitation_source_score_;
    }
}
