#include "pixel_art.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#ifndef RUNNER_SOURCE_ASSET_DIRECTORY
#define RUNNER_SOURCE_ASSET_DIRECTORY "assets"
#endif

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "Pixel-art test failed: " << message << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    const std::filesystem::path source_art =
        std::filesystem::path{ RUNNER_SOURCE_ASSET_DIRECTORY } / "chicken.ppm";
    runner::art::PixelArt art{};
    std::string error{};
    require(runner::art::load_p3_pixel_art(source_art, art, error),
        error.empty() ? "packaged chicken.ppm did not load" : error.c_str());
    require(art.loaded(), "loaded artwork is internally incomplete");
    require(art.width == 32 && art.height == 20,
        "original Runner artwork dimensions changed unexpectedly");
    require(art.pixels.size() == 640u,
        "original Runner artwork pixel count is incorrect");

    const std::filesystem::path temporary =
        std::filesystem::temp_directory_path() / "runner-p3-parser-test.ppm";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        const std::string data =
            "\xef\xbb\xbfP3\r\n"
            "# comment before dimensions\r\n"
            "2 1\r\n"
            "255\r\n"
            "# comment before pixels\r\n"
            "255 0 0   0 255 0\r\n";
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    runner::art::PixelArt bom_art{};
    require(runner::art::load_p3_pixel_art(temporary, bom_art, error),
        error.empty() ? "BOM/comment/CRLF P3 did not load" : error.c_str());
    require(bom_art.width == 2 && bom_art.height == 1
            && bom_art.pixels.size() == 2u,
        "BOM/comment/CRLF P3 decoded incorrectly");

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << "P3\n1 1\n255\n255 0\n";
    }
    runner::art::PixelArt malformed{};
    require(!runner::art::load_p3_pixel_art(temporary, malformed, error),
        "incomplete P3 pixel data was accepted");
    std::error_code remove_error{};
    std::filesystem::remove(temporary, remove_error);

    const runner::art::SkinEnvelopeDimensions compact_skin =
        runner::art::skin_envelope_dimensions(50.0f, 8.0f);
    const runner::art::SkinEnvelopeDimensions large_skin =
        runner::art::skin_envelope_dimensions(100.0f, 16.0f);
    const runner::art::SkinEnvelopeDimensions invalid_skin =
        runner::art::skin_envelope_dimensions(
            std::numeric_limits<float>::infinity(), -1.0f);
    require(compact_skin.shoulder_width >= 47.0f
            && compact_skin.chest_radius >= 14.5f
            && compact_skin.pelvis_half_width >= 11.0f,
        "skin envelope preserves a connected, broad torso silhouette");
    require(std::abs(large_skin.shoulder_width
                / compact_skin.shoulder_width - 2.0f) < 0.001f
            && std::isfinite(invalid_skin.shoulder_width)
            && invalid_skin.shoulder_width > 0.0f,
        "skin envelope does not scale or sanitize invalid authored geometry");
    const float compact_armor_scale =
        runner::art::assembled_armor_scale(50.0f, 8.0f);
    const float large_armor_scale =
        runner::art::assembled_armor_scale(100.0f, 16.0f);
    const float invalid_armor_scale = runner::art::assembled_armor_scale(
        std::numeric_limits<float>::infinity(), -1.0f);
    require(compact_armor_scale >= 1.24f && compact_armor_scale <= 1.34f
            && std::abs(compact_armor_scale - large_armor_scale) < 0.001f
            && std::isfinite(invalid_armor_scale)
            && invalid_armor_scale >= 1.24f && invalid_armor_scale <= 1.34f,
        "assembled armor scale is undersized, non-finite, or morphology-unstable");

    std::cout << "Runner pixel-art parser and skin-envelope tests passed\n";
    return 0;
}
