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
                    environment, raw_action, course_stage_);
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
            if (!qualification.valid)
                continue;

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
                && (qualification.quality_key > best_quality
                    || (qualification.quality_key == best_quality && score > best_score)))
            {
                best_quality = qualification.quality_key;
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
