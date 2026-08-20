#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace runner::ui_layout
{
    struct Box
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct SurfaceScale
    {
        float x{ 1.0f };
        float y{ 1.0f };
    };
    struct BlueprintFit
    {
        float camera_x{};
        float world_center_y{};
        float pixels_per_meter{ 42.0f };
        float content_center_y{};
    };

    struct RigLabTestLayout
    {
        Box selection_row{};
        Box range_row{};
        Box pattern_row{};
        Box status_row{};
        Box manual_slider{};
    };

    inline constexpr float minimum_window_width = 1280.0f;
    inline constexpr float minimum_window_height = 820.0f;
    inline constexpr float minimum_content_width = 1150.0f;
    inline constexpr float minimum_content_height = 700.0f;
    inline constexpr float top_bar_height = 78.0f;
    inline constexpr float content_margin = 12.0f;
    inline constexpr float panel_gap = 12.0f;
    inline constexpr float card_margin = 14.0f;
    inline constexpr float bottom_telemetry_height = 44.0f;
    inline constexpr float minimum_readable_text_scale = 0.78f;

    enum class DistanceUnits { metric, imperial };

    inline constexpr std::array<std::string_view, 3> training_totals_scope_headings{
        "SELECTED RIG - THIS SELECTION",
        "THIS APP SESSION - ALL RIGS",
        "SELECTED RIG - LIFETIME"
    };
    inline constexpr std::string_view totals_wall_time_label = "WALL TIME";
    inline constexpr std::string_view totals_agent_sim_time_label = "AGENT-SIM TIME";
    inline constexpr std::string_view totals_support_cycles_label = "SUPPORT GAIT CYCLES";

    [[nodiscard]] constexpr Box top_bar_box(float window_width) noexcept
    {
        return { 0.0f, 0.0f, std::max(0.0f, window_width), top_bar_height };
    }

    [[nodiscard]] constexpr float course_reference_marker_spacing_m(
        DistanceUnits units) noexcept
    {
        return units == DistanceUnits::metric ? 10.0f : 15.24f;
    }

    [[nodiscard]] constexpr std::uint64_t lifetime_delta(
        std::uint64_t total, std::uint64_t start) noexcept
    {
        return total >= start ? total - start : 0u;
    }

    [[nodiscard]] constexpr double lifetime_delta_value(
        double total, double start) noexcept
    {
        return total >= start ? total - start : 0.0;
    }

    struct TrainingTotals
    {
        std::uint64_t optimizer_updates{};
        std::uint64_t completed_agent_runs{};
        std::uint64_t passed_stage_checks{};
        std::uint64_t failed_stage_checks{};
        std::uint64_t episode_restarts{};
        std::uint64_t support_gait_cycles{};
        std::uint64_t falls{};
        std::uint64_t collisions{};
        std::uint64_t features_cleared{};
        std::uint64_t rollbacks{};
        double agent_sim_seconds{};
        double agent_distance{};
    };

    [[nodiscard]] constexpr bool training_totals_consistent(
        const TrainingTotals& totals) noexcept
    {
        return totals.completed_agent_runs
            == totals.passed_stage_checks + totals.failed_stage_checks;
    }

    [[nodiscard]] constexpr TrainingTotals training_totals_delta(
        const TrainingTotals& current, const TrainingTotals& start) noexcept
    {
        return {
            lifetime_delta(current.optimizer_updates, start.optimizer_updates),
            lifetime_delta(current.completed_agent_runs, start.completed_agent_runs),
            lifetime_delta(current.passed_stage_checks, start.passed_stage_checks),
            lifetime_delta(current.failed_stage_checks, start.failed_stage_checks),
            lifetime_delta(current.episode_restarts, start.episode_restarts),
            lifetime_delta(current.support_gait_cycles, start.support_gait_cycles),
            lifetime_delta(current.falls, start.falls),
            lifetime_delta(current.collisions, start.collisions),
            lifetime_delta(current.features_cleared, start.features_cleared),
            lifetime_delta(current.rollbacks, start.rollbacks),
            lifetime_delta_value(current.agent_sim_seconds, start.agent_sim_seconds),
            lifetime_delta_value(current.agent_distance, start.agent_distance)
        };
    }

    constexpr void accumulate_training_totals(
        TrainingTotals& destination, const TrainingTotals& delta) noexcept
    {
        destination.optimizer_updates += delta.optimizer_updates;
        destination.completed_agent_runs += delta.completed_agent_runs;
        destination.passed_stage_checks += delta.passed_stage_checks;
        destination.failed_stage_checks += delta.failed_stage_checks;
        destination.episode_restarts += delta.episode_restarts;
        destination.support_gait_cycles += delta.support_gait_cycles;
        destination.falls += delta.falls;
        destination.collisions += delta.collisions;
        destination.features_cleared += delta.features_cleared;
        destination.rollbacks += delta.rollbacks;
        destination.agent_sim_seconds += delta.agent_sim_seconds;
        destination.agent_distance += delta.agent_distance;
    }

    [[nodiscard]] constexpr bool overlaps(Box a, Box b) noexcept
    {
        return a.x < b.x + b.width && a.x + a.width > b.x
            && a.y < b.y + b.height && a.y + a.height > b.y;
    }

    [[nodiscard]] constexpr bool contains(Box outer, Box inner) noexcept
    {
        return inner.x >= outer.x && inner.y >= outer.y
            && inner.x + inner.width <= outer.x + outer.width
            && inner.y + inner.height <= outer.y + outer.height;
    }

    [[nodiscard]] constexpr Box inset(Box box, float amount) noexcept
    {
        const float inset_amount = std::max(0.0f, amount);
        return { box.x + inset_amount, box.y + inset_amount,
            std::max(0.0f, box.width - inset_amount * 2.0f),
            std::max(0.0f, box.height - inset_amount * 2.0f) };
    }

    [[nodiscard]] constexpr Box content_box(float window_width, float window_height) noexcept
    {
        return { content_margin, top_bar_height,
            std::max(0.0f, window_width - content_margin * 2.0f),
            std::max(0.0f, window_height - top_bar_height - content_margin) };
    }

    [[nodiscard]] constexpr float live_panel_width(float content_width) noexcept
    {
        return std::clamp(content_width * 0.38f, 500.0f, 650.0f);
    }

    [[nodiscard]] constexpr Box live_world_box(Box content) noexcept
    {
        const float panel = live_panel_width(content.width);
        return { content.x, content.y,
            std::max(0.0f, content.width - panel - panel_gap), content.height };
    }

    [[nodiscard]] constexpr Box live_panel_box(Box content) noexcept
    {
        const Box world = live_world_box(content);
        return { world.x + world.width + panel_gap, content.y,
            live_panel_width(content.width), content.height };
    }

    [[nodiscard]] constexpr Box training_pip_box(Box world) noexcept
    {
        const float width = std::clamp(world.width * 0.34f, 280.0f, 420.0f);
        const float height = std::clamp(world.height * 0.28f, 190.0f, 250.0f);
        return { world.x + world.width - width - card_margin,
            world.y + card_margin, width, height };
    }

    [[nodiscard]] constexpr Box primary_telemetry_box(Box world) noexcept
    {
        const Box pip = training_pip_box(world);
        return { world.x + card_margin, world.y + card_margin,
            std::max(260.0f, pip.x - world.x - card_margin * 2.0f), pip.height };
    }

    [[nodiscard]] constexpr Box bottom_telemetry_box(Box world) noexcept
    {
        return { world.x + card_margin,
            world.y + std::max(0.0f, world.height - bottom_telemetry_height - card_margin),
            std::max(0.0f, world.width - card_margin * 2.0f),
            bottom_telemetry_height };
    }

    [[nodiscard]] constexpr SurfaceScale logical_surface_scale(
        float logical_width, float logical_height,
        float surface_width, float surface_height) noexcept
    {
        return {
            logical_width > 0.0f ? surface_width / logical_width : 1.0f,
            logical_height > 0.0f ? surface_height / logical_height : 1.0f
        };
    }

    [[nodiscard]] constexpr bool supported_window(float width, float height) noexcept
    {
        const Box content = content_box(width, height);
        return width >= minimum_window_width
            && height >= minimum_window_height
            && content.width >= minimum_content_width
            && content.height >= minimum_content_height;
    }

    [[nodiscard]] constexpr bool live_layout_valid(float width, float height) noexcept
    {
        if (!supported_window(width, height))
            return false;
        const Box content = content_box(width, height);
        const Box world = live_world_box(content);
        const Box panel = live_panel_box(content);
        const Box pip = training_pip_box(world);
        const Box telemetry = primary_telemetry_box(world);
        const Box bottom = bottom_telemetry_box(world);
        return world.width >= 620.0f && panel.width >= 500.0f
            && contains(content, world) && contains(content, panel)
            && contains(world, pip) && contains(world, telemetry)
            && contains(world, bottom)
            && !overlaps(world, panel)
            && !overlaps(pip, telemetry)
            && !overlaps(pip, bottom)
            && !overlaps(telemetry, bottom);
    }

    [[nodiscard]] constexpr float rig_lab_panel_width(float content_width) noexcept
    {
        return std::clamp(content_width * 0.31f, 420.0f, 560.0f);
    }

    [[nodiscard]] constexpr Box rig_lab_panel_box(Box content) noexcept
    {
        return { content.x, content.y,
            rig_lab_panel_width(content.width), content.height };
    }

    [[nodiscard]] constexpr Box rig_lab_world_box(Box content) noexcept
    {
        const Box panel = rig_lab_panel_box(content);
        return { panel.x + panel.width + panel_gap, content.y,
            std::max(0.0f, content.width - panel.width - panel_gap),
            content.height };
    }

    [[nodiscard]] constexpr RigLabTestLayout rig_lab_test_layout(Box card) noexcept
    {
        const float inset_width = std::max(0.0f, card.width - 28.0f);
        return {
            { card.x + 14.0f, card.y + 39.0f, inset_width, 31.0f },
            { card.x + 14.0f, card.y + 75.0f, inset_width, 31.0f },
            { card.x + 14.0f, card.y + 111.0f, inset_width, 31.0f },
            { card.x + 14.0f, card.y + 149.0f, inset_width, 17.0f },
            { card.x + 14.0f, card.y + 174.0f, inset_width, 36.0f }
        };
    }

    [[nodiscard]] constexpr BlueprintFit fit_blueprint(
        Box viewport, float minimum_x, float maximum_x,
        float minimum_y, float maximum_y) noexcept
    {
        minimum_x = std::min(minimum_x, 0.0f);
        maximum_x = std::max(maximum_x, 0.0f);
        minimum_y = std::min(minimum_y, 0.0f);
        maximum_y = std::max(maximum_y, 0.0f);
        const float horizontal_span = std::max(0.5f, maximum_x - minimum_x);
        const float vertical_span = std::max(0.5f, maximum_y - minimum_y);
        const float usable_width = std::max(1.0f, viewport.width - 96.0f);
        const float content_top = viewport.y + 70.0f;
        const float content_bottom = viewport.y
            + std::max(90.0f, viewport.height - 48.0f);
        const float usable_height = std::max(1.0f, content_bottom - content_top);
        return {
            (minimum_x + maximum_x) * 0.5f,
            (minimum_y + maximum_y) * 0.5f,
            std::clamp(std::min(usable_width / horizontal_span,
                usable_height / vertical_span), 1.0f, 168.0f),
            (content_top + content_bottom) * 0.5f
        };
    }

    [[nodiscard]] constexpr float blueprint_screen_x(
        const BlueprintFit& fit, Box viewport, float world_x) noexcept
    {
        return viewport.x + viewport.width * 0.5f
            + (world_x - fit.camera_x) * fit.pixels_per_meter;
    }

    [[nodiscard]] constexpr float blueprint_screen_y(
        const BlueprintFit& fit, float world_y) noexcept
    {
        return fit.content_center_y
            - (world_y - fit.world_center_y) * fit.pixels_per_meter;
    }

    [[nodiscard]] constexpr bool rig_lab_layout_valid(
        float width, float height) noexcept
    {
        if (!supported_window(width, height))
            return false;
        const Box content = content_box(width, height);
        const Box panel = rig_lab_panel_box(content);
        const Box world = rig_lab_world_box(content);
        return panel.width >= 420.0f && world.width >= 680.0f
            && contains(content, panel) && contains(content, world)
            && !overlaps(panel, world);
    }

    inline constexpr std::array<std::array<float, 2>, 5> validation_sizes{
        std::array<float, 2>{ 1280.0f, 820.0f },
        std::array<float, 2>{ 1600.0f, 900.0f },
        std::array<float, 2>{ 1920.0f, 1080.0f },
        std::array<float, 2>{ 2047.0f, 1112.0f },
        std::array<float, 2>{ 2560.0f, 1440.0f }
    };
}
