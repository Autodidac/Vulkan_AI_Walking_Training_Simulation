#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace runner::sim
{
    class DeformableTerrain
    {
    public:
        struct Cell
        {
            float height{};
            float rest_height{};
            float firmness{ 0.35f };
            float loose_fraction{ 0.65f };
        };

        static constexpr std::size_t cell_count = 224;
        static constexpr float cell_spacing = 0.25f;
        static constexpr float period = static_cast<float>(cell_count) * cell_spacing;

        void reset(std::uint64_t seed, float difficulty) noexcept
        {
            seed_ = seed == 0u ? 1u : seed;
            difficulty_ = std::clamp(difficulty, 0.0f, 1.0f);
            transfers_.fill(0.0f);
            for (std::size_t index = 0; index < cell_count; ++index)
            {
                const float course_x = static_cast<float>(index) * cell_spacing;
                const float base = authored_base_height(course_x, difficulty_);
                const float noise = (unit_hash(seed_ + static_cast<std::uint64_t>(index) * 0x9e3779b97f4a7c15ULL)
                    - 0.5f) * (0.035f + difficulty_ * 0.075f);
                Cell& cell = cells_[index];
                cell.height = std::max(-0.08f, base + noise);
                cell.rest_height = cell.height;
                cell.firmness = std::clamp(0.28f + unit_hash(seed_ ^ (static_cast<std::uint64_t>(index) * 0xbf58476d1ce4e5b9ULL))
                    * 0.30f, 0.18f, 0.72f);
                cell.loose_fraction = 1.0f - cell.firmness;
            }
        }

        [[nodiscard]] float height_at(float course_x) const noexcept
        {
            const Sample sample = sample_coordinates(course_x);
            return std::lerp(cells_[sample.first].height, cells_[sample.second].height,
                sample.fraction);
        }

        [[nodiscard]] float firmness_at(float course_x) const noexcept
        {
            const Sample sample = sample_coordinates(course_x);
            return std::clamp(std::lerp(cells_[sample.first].firmness,
                cells_[sample.second].firmness, sample.fraction), 0.0f, 1.0f);
        }

        [[nodiscard]] float looseness_at(float course_x) const noexcept
        {
            const Sample sample = sample_coordinates(course_x);
            return std::clamp(std::lerp(cells_[sample.first].loose_fraction,
                cells_[sample.second].loose_fraction, sample.fraction), 0.0f, 1.0f);
        }

        [[nodiscard]] float slope_at(float course_x) const noexcept
        {
            return (height_at(course_x + cell_spacing) - height_at(course_x - cell_spacing))
                / (cell_spacing * 2.0f);
        }

        void apply_pressure(float course_x, float normalized_load, float slip_speed,
            float dt) noexcept
        {
            const std::size_t center = nearest_index(course_x);
            Cell& cell = cells_[center];
            const float load = std::clamp(normalized_load, 0.0f, 4.0f);
            const float slip = std::clamp(std::abs(slip_speed), 0.0f, 5.0f);
            const float softness = std::clamp(1.0f - cell.firmness, 0.0f, 1.0f);
            const float displacement = std::min(0.018f,
                (load * 0.065f + slip * 0.015f) * softness * std::clamp(dt, 0.0f, 0.05f));
            if (displacement <= 0.0f)
                return;

            cell.height -= displacement;
            cell.firmness = std::clamp(cell.firmness
                + load * dt * 0.12f, 0.0f, 1.0f);
            cell.loose_fraction = std::clamp(cell.loose_fraction
                - load * dt * 0.08f, 0.0f, 1.0f);

            const std::size_t left = wrap_index(static_cast<std::ptrdiff_t>(center) - 1);
            const std::size_t right = wrap_index(static_cast<std::ptrdiff_t>(center) + 1);
            cells_[left].height += displacement * 0.50f;
            cells_[right].height += displacement * 0.50f;
            cells_[left].loose_fraction = std::clamp(cells_[left].loose_fraction + displacement * 2.0f,
                0.0f, 1.0f);
            cells_[right].loose_fraction = std::clamp(cells_[right].loose_fraction + displacement * 2.0f,
                0.0f, 1.0f);
        }

        void deposit(float course_x, float height_volume, float material_firmness) noexcept
        {
            const std::size_t center = nearest_index(course_x);
            const float amount = std::max(0.0f, height_volume);
            constexpr std::array<float, 5> weights{ 0.10f, 0.22f, 0.36f, 0.22f, 0.10f };
            for (std::size_t offset = 0; offset < weights.size(); ++offset)
            {
                const auto signed_offset = static_cast<std::ptrdiff_t>(offset) - 2;
                Cell& cell = cells_[wrap_index(static_cast<std::ptrdiff_t>(center) + signed_offset)];
                const float addition = amount * weights[offset];
                cell.height += addition;
                cell.firmness = std::lerp(cell.firmness,
                    std::clamp(material_firmness, 0.0f, 1.0f),
                    std::clamp(addition * 5.0f, 0.0f, 0.30f));
                cell.loose_fraction = std::clamp(cell.loose_fraction + addition * 4.0f,
                    0.0f, 1.0f);
            }
        }

        void step(float dt) noexcept
        {
            const float bounded_dt = std::clamp(dt, 0.0f, 0.05f);
            transfers_.fill(0.0f);
            for (std::size_t index = 0; index < cell_count; ++index)
            {
                const std::size_t right = (index + 1u) % cell_count;
                const float difference = cells_[index].height - cells_[right].height;
                const float average_firmness = 0.5f
                    * (cells_[index].firmness + cells_[right].firmness);
                const float repose = 0.075f + average_firmness * 0.12f;
                const float excess = std::abs(difference) - repose;
                if (excess <= 0.0f)
                    continue;
                const float mobility = std::clamp(1.0f - average_firmness, 0.08f, 1.0f);
                const float movement = std::min(excess * 0.25f,
                    excess * mobility * bounded_dt * 3.5f);
                if (difference > 0.0f)
                {
                    transfers_[index] -= movement;
                    transfers_[right] += movement;
                }
                else
                {
                    transfers_[index] += movement;
                    transfers_[right] -= movement;
                }
            }

            for (std::size_t index = 0; index < cell_count; ++index)
            {
                Cell& cell = cells_[index];
                cell.height += transfers_[index];
                const float disturbed = std::min(1.0f, std::abs(transfers_[index]) * 15.0f);
                cell.loose_fraction = std::clamp(cell.loose_fraction + disturbed * 0.08f,
                    0.0f, 1.0f);
                cell.firmness = std::clamp(cell.firmness
                    + bounded_dt * (0.006f - cell.loose_fraction * 0.004f), 0.0f, 1.0f);
            }
        }

        [[nodiscard]] float total_height_volume() const noexcept
        {
            float result = 0.0f;
            for (const Cell& cell : cells_)
                result += cell.height;
            return result;
        }

        [[nodiscard]] float maximum_neighbor_delta() const noexcept
        {
            float result = 0.0f;
            for (std::size_t index = 0; index < cell_count; ++index)
                result = std::max(result, std::abs(cells_[index].height
                    - cells_[(index + 1u) % cell_count].height));
            return result;
        }

        [[nodiscard]] const std::array<Cell, cell_count>& cells() const noexcept
        {
            return cells_;
        }

    private:
        struct Sample
        {
            std::size_t first{};
            std::size_t second{};
            float fraction{};
        };

        [[nodiscard]] static std::uint64_t mix(std::uint64_t value) noexcept
        {
            value ^= value >> 30u;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27u;
            value *= 0x94d049bb133111ebULL;
            value ^= value >> 31u;
            return value;
        }

        [[nodiscard]] static float unit_hash(std::uint64_t value) noexcept
        {
            constexpr double denominator = static_cast<double>(1ULL << 53u);
            return static_cast<float>(static_cast<double>(mix(value) >> 11u) / denominator);
        }

        [[nodiscard]] static float authored_base_height(float course_x,
            float difficulty) noexcept
        {
            float local = std::fmod(course_x, period);
            if (local < 0.0f)
                local += period;
            const float amplitude = 0.16f + difficulty * 0.40f;
            float height = 0.0f;
            if (local >= 28.0f && local < 34.0f)
            {
                const float t = (local - 28.0f) / 6.0f;
                const float smooth = t * t * (3.0f - 2.0f * t);
                height = amplitude * smooth;
            }
            else if (local >= 34.0f && local < 38.0f)
            {
                height = amplitude;
            }
            else if (local >= 38.0f && local < 44.0f)
            {
                const float t = (local - 38.0f) / 6.0f;
                const float smooth = t * t * (3.0f - 2.0f * t);
                height = amplitude * (1.0f - smooth);
            }
            const float roughness = 0.025f + difficulty * 0.055f;
            height += std::sin(course_x * 0.83f) * roughness;
            height += std::sin(course_x * 2.17f + 0.7f) * roughness * 0.42f;
            return std::max(-0.08f, height);
        }

        [[nodiscard]] static std::size_t wrap_index(std::ptrdiff_t index) noexcept
        {
            const auto count = static_cast<std::ptrdiff_t>(cell_count);
            index %= count;
            if (index < 0)
                index += count;
            return static_cast<std::size_t>(index);
        }

        [[nodiscard]] static float wrapped_course_x(float course_x) noexcept
        {
            float wrapped = std::fmod(course_x, period);
            if (wrapped < 0.0f)
                wrapped += period;
            return wrapped;
        }

        [[nodiscard]] static Sample sample_coordinates(float course_x) noexcept
        {
            const float scaled = wrapped_course_x(course_x) / cell_spacing;
            const auto first_signed = static_cast<std::ptrdiff_t>(std::floor(scaled));
            const std::size_t first = wrap_index(first_signed);
            return { first, (first + 1u) % cell_count,
                scaled - static_cast<float>(first_signed) };
        }

        [[nodiscard]] static std::size_t nearest_index(float course_x) noexcept
        {
            const float scaled = wrapped_course_x(course_x) / cell_spacing;
            return wrap_index(static_cast<std::ptrdiff_t>(std::floor(scaled + 0.5f)));
        }

        std::array<Cell, cell_count> cells_{};
        std::array<float, cell_count> transfers_{};
        std::uint64_t seed_{ 1u };
        float difficulty_{ 0.25f };
    };
}
