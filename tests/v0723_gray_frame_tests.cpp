#include "app.hpp"
#include "ui_frame_probe.hpp"
#include "ui_layout.hpp"
#include "ui_render_contract.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

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

    void require_visible(std::span<const runner::render::Vertex> vertices,
        runner::ui_layout::Box region, std::string_view name)
    {
        const runner::ui_frame_probe::RegionStats stats =
            runner::ui_frame_probe::analyze(vertices, region);
        if (!runner::ui_frame_probe::visibly_populated(stats))
        {
            std::cerr << "FAILED: " << name
                << " remained black or visually empty; samples=" << stats.sample_count
                << " non_black=" << stats.non_black_samples
                << " final_colors=" << stats.distinct_color_buckets
                << " source_vertices=" << stats.source_vertex_count
                << " source_colors=" << stats.source_color_buckets << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    using namespace runner;

    require(ui_render::is_explicitly_transparent(ui_render::transparent_fill),
        "border-only fill is not explicitly transparent");
    require(Color{}.a == 1.0f,
        "test fixture no longer proves that default Color is opaque");

    {
        render::Canvas border_canvas{};
        constexpr Color background{ 0.12f, 0.31f, 0.55f, 1.0f };
        constexpr Color outline{ 0.82f, 0.24f, 0.10f, 1.0f };
        border_canvas.quad({ 0.0f, 0.0f }, { 200.0f, 120.0f }, background);
        ui_render::rounded_rect(border_canvas, { 0.0f, 0.0f }, { 200.0f, 120.0f },
            18.0f, ui_render::transparent_fill, outline, 4.0f);
        const Color center = ui_frame_probe::sample_color(
            border_canvas.vertices(), { 100.0f, 60.0f });
        const Color edge = ui_frame_probe::sample_color(
            border_canvas.vertices(), { 2.0f, 60.0f });
        const auto close = [](float lhs, float rhs) noexcept
        {
            return std::abs(lhs - rhs) <= 1.0e-4f;
        };
        require(close(center.r, background.r) && close(center.g, background.g)
                && close(center.b, background.b),
            "border-only rounded rectangle changed the center composite");
        require(std::abs(edge.r - background.r) + std::abs(edge.g - background.g)
                + std::abs(edge.b - background.b) > 0.20f,
            "rounded outline did not reach the perimeter sample");
    }

    constexpr int width = 1600;
    constexpr int height = 900;
    constexpr float dt = 1.0f / 60.0f;
    const ui_layout::Box content = ui_layout::content_box(
        static_cast<float>(width), static_cast<float>(height));
    const ui_layout::Box live_world = ui_layout::live_world_box(content);
    const ui_layout::Box live_panel = ui_layout::live_panel_box(content);
    const ui_layout::Box live_pip = ui_layout::training_pip_box(live_world);
    const ui_layout::Box rig_panel = ui_layout::rig_lab_panel_box(content);
    const ui_layout::Box rig_world = ui_layout::rig_lab_world_box(content);

    Application application{};
    InputState input{};
    application.frame(input, dt, width, height);
    require_visible(application.vertices(), live_world, "Live world");
    require_visible(application.vertices(), live_panel, "Live dashboard");
    require_visible(application.vertices(), live_pip, "training PIP");

    InputState switch_to_rig{};
    switch_to_rig.tab_pressed = true;
    application.frame(switch_to_rig, dt, width, height);
    require_visible(application.vertices(), rig_panel, "Rig Lab PRESETS page");
    require_visible(application.vertices(), rig_world, "Rig Lab viewport");

    const float usable_width = rig_panel.width - 36.0f;
    const float tab_width = (usable_width - 18.0f) * 0.25f;
    const float tab_y = rig_panel.y + 16.0f + 38.0f + 17.5f;
    const auto select_page = [&](int slot, std::string_view name)
    {
        InputState click{};
        click.left_pressed = true;
        click.mouse = {
            rig_panel.x + 18.0f
                + static_cast<float>(slot) * (tab_width + 6.0f)
                + tab_width * 0.5f,
            tab_y
        };
        application.frame(click, dt, width, height);
        require_visible(application.vertices(), rig_panel, name);
        require_visible(application.vertices(), rig_world, "Rig Lab viewport after page switch");
    };

    select_page(1, "Rig Lab STRUCTURE page");
    select_page(2, "Rig Lab MOTORS page");
    select_page(3, "Rig Lab TEST page");
    select_page(0, "Rig Lab PRESETS page after repeated switching");

    std::cout << "Runner v0.7.23 rounded-outline and final-frame tests passed\n";
    return EXIT_SUCCESS;
}
