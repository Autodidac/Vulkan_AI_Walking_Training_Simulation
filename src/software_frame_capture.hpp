#pragma once

#include "renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace runner::software_frame_capture
{
    inline constexpr int maximum_dimension = 8192;
    inline constexpr std::size_t maximum_pixel_count =
        static_cast<std::size_t>(maximum_dimension)
        * static_cast<std::size_t>(maximum_dimension);
    inline constexpr Color production_clear_color{
        0.032f, 0.039f, 0.052f, 1.0f };

    struct Image
    {
        int width{};
        int height{};
        std::vector<std::uint8_t> rgb{};

        [[nodiscard]] bool valid() const noexcept
        {
            if (width <= 0 || height <= 0)
                return false;
            const std::size_t pixel_count = static_cast<std::size_t>(width)
                * static_cast<std::size_t>(height);
            return pixel_count <= std::numeric_limits<std::size_t>::max() / 3u
                && rgb.size() == pixel_count * 3u;
        }

        [[nodiscard]] std::array<std::uint8_t, 3> pixel(
            int x, int y) const noexcept
        {
            if (!valid() || x < 0 || y < 0 || x >= width || y >= height)
                return {};
            const std::size_t index = (static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(width)
                + static_cast<std::size_t>(x)) * 3u;
            return { rgb[index], rgb[index + 1u], rgb[index + 2u] };
        }
    };

    struct RasterStats
    {
        std::size_t input_triangles{};
        std::size_t rasterized_triangles{};
        std::size_t non_finite_triangles{};
        std::size_t degenerate_triangles{};
        std::size_t offscreen_triangles{};
        std::size_t trailing_vertices{};
        std::size_t shaded_samples{};
    };

    struct PixelRect
    {
        int x{};
        int y{};
        int width{};
        int height{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return width > 0 && height > 0;
        }
    };

    struct DifferenceStats
    {
        bool comparable{};
        std::size_t changed_pixels{};
        PixelRect bounds{};
    };

    struct RegionStats
    {
        std::size_t samples{};
        std::size_t non_clear_samples{};
        std::size_t distinct_color_buckets{};
    };

    [[nodiscard]] inline bool finite(Vec2 value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    [[nodiscard]] inline bool finite(Color value) noexcept
    {
        return std::isfinite(value.r) && std::isfinite(value.g)
            && std::isfinite(value.b) && std::isfinite(value.a);
    }

    [[nodiscard]] inline double edge(
        Vec2 a, Vec2 b, double x, double y) noexcept
    {
        return static_cast<double>(b.x - a.x)
                * (y - static_cast<double>(a.y))
            - static_cast<double>(b.y - a.y)
                * (x - static_cast<double>(a.x));
    }

    // Runner uses top-left-origin, Y-down coordinates and a positive-height
    // Vulkan viewport. Normalize triangles to positive screen-space area, then
    // use the matching top/left inclusion rule so the two halves of an
    // alpha-blended Canvas quad do not composite their shared edge twice.
    [[nodiscard]] inline bool top_left_edge(Vec2 a, Vec2 b) noexcept
    {
        const float dy = b.y - a.y;
        const float dx = b.x - a.x;
        return dy < 0.0f || (dy == 0.0f && dx > 0.0f);
    }

    [[nodiscard]] inline bool accepted_edge(
        double value, bool top_left) noexcept
    {
        return value > 0.0 || (value == 0.0 && top_left);
    }

    [[nodiscard]] inline float linear_to_srgb(float linear) noexcept
    {
        linear = std::clamp(linear, 0.0f, 1.0f);
        if (linear <= 0.0031308f)
            return linear * 12.92f;
        return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    }

    [[nodiscard]] inline std::uint8_t srgb_byte(float linear) noexcept
    {
        return static_cast<std::uint8_t>(std::clamp(
            std::lround(linear_to_srgb(linear) * 255.0f), 0l, 255l));
    }

    [[nodiscard]] inline Color source_over(
        Color source, Color destination) noexcept
    {
        source.r = std::clamp(source.r, 0.0f, 1.0f);
        source.g = std::clamp(source.g, 0.0f, 1.0f);
        source.b = std::clamp(source.b, 0.0f, 1.0f);
        source.a = std::clamp(source.a, 0.0f, 1.0f);
        const float inverse_alpha = 1.0f - source.a;
        return {
            source.r * source.a + destination.r * inverse_alpha,
            source.g * source.a + destination.g * inverse_alpha,
            source.b * source.a + destination.b * inverse_alpha,
            source.a + destination.a * inverse_alpha
        };
    }

    [[nodiscard]] inline bool rasterize(
        std::span<const render::Vertex> vertices, int width, int height,
        Image& output, RasterStats& stats, std::string& error)
    {
        output = {};
        stats = {};
        if (width <= 0 || height <= 0
            || width > maximum_dimension || height > maximum_dimension)
        {
            error = "software frame dimensions must be within 1..8192";
            return false;
        }
        const std::size_t pixel_count = static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height);
        if (pixel_count > maximum_pixel_count
            || pixel_count > std::numeric_limits<std::size_t>::max()
                / sizeof(Color))
        {
            error = "software frame pixel count overflowed its bounded buffer";
            return false;
        }

        std::vector<Color> framebuffer(pixel_count, production_clear_color);
        const std::size_t triangle_vertex_count = vertices.size()
            - vertices.size() % 3u;
        stats.input_triangles = triangle_vertex_count / 3u;
        stats.trailing_vertices = vertices.size() - triangle_vertex_count;
        for (std::size_t index = 0u;
            index < triangle_vertex_count; index += 3u)
        {
            render::Vertex first = vertices[index];
            render::Vertex second = vertices[index + 1u];
            render::Vertex third = vertices[index + 2u];
            if (!finite(first.position) || !finite(second.position)
                || !finite(third.position) || !finite(first.color)
                || !finite(second.color) || !finite(third.color))
            {
                ++stats.non_finite_triangles;
                continue;
            }

            double area = edge(first.position, second.position,
                static_cast<double>(third.position.x),
                static_cast<double>(third.position.y));
            if (area == 0.0)
            {
                ++stats.degenerate_triangles;
                continue;
            }
            if (area < 0.0)
            {
                std::swap(second, third);
                area = -area;
            }

            const double minimum_x = std::min({
                static_cast<double>(first.position.x),
                static_cast<double>(second.position.x),
                static_cast<double>(third.position.x) });
            const double maximum_x = std::max({
                static_cast<double>(first.position.x),
                static_cast<double>(second.position.x),
                static_cast<double>(third.position.x) });
            const double minimum_y = std::min({
                static_cast<double>(first.position.y),
                static_cast<double>(second.position.y),
                static_cast<double>(third.position.y) });
            const double maximum_y = std::max({
                static_cast<double>(first.position.y),
                static_cast<double>(second.position.y),
                static_cast<double>(third.position.y) });
            const double last_x = static_cast<double>(width) - 0.5;
            const double last_y = static_cast<double>(height) - 0.5;
            if (maximum_x < 0.5 || maximum_y < 0.5
                || minimum_x > last_x || minimum_y > last_y)
            {
                ++stats.offscreen_triangles;
                continue;
            }
            const int first_x = static_cast<int>(std::ceil(
                std::clamp(minimum_x, 0.5, last_x) - 0.5));
            const int final_x = static_cast<int>(std::floor(
                std::clamp(maximum_x, 0.5, last_x) - 0.5));
            const int first_y = static_cast<int>(std::ceil(
                std::clamp(minimum_y, 0.5, last_y) - 0.5));
            const int final_y = static_cast<int>(std::floor(
                std::clamp(maximum_y, 0.5, last_y) - 0.5));
            if (first_x > final_x || first_y > final_y)
            {
                ++stats.offscreen_triangles;
                continue;
            }

            const bool edge_zero_top_left = top_left_edge(
                second.position, third.position);
            const bool edge_one_top_left = top_left_edge(
                third.position, first.position);
            const bool edge_two_top_left = top_left_edge(
                first.position, second.position);
            const double inverse_area = 1.0 / area;
            bool triangle_rasterized{};
            for (int y = first_y; y <= final_y; ++y)
            {
                const double sample_y = static_cast<double>(y) + 0.5;
                for (int x = first_x; x <= final_x; ++x)
                {
                    const double sample_x = static_cast<double>(x) + 0.5;
                    const double weight_zero = edge(second.position,
                        third.position, sample_x, sample_y);
                    const double weight_one = edge(third.position,
                        first.position, sample_x, sample_y);
                    const double weight_two = edge(first.position,
                        second.position, sample_x, sample_y);
                    if (!accepted_edge(weight_zero, edge_zero_top_left)
                        || !accepted_edge(weight_one, edge_one_top_left)
                        || !accepted_edge(weight_two, edge_two_top_left))
                        continue;

                    const float barycentric_zero = static_cast<float>(
                        weight_zero * inverse_area);
                    const float barycentric_one = static_cast<float>(
                        weight_one * inverse_area);
                    const float barycentric_two = static_cast<float>(
                        weight_two * inverse_area);
                    Color source{
                        first.color.r * barycentric_zero
                            + second.color.r * barycentric_one
                            + third.color.r * barycentric_two,
                        first.color.g * barycentric_zero
                            + second.color.g * barycentric_one
                            + third.color.g * barycentric_two,
                        first.color.b * barycentric_zero
                            + second.color.b * barycentric_one
                            + third.color.b * barycentric_two,
                        first.color.a * barycentric_zero
                            + second.color.a * barycentric_one
                            + third.color.a * barycentric_two
                    };
                    Color& destination = framebuffer[
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(width)
                        + static_cast<std::size_t>(x)];
                    destination = source_over(source, destination);
                    triangle_rasterized = true;
                    ++stats.shaded_samples;
                }
            }
            if (triangle_rasterized)
                ++stats.rasterized_triangles;
            else
                ++stats.offscreen_triangles;
        }

        output.width = width;
        output.height = height;
        output.rgb.resize(pixel_count * 3u);
        for (std::size_t index = 0u; index < pixel_count; ++index)
        {
            output.rgb[index * 3u] = srgb_byte(framebuffer[index].r);
            output.rgb[index * 3u + 1u] = srgb_byte(framebuffer[index].g);
            output.rgb[index * 3u + 2u] = srgb_byte(framebuffer[index].b);
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline std::vector<std::uint8_t> ppm_bytes(
        const Image& image)
    {
        if (!image.valid())
            return {};
        const std::string header = "P6\n" + std::to_string(image.width) + " "
            + std::to_string(image.height) + "\n255\n";
        std::vector<std::uint8_t> bytes{};
        bytes.reserve(header.size() + image.rgb.size());
        bytes.insert(bytes.end(), header.begin(), header.end());
        bytes.insert(bytes.end(), image.rgb.begin(), image.rgb.end());
        return bytes;
    }

    [[nodiscard]] constexpr std::uint64_t fnv1a64(
        std::span<const std::uint8_t> bytes) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const std::uint8_t byte : bytes)
        {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    [[nodiscard]] inline PixelRect clamp_rect(
        PixelRect rect, int width, int height) noexcept
    {
        const long long requested_end_x = static_cast<long long>(rect.x)
            + static_cast<long long>(std::max(0, rect.width));
        const long long requested_end_y = static_cast<long long>(rect.y)
            + static_cast<long long>(std::max(0, rect.height));
        const int beginning_x = std::clamp(rect.x, 0, std::max(0, width));
        const int beginning_y = std::clamp(rect.y, 0, std::max(0, height));
        const int ending_x = static_cast<int>(std::clamp(requested_end_x,
            0ll, static_cast<long long>(std::max(0, width))));
        const int ending_y = static_cast<int>(std::clamp(requested_end_y,
            0ll, static_cast<long long>(std::max(0, height))));
        return { beginning_x, beginning_y,
            std::max(0, ending_x - beginning_x),
            std::max(0, ending_y - beginning_y) };
    }

    [[nodiscard]] inline DifferenceStats difference(
        const Image& first, const Image& second, PixelRect region,
        std::uint8_t threshold = 0u) noexcept
    {
        DifferenceStats result{};
        if (!first.valid() || !second.valid()
            || first.width != second.width || first.height != second.height)
            return result;
        result.comparable = true;
        region = clamp_rect(region, first.width, first.height);
        int minimum_x = first.width;
        int minimum_y = first.height;
        int maximum_x = -1;
        int maximum_y = -1;
        for (int y = region.y; y < region.y + region.height; ++y)
        {
            for (int x = region.x; x < region.x + region.width; ++x)
            {
                const std::size_t index = (static_cast<std::size_t>(y)
                        * static_cast<std::size_t>(first.width)
                    + static_cast<std::size_t>(x)) * 3u;
                bool changed{};
                for (std::size_t channel = 0u; channel < 3u; ++channel)
                {
                    const int delta = std::abs(
                        static_cast<int>(first.rgb[index + channel])
                        - static_cast<int>(second.rgb[index + channel]));
                    changed = changed || delta > static_cast<int>(threshold);
                }
                if (!changed)
                    continue;
                ++result.changed_pixels;
                minimum_x = std::min(minimum_x, x);
                minimum_y = std::min(minimum_y, y);
                maximum_x = std::max(maximum_x, x);
                maximum_y = std::max(maximum_y, y);
            }
        }
        if (result.changed_pixels > 0u)
            result.bounds = { minimum_x, minimum_y,
                maximum_x - minimum_x + 1, maximum_y - minimum_y + 1 };
        return result;
    }

    [[nodiscard]] inline DifferenceStats difference(
        const Image& first, const Image& second,
        std::uint8_t threshold = 0u) noexcept
    {
        return difference(first, second,
            { 0, 0, first.width, first.height }, threshold);
    }

    [[nodiscard]] inline RegionStats analyze_region(
        const Image& image, PixelRect region,
        std::size_t columns = 24u, std::size_t rows = 18u) noexcept
    {
        RegionStats result{};
        if (!image.valid() || columns == 0u || rows == 0u)
            return result;
        region = clamp_rect(region, image.width, image.height);
        if (!region.valid())
            return result;
        std::array<bool, 4096u> buckets{};
        const std::array clear{
            srgb_byte(production_clear_color.r),
            srgb_byte(production_clear_color.g),
            srgb_byte(production_clear_color.b) };
        for (std::size_t row = 0u; row < rows; ++row)
        {
            const int y = region.y + std::min(region.height - 1,
                static_cast<int>((row * static_cast<std::size_t>(region.height)
                    + rows / 2u) / rows));
            for (std::size_t column = 0u; column < columns; ++column)
            {
                const int x = region.x + std::min(region.width - 1,
                    static_cast<int>((column
                            * static_cast<std::size_t>(region.width)
                        + columns / 2u) / columns));
                const auto pixel = image.pixel(x, y);
                ++result.samples;
                if (pixel != clear)
                    ++result.non_clear_samples;
                const std::uint16_t bucket = static_cast<std::uint16_t>(
                    (pixel[0] >> 4u) | ((pixel[1] >> 4u) << 4u)
                    | ((pixel[2] >> 4u) << 8u));
                if (!buckets[bucket])
                {
                    buckets[bucket] = true;
                    ++result.distinct_color_buckets;
                }
            }
        }
        return result;
    }

    [[nodiscard]] constexpr bool visibly_populated(
        const RegionStats& stats) noexcept
    {
        return stats.samples > 0u && stats.non_clear_samples >= 3u
            && stats.distinct_color_buckets >= 2u;
    }
}
