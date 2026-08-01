#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>
#include <span>
#include <string_view>

namespace epochrunner::rl
{
    const PolicyNetwork::Layout PolicyNetwork::layout_ = PolicyNetwork::make_layout();

    namespace
    {
        constexpr float log_two_pi = 1.83787706640934548356f;
        constexpr float epsilon = 1.0e-8f;
        constexpr float minimum_log_standard_deviation = -4.0f;
        constexpr float maximum_log_standard_deviation = -1.51412773f;

        [[nodiscard]] float tanh_derivative_from_output(float value) noexcept
        {
            return 1.0f - value * value;
        }
    }

    PolicyNetwork::PolicyNetwork()
        : PolicyNetwork(0xBADC0FFEEULL)
    {
    }

    PolicyNetwork::PolicyNetwork(std::uint64_t seed)
        : parameters_(layout_.total, 0.0f), gradients_(layout_.total, 0.0f), random_state_(seed == 0 ? 1 : seed)
    {
        const float scale1 = std::sqrt(2.0f / static_cast<float>(input_size));
        const float scale2 = std::sqrt(2.0f / static_cast<float>(hidden_size));
        for (std::size_t index = 0; index < hidden_size * input_size; ++index)
            parameters_[layout_.w1 + index] = random_normal() * scale1;
        for (std::size_t index = 0; index < hidden_size * hidden_size; ++index)
            parameters_[layout_.w2 + index] = random_normal() * scale2;
        for (std::size_t index = 0; index < output_size * hidden_size; ++index)
            parameters_[layout_.actor_w + index] = random_normal() * 0.0035f;
        for (std::size_t index = 0; index < hidden_size; ++index)
            parameters_[layout_.value_w + index] = random_normal() * 0.01f;
        for (std::size_t index = 0; index < output_size; ++index)
            parameters_[layout_.log_std + index] = std::log(0.08f);
    }

    float PolicyNetwork::random_normal() noexcept
    {
        auto next = [this]() noexcept
        {
            random_state_ ^= random_state_ >> 12;
            random_state_ ^= random_state_ << 25;
            random_state_ ^= random_state_ >> 27;
            const std::uint64_t value = random_state_ * 2685821657736338717ULL;
            return std::max(1.0e-7f, static_cast<float>(value >> 40) * (1.0f / 16777216.0f));
        };
        const float u1 = next();
        const float u2 = next();
        return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * pi * u2);
    }

    PolicyNetwork::Evaluation PolicyNetwork::evaluate(std::span<const float, input_size> observation) const noexcept
    {
        std::array<float, hidden_size> h1{};
        std::array<float, hidden_size> h2{};
        for (std::size_t row = 0; row < hidden_size; ++row)
        {
            float value = parameters_[layout_.b1 + row];
            const std::size_t base = layout_.w1 + row * input_size;
            for (std::size_t column = 0; column < input_size; ++column)
                value += parameters_[base + column] * observation[column];
            h1[row] = std::tanh(value);
        }
        for (std::size_t row = 0; row < hidden_size; ++row)
        {
            float value = parameters_[layout_.b2 + row];
            const std::size_t base = layout_.w2 + row * hidden_size;
            for (std::size_t column = 0; column < hidden_size; ++column)
                value += parameters_[base + column] * h1[column];
            h2[row] = std::tanh(value);
        }

        Evaluation result{};
        for (std::size_t output = 0; output < output_size; ++output)
        {
            float value = parameters_[layout_.actor_b + output];
            const std::size_t base = layout_.actor_w + output * hidden_size;
            for (std::size_t column = 0; column < hidden_size; ++column)
                value += parameters_[base + column] * h2[column];
            result.mean[output] = std::tanh(value);
        }
        result.value = parameters_[layout_.value_b];
        for (std::size_t column = 0; column < hidden_size; ++column)
            result.value += parameters_[layout_.value_w + column] * h2[column];
        return result;
    }

    std::array<float, PolicyNetwork::output_size> PolicyNetwork::deterministic_action(
        std::span<const float, input_size> observation) const noexcept
    {
        return evaluate(observation).mean;
    }

    std::array<float, PolicyNetwork::output_size> PolicyNetwork::standard_deviation() const noexcept
    {
        std::array<float, output_size> result{};
        for (std::size_t index = 0; index < output_size; ++index)
            result[index] = std::exp(std::clamp(parameters_[layout_.log_std + index],
                minimum_log_standard_deviation, maximum_log_standard_deviation));
        return result;
    }

    void PolicyNetwork::set_exploration(float standard_deviation) noexcept
    {
        const float value = std::log(clamp(standard_deviation, 0.02f, 0.22f));
        for (std::size_t index = 0; index < output_size; ++index)
            parameters_[layout_.log_std + index] = value;
    }

    float PolicyNetwork::mean_exploration() const noexcept
    {
        const auto values = standard_deviation();
        float total = 0.0f;
        for (const float value : values)
            total += value;
        return total / static_cast<float>(values.size());
    }

    float PolicyNetwork::log_probability(
        std::span<const float, output_size> action,
        const Evaluation& evaluation) const noexcept
    {
        float result = 0.0f;
        for (std::size_t index = 0; index < output_size; ++index)
        {
            const float log_std = std::clamp(parameters_[layout_.log_std + index],
                minimum_log_standard_deviation, maximum_log_standard_deviation);
            const float stddev = std::exp(log_std);
            const float normalized_delta = (action[index] - evaluation.mean[index]) / stddev;
            result += -0.5f * (normalized_delta * normalized_delta + 2.0f * log_std + log_two_pi);
        }
        return result;
    }

    void PolicyNetwork::zero_gradients() noexcept
    {
        std::fill(gradients_.begin(), gradients_.end(), 0.0f);
    }

    void PolicyNetwork::accumulate_gradient(
        std::span<const float, input_size> observation,
        std::span<const float, output_size> action,
        float old_log_probability,
        float advantage,
        float target_value,
        float clip_range,
        float value_coefficient,
        float entropy_coefficient,
        float& policy_loss,
        float& value_loss,
        float& entropy) noexcept
    {
        std::array<float, hidden_size> h1{};
        std::array<float, hidden_size> h2{};
        std::array<float, output_size> actor_pre_activation{};

        for (std::size_t row = 0; row < hidden_size; ++row)
        {
            float value = parameters_[layout_.b1 + row];
            const std::size_t base = layout_.w1 + row * input_size;
            for (std::size_t column = 0; column < input_size; ++column)
                value += parameters_[base + column] * observation[column];
            h1[row] = std::tanh(value);
        }
        for (std::size_t row = 0; row < hidden_size; ++row)
        {
            float value = parameters_[layout_.b2 + row];
            const std::size_t base = layout_.w2 + row * hidden_size;
            for (std::size_t column = 0; column < hidden_size; ++column)
                value += parameters_[base + column] * h1[column];
            h2[row] = std::tanh(value);
        }

        Evaluation evaluation{};
        for (std::size_t output = 0; output < output_size; ++output)
        {
            float value = parameters_[layout_.actor_b + output];
            const std::size_t base = layout_.actor_w + output * hidden_size;
            for (std::size_t column = 0; column < hidden_size; ++column)
                value += parameters_[base + column] * h2[column];
            actor_pre_activation[output] = value;
            evaluation.mean[output] = std::tanh(value);
        }
        evaluation.value = parameters_[layout_.value_b];
        for (std::size_t column = 0; column < hidden_size; ++column)
            evaluation.value += parameters_[layout_.value_w + column] * h2[column];

        const float new_log_probability = log_probability(action, evaluation);
        const float ratio = std::exp(std::clamp(new_log_probability - old_log_probability, -12.0f, 12.0f));
        const float clipped_ratio = std::clamp(ratio, 1.0f - clip_range, 1.0f + clip_range);
        const float unclipped_objective = ratio * advantage;
        const float clipped_objective = clipped_ratio * advantage;
        const bool unclipped_selected = unclipped_objective <= clipped_objective;
        const float selected_objective = std::min(unclipped_objective, clipped_objective);
        const float d_loss_d_log_probability = unclipped_selected ? -advantage * ratio : 0.0f;

        const float value_error = evaluation.value - target_value;
        const float d_loss_d_value = 2.0f * value_coefficient * value_error;
        policy_loss += -selected_objective;
        value_loss += value_error * value_error;

        std::array<float, output_size> d_actor_pre{};
        for (std::size_t output = 0; output < output_size; ++output)
        {
            const float log_std = std::clamp(parameters_[layout_.log_std + output],
                minimum_log_standard_deviation, maximum_log_standard_deviation);
            const float variance = std::exp(2.0f * log_std);
            const float delta = action[output] - evaluation.mean[output];
            const float d_logp_d_mean = delta / variance;
            const float d_logp_d_log_std = delta * delta / variance - 1.0f;
            d_actor_pre[output] = d_loss_d_log_probability * d_logp_d_mean
                * tanh_derivative_from_output(evaluation.mean[output]);
            gradients_[layout_.log_std + output] += d_loss_d_log_probability * d_logp_d_log_std - entropy_coefficient;
            entropy += log_std + 0.5f * (1.0f + log_two_pi);
        }

        std::array<float, hidden_size> d_h2{};
        for (std::size_t output = 0; output < output_size; ++output)
        {
            const std::size_t base = layout_.actor_w + output * hidden_size;
            for (std::size_t column = 0; column < hidden_size; ++column)
            {
                gradients_[base + column] += d_actor_pre[output] * h2[column];
                d_h2[column] += parameters_[base + column] * d_actor_pre[output];
            }
            gradients_[layout_.actor_b + output] += d_actor_pre[output];
        }

        for (std::size_t column = 0; column < hidden_size; ++column)
        {
            gradients_[layout_.value_w + column] += d_loss_d_value * h2[column];
            d_h2[column] += parameters_[layout_.value_w + column] * d_loss_d_value;
        }
        gradients_[layout_.value_b] += d_loss_d_value;

        std::array<float, hidden_size> d_h1{};
        for (std::size_t row = 0; row < hidden_size; ++row)
        {
            const float d_pre = d_h2[row] * tanh_derivative_from_output(h2[row]);
            gradients_[layout_.b2 + row] += d_pre;
            const std::size_t base = layout_.w2 + row * hidden_size;
            for (std::size_t column = 0; column < hidden_size; ++column)
            {
                gradients_[base + column] += d_pre * h1[column];
                d_h1[column] += parameters_[base + column] * d_pre;
            }
        }

        for (std::size_t row = 0; row < hidden_size; ++row)
        {
            const float d_pre = d_h1[row] * tanh_derivative_from_output(h1[row]);
            gradients_[layout_.b1 + row] += d_pre;
            const std::size_t base = layout_.w1 + row * input_size;
            for (std::size_t column = 0; column < input_size; ++column)
                gradients_[base + column] += d_pre * observation[column];
        }
    }

    bool PolicyNetwork::save(const std::filesystem::path& path, std::string& error) const
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open policy file for writing: " + path.string();
            return false;
        }
        constexpr std::array<char, 8> magic{ 'E', 'P', 'P', 'O', '2', '3', '\0', '\1' };
        const std::uint64_t count = parameters_.size();
        output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        output.write(reinterpret_cast<const char*>(parameters_.data()), static_cast<std::streamsize>(parameters_.size() * sizeof(float)));
        if (!output)
        {
            error = "Failed while writing policy file: " + path.string();
            return false;
        }
        error.clear();
        return true;
    }

    bool PolicyNetwork::load(const std::filesystem::path& path, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Could not open policy file: " + path.string();
            return false;
        }
        std::array<char, 8> magic{};
        std::uint64_t count{};
        input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        input.read(reinterpret_cast<char*>(&count), sizeof(count));
        constexpr std::array<char, 8> expected{ 'E', 'P', 'P', 'O', '2', '3', '\0', '\1' };
        if (!input || magic != expected || count != parameters_.size())
        {
            error = "Invalid or incompatible EpochRunner policy file.";
            return false;
        }
        input.read(reinterpret_cast<char*>(parameters_.data()), static_cast<std::streamsize>(parameters_.size() * sizeof(float)));
        if (!input)
        {
            error = "Truncated policy file.";
            return false;
        }
        error.clear();
        return true;
    }

}
