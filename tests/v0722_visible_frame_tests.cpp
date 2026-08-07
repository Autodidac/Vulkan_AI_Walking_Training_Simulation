#include "app.hpp"
#include "ui_frame_probe.hpp"
#include "ui_layout.hpp"
#include "ui_render_contract.hpp"

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
                << " colors=" << stats.distinct_color_buckets << '\n';
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

    std::cout << "Runner v0.7.22 visible Live and Rig Lab frame tests passed\n";
    return EXIT_SUCCESS;
}
