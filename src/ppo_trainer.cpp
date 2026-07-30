#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace epochrunner::rl
{
    namespace
    {
        constexpr float epsilon = 1.0e-8f;
    }

    PpoTrainer::PpoTrainer(const sim::CreatureBlueprint& blueprint, std::size_t environment_count)
        : blueprint_(blueprint), preview_(blueprint, 0xDEADBEEFu), policy_(0xC0FFEEu)
    {
        environment_count = std::clamp<std::size_t>(environment_count, 4, 256);
        environments_.reserve(environment_count);
        for (std::size_t index = 0; index < environment_count; ++index)
            environments_.emplace_back(blueprint_, 0x1000u + index * 7919u);
        episode_rewards_.assign(environment_count, 0.0f);
        episode_distances_.assign(environment_count, 0.0f);
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
    }

    void PpoTrainer::set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy)
    {
        const bool changed = blueprint.signature() != blueprint_.signature();
        blueprint_ = blueprint;
        for (sim::Environment& environment : environments_)
            environment.set_blueprint(blueprint_);
        preview_.set_blueprint(blueprint_);
        if (!changed)
            return;

        if (preserve_policy)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
        }
        else
        {
            reset_policy();
        }
    }

    void PpoTrainer::reset_training_state(bool clear_best) noexcept
    {
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        metrics_ = {};
        reward_history_.clear();
        speed_history_.clear();
        if (clear_best)
            best_parameters_.clear();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (std::size_t index = 0; index < environments_.size(); ++index)
            environments_[index].reset(0x1000u + index * 7919u);
        preview_.reset(0xDEADBEEFu);
    }

    void PpoTrainer::reset_policy(std::uint64_t seed)
    {
        policy_ = PolicyNetwork(seed);
        reset_training_state();
        controller_state_ = ControllerState::fresh;
    }

    void PpoTrainer::set_exploration(float standard_deviation) noexcept
    {
        policy_.set_exploration(standard_deviation);
    }

    std::string_view PpoTrainer::controller_state_name() const noexcept
    {
        switch (controller_state_)
        {
        case ControllerState::fresh: return "FRESH";
        case ControllerState::training: return "TRAINING";
        case ControllerState::resumed: return "RESUMED";
        case ControllerState::transferred: return "TRANSFERRED";
        }
        return "UNKNOWN";
    }

    float PpoTrainer::random_uniform() noexcept
    {
        random_state_ ^= random_state_ >> 12;
        random_state_ ^= random_state_ << 25;
        random_state_ ^= random_state_ >> 27;
        const std::uint64_t value = random_state_ * 2685821657736338717ULL;
        return std::max(1.0e-7f, static_cast<float>(value >> 40) * (1.0f / 16777216.0f));
    }

    float PpoTrainer::random_normal() noexcept
    {
        const float u1 = random_uniform();
        const float u2 = random_uniform();
        return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * pi * u2);
    }

    std::array<float, sim::action_count> PpoTrainer::sample_action(
        const PolicyNetwork::Evaluation& evaluation,
        float& log_probability) noexcept
    {
        const auto stddev = policy_.standard_deviation();
        std::array<float, sim::action_count> action{};
        for (std::size_t index = 0; index < action.size(); ++index)
            action[index] = clamp(evaluation.mean[index] + stddev[index] * random_normal(), -1.0f, 1.0f);
        log_probability = policy_.log_probability(action, evaluation);
        return action;
    }

    void PpoTrainer::train_one_update()
    {
        constexpr std::size_t horizon = 128;
        constexpr float gamma = 0.995f;
        constexpr float gae_lambda = 0.95f;
        rollout_.clear();
        rollout_.resize(horizon * environments_.size());

        float accumulated_speed = 0.0f;
        float completed_reward = 0.0f;
        float completed_distance = 0.0f;
        std::size_t completed_episodes = 0;

        for (std::size_t step = 0; step < horizon; ++step)
        {
            for (std::size_t environment_index = 0; environment_index < environments_.size(); ++environment_index)
            {
                sim::Environment& environment = environments_[environment_index];
                Transition& transition = rollout_[step * environments_.size() + environment_index];
                transition.observation = environment.observation();
                const PolicyNetwork::Evaluation evaluation = policy_.evaluate(transition.observation);
                transition.value = evaluation.value;
                transition.action = sample_action(evaluation, transition.log_probability);
                const sim::StepResult result = environment.step(transition.action);
                transition.reward = result.reward;
                transition.terminal = result.terminated;
                episode_rewards_[environment_index] += result.reward;
                episode_distances_[environment_index] = environment.distance_travelled();
                accumulated_speed += result.forward_speed;
                if (result.terminated)
                {
                    completed_reward += episode_rewards_[environment_index];
                    completed_distance += episode_distances_[environment_index];
                    ++completed_episodes;
                    episode_rewards_[environment_index] = 0.0f;
                    episode_distances_[environment_index] = 0.0f;
                    environment.reset(0x100000u + metrics_.environment_steps + environment_index * 17u + step);
                }
            }
        }

        std::vector<float> next_values(environments_.size(), 0.0f);
        for (std::size_t environment_index = 0; environment_index < environments_.size(); ++environment_index)
            next_values[environment_index] = policy_.evaluate(environments_[environment_index].observation()).value;

        for (std::size_t environment_index = 0; environment_index < environments_.size(); ++environment_index)
        {
            float next_value = next_values[environment_index];
            float next_advantage = 0.0f;
            for (std::size_t reverse = horizon; reverse-- > 0;)
            {
                Transition& transition = rollout_[reverse * environments_.size() + environment_index];
                const float continuation = transition.terminal ? 0.0f : 1.0f;
                const float delta = transition.reward + gamma * next_value * continuation - transition.value;
                transition.advantage = delta + gamma * gae_lambda * continuation * next_advantage;
                transition.return_value = transition.advantage + transition.value;
                next_advantage = transition.advantage;
                next_value = transition.value;
            }
        }

        float mean_advantage = 0.0f;
        for (const Transition& transition : rollout_)
            mean_advantage += transition.advantage;
        mean_advantage /= static_cast<float>(rollout_.size());
        float variance = 0.0f;
        for (const Transition& transition : rollout_)
        {
            const float delta = transition.advantage - mean_advantage;
            variance += delta * delta;
        }
        const float inverse_std = 1.0f / std::sqrt(variance / static_cast<float>(rollout_.size()) + 1.0e-6f);
        for (Transition& transition : rollout_)
            transition.advantage = (transition.advantage - mean_advantage) * inverse_std;

        update_policy();
        ++metrics_.update;
        metrics_.environment_steps += rollout_.size();
        metrics_.mean_speed = accumulated_speed / static_cast<float>(rollout_.size());
        if (completed_episodes > 0)
        {
            metrics_.mean_reward = completed_reward / static_cast<float>(completed_episodes);
            metrics_.mean_episode_distance = completed_distance / static_cast<float>(completed_episodes);
        }
        else
        {
            float partial_reward = std::accumulate(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
            metrics_.mean_reward = partial_reward / static_cast<float>(episode_rewards_.size());
        }
        append_history(reward_history_, metrics_.mean_reward);
        append_history(speed_history_, metrics_.mean_speed * 3.6f);
        controller_state_ = ControllerState::training;
        if (metrics_.update == 1 || metrics_.update % 5 == 0)
            evaluate_policy();
    }

    void PpoTrainer::evaluate_policy()
    {
        constexpr std::size_t evaluation_agents = 4;
        constexpr int maximum_steps = 600;
        float reward_total = 0.0f;
        float distance_total = 0.0f;
        float speed_total = 0.0f;
        std::size_t speed_samples = 0;
        for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
        {
            sim::Environment environment{ blueprint_, 0xE000u + agent * 4099u };
            float episode_reward = 0.0f;
            for (int step = 0; step < maximum_steps; ++step)
            {
                const auto action = policy_.deterministic_action(environment.observation());
                const sim::StepResult result = environment.step(action);
                episode_reward += result.reward;
                speed_total += result.forward_speed;
                ++speed_samples;
                if (result.terminated)
                    break;
            }
            reward_total += episode_reward;
            distance_total += environment.distance_travelled();
        }
        metrics_.evaluation_reward = reward_total / static_cast<float>(evaluation_agents);
        metrics_.evaluation_distance = distance_total / static_cast<float>(evaluation_agents);
        metrics_.evaluation_speed = speed_samples > 0 ? speed_total / static_cast<float>(speed_samples) : 0.0f;
        ++metrics_.evaluation_count;
        if (best_parameters_.empty() || metrics_.evaluation_distance > metrics_.best_evaluation_distance)
        {
            best_parameters_ = policy_.parameters();
            metrics_.best_evaluation_distance = metrics_.evaluation_distance;
            metrics_.best_update = metrics_.update;
        }
    }

    bool PpoTrainer::restore_best_policy() noexcept
    {
        if (best_parameters_.size() != policy_.parameter_count())
            return false;
        policy_.parameters() = best_parameters_;
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        preview_.reset(0xDEADBEEFu + metrics_.update);
        controller_state_ = ControllerState::resumed;
        return true;
    }

    void PpoTrainer::update_policy()
    {
        constexpr std::size_t epochs = 4;
        constexpr std::size_t minibatch_size = 256;
        constexpr float clip_range = 0.20f;
        constexpr float value_coefficient = 0.50f;
        constexpr float entropy_coefficient = 0.0025f;
        constexpr float max_gradient_norm = 0.75f;

        std::vector<std::size_t> indices(rollout_.size());
        std::iota(indices.begin(), indices.end(), 0);
        float total_policy_loss = 0.0f;
        float total_value_loss = 0.0f;
        float total_entropy = 0.0f;
        std::size_t sample_count = 0;

        for (std::size_t epoch = 0; epoch < epochs; ++epoch)
        {
            for (std::size_t index = indices.size(); index > 1; --index)
            {
                const std::size_t other = static_cast<std::size_t>(random_uniform() * static_cast<float>(index));
                std::swap(indices[index - 1], indices[std::min(other, index - 1)]);
            }

            for (std::size_t begin = 0; begin < indices.size(); begin += minibatch_size)
            {
                const std::size_t end = std::min(indices.size(), begin + minibatch_size);
                policy_.zero_gradients();
                float batch_policy_loss = 0.0f;
                float batch_value_loss = 0.0f;
                float batch_entropy = 0.0f;
                for (std::size_t cursor = begin; cursor < end; ++cursor)
                {
                    const Transition& transition = rollout_[indices[cursor]];
                    policy_.accumulate_gradient(
                        transition.observation,
                        transition.action,
                        transition.log_probability,
                        transition.advantage,
                        transition.return_value,
                        clip_range,
                        value_coefficient,
                        entropy_coefficient,
                        batch_policy_loss,
                        batch_value_loss,
                        batch_entropy);
                }

                const float inverse_batch = 1.0f / static_cast<float>(end - begin);
                float norm_squared = 0.0f;
                for (const float gradient : policy_.gradients())
                {
                    const float scaled = gradient * inverse_batch;
                    norm_squared += scaled * scaled;
                }
                const float norm = std::sqrt(norm_squared);
                const float clip_scale = norm > max_gradient_norm ? max_gradient_norm / norm : 1.0f;
                const float learning_rate = metrics_.learning_rate * std::max(0.12f, 1.0f - static_cast<float>(metrics_.update) / 4000.0f);
                apply_adam(learning_rate, inverse_batch * clip_scale);

                total_policy_loss += batch_policy_loss;
                total_value_loss += batch_value_loss;
                total_entropy += batch_entropy;
                sample_count += end - begin;
            }
        }

        const float inverse_samples = sample_count > 0 ? 1.0f / static_cast<float>(sample_count) : 0.0f;
        metrics_.policy_loss = total_policy_loss * inverse_samples;
        metrics_.value_loss = total_value_loss * inverse_samples;
        metrics_.entropy = total_entropy * inverse_samples;
    }

    void PpoTrainer::apply_adam(float learning_rate, float gradient_scale)
    {
        constexpr float beta1 = 0.9f;
        constexpr float beta2 = 0.999f;
        ++adam_.step;
        const float bias1 = 1.0f - std::pow(beta1, static_cast<float>(adam_.step));
        const float bias2 = 1.0f - std::pow(beta2, static_cast<float>(adam_.step));
        std::vector<float>& parameters = policy_.parameters();
        const std::vector<float>& gradients = policy_.gradients();
        for (std::size_t index = 0; index < parameters.size(); ++index)
        {
            const float gradient = gradients[index] * gradient_scale;
            adam_.first_moment[index] = beta1 * adam_.first_moment[index] + (1.0f - beta1) * gradient;
            adam_.second_moment[index] = beta2 * adam_.second_moment[index] + (1.0f - beta2) * gradient * gradient;
            const float first = adam_.first_moment[index] / bias1;
            const float second = adam_.second_moment[index] / bias2;
            parameters[index] -= learning_rate * first / (std::sqrt(second) + epsilon);
        }
    }

    void PpoTrainer::step_preview(float dt)
    {
        const auto observation = preview_.observation();
        const auto action = policy_.deterministic_action(observation);
        if (preview_.step(action, dt).terminated)
            preview_.reset(0xDEADBEEFu + metrics_.update);
    }

    void PpoTrainer::reset_preview(std::uint64_t seed) noexcept
    {
        preview_.reset(seed);
    }

    void PpoTrainer::append_history(std::vector<float>& history, float value)
    {
        constexpr std::size_t maximum = 240;
        if (history.size() == maximum)
            history.erase(history.begin());
        history.push_back(value);
    }
}
