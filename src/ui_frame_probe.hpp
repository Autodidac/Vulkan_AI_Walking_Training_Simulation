#pragma once

#include "renderer.hpp"
#include "ui_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace runner::ui_frame_probe
{
    struct RegionStats
    {
        std::size_t sample_count{};
        std::size_t non_black_samples{};
        std::size_t distinct_color_buckets{};
    };

    [[nodiscard]] inline bool triangle_contains(Vec2 point,
        Vec2 a, Vec2 b, Vec2 c) noexcept
    {
        constexpr float epsilon = 1.0e-4f;
        if (std::abs(cross(b - a, c - a)) <= epsilon)
            return false;
        const float ab = cross(b - a, point - a);
        const float bc = cross(c - b, point - b);
        const float ca = cross(a - c, point - c);
        const bool negative = ab < -epsilon || bc < -epsilon || ca < -epsilon;
        const bool positive = ab > epsilon || bc > epsilon || ca > epsilon;
        return !(negative && positive);
    }

    [[nodiscard]] constexpr Color blend_over(Color destination,
        Color source) noexcept
    {
        const float inverse = 1.0f - std::clamp(source.a, 0.0f, 1.0f);
        return {
            source.r * source.a + destination.r * inverse,
            source.g * source.a + destination.g * inverse,
            source.b * source.a + destination.b * inverse,
            source.a + destination.a * inverse
        };
    }

    [[nodiscard]] inline Color sample_color(
        std::span<const render::Vertex> vertices, Vec2 point) noexcept
    {
        Color result{ 0.0f, 0.0f, 0.0f, 0.0f };
        const std::size_t triangle_vertex_count = vertices.size()
            - vertices.size() % 3u;
        for (std::size_t index = 0; index < triangle_vertex_count; index += 3u)
        {
            const render::Vertex& a = vertices[index];
            const render::Vertex& b = vertices[index + 1u];
            const render::Vertex& c = vertices[index + 2u];
            if (triangle_contains(point, a.position, b.position, c.position))
                result = blend_over(result, a.color);
        }
        return result;
    }

    [[nodiscard]] constexpr std::uint16_t color_bucket(Color color) noexcept
    {
        const auto quantize = [](float value) constexpr -> std::uint16_t
        {
            return static_cast<std::uint16_t>(std::clamp(value, 0.0f, 1.0f)
                * 15.0f + 0.5f);
        };
        return static_cast<std::uint16_t>(quantize(color.r)
            | (quantize(color.g) << 4u)
            | (quantize(color.b) << 8u));
    }

    [[nodiscard]] inline RegionStats analyze(
        std::span<const render::Vertex> vertices, ui_layout::Box region) noexcept
    {
        RegionStats result{};
        if (region.width <= 12.0f || region.height <= 12.0f)
            return result;

        constexpr std::size_t columns = 20u;
        constexpr std::size_t rows = 14u;
        std::array<std::uint16_t, columns * rows> buckets{};
        std::size_t bucket_count{};
        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t column = 0; column < columns; ++column)
            {
                const float x = region.x + region.width
                    * (static_cast<float>(column) + 0.5f)
                    / static_cast<float>(columns);
                const float y = region.y + region.height
                    * (static_cast<float>(row) + 0.5f)
                    / static_cast<float>(rows);
                const Color color = sample_color(vertices, { x, y });
                ++result.sample_count;
                if (std::max({ color.r, color.g, color.b }) > 0.105f)
                    ++result.non_black_samples;
                const std::uint16_t bucket = color_bucket(color);
                const auto begin = buckets.begin();
                const auto end = begin + static_cast<std::ptrdiff_t>(bucket_count);
                if (std::find(begin, end, bucket) == end)
                    buckets[bucket_count++] = bucket;
            }
        }
        result.distinct_color_buckets = bucket_count;
        return result;
    }

    [[nodiscard]] constexpr bool visibly_populated(
        const RegionStats& stats) noexcept
    {
        return stats.sample_count > 0u
            && stats.non_black_samples >= 3u
            && stats.distinct_color_buckets >= 3u;
    }
}
