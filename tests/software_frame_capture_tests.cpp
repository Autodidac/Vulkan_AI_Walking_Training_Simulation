#include "software_frame_capture.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void append_quad(std::vector<runner::render::Vertex>& vertices,
        runner::Vec2 minimum, runner::Vec2 maximum, runner::Color color)
    {
        vertices.insert(vertices.end(), {
            { { minimum.x, minimum.y }, color },
            { { maximum.x, minimum.y }, color },
            { { maximum.x, maximum.y }, color },
            { { minimum.x, minimum.y }, color },
            { { maximum.x, maximum.y }, color },
            { { minimum.x, maximum.y }, color }
        });
    }
}

int main()
{
    using namespace runner;
    namespace capture = software_frame_capture;

    capture::Image image{};
    capture::RasterStats stats{};
    std::string error{};
    require(!capture::rasterize({}, 0, 4, image, stats, error)
            && !error.empty() && !image.valid(),
        "zero-width software frame was accepted");
    require(!capture::rasterize({}, capture::maximum_dimension + 1, 1,
            image, stats, error),
        "unbounded software frame dimension was accepted");

    std::vector<render::Vertex> vertices{};
    append_quad(vertices, { 0.0f, 0.0f }, { 4.0f, 4.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f });
    append_quad(vertices, { 1.0f, 1.0f }, { 3.0f, 3.0f },
        { 1.0f, 0.0f, 0.0f, 0.5f });
    vertices.insert(vertices.end(), {
        { { -20.0f, -20.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -10.0f, -20.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -10.0f, -10.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { 2.0f, 2.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { std::numeric_limits<float>::quiet_NaN(), 0.0f },
            { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { 2.0f, 2.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
    });
    require(capture::rasterize(vertices, 4, 4, image, stats, error)
            && error.empty() && image.valid(),
        "valid ordered production triangles did not rasterize");
    require(stats.input_triangles == 7u
            && stats.rasterized_triangles == 4u
            && stats.offscreen_triangles == 1u
            && stats.degenerate_triangles == 1u
            && stats.non_finite_triangles == 1u
            && stats.trailing_vertices == 1u,
        "software rasterizer did not classify malformed/off-screen input exactly");
    require(image.pixel(0, 0) == std::array<std::uint8_t, 3>{
                capture::srgb_byte(0.0f), capture::srgb_byte(0.0f),
                capture::srgb_byte(1.0f) },
        "opaque production triangle did not replace the Vulkan clear color");
    const std::array blended{
        capture::srgb_byte(0.5f), capture::srgb_byte(0.0f),
        capture::srgb_byte(0.5f) };
    require(image.pixel(1, 1) == blended && image.pixel(2, 2) == blended,
        "alpha quad did not blend once across its shared top-left edge");

    const std::vector<std::uint8_t> first_ppm = capture::ppm_bytes(image);
    constexpr std::string_view expected_header{ "P6\n4 4\n255\n" };
    require(first_ppm.size() == expected_header.size() + 4u * 4u * 3u
            && std::equal(expected_header.begin(), expected_header.end(),
                first_ppm.begin()),
        "binary PPM did not preserve its exact dimensions/header/payload size");
    capture::Image repeated{};
    capture::RasterStats repeated_stats{};
    require(capture::rasterize(vertices, 4, 4, repeated, repeated_stats, error)
            && repeated.rgb == image.rgb
            && capture::ppm_bytes(repeated) == first_ppm
            && capture::fnv1a64(capture::ppm_bytes(repeated))
                == capture::fnv1a64(first_ppm),
        "same production vertices did not produce repeated-byte-identical output");

    std::vector<render::Vertex> red_then_green{};
    append_quad(red_then_green, { 0.0f, 0.0f }, { 2.0f, 2.0f },
        { 1.0f, 0.0f, 0.0f, 0.5f });
    append_quad(red_then_green, { 0.0f, 0.0f }, { 2.0f, 2.0f },
        { 0.0f, 1.0f, 0.0f, 0.5f });
    std::vector<render::Vertex> green_then_red{};
    append_quad(green_then_red, { 0.0f, 0.0f }, { 2.0f, 2.0f },
        { 0.0f, 1.0f, 0.0f, 0.5f });
    append_quad(green_then_red, { 0.0f, 0.0f }, { 2.0f, 2.0f },
        { 1.0f, 0.0f, 0.0f, 0.5f });
    capture::Image first_order{};
    capture::Image second_order{};
    require(capture::rasterize(red_then_green, 2, 2,
                first_order, stats, error)
            && capture::rasterize(green_then_red, 2, 2,
                second_order, stats, error)
            && first_order.pixel(0, 0) != second_order.pixel(0, 0)
            && first_order.pixel(0, 0)[1] > first_order.pixel(0, 0)[0]
            && second_order.pixel(0, 0)[0] > second_order.pixel(0, 0)[1],
        "triangle submission order no longer controls source-over composition");

    const capture::DifferenceStats same = capture::difference(image, repeated);
    require(same.comparable && same.changed_pixels == 0u
            && !same.bounds.valid(),
        "identical frames reported a visual difference");
    repeated.rgb[(2u * 4u + 3u) * 3u] ^= 0xffu;
    const capture::DifferenceStats changed = capture::difference(
        image, repeated, { 2, 1, 2, 2 });
    require(changed.comparable && changed.changed_pixels == 1u
            && changed.bounds.x == 3 && changed.bounds.y == 2
            && changed.bounds.width == 1 && changed.bounds.height == 1,
        "bounded pixel-difference evidence reported the wrong changed region");

    const capture::RegionStats populated = capture::analyze_region(
        image, { 0, 0, 4, 4 }, 4u, 4u);
    require(capture::visibly_populated(populated)
            && populated.samples == 16u
            && populated.distinct_color_buckets >= 2u,
        "visible-region sampling rejected a populated production frame");
    capture::Image clear_image{ 2, 2, std::vector<std::uint8_t>(12u) };
    for (std::size_t index = 0u; index < 4u; ++index)
    {
        clear_image.rgb[index * 3u] = capture::srgb_byte(
            capture::production_clear_color.r);
        clear_image.rgb[index * 3u + 1u] = capture::srgb_byte(
            capture::production_clear_color.g);
        clear_image.rgb[index * 3u + 2u] = capture::srgb_byte(
            capture::production_clear_color.b);
    }
    require(!capture::visibly_populated(capture::analyze_region(
                clear_image, { 0, 0, 2, 2 }, 2u, 2u)),
        "clear-only image was accepted as a populated UI region");

    std::cout << "Runner software production-vertex capture tests passed\n";
    return EXIT_SUCCESS;
}
