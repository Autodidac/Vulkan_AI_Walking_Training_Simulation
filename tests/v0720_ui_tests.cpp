#include "preview_sync.hpp"
#include "renderer.hpp"
#include "ui_layout.hpp"
#include "ui_render_contract.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#ifndef RUNNER_GENERATED_ASSET_DIRECTORY
#define RUNNER_GENERATED_ASSET_DIRECTORY "."
#endif

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        require(static_cast<bool>(input), "required generated icon file is missing");
        const std::streamsize size = input.tellg();
        require(size > 0, "generated icon file is empty");
        input.seekg(0);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char*>(bytes.data()), size);
        require(static_cast<bool>(input), "generated icon file could not be read");
        return bytes;
    }

    std::uint16_t u16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        return static_cast<std::uint16_t>(bytes[offset])
            | static_cast<std::uint16_t>(bytes[offset + 1u] << 8u);
    }
}

int main()
{
    for (const auto& size : runner::ui_layout::validation_sizes)
        require(runner::ui_layout::live_layout_valid(size[0], size[1]),
            "supported live layout must remain contained and non-overlapping");

    const auto dpi = runner::ui_layout::logical_surface_scale(
        1600.0f, 900.0f, 2400.0f, 1350.0f);
    require(std::abs(dpi.x - 1.5f) < 1.0e-5f
            && std::abs(dpi.y - 1.5f) < 1.0e-5f,
        "logical-to-surface scaling must preserve high-DPI coordinates");
    require(runner::ui_layout::minimum_readable_text_scale >= 0.78f,
        "minimum live text scale regressed below the readability gate");

    const auto telemetry = runner::ui_layout::primary_telemetry_box(
        runner::ui_layout::live_world_box(runner::ui_layout::content_box(1920.0f, 1080.0f)));
    const auto pip = runner::ui_layout::training_pip_box(
        runner::ui_layout::live_world_box(runner::ui_layout::content_box(1920.0f, 1080.0f)));
    require(!runner::ui_layout::overlaps(telemetry, pip),
        "telemetry and PIP must not overlap");

    const auto stable = runner::preview_sync::decide(false, false, false);
    const auto champion = runner::preview_sync::decide(false, false, true);
    const auto course = runner::preview_sync::decide(false, true, false);
    require(!stable.reset_episode && !champion.reset_episode,
        "normal publication or new champion must not restart the live preview");
    require(course.reset_episode && course.replace_course,
        "real course changes must reset the live preview");

    runner::render::Canvas canvas{};
    canvas.push_clip({ 10.0f, 20.0f }, { 40.0f, 50.0f });
    canvas.quad({ -100.0f, -100.0f }, { 100.0f, 100.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f });
    canvas.pop_clip();
    require(!canvas.vertices().empty(),
        "clipped primitive emitted no triangles and made the clip test vacuous");
    require(canvas.vertices().size() % 3u == 0u,
        "clipped canvas output is not a complete triangle list");
    require(canvas.clip_depth() == 0u, "canvas clip stack did not unwind");
    for (const runner::render::Vertex& vertex : canvas.vertices())
    {
        require(vertex.position.x >= 10.0f && vertex.position.x <= 40.0f
                && vertex.position.y >= 20.0f && vertex.position.y <= 50.0f,
            "clipped canvas vertex escaped its viewport");
    }

    runner::render::Canvas nested{};
    nested.push_clip({ 0.0f, 0.0f }, { 80.0f, 80.0f });
    nested.push_clip({ 20.0f, 25.0f }, { 55.0f, 60.0f });
    nested.quad({ -20.0f, -20.0f }, { 100.0f, 100.0f },
        { 0.2f, 0.7f, 0.9f, 1.0f });
    nested.pop_clip();
    nested.pop_clip();
    require(!nested.vertices().empty(),
        "nested clip intersection emitted no visible geometry");
    require(nested.clip_depth() == 0u,
        "nested clip stack did not unwind to zero");
    for (const runner::render::Vertex& vertex : nested.vertices())
    {
        require(vertex.position.x >= 20.0f && vertex.position.x <= 55.0f
                && vertex.position.y >= 25.0f && vertex.position.y <= 60.0f,
            "nested clipped vertex escaped the intersected viewport");
    }
    require(runner::ui_render::is_explicitly_transparent(
            runner::ui_render::transparent_fill),
        "named border-only fill is not transparent");
    require(runner::Color{}.a == 1.0f,
        "default Color no longer demonstrates why explicit transparency is required");

    const std::filesystem::path assets{ RUNNER_GENERATED_ASSET_DIRECTORY };
    const std::vector<std::uint8_t> ico = read_bytes(assets / "runner.ico");
    require(ico.size() > 6u && u16(ico, 0u) == 0u && u16(ico, 2u) == 1u,
        "Windows icon header is invalid");
    require(u16(ico, 4u) >= 9u, "Windows icon lacks required resolutions");
    const std::vector<std::uint8_t> bmp = read_bytes(assets / "runner_icon.bmp");
    require(bmp.size() > 54u && bmp[0] == 'B' && bmp[1] == 'M',
        "SDL runtime BMP icon is invalid");
    const std::vector<std::uint8_t> png = read_bytes(assets / "runner_icon.png");
    const std::array<std::uint8_t, 8> png_signature{
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au };
    require(png.size() > png_signature.size()
            && std::equal(png_signature.begin(), png_signature.end(), png.begin()),
        "source PNG icon is invalid");

    std::cout << "Runner v0.7.20 UI, DPI, clipping, preview, and icon tests passed\n";
    return EXIT_SUCCESS;
}
