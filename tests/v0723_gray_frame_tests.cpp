#include "app.hpp"
#include "simulation.hpp"
#include "ui_frame_probe.hpp"
#include "ui_layout.hpp"
#include "ui_render_contract.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

    void require_rasterized_text(
        std::span<const runner::render::Vertex> vertices,
        runner::ui_layout::Box region, std::string_view name)
    {
        const runner::ui_frame_probe::RegionStats stats =
            runner::ui_frame_probe::analyze(vertices, region);
        if (stats.non_black_samples < 2u || stats.source_vertex_count < 18u)
        {
            std::cerr << "FAILED: " << name
                << " had no rasterized text over its card background; samples="
                << stats.sample_count << " non_black=" << stats.non_black_samples
                << " source_vertices=" << stats.source_vertex_count
                << " source_colors=" << stats.source_color_buckets << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    [[nodiscard]] std::uint64_t region_signature(
        std::span<const runner::render::Vertex> vertices,
        runner::ui_layout::Box region)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (int y = 0; y < 20; ++y)
        {
            for (int x = 0; x < 20; ++x)
            {
                const runner::Color color = runner::ui_frame_probe::sample_color(
                    vertices, { region.x + region.width
                            * (static_cast<float>(x) + 0.5f) / 20.0f,
                        region.y + region.height
                            * (static_cast<float>(y) + 0.5f) / 20.0f });
                for (const float channel : { color.r, color.g, color.b, color.a })
                {
                    hash ^= static_cast<std::uint64_t>(std::lround(
                        std::clamp(channel, 0.0f, 1.0f) * 255.0f));
                    hash *= 1099511628211ULL;
                }
            }
        }
        return hash;
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

    constexpr int width = 1368;
    constexpr int height = 799;
    constexpr float dt = 1.0f / 60.0f;
    const ui_layout::Box content = ui_layout::content_box(
        static_cast<float>(width), static_cast<float>(height));
    const ui_layout::Box live_world = ui_layout::live_world_box(content);
    const ui_layout::Box live_panel = ui_layout::live_panel_box(content);
    const ui_layout::Box footer = ui_layout::status_footer_box(
        static_cast<float>(width), static_cast<float>(height));
    const ui_layout::RigLabWorkspaceLayout inspector_layout =
        ui_layout::rig_lab_workspace_layout(content,
            ui_layout::RigLabWorkspacePage::inspector);
    const ui_layout::RigLabWorkspaceLayout trainer_layout =
        ui_layout::rig_lab_workspace_layout(content,
            ui_layout::RigLabWorkspacePage::trainer);
    const ui_layout::Box rig_panel = inspector_layout.inspector;
    const ui_layout::Box rig_world = inspector_layout.world;
    const ui_layout::Box rig_trainer = trainer_layout.trainer;
    const ui_layout::Box workspace_bar = inspector_layout.workspace_bar;
    const ui_layout::RigLabWorkspaceNavigationLayout workspace_navigation =
        ui_layout::rig_lab_workspace_navigation_layout(workspace_bar);

    Application application{};
    std::string initialization_error{};
    require(application.initialize("assets", initialization_error,
            presentation::StartupMode::isolated_visual_proof),
        "application initialization failed before compact UI rendering");
    InputState input{};
    application.frame(input, dt, width, height);
    require_visible(application.vertices(), live_world, "Live world");
    require_visible(application.vertices(), live_panel, "Live dashboard");

    const ui_layout::TrainerControlGroupLayout live_summary_controls =
        ui_layout::trainer_control_group_layout(
            live_panel, 0.0f, true, true);
    const std::array<ui_layout::Box, 4> live_quick_actions =
        ui_layout::trainer_quick_action_layout(
            live_summary_controls.page_tabs);
    const std::uint64_t live_summary_signature = region_signature(
        application.vertices(), live_panel);
    InputState open_mission{};
    open_mission.left_pressed = true;
    open_mission.mouse = {
        live_quick_actions[0].x + live_quick_actions[0].width * 0.5f,
        live_quick_actions[0].y + live_quick_actions[0].height * 0.5f };
    application.frame(open_mission, 0.0f, width, height);
    application.frame({}, 0.0f, width, height);
    const std::uint64_t live_mission_signature = region_signature(
        application.vertices(), live_panel);
    require(live_mission_signature != live_summary_signature,
        "compact Mission action did not leave the sparse Autopilot home");

    const ui_layout::TrainerControlGroupLayout live_detail_controls =
        ui_layout::trainer_control_group_layout(
            live_panel, 0.0f, false, true);
    const std::array<ui_layout::Box, 3> live_page_actions =
        ui_layout::trainer_live_page_button_layout(
            live_detail_controls.page_tabs);
    const auto click_live_page = [&](std::size_t page)
    {
        InputState click{};
        click.left_pressed = true;
        click.mouse = {
            live_page_actions[page].x
                + live_page_actions[page].width * 0.5f,
            live_page_actions[page].y
                + live_page_actions[page].height * 0.5f };
        application.frame(click, 0.0f, width, height);
        application.frame({}, 0.0f, width, height);
        return region_signature(application.vertices(), live_panel);
    };
    const std::uint64_t live_view_signature = click_live_page(1u);
    const std::uint64_t live_details_signature = click_live_page(2u);
    require(live_view_signature != live_mission_signature
            && live_details_signature != live_view_signature,
        "compact Mission/View/Details pointer routes are not distinct");
    InputState inert_world_click{};
    inert_world_click.left_pressed = true;
    inert_world_click.mouse = {
        live_world.x + live_world.width * 0.5f,
        live_world.y + live_world.height * 0.5f };
    application.frame(inert_world_click, 0.0f, width, height);
    application.frame({}, 0.0f, width, height);
    require(region_signature(application.vertices(), live_panel)
                == live_details_signature,
        "world click leaked into the compact Live navigation rail");

    require(application.prepare_rig_lab_page(
            RigLabStartupPage::presets, 0u),
        "could not prepare canonical Rig Lab preset page");
    application.frame({}, dt, width, height);
    require_visible(application.vertices(), workspace_bar,
        "compact Rig Lab workspace selector");
    require_visible(application.vertices(), rig_panel, "Rig Lab PRESETS page");
    require_visible(application.vertices(), rig_world, "Rig Lab viewport");

    const auto select_workspace = [&](ui_layout::RigLabWorkspacePage page)
    {
        const ui_layout::Box target = page
                == ui_layout::RigLabWorkspacePage::trainer
            ? workspace_navigation.trainer_button
            : workspace_navigation.inspector_button;
        InputState click{};
        click.left_pressed = true;
        click.mouse = { target.x + target.width * 0.5f,
            target.y + target.height * 0.5f };
        application.frame(click, 0.0f, width, height);
        // Clear hover-only raster changes before comparing panel signatures.
        application.frame({}, 0.0f, width, height);
    };
    select_workspace(ui_layout::RigLabWorkspacePage::trainer);
    require_visible(application.vertices(), rig_trainer,
        "complete trainer workspace while Rig Lab is active");
    require_visible(application.vertices(), rig_world,
        "Rig Lab viewport beside trainer workspace");
    select_workspace(ui_layout::RigLabWorkspacePage::inspector);

    require(application.prepare_rig_lab_page(
            RigLabStartupPage::art, 0u),
        "could not prepare scrollable canonical Rig Lab Art page");
    application.frame({}, 0.0f, width, height);

    const std::uint64_t compact_command_rig_signature =
        application.rig_preview_signature();
    const std::uint64_t compact_command_bar_before = region_signature(
        application.vertices(), workspace_bar);
    const auto click_compact_command = [&](ui_layout::Box target)
    {
        InputState click{};
        click.left_pressed = true;
        click.mouse = { target.x + target.width * 0.5f,
            target.y + target.height * 0.5f };
        application.frame(click, 0.0f, width, height);
        application.frame({}, 0.0f, width, height);
        require_visible(application.vertices(), rig_panel,
            "compact Inspector retained after persistent command");
        require_visible(application.vertices(), workspace_bar,
            "compact persistent command strip after command");
    };
    click_compact_command(workspace_navigation.run_button);
    require(region_signature(application.vertices(), workspace_bar)
                != compact_command_bar_before,
        "compact persistent Run/Pause command did not update its state");
    click_compact_command(workspace_navigation.run_button);
    click_compact_command(workspace_navigation.controls_button);
    const ui_layout::RigLabControlDrawerLayout compact_drawer =
        ui_layout::rig_lab_control_drawer_layout(rig_panel);
    click_compact_command(compact_drawer.authority_button);
    click_compact_command(compact_drawer.authority_button);
    click_compact_command(compact_drawer.profile_button);
    click_compact_command(compact_drawer.profile_button);
    click_compact_command(compact_drawer.profile_button);
    click_compact_command(compact_drawer.gait_buttons[0]);
    click_compact_command(compact_drawer.rig_art_button);
    click_compact_command(compact_drawer.rig_art_button);
    click_compact_command(workspace_navigation.controls_button);
    require(application.rig_preview_signature()
                == compact_command_rig_signature,
        "compact persistent commands changed selected rig identity");

    const std::uint64_t inspector_before = region_signature(
        application.vertices(), rig_panel);
    select_workspace(ui_layout::RigLabWorkspacePage::trainer);
    const std::uint64_t trainer_before = region_signature(
        application.vertices(), rig_trainer);
    const std::uint64_t trainer_world_before = region_signature(
        application.vertices(), rig_world);
    InputState scroll_trainer{};
    scroll_trainer.mouse = { rig_trainer.x + rig_trainer.width * 0.5f,
        rig_trainer.y + rig_trainer.height * 0.5f };
    scroll_trainer.wheel = -6.0f;
    application.frame(scroll_trainer, 0.0f, width, height);
    application.frame({}, 0.0f, width, height);
    const std::uint64_t trainer_scrolled = region_signature(
        application.vertices(), rig_trainer);
    require(region_signature(application.vertices(), rig_trainer)
                != trainer_before
            && region_signature(application.vertices(), rig_world)
                == trainer_world_before,
        "trainer scrolling is inert or leaks into the Rig Lab preview");

    select_workspace(ui_layout::RigLabWorkspacePage::inspector);
    require(region_signature(application.vertices(), rig_panel)
                == inspector_before
            && region_signature(application.vertices(), rig_world)
                == trainer_world_before,
        "trainer wheel input leaked into the hidden inspector or preview");
    const std::uint64_t panel_before = region_signature(
        application.vertices(), rig_panel);
    const std::uint64_t inspector_world_before = region_signature(
        application.vertices(), rig_world);
    InputState scroll_editor{};
    scroll_editor.mouse = { rig_panel.x + rig_panel.width * 0.5f,
        rig_panel.y + rig_panel.height * 0.5f };
    scroll_editor.wheel = -6.0f;
    application.frame(scroll_editor, 0.0f, width, height);
    application.frame({}, 0.0f, width, height);
    require(region_signature(application.vertices(), rig_panel) != panel_before
            && region_signature(application.vertices(), rig_world)
                == inspector_world_before,
        "Rig Inspector scrolling is inert or leaks into the preview");

    select_workspace(ui_layout::RigLabWorkspacePage::trainer);
    require(region_signature(application.vertices(), rig_trainer)
                == trainer_scrolled
            && region_signature(application.vertices(), rig_world)
                == inspector_world_before,
        "Rig Inspector wheel input leaked into the hidden trainer or preview");
    select_workspace(ui_layout::RigLabWorkspacePage::inspector);

    InputState rewind_editor{};
    rewind_editor.mouse = scroll_editor.mouse;
    rewind_editor.wheel = 100.0f;
    application.frame(rewind_editor, 0.0f, width, height);

    const std::array<ui_layout::Box, 5> inspector_page_tabs =
        ui_layout::rig_inspector_page_button_layout(rig_panel);
    const auto select_page = [&](int slot, std::string_view name)
    {
        const ui_layout::Box tab = inspector_page_tabs[
            static_cast<std::size_t>(slot)];
        InputState click{};
        click.left_pressed = true;
        click.mouse = {
            tab.x + tab.width * 0.5f,
            tab.y + tab.height * 0.5f
        };
        application.frame(click, dt, width, height);
        require_visible(application.vertices(), rig_panel, name);
        require_visible(application.vertices(), workspace_bar,
            "workspace selector retained while switching Rig Inspector pages");
        require_visible(application.vertices(), rig_world, "Rig Lab viewport after page switch");
    };

    select_page(1, "Rig Lab STRUCTURE page");
    select_page(2, "Rig Lab MOTORS page");
    select_page(3, "Rig Lab ART page");
    select_page(4, "Rig Lab TEST page");
    select_page(0, "Rig Lab PRESETS page after repeated switching");
    select_workspace(ui_layout::RigLabWorkspacePage::trainer);
    require_visible(application.vertices(), rig_trainer,
        "trainer statistics remain reachable after repeated Inspector page switches");
    select_workspace(ui_layout::RigLabWorkspacePage::inspector);

    {
        constexpr int minimum_width =
            static_cast<int>(ui_layout::minimum_window_width);
        constexpr int minimum_height =
            static_cast<int>(ui_layout::minimum_window_height);
        const ui_layout::Box minimum_content = ui_layout::content_box(
            static_cast<float>(minimum_width),
            static_cast<float>(minimum_height));
        const ui_layout::RigLabWorkspaceLayout minimum_inspector =
            ui_layout::rig_lab_workspace_layout(minimum_content,
                ui_layout::RigLabWorkspacePage::inspector);
        const ui_layout::RigLabWorkspaceLayout minimum_trainer =
            ui_layout::rig_lab_workspace_layout(minimum_content,
                ui_layout::RigLabWorkspacePage::trainer);
        const ui_layout::RigLabWorkspaceNavigationLayout minimum_navigation =
            ui_layout::rig_lab_workspace_navigation_layout(
                minimum_inspector.workspace_bar);
        require(application.prepare_rig_lab_page(
                RigLabStartupPage::presets, 0u),
            "could not prepare minimum-size workspace reachability fixture");
        application.frame({}, 0.0f, minimum_width, minimum_height);
        require_visible(application.vertices(), minimum_inspector.workspace_bar,
            "minimum-size workspace selector");
        require_visible(application.vertices(), minimum_inspector.inspector,
            "minimum-size Rig Inspector");
        require_visible(application.vertices(), minimum_inspector.world,
            "minimum-size Rig Lab preview");
        InputState minimum_trainer_click{};
        minimum_trainer_click.left_pressed = true;
        minimum_trainer_click.mouse = {
            minimum_navigation.trainer_button.x
                + minimum_navigation.trainer_button.width * 0.5f,
            minimum_navigation.trainer_button.y
                + minimum_navigation.trainer_button.height * 0.5f };
        application.frame(minimum_trainer_click, 0.0f,
            minimum_width, minimum_height);
        application.frame({}, 0.0f, minimum_width, minimum_height);
        require_visible(application.vertices(), minimum_trainer.trainer,
            "minimum-size Trainer statistics workspace");
        require_visible(application.vertices(), minimum_trainer.world,
            "minimum-size preview beside Trainer statistics");

        constexpr int default_width = ui_layout::default_window_width;
        constexpr int default_height = ui_layout::default_window_height;
        const ui_layout::RigLabWorkspaceLayout default_layout =
            ui_layout::rig_lab_workspace_layout(ui_layout::content_box(
                    static_cast<float>(default_width),
                    static_cast<float>(default_height)),
                ui_layout::RigLabWorkspacePage::trainer);
        application.frame({}, 0.0f, default_width, default_height);
        require(default_layout.compact
                && default_layout.workspace_bar.width > 0.0f
                && default_layout.inspector.width == 0.0f,
            "default high-DPI desktop reopened the cluttered dual-inspector wall");
        require_visible(application.vertices(), default_layout.workspace_bar,
            "default-size workspace selector");
        require_visible(application.vertices(), default_layout.trainer,
            "default-size selected Trainer workspace");
        require_visible(application.vertices(), default_layout.world,
            "default-size Rig Lab preview");

        // Some Windows hosts report the complete 3840x2160 framebuffer while
        // others report its 2560x1440 DPI-independent canvas. The application
        // must render and hit-test both as one physical presentation.
        constexpr int fullscreen_width = 3840;
        constexpr int fullscreen_height = 2160;
        constexpr ui_layout::PresentationSurface fullscreen_surface =
            ui_layout::presentation_surface(
                fullscreen_width, fullscreen_height);
        const ui_layout::Box fullscreen_content = ui_layout::content_box(
            static_cast<float>(fullscreen_surface.canvas_width),
            static_cast<float>(fullscreen_surface.canvas_height));
        const ui_layout::RigLabWorkspaceLayout fullscreen_trainer_design =
            ui_layout::rig_lab_workspace_layout(fullscreen_content,
                ui_layout::RigLabWorkspacePage::trainer);
        const auto presented = [&](ui_layout::Box box) noexcept
        {
            return ui_layout::presentation_box(box, fullscreen_surface);
        };
        const ui_layout::Box fullscreen_trainer = presented(
            fullscreen_trainer_design.trainer);
        const ui_layout::Box fullscreen_inspector = presented(
            fullscreen_trainer_design.inspector);
        const ui_layout::Box fullscreen_world = presented(
            fullscreen_trainer_design.world);
        const ui_layout::Box fullscreen_setup_design =
            ui_layout::rig_lab_dual_setup_button_layout(
                fullscreen_trainer_design.trainer);
        const ui_layout::Box fullscreen_setup = presented(
            fullscreen_setup_design);
        InputState fullscreen_trainer_top{};
        fullscreen_trainer_top.mouse = {
            fullscreen_trainer.x + fullscreen_trainer.width * 0.5f,
            fullscreen_trainer.y + fullscreen_trainer.height * 0.5f };
        fullscreen_trainer_top.wheel = 100.0f;
        application.frame(fullscreen_trainer_top, 0.0f,
            fullscreen_width, fullscreen_height);
        application.frame({}, 0.0f, fullscreen_width, fullscreen_height);
        require(fullscreen_surface.density_scaled
                && fullscreen_surface.canvas_width == 2560
                && fullscreen_surface.canvas_height == 1440
                && !fullscreen_trainer_design.compact
                && fullscreen_trainer_design.live.width == 0.0f
                && fullscreen_trainer_design.workspace_bar.width == 0.0f
                && fullscreen_trainer.width >= 900.0f
                && fullscreen_inspector.width >= 800.0f
                && fullscreen_world.width >= 1900.0f
                && fullscreen_world.width > fullscreen_trainer.width,
            "native 4K presentation lost its simultaneous production panes");
        require_visible(application.vertices(), fullscreen_trainer,
            "native 4K Trainer workspace");
        require_visible(application.vertices(), fullscreen_inspector,
            "native 4K Rig Inspector workspace");
        require_visible(application.vertices(), fullscreen_world,
            "native 4K Rig Lab preview");
        require_rasterized_text(application.vertices(), fullscreen_setup,
            "native 4K Setup action");

        const ui_layout::TrainerControlGroupLayout fullscreen_groups_design =
            ui_layout::trainer_control_group_layout(
                fullscreen_trainer_design.trainer, 0.0f, true);
        const ui_layout::TrainerSummaryLayout fullscreen_summary_design =
            ui_layout::trainer_summary_layout(
                fullscreen_groups_design.page_body);
        const auto fullscreen_pages_design =
            ui_layout::trainer_page_button_layout(
                fullscreen_groups_design.page_tabs);
        require(fullscreen_groups_design.primary.width > 0.0f
                && fullscreen_groups_design.primary.height > 0.0f
                && fullscreen_groups_design.secondary.width == 0.0f
                && !ui_layout::overlaps(fullscreen_groups_design.page_tabs,
                    fullscreen_summary_design.card),
            "embedded Trainer lost its Session/statistics card");
        require_rasterized_text(application.vertices(),
            presented(fullscreen_groups_design.page_tabs),
            "native 4K progressive-disclosure navigation");
        require_rasterized_text(application.vertices(),
            presented(fullscreen_summary_design.headline),
            "native 4K Summary headline");
        require_rasterized_text(application.vertices(),
            presented(fullscreen_summary_design.evidence),
            "native 4K Summary evidence");
        require_rasterized_text(application.vertices(),
            presented(fullscreen_summary_design.retained_controller),
            "native 4K Summary retained controller");
        require_rasterized_text(application.vertices(),
            presented(fullscreen_summary_design.next_goal),
            "native 4K Summary next goal");
        const std::uint64_t fullscreen_status_signature = region_signature(
            application.vertices(), fullscreen_trainer);
        const std::uint64_t fullscreen_rig_signature =
            application.rig_preview_signature();

        const std::array<float, 3> page_heights{
            fullscreen_summary_design.card.height,
            ui_layout::trainer_mission_card_height,
            ui_layout::trainer_advanced_card_height };
        constexpr std::array<std::string_view, 3> page_names{
            "Status", "Mission", "Details" };
        for (std::size_t page_index = 1u;
            page_index < fullscreen_pages_design.size(); ++page_index)
        {
            const ui_layout::Box page_button = presented(
                fullscreen_pages_design[page_index]);
            InputState page_click{};
            page_click.left_pressed = true;
            page_click.mouse = {
                page_button.x + page_button.width * 0.5f,
                page_button.y + page_button.height * 0.5f };
            application.frame(page_click, 0.0f,
                fullscreen_width, fullscreen_height);
            application.frame({}, 0.0f,
                fullscreen_width, fullscreen_height);
            const ui_layout::Box page_card = presented({
                fullscreen_groups_design.page_body.x,
                fullscreen_groups_design.page_body.y,
                fullscreen_groups_design.page_body.width,
                std::min(page_heights[page_index],
                    fullscreen_trainer_design.trainer.y
                        + fullscreen_trainer_design.trainer.height
                        - fullscreen_groups_design.page_body.y) });
            require_rasterized_text(application.vertices(), page_card,
                std::format("native 4K {} page", page_names[page_index]));
        }
        require(region_signature(application.vertices(), fullscreen_trainer)
                    != fullscreen_status_signature
                && application.rig_preview_signature()
                    == fullscreen_rig_signature,
            "4K Trainer page navigation is inert or mutates rig identity");

        const auto click_control = [&](ui_layout::Box design_box)
        {
            const ui_layout::Box target = presented(design_box);
            InputState click{};
            click.left_pressed = true;
            click.mouse = { target.x + target.width * 0.5f,
                target.y + target.height * 0.5f };
            application.frame(click, 0.0f,
                fullscreen_width, fullscreen_height);
            application.frame({}, 0.0f,
                fullscreen_width, fullscreen_height);
        };
        const std::uint64_t trainer_before_setup = region_signature(
            application.vertices(), fullscreen_trainer);
        const std::uint64_t inspector_before_setup = region_signature(
            application.vertices(), fullscreen_inspector);
        click_control(fullscreen_setup_design);
        const ui_layout::RigLabControlDrawerLayout fullscreen_drawer =
            ui_layout::rig_lab_control_drawer_layout(
                fullscreen_trainer_design.trainer);
        require(region_signature(application.vertices(), fullscreen_trainer)
                    != trainer_before_setup
                && region_signature(application.vertices(), fullscreen_inspector)
                    == inspector_before_setup,
            "4K Setup drawer is inert or hides/mutates the Inspector");
        require_visible(application.vertices(), fullscreen_trainer,
            "native 4K Setup drawer");
        require_visible(application.vertices(), fullscreen_inspector,
            "native 4K Inspector retained beside Setup");
        require_visible(application.vertices(), fullscreen_world,
            "native 4K preview retained beside Setup");
        click_control(fullscreen_drawer.authority_button);
        click_control(fullscreen_drawer.authority_button);
        click_control(fullscreen_drawer.profile_button);
        click_control(fullscreen_drawer.profile_button);
        click_control(fullscreen_drawer.profile_button);
        click_control(fullscreen_drawer.gait_buttons[0]);
        click_control(fullscreen_drawer.rig_art_button);
        click_control(fullscreen_drawer.rig_art_button);
        click_control(fullscreen_setup_design);
        require_rasterized_text(application.vertices(),
            presented(fullscreen_groups_design.primary),
            "native 4K restored Trainer session controls");
        require_visible(application.vertices(), fullscreen_inspector,
            "native 4K Inspector retained after Setup routing");
        require(application.rig_preview_signature() == fullscreen_rig_signature,
            "persistent 4K commands changed the selected rig identity");
    }

    {
        // Exercise the genuinely oversized four-pane path because it is the
        // only layout
        // where a stale projection can make Live render the prior species while
        // the editor renders the newly selected canonical rig. Start from an
        // exact unscrolled Fit view so button coordinates and pixel evidence
        // are deterministic.
        InputState reset_view{};
        reset_view.reset_pressed = true;
        application.frame(reset_view, 0.0f, width, height);
        require(application.prepare_rig_lab_page(
                RigLabStartupPage::art, 0u),
            "could not prepare Human Art page for same-frame parity coverage");

        constexpr int wide_width = 6144;
        constexpr int wide_height = 3456;
        const ui_layout::Box wide_content = ui_layout::content_box(
            static_cast<float>(wide_width), static_cast<float>(wide_height));
        const ui_layout::Box wide_live = ui_layout::rig_lab_live_box(wide_content);
        const ui_layout::Box wide_trainer =
            ui_layout::rig_lab_trainer_panel_box(wide_content);
        const ui_layout::Box wide_panel =
            ui_layout::rig_lab_panel_box(wide_content);
        const ui_layout::Box wide_world =
            ui_layout::rig_lab_world_box(wide_content);
        require(ui_layout::rig_lab_shows_live(wide_content),
            "ultrawide parity fixture unexpectedly hid the Live pane");

        const auto subject_region = [](ui_layout::Box viewport) noexcept
        {
            return ui_layout::Box{
                viewport.x + viewport.width * 0.25f,
                viewport.y + viewport.height * 0.16f,
                viewport.width * 0.50f,
                viewport.height * 0.62f };
        };
        const ui_layout::Box live_subject = subject_region(wide_live);
        const ui_layout::Box editor_subject = subject_region(wide_world);
        // Stabilize the preview before proving the embedded Session target.
        // Stopping an active worker can legitimately publish its last physical
        // frame, so take the comparison baseline only after that handoff.
        InputState stop_wide_training{};
        stop_wide_training.space_pressed = true;
        application.frame(stop_wide_training, 0.0f,
            wide_width, wide_height);
        application.frame({}, 0.0f, wide_width, wide_height);
        require_visible(application.vertices(), wide_live,
            "ultrawide retained Live pane");
        require_rasterized_text(application.vertices(), wide_trainer,
            "ultrawide Trainer pane");
        require_visible(application.vertices(), wide_panel,
            "ultrawide Rig Inspector pane");
        require_visible(application.vertices(), wide_world,
            "ultrawide Rig Lab preview");
        const std::uint64_t fit_live = region_signature(
            application.vertices(), live_subject);
        const std::uint64_t fit_editor = region_signature(
            application.vertices(), editor_subject);
        const std::uint64_t wide_rig_signature =
            application.rig_preview_signature();

        const ui_layout::TrainerControlGroupLayout wide_trainer_controls =
            ui_layout::trainer_control_group_layout(
                wide_trainer, 0.0f, true, false);
        const ui_layout::Box wide_session_button{
            wide_trainer_controls.primary.x + 14.0f,
            wide_trainer_controls.primary.y + 40.0f,
            std::max(0.0f, wide_trainer_controls.primary.width - 14.0f),
            ui_layout::trainer_primary_button_height
        };
        require_rasterized_text(application.vertices(), wide_session_button,
            "ultrawide Rig Lab Start/Pause training action");
        const std::uint64_t session_before = region_signature(
            application.vertices(), wide_session_button);
        InputState toggle_wide_session{};
        toggle_wide_session.left_pressed = true;
        toggle_wide_session.mouse = {
            wide_session_button.x + wide_session_button.width * 0.5f,
            wide_session_button.y + wide_session_button.height * 0.5f };
        application.frame(toggle_wide_session, 0.0f,
            wide_width, wide_height);
        const std::uint64_t session_paused = region_signature(
            application.vertices(), wide_session_button);
        if (session_paused == session_before
            || application.rig_preview_signature() != wide_rig_signature)
        {
            std::cerr << "FAILED: ultrawide Rig Lab Start/Pause did not "
                "toggle visibly or changed the selected rig; session="
                << session_before << "->" << session_paused
                << " rig=" << wide_rig_signature << "->"
                << application.rig_preview_signature() << '\n';
            return EXIT_FAILURE;
        }
        // Resuming the real worker may immediately publish a newly advanced
        // physical pose. That is expected; the editor must stay populated and
        // the selected canonical rig must not change.
        require_visible(application.vertices(), wide_world,
            "ultrawide Rig Lab preview after starting training");
        application.frame(toggle_wide_session, 0.0f,
            wide_width, wide_height);
        // Clear hover-only pixels before comparing the restored inactive
        // Session target with its no-hover baseline.
        application.frame({}, 0.0f, wide_width, wide_height);
        require(region_signature(application.vertices(), wide_session_button)
                    == session_before
                && application.rig_preview_signature() == wide_rig_signature,
            "ultrawide Rig Lab Start/Pause did not restore its state or changed the selected rig");
        require_visible(application.vertices(), wide_world,
            "ultrawide Rig Lab preview after Start/Pause round trip");

        const ui_layout::RigPreviewToolbarLayout preview_toolbar =
            ui_layout::rig_preview_toolbar_layout(wide_world);
        InputState zoom_in{};
        zoom_in.left_pressed = true;
        zoom_in.mouse = {
            preview_toolbar.zoom_in.x
                + preview_toolbar.zoom_in.width * 0.5f,
            preview_toolbar.zoom_in.y
                + preview_toolbar.zoom_in.height * 0.5f };
        application.frame(zoom_in, 0.0f, wide_width, wide_height);
        const std::uint64_t zoomed_live = region_signature(
            application.vertices(), live_subject);
        const std::uint64_t zoomed_editor = region_signature(
            application.vertices(), editor_subject);
        require(zoomed_live != fit_live && zoomed_editor != fit_editor,
            "same-event editor zoom did not update both shared Rig Lab panes");

        InputState fit_view{};
        fit_view.left_pressed = true;
        fit_view.mouse = {
            preview_toolbar.fit.x + preview_toolbar.fit.width * 0.5f,
            preview_toolbar.fit.y + preview_toolbar.fit.height * 0.5f };
        application.frame(fit_view, 0.0f, wide_width, wide_height);
        require(region_signature(application.vertices(), live_subject)
                    != zoomed_live
                && region_signature(application.vertices(), editor_subject)
                    != zoomed_editor,
            "Fit 1.00X did not synchronously restore the shared projection");

        const float panel_usable = wide_panel.width - 36.0f;
        const ui_layout::RigArtActionLayout art_actions =
            ui_layout::rig_art_action_layout({
                wide_panel.x + 18.0f,
                wide_panel.y
                    + ui_layout::rig_inspector_header_height_for_width(
                        wide_panel.width)
                    + 18.0f + 120.0f + 16.0f + 60.0f,
                panel_usable, 0.0f });
        const ui_layout::Box freeze_button = art_actions.freeze;
        InputState freeze_play{};
        freeze_play.left_pressed = true;
        freeze_play.mouse = {
            freeze_button.x + freeze_button.width * 0.5f,
            freeze_button.y + freeze_button.height * 0.5f };
        application.frame(freeze_play, 0.0f, wide_width, wide_height);
        const std::uint64_t frozen_live = region_signature(
            application.vertices(), live_subject);
        const std::uint64_t frozen_editor = region_signature(
            application.vertices(), editor_subject);
        const std::uint64_t frozen_controls = region_signature(
            application.vertices(), freeze_button);
        for (int frame = 0; frame < 30; ++frame)
            application.frame({}, dt, wide_width, wide_height);
        require(region_signature(application.vertices(), live_subject)
                    == frozen_live
                && region_signature(application.vertices(), editor_subject)
                    == frozen_editor,
            "frozen shared Rig Lab pose advanced in one of its panes");
        application.frame(freeze_play, 0.0f, wide_width, wide_height);
        const std::uint64_t released_controls = region_signature(
            application.vertices(), freeze_button);
        for (int frame = 0; frame < 30; ++frame)
            application.frame({}, dt, wide_width, wide_height);
        require(released_controls != frozen_controls
                && region_signature(application.vertices(), live_subject) != 0u
                && region_signature(application.vertices(), editor_subject) != 0u,
            "PLAY did not release the frozen control or left a dangling shared projection");

        const float wide_tab = (panel_usable - 24.0f) * 0.20f;
        InputState presets_tab{};
        presets_tab.left_pressed = true;
        presets_tab.mouse = { wide_panel.x + 18.0f + wide_tab * 0.5f,
            wide_panel.y + 48.0f + 24.0f };
        application.frame(presets_tab, 0.0f, wide_width, wide_height);
        const std::uint64_t human_live = region_signature(
            application.vertices(), live_subject);
        const std::uint64_t human_editor = region_signature(
            application.vertices(), editor_subject);

        const float preset_page_usable = panel_usable - 28.0f;
        const float preset_half = (preset_page_usable - 6.0f) * 0.5f;
        InputState dog_preset{};
        dog_preset.left_pressed = true;
        dog_preset.mouse = {
            wide_panel.x + 32.0f + preset_half * 0.5f,
            wide_panel.y
                + ui_layout::rig_inspector_header_height_for_width(
                    wide_panel.width)
                + 18.0f + 120.0f + 16.0f
                + 58.0f + 25.0f };
        application.frame(dog_preset, 0.0f, wide_width, wide_height);
        require(application.rig_preview_signature()
                    == sim::CreatureBlueprint::crawler4().signature()
                && region_signature(application.vertices(), live_subject)
                    != human_live
                && region_signature(application.vertices(), editor_subject)
                    != human_editor,
            "same-event Dog preset left Live or editor on the prior Human snapshot");
    }

    {
        // A failed physical Test is evidence, not an animation loop boundary.
        // Force a genuine Environment terminal and prove repeated UI frames do
        // not replace its cause, pose, or generation until the user presses R.
        require(application.prepare_rig_lab_page(
                RigLabStartupPage::test, 0u),
            "could not prepare Human physical Test terminal fixture");
        application.frame({}, 0.0f, width, height);
        const std::uint64_t initial_generation =
            application.rig_preview_terminal_evidence()
                .physical_test_generation;
        require(application.prepare_forced_rig_test_terminal(),
            "could not force the physical Test into a deterministic terminal pose");
        for (int frame = 0; frame < 30
            && !application.rig_preview_terminal_evidence().frozen; ++frame)
            application.frame({}, dt, width, height);
        const RigPreviewTerminalEvidence terminal =
            application.rig_preview_terminal_evidence();
        require(terminal.frozen
                && terminal.cause != sim::TrialTerminalCause::none
                && terminal.reason != sim::InvalidMotion::none
                && terminal.physical_test_generation == initial_generation
                && terminal.pose_signature != 0u,
            "forced physical Test did not retain its exact terminal evidence");
        const std::uint64_t terminal_frame = region_signature(
            application.vertices(), rig_world);
        for (int frame = 0; frame < 120; ++frame)
            application.frame({}, dt, width, height);
        const RigPreviewTerminalEvidence held =
            application.rig_preview_terminal_evidence();
        require(held.frozen
                && held.cause == terminal.cause
                && held.reason == terminal.reason
                && held.position.x == terminal.position.x
                && held.position.y == terminal.position.y
                && held.terrain == terminal.terrain
                && held.water_depth == terminal.water_depth
                && held.physical_test_generation
                    == terminal.physical_test_generation
                && held.pose_signature == terminal.pose_signature
                && region_signature(application.vertices(), rig_world)
                    == terminal_frame,
            "physical Test silently recycled or altered its frozen terminal pose");

        // At the wide combined-layout threshold the same frozen Test is also
        // rendered in the Rig Lab Live PIP. Exercise the real raster lanes so
        // the terminal label/evidence fix cannot regress into geometry-only
        // coverage that still produces an empty or clipped card at runtime.
        constexpr int terminal_wide_width = 6144;
        constexpr int terminal_wide_height = 3456;
        const ui_layout::Box terminal_wide_content = ui_layout::content_box(
            static_cast<float>(terminal_wide_width),
            static_cast<float>(terminal_wide_height));
        require(ui_layout::rig_lab_shows_live(terminal_wide_content),
            "wide frozen-terminal fixture did not expose the Rig Lab Live pane");
        const ui_layout::Box terminal_live =
            ui_layout::rig_lab_live_box(terminal_wide_content);
        const ui_layout::Box terminal_pip =
            ui_layout::training_pip_box(terminal_live);
        application.frame({}, 0.0f, terminal_wide_width,
            terminal_wide_height);
        require_rasterized_text(application.vertices(),
            ui_layout::training_pip_title_box(terminal_pip),
            "frozen physical Test PIP title lane");
        require_rasterized_text(application.vertices(),
            ui_layout::training_pip_state_box(terminal_pip),
            "frozen physical Test PIP state lane");
        require_rasterized_text(application.vertices(),
            ui_layout::training_pip_footer_box(terminal_pip),
            "frozen physical Test PIP evidence/footer lane");
        const RigPreviewTerminalEvidence wide_held =
            application.rig_preview_terminal_evidence();
        require(wide_held.frozen
                && wide_held.physical_test_generation
                    == terminal.physical_test_generation
                && wide_held.pose_signature == terminal.pose_signature,
            "rendering the frozen terminal PIP advanced or replaced its Test");
        application.frame({}, 0.0f, width, height);

        // The modern Inspector keeps its page selector sticky and puts the
        // Test card in the independently scrolling body. Reach the terminal
        // controls through that real viewport instead of clicking an
        // off-screen legacy coordinate.
        constexpr float terminal_scroll_offset =
            ui_layout::panel_scroll_step * 3.0f;
        InputState scroll_terminal_controls{};
        scroll_terminal_controls.mouse = {
            rig_panel.x + rig_panel.width * 0.5f,
            rig_panel.y
                + ui_layout::rig_inspector_header_height_for_width(
                    rig_panel.width) + 40.0f };
        scroll_terminal_controls.wheel = -3.0f;
        application.frame(scroll_terminal_controls, 0.0f, width, height);
        application.frame({}, 0.0f, width, height);
        const std::uint64_t scrolled_terminal_frame = region_signature(
            application.vertices(), rig_world);

        const ui_layout::Box terminal_test_card{
            rig_panel.x + 18.0f,
            rig_panel.y
                + ui_layout::rig_inspector_header_height_for_width(
                    rig_panel.width)
                + 18.0f + 120.0f + 16.0f,
            rig_panel.width - 36.0f,
            ui_layout::rig_lab_test_card_height };
        const ui_layout::RigLabTestLayout terminal_controls =
            ui_layout::rig_lab_test_layout(terminal_test_card);
        InputState blocked_gait{};
        blocked_gait.left_pressed = true;
        blocked_gait.mouse = {
            terminal_controls.pattern_row.x
                + terminal_controls.pattern_row.width * 0.375f,
            terminal_controls.pattern_row.y
                + terminal_controls.pattern_row.height * 0.5f
                - terminal_scroll_offset };
        application.frame(blocked_gait, 0.0f, width, height);
        const RigPreviewTerminalEvidence after_blocked_gait =
            application.rig_preview_terminal_evidence();
        require(after_blocked_gait.frozen
                && after_blocked_gait.cause == terminal.cause
                && after_blocked_gait.reason == terminal.reason
                && after_blocked_gait.physical_test_generation
                    == terminal.physical_test_generation
                && after_blocked_gait.pose_signature == terminal.pose_signature
                && region_signature(application.vertices(), rig_world)
                    == scrolled_terminal_frame,
            "disabled gait scrub escaped the explicit Retry/R terminal gate");
        InputState retry_button{};
        retry_button.left_pressed = true;
        retry_button.mouse = {
            terminal_controls.manual_slider.x
                + terminal_controls.manual_slider.width * 0.5f,
            terminal_controls.manual_slider.y
                + terminal_controls.manual_slider.height * 0.5f
                - terminal_scroll_offset };
        application.frame(retry_button, 0.0f, width, height);
        const RigPreviewTerminalEvidence button_restarted =
            application.rig_preview_terminal_evidence();
        require(!button_restarted.frozen
                && button_restarted.cause == sim::TrialTerminalCause::none
                && button_restarted.reason == sim::InvalidMotion::none
                && button_restarted.physical_test_generation
                    == terminal.physical_test_generation + 1u
                && button_restarted.pose_signature != terminal.pose_signature,
            "explicit Retry button did not start exactly one fresh physical Test generation");

        require(application.prepare_forced_rig_test_terminal(),
            "could not prepare the second forced terminal for R coverage");
        for (int frame = 0; frame < 30
            && !application.rig_preview_terminal_evidence().frozen; ++frame)
            application.frame({}, dt, width, height);
        const RigPreviewTerminalEvidence before_r =
            application.rig_preview_terminal_evidence();
        require(before_r.frozen,
            "second forced physical Test did not become terminal");
        InputState retry_key{};
        retry_key.reset_pressed = true;
        application.frame(retry_key, 0.0f, width, height);
        const RigPreviewTerminalEvidence key_restarted =
            application.rig_preview_terminal_evidence();
        require(!key_restarted.frozen
                && key_restarted.cause == sim::TrialTerminalCause::none
                && key_restarted.reason == sim::InvalidMotion::none
                && key_restarted.physical_test_generation
                    == before_r.physical_test_generation + 1u,
            "explicit R did not start exactly one fresh physical Test generation");
    }

    {
        // Rig Lab borrows one fitted camera for its Live/editor parity panes,
        // but that borrowed projection must never become the user's Live camera.
        Application camera_application{};
        std::string error{};
        require(camera_application.initialize("assets", error,
                presentation::StartupMode::isolated_visual_proof),
            "camera round-trip application initialization failed");
        require(camera_application.prepare_rig_lab_page(
                RigLabStartupPage::art, 0u),
            "could not prepare Rig Lab camera round-trip fixture");

        constexpr int wide_width = 3840;
        constexpr int wide_height = 2160;
        constexpr ui_layout::PresentationSurface camera_surface =
            ui_layout::presentation_surface(wide_width, wide_height);
        const ui_layout::Box wide_content = ui_layout::content_box(
            static_cast<float>(camera_surface.canvas_width),
            static_cast<float>(camera_surface.canvas_height));
        const ui_layout::Box wide_live_world =
            ui_layout::presentation_box(
                ui_layout::live_world_box(wide_content), camera_surface);
        const ui_layout::Box wide_rig_world =
            ui_layout::presentation_box(
                ui_layout::rig_lab_workspace_layout(wide_content,
                    ui_layout::RigLabWorkspacePage::inspector).world,
                camera_surface);

        InputState switch_to_live{};
        switch_to_live.tab_pressed = true;
        camera_application.frame(switch_to_live, 0.0f,
            wide_width, wide_height);
        InputState manual_live_zoom{};
        manual_live_zoom.mouse = { wide_live_world.x
                + wide_live_world.width * 0.5f,
            wide_live_world.y + wide_live_world.height * 0.5f };
        manual_live_zoom.wheel = 2.0f;
        camera_application.frame(manual_live_zoom, 0.0f,
            wide_width, wide_height);
        const LiveCameraEvidence manual =
            camera_application.live_camera_evidence();
        require(!manual.auto_view && manual.zoom_factor > 1.0f
                && manual.projection_initialized,
            "Live manual zoom fixture did not own a stable camera state");
        const auto same_camera = [](const LiveCameraEvidence& lhs,
            const LiveCameraEvidence& rhs) noexcept
        {
            return lhs.camera_x == rhs.camera_x
                && lhs.pixels_per_meter == rhs.pixels_per_meter
                && lhs.zoom_factor == rhs.zoom_factor
                && lhs.auto_view == rhs.auto_view
                && lhs.projection_initialized == rhs.projection_initialized;
        };

        InputState switch_to_rig_lab{};
        switch_to_rig_lab.tab_pressed = true;
        camera_application.frame(switch_to_rig_lab, 0.0f,
            wide_width, wide_height);
        require(same_camera(camera_application.live_camera_evidence(), manual),
            "entering Rig Lab overwrote the independent Live camera");
        InputState editor_zoom{};
        editor_zoom.mouse = { wide_rig_world.x + wide_rig_world.width * 0.5f,
            wide_rig_world.y + wide_rig_world.height * 0.5f };
        editor_zoom.wheel = 3.0f;
        camera_application.frame(editor_zoom, 0.0f,
            wide_width, wide_height);
        for (int frame = 0; frame < 12; ++frame)
            camera_application.frame({}, 0.0f, wide_width, wide_height);
        require(same_camera(camera_application.live_camera_evidence(), manual),
            "Rig Lab editor zoom or shared Live pane leaked into Live camera state");

        camera_application.frame(switch_to_live, 0.0f,
            wide_width, wide_height);
        require(same_camera(camera_application.live_camera_evidence(), manual),
            "Live -> Rig Lab zoom -> Live did not preserve manual zoom/follow state");
    }

    constexpr std::array pages{ RigLabStartupPage::test,
        RigLabStartupPage::art, RigLabStartupPage::structure,
        RigLabStartupPage::motors, RigLabStartupPage::presets };
    const std::array signatures{
        sim::CreatureBlueprint::humanoid().signature(),
        sim::CreatureBlueprint::chicken().signature(),
        sim::CreatureBlueprint::crawler4().signature(),
        sim::CreatureBlueprint::hexapod().signature() };
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        for (std::size_t species = 0; species < signatures.size(); ++species)
        {
            require(application.prepare_rig_lab_page(
                    pages[(species + static_cast<std::size_t>(repeat))
                        % pages.size()], species)
                    && application.rig_preview_signature()
                        == signatures[species],
                "canonical Rig Lab request rendered a stale species snapshot");
        }
    }
    application.frame({}, 0.0f, width, height);
    require_visible(application.vertices(), footer,
        "reserved wrapped status/toast footer");
    require(!ui_layout::overlaps(footer,
            ui_layout::bottom_telemetry_box(rig_world)),
        "status/toast footer overlaps Rig Lab content telemetry");

    std::cout << "Runner v0.7.23 rounded-outline and final-frame tests passed\n";
    return EXIT_SUCCESS;
}
