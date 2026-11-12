#pragma once

#include <algorithm>
#include <array>
#include <cmath>
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

    struct PresentationSurface
    {
        int canvas_width{};
        int canvas_height{};
        float scale_x{ 1.0f };
        float scale_y{ 1.0f };
        bool density_scaled{};
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

    struct ScrollbarLayout
    {
        Box track{};
        Box thumb{};
        float maximum_offset{};
        bool scrollable{};
    };

    enum class RigLabWorkspacePage : std::uint8_t
    {
        trainer,
        inspector
    };

    struct RigLabWorkspaceLayout
    {
        Box workspace_bar{};
        Box live{};
        Box trainer{};
        Box inspector{};
        Box world{};
        bool compact{};
    };

    struct RigLabWorkspaceNavigationLayout
    {
        Box mission{};
        Box run_button{};
        Box controls_button{};
        Box trainer_button{};
        Box inspector_button{};
    };

    struct RigLabControlDrawerLayout
    {
        Box card{};
        Box heading{};
        std::array<Box, 4> gait_buttons{};
        Box authority_button{};
        Box profile_button{};
        Box rig_art_button{};
    };

    struct RigPreviewToolbarLayout
    {
        Box zoom_out{};
        Box fit{};
        Box zoom_in{};
    };

    struct RigArtActionLayout
    {
        Box skeleton{};
        Box freeze{};
        Box backdrop{};
    };

    struct TrainerControlGroupLayout
    {
        Box primary{};
        // Retained as an intentionally empty compatibility lane. Secondary
        // controls now live on the named View page instead of forming a
        // permanently visible control wall.
        Box secondary{};
        Box page_tabs{};
        Box page_body{};
    };

    struct TrainerSummaryLayout
    {
        Box card{};
        Box heading{};
        Box headline{};
        Box evidence{};
        Box retained_controller{};
        Box next_goal_label{};
        Box next_goal{};
        bool wide{};
    };

    inline constexpr float minimum_window_width = 1280.0f;
    inline constexpr float minimum_window_height = 799.0f;
    inline constexpr int default_window_width = 2048;
    inline constexpr int default_window_height = 1152;
    inline constexpr float minimum_content_width = 1150.0f;
    inline constexpr float minimum_content_height = 620.0f;
    inline constexpr float top_bar_height = 78.0f;
    inline constexpr float content_margin = 12.0f;
    inline constexpr float panel_gap = 12.0f;
    inline constexpr float card_margin = 14.0f;
    inline constexpr float bottom_telemetry_height = 52.0f;
    // Two readable status lines are part of the minimum-height contract. The
    // former 64-pixel lane left only 42 pixels after padding, clipping the
    // second line of long visual-proof receipts.
    inline constexpr float footer_lane_height = 84.0f;
    inline constexpr float pip_header_height = 112.0f;
    inline constexpr float pip_footer_height = 144.0f;
    inline constexpr float pip_compact_footer_height = 72.0f;
    inline constexpr float primary_telemetry_compact_height = 124.0f;
    inline constexpr float panel_scroll_step = 54.0f;
    inline constexpr float scrollbar_minimum_thumb_height = 48.0f;
    inline constexpr float minimum_readable_text_scale = 1.40f;
    inline constexpr float persistent_text_minimum_scale = 1.40f;
    inline constexpr float section_heading_text_scale = 1.70f;
    inline constexpr float workspace_heading_text_scale = 1.90f;
    // A 3840x2160 framebuffer at 150% DPI normalizes to a 2560x1440 logical
    // canvas. At that size the Trainer, Inspector, and physical preview all
    // retain readable widths, so expose the complete production workspace.
    // Default 2048 and compact 1368 surfaces remain focused/switchable.
    inline constexpr float rig_lab_dual_panel_min_content_width = 2400.0f;
    inline constexpr float rig_lab_workspace_bar_height = 80.0f;
    inline constexpr float rig_lab_workspace_bar_gap = 12.0f;
    inline constexpr float rig_inspector_header_height = 232.0f;
    inline constexpr float rig_lab_test_card_height = 336.0f;
    inline constexpr float rig_lab_combined_min_content_width = 5120.0f;
    inline constexpr float presentation_reference_width = 2560.0f;
    inline constexpr float presentation_reference_height = 1440.0f;
    inline constexpr float presentation_scaled_window_limit =
        rig_lab_combined_min_content_width + content_margin * 2.0f;
    inline constexpr float rig_lab_combined_live_ratio = 0.34f;
    inline constexpr float rig_lab_default_zoom = 1.00f;
    inline constexpr float rig_lab_minimum_zoom = 0.28f;
    inline constexpr float rig_lab_maximum_zoom = 3.60f;
    inline constexpr float trainer_primary_card_height = 240.0f;
    inline constexpr float trainer_secondary_card_height = 286.0f;
    inline constexpr float trainer_control_group_gap = 14.0f;
    inline constexpr float trainer_page_tab_height = 104.0f;
    inline constexpr float trainer_page_tab_button_height = 48.0f;
    inline constexpr float trainer_page_tab_gap = 8.0f;
    inline constexpr float trainer_responsive_row_breakpoint = 520.0f;
    inline constexpr float trainer_page_body_gap = 12.0f;
    inline constexpr float trainer_primary_button_height = 64.0f;
    inline constexpr float trainer_gait_button_height = 56.0f;
    inline constexpr float trainer_gait_button_gap = 8.0f;
    inline constexpr float trainer_task_button_height = 48.0f;
    inline constexpr float trainer_secondary_button_height = 48.0f;
    inline constexpr float trainer_primary_minimum_text_scale =
        persistent_text_minimum_scale;
    // The capped 720-pixel Rig Trainer pane has 22 pixels of card insets.
    // Enter the wide summary at that usable width instead of missing it by
    // two pixels and falling back to the taller prose layout at 4K.
    inline constexpr float trainer_summary_wide_minimum_width = 680.0f;
    inline constexpr float trainer_summary_wide_height = 282.0f;
    inline constexpr float trainer_summary_narrow_height = 348.0f;
    inline constexpr float trainer_mission_card_height = 820.0f;
    inline constexpr float trainer_view_card_height = 286.0f;
    inline constexpr float trainer_totals_card_height = 820.0f;
    inline constexpr float trainer_advanced_card_height = 1700.0f;
    inline constexpr float trainer_detail_card_height = 160.0f;
    inline constexpr float trainer_detail_card_spacing = 12.0f;
    inline constexpr float trainer_detail_value_top = 46.0f;
    inline constexpr float trainer_detail_value_bottom = 8.0f;

    [[nodiscard]] inline float rig_lab_zoom_step(
        float current, float wheel_steps) noexcept
    {
        if (!std::isfinite(current))
            current = rig_lab_default_zoom;
        current = std::clamp(current,
            rig_lab_minimum_zoom, rig_lab_maximum_zoom);
        if (!std::isfinite(wheel_steps)
            || std::abs(wheel_steps) < 0.01f)
            return current;
        return std::clamp(current * std::pow(1.25f, wheel_steps),
            rig_lab_minimum_zoom, rig_lab_maximum_zoom);
    }

    [[nodiscard]] inline float maximum_scroll_offset(
        float content_height, float viewport_height) noexcept
    {
        if (!std::isfinite(content_height) || !std::isfinite(viewport_height)
            || !(content_height > 0.0f) || !(viewport_height > 0.0f))
            return 0.0f;
        return std::max(0.0f, content_height - viewport_height);
    }

    [[nodiscard]] inline float clamp_scroll_offset(float offset,
        float content_height, float viewport_height) noexcept
    {
        if (!std::isfinite(offset))
            offset = 0.0f;
        return std::clamp(offset, 0.0f,
            maximum_scroll_offset(content_height, viewport_height));
    }

    [[nodiscard]] inline float panel_scroll_offset(float offset,
        float wheel_steps, float content_height, float viewport_height) noexcept
    {
        offset = clamp_scroll_offset(offset, content_height, viewport_height);
        if (!std::isfinite(wheel_steps) || std::abs(wheel_steps) < 0.01f)
            return offset;
        return clamp_scroll_offset(offset - wheel_steps * panel_scroll_step,
            content_height, viewport_height);
    }

    [[nodiscard]] inline ScrollbarLayout panel_scrollbar_layout(Box viewport,
        float content_height, float offset) noexcept
    {
        const Box track{ viewport.x + std::max(0.0f, viewport.width - 22.0f),
            viewport.y + 10.0f, 16.0f,
            std::max(0.0f, viewport.height - 20.0f) };
        const float maximum_offset = maximum_scroll_offset(
            content_height, viewport.height);
        const bool scrollable = maximum_offset > 0.5f && track.height > 0.0f;
        const float ratio = content_height > 0.0f
            ? std::clamp(viewport.height / content_height, 0.0f, 1.0f) : 1.0f;
        const float thumb_height = scrollable
            ? std::min(track.height, std::max(scrollbar_minimum_thumb_height,
                track.height * ratio))
            : track.height;
        const float progress = scrollable
            ? clamp_scroll_offset(offset, content_height, viewport.height)
                / maximum_offset
            : 0.0f;
        return { track,
            { track.x, track.y + (track.height - thumb_height) * progress,
                track.width, thumb_height },
            maximum_offset, scrollable };
    }

    [[nodiscard]] constexpr bool visual_session_stacks_authority_controls(
        float usable_width) noexcept
    {
        return usable_width < 680.0f;
    }

    enum class DistanceUnits { metric, imperial };

    inline constexpr int sparse_course_marker_stride = 4;

    [[nodiscard]] constexpr bool course_reference_marker_visible(
        int marker_index, bool detailed) noexcept
    {
        return marker_index >= 0
            && (detailed || marker_index == 0
                || marker_index % sparse_course_marker_stride == 0);
    }

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
    [[nodiscard]] constexpr float course_reference_marker_label_offset_pixels(
        int marker_index) noexcept
    {
        return marker_index == 0 ? -116.0f : 0.0f;
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

    [[nodiscard]] constexpr TrainerControlGroupLayout
        trainer_control_group_layout(Box panel, float scroll_offset = 0.0f,
            bool show_primary_session = true,
            bool simplified_live_navigation = false) noexcept
    {
        const float card_x = panel.x + 11.0f;
        const float card_width = std::max(0.0f, panel.width - 22.0f);
        const float primary_y = panel.y + 51.0f - scroll_offset;
        const Box primary = show_primary_session
            ? Box{ card_x, primary_y, card_width,
                trainer_primary_card_height + (simplified_live_navigation
                    ? trainer_primary_button_height + 12.0f : 0.0f) }
            : Box{};
        const Box secondary{};
        const float page_width = std::max(0.0f, panel.width - 36.0f);
        const float responsive_height = page_width
                < trainer_responsive_row_breakpoint
            ? trainer_page_tab_button_height * 2.0f
                + trainer_page_tab_gap
            : trainer_page_tab_button_height;
        const float page_height = show_primary_session
            ? (page_width < trainer_responsive_row_breakpoint
                ? trainer_primary_button_height * 2.0f
                    + trainer_page_tab_gap
                : trainer_primary_button_height)
            : responsive_height;
        const Box page_tabs{ panel.x + 18.0f,
            show_primary_session
                ? primary.y + primary.height + trainer_control_group_gap
                : primary_y,
            page_width, page_height };
        return {
            primary,
            secondary,
            page_tabs,
            { card_x, page_tabs.y + page_tabs.height + trainer_page_body_gap,
                card_width, 0.0f }
        };
    }

    [[nodiscard]] constexpr std::array<Box, 3>
        trainer_live_page_button_layout(Box tabs) noexcept
    {
        if (tabs.width >= trainer_responsive_row_breakpoint)
        {
            const float third = std::max(0.0f,
                (tabs.width - trainer_page_tab_gap * 2.0f) / 3.0f);
            return {
                Box{ tabs.x, tabs.y, third,
                    trainer_page_tab_button_height },
                Box{ tabs.x + third + trainer_page_tab_gap, tabs.y, third,
                    trainer_page_tab_button_height },
                Box{ tabs.x + (third + trainer_page_tab_gap) * 2.0f,
                    tabs.y, third, trainer_page_tab_button_height }
            };
        }
        const float half = std::max(0.0f,
            (tabs.width - trainer_page_tab_gap) * 0.5f);
        const float second_y = tabs.y + trainer_page_tab_button_height
            + trainer_page_tab_gap;
        return {
            Box{ tabs.x, tabs.y, half,
                trainer_page_tab_button_height },
            Box{ tabs.x + half + trainer_page_tab_gap, tabs.y, half,
                trainer_page_tab_button_height },
            Box{ tabs.x, second_y, tabs.width,
                trainer_page_tab_button_height }
        };
    }

    [[nodiscard]] constexpr std::array<Box, 3> trainer_page_button_layout(
        Box tabs) noexcept
    {
        return trainer_live_page_button_layout(tabs);
    }

    [[nodiscard]] constexpr std::array<Box, 4> trainer_quick_action_layout(
        Box lane) noexcept
    {
        if (lane.width < trainer_responsive_row_breakpoint)
        {
            const float half = std::max(0.0f,
                (lane.width - trainer_page_tab_gap) * 0.5f);
            const float second_y = lane.y + trainer_primary_button_height
                + trainer_page_tab_gap;
            return {
                Box{ lane.x, lane.y, half,
                    trainer_primary_button_height },
                Box{ lane.x + half + trainer_page_tab_gap, lane.y, half,
                    trainer_primary_button_height },
                Box{ lane.x, second_y, half,
                    trainer_primary_button_height },
                Box{ lane.x + half + trainer_page_tab_gap, second_y, half,
                    trainer_primary_button_height }
            };
        }
        const float quarter = std::max(0.0f,
            (lane.width - trainer_page_tab_gap * 3.0f) / 4.0f);
        return {
            Box{ lane.x, lane.y, quarter, trainer_primary_button_height },
            Box{ lane.x + quarter + trainer_page_tab_gap, lane.y, quarter,
                trainer_primary_button_height },
            Box{ lane.x + (quarter + trainer_page_tab_gap) * 2.0f, lane.y,
                quarter, trainer_primary_button_height },
            Box{ lane.x + (quarter + trainer_page_tab_gap) * 3.0f, lane.y,
                quarter, trainer_primary_button_height }
        };
    }

    [[nodiscard]] constexpr Box trainer_quick_page_body(Box tabs) noexcept
    {
        const std::array<Box, 4> actions =
            trainer_quick_action_layout(tabs);
        float action_bottom = tabs.y;
        for (const Box action : actions)
            action_bottom = std::max(action_bottom,
                action.y + action.height);
        return { tabs.x - 7.0f,
            action_bottom + trainer_page_body_gap,
            tabs.width + 14.0f, 0.0f };
    }

    [[nodiscard]] constexpr std::array<Box, 3>
        trainer_three_choice_layout(Box row) noexcept
    {
        if (row.width < 400.0f)
        {
            const float stride = trainer_secondary_button_height
                + trainer_page_tab_gap;
            return {
                Box{ row.x, row.y, row.width,
                    trainer_secondary_button_height },
                Box{ row.x, row.y + stride, row.width,
                    trainer_secondary_button_height },
                Box{ row.x, row.y + stride * 2.0f, row.width,
                    trainer_secondary_button_height }
            };
        }
        if (row.width >= trainer_responsive_row_breakpoint)
        {
            const float third = std::max(0.0f,
                (row.width - trainer_page_tab_gap * 2.0f) / 3.0f);
            return {
                Box{ row.x, row.y, third, trainer_secondary_button_height },
                Box{ row.x + third + trainer_page_tab_gap, row.y, third,
                    trainer_secondary_button_height },
                Box{ row.x + (third + trainer_page_tab_gap) * 2.0f,
                    row.y, third, trainer_secondary_button_height }
            };
        }
        const float half = std::max(0.0f,
            (row.width - trainer_page_tab_gap) * 0.5f);
        const float second_y = row.y + trainer_secondary_button_height
            + trainer_page_tab_gap;
        return {
            Box{ row.x, row.y, half, trainer_secondary_button_height },
            Box{ row.x + half + trainer_page_tab_gap, row.y, half,
                trainer_secondary_button_height },
            Box{ row.x, second_y, row.width,
                trainer_secondary_button_height }
        };
    }

    [[nodiscard]] constexpr float trainer_three_choice_height(
        float width) noexcept
    {
        return width < 400.0f
            ? trainer_secondary_button_height * 3.0f
                + trainer_page_tab_gap * 2.0f
            : width >= trainer_responsive_row_breakpoint
            ? trainer_secondary_button_height
            : trainer_secondary_button_height * 2.0f
                + trainer_page_tab_gap;
    }

    [[nodiscard]] constexpr float trainer_view_card_height_for_width(
        float width) noexcept
    {
        return trainer_view_card_height
            + (width < 400.0f
                ? (trainer_secondary_button_height
                    + trainer_page_tab_gap) * 4.0f
                : width < trainer_responsive_row_breakpoint
                ? (trainer_secondary_button_height
                    + trainer_page_tab_gap) * 2.0f
                : 0.0f);
    }

    [[nodiscard]] constexpr std::array<Box, 4> trainer_gait_button_layout(
        Box row) noexcept
    {
        const float width = std::max(0.0f,
            (row.width - trainer_gait_button_gap) * 0.5f);
        return {
            Box{ row.x, row.y, width, trainer_gait_button_height },
            Box{ row.x + width + trainer_gait_button_gap, row.y,
                width, trainer_gait_button_height },
            Box{ row.x, row.y + trainer_gait_button_height
                    + trainer_gait_button_gap,
                width, trainer_gait_button_height },
            Box{ row.x + width + trainer_gait_button_gap,
                row.y + trainer_gait_button_height + trainer_gait_button_gap,
                width, trainer_gait_button_height }
        };
    }

    [[nodiscard]] constexpr TrainerSummaryLayout trainer_summary_layout(
        Box page_body) noexcept
    {
        const bool wide = page_body.width >= trainer_summary_wide_minimum_width;
        const float card_height = wide
            ? trainer_summary_wide_height : trainer_summary_narrow_height;
        const Box card{ page_body.x, page_body.y, page_body.width, card_height };
        const float x = card.x + 12.0f;
        const float width = std::max(0.0f, card.width - 24.0f);
        if (wide)
        {
            return {
                card,
                { x, card.y + 14.0f, width, 24.0f },
                { x, card.y + 46.0f, width, 30.0f },
                { x, card.y + 84.0f, width, 28.0f },
                { x, card.y + 120.0f, width, 28.0f },
                { x, card.y + 156.0f, width, 22.0f },
                { x, card.y + 186.0f, width, 82.0f },
                true
            };
        }
        const float narrow_x = card.x + 8.0f;
        const float narrow_width = std::max(0.0f, card.width - 16.0f);
        return {
            card,
            { narrow_x, card.y + 12.0f, narrow_width, 24.0f },
            { narrow_x, card.y + 42.0f, narrow_width, 30.0f },
            { narrow_x, card.y + 78.0f, narrow_width, 64.0f },
            { narrow_x, card.y + 148.0f, narrow_width, 38.0f },
            { narrow_x, card.y + 192.0f, narrow_width, 22.0f },
            { narrow_x, card.y + 220.0f, narrow_width, 116.0f },
            false
        };
    }

    [[nodiscard]] constexpr Box content_box(float window_width, float window_height) noexcept
    {
        return { content_margin, top_bar_height,
            std::max(0.0f, window_width - content_margin * 2.0f),
            std::max(0.0f, window_height - top_bar_height - content_margin
                - footer_lane_height) };
    }

    [[nodiscard]] constexpr Box status_footer_box(
        float window_width, float window_height) noexcept
    {
        const Box content = content_box(window_width, window_height);
        const float y = content.y + content.height + 8.0f;
        return { 20.0f, y, std::max(0.0f, window_width - 40.0f),
            std::max(0.0f, window_height - content_margin - y) };
    }

    [[nodiscard]] constexpr float live_panel_width(float content_width) noexcept
    {
        return std::clamp(content_width * 0.29f, 390.0f, 740.0f);
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
        const float width = std::clamp(world.width * 0.34f, 360.0f, 420.0f);
        const float height = std::clamp(world.height * 0.36f, 340.0f, 380.0f);
        return { world.x + world.width - width - card_margin,
            world.y + card_margin, width, height };
    }

    [[nodiscard]] constexpr Box training_pip_title_box(Box pip) noexcept
    {
        return { pip.x + 12.0f, pip.y + 8.0f,
            std::max(0.0f, pip.width - 24.0f), 22.0f };
    }

    [[nodiscard]] constexpr Box training_pip_state_box(Box pip) noexcept
    {
        return { pip.x + 12.0f, pip.y + 34.0f,
            std::max(0.0f, pip.width - 24.0f), 72.0f };
    }

    [[nodiscard]] constexpr float training_pip_footer_lane_height(
        bool expanded_footer) noexcept
    {
        return expanded_footer ? pip_footer_height : pip_compact_footer_height;
    }

    [[nodiscard]] constexpr Box training_pip_world_box(Box pip,
        bool expanded_footer = true) noexcept
    {
        const float footer_height = training_pip_footer_lane_height(
            expanded_footer);
        return { pip.x + 8.0f, pip.y + pip_header_height,
            std::max(0.0f, pip.width - 16.0f),
            std::max(0.0f, pip.height - pip_header_height
                - footer_height) };
    }

    [[nodiscard]] constexpr Box training_pip_footer_box(Box pip,
        bool expanded_footer = true) noexcept
    {
        const float footer_height = training_pip_footer_lane_height(
            expanded_footer);
        return { pip.x + 10.0f,
            pip.y + std::max(0.0f, pip.height - footer_height + 4.0f),
            std::max(0.0f, pip.width - 20.0f),
            std::max(0.0f, footer_height - 10.0f) };
    }

    [[nodiscard]] constexpr Box primary_telemetry_box(Box world) noexcept
    {
        const Box pip = training_pip_box(world);
        return { world.x + card_margin, world.y + card_margin,
            std::max(260.0f, pip.x - world.x - card_margin * 2.0f), pip.height };
    }

    [[nodiscard]] constexpr Box primary_telemetry_display_box(
        Box world, bool expanded) noexcept
    {
        Box telemetry = primary_telemetry_box(world);
        if (!expanded)
            telemetry.height = std::min(telemetry.height,
                primary_telemetry_compact_height);
        return telemetry;
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

    [[nodiscard]] constexpr bool training_pip_evaluation_waiting(
        std::uint64_t completed_evaluations, bool terminal,
        bool specialized_proof) noexcept
    {
        return completed_evaluations == 0u && !terminal
            && !specialized_proof;
    }

    // SDL may expose either DPI-independent coordinates or the complete
    // high-density framebuffer. Normalize ordinary high-density desktops to a
    // deliberate presentation canvas so both paths produce the same physical
    // text, spacing, hit-target, and control-rail size. Truly oversized
    // canvases retain the existing multi-pane workspace.
    [[nodiscard]] constexpr PresentationSurface presentation_surface(
        int surface_width, int surface_height) noexcept
    {
        if (surface_width <= 0 || surface_height <= 0)
            return { surface_width, surface_height, 1.0f, 1.0f, false };
        const float width = static_cast<float>(surface_width);
        const float height = static_cast<float>(surface_height);
        const bool can_scale = width < presentation_scaled_window_limit;
        const float density = can_scale
            ? std::max(1.0f, std::min(width / presentation_reference_width,
                  height / presentation_reference_height))
            : 1.0f;
        const int canvas_width = std::max(1,
            static_cast<int>(width / density + 0.5f));
        const int canvas_height = std::max(1,
            static_cast<int>(height / density + 0.5f));
        const float scale_x = width / static_cast<float>(canvas_width);
        const float scale_y = height / static_cast<float>(canvas_height);
        return { canvas_width, canvas_height, scale_x, scale_y,
            density > 1.001f };
    }

    [[nodiscard]] constexpr Box presentation_box(
        Box box, const PresentationSurface& surface) noexcept
    {
        return { box.x * surface.scale_x, box.y * surface.scale_y,
            box.width * surface.scale_x, box.height * surface.scale_y };
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
        const Box footer = status_footer_box(width, height);
        const Box pip_title = training_pip_title_box(pip);
        const Box pip_state = training_pip_state_box(pip);
        const Box pip_world = training_pip_world_box(pip);
        const Box pip_footer = training_pip_footer_box(pip);
        const Box window{ 0.0f, 0.0f, width, height };
        return world.width >= 850.0f && panel.width >= 390.0f
            && contains(content, world) && contains(content, panel)
            && contains(world, pip) && contains(world, telemetry)
            && contains(world, bottom)
            && contains(pip, pip_title) && contains(pip, pip_state)
            && contains(pip, pip_world) && contains(pip, pip_footer)
            && contains(window, footer)
            && !overlaps(world, panel)
            && !overlaps(pip, telemetry)
            && !overlaps(pip, bottom)
            && !overlaps(telemetry, bottom)
            && !overlaps(pip_title, pip_state)
            && !overlaps(pip_state, pip_world)
            && !overlaps(pip_world, pip_footer)
            && !overlaps(bottom, footer);
    }

    [[nodiscard]] constexpr float rig_lab_panel_width(float content_width) noexcept
    {
        return std::clamp(content_width * 0.22f, 420.0f, 560.0f);
    }

    [[nodiscard]] constexpr float rig_lab_trainer_panel_width(
        float content_width) noexcept
    {
        return std::clamp(content_width * 0.25f, 400.0f, 650.0f);
    }

    [[nodiscard]] constexpr bool rig_lab_shows_live(Box content) noexcept
    {
        return content.width >= rig_lab_combined_min_content_width;
    }

    [[nodiscard]] constexpr Box rig_lab_live_box(Box content) noexcept
    {
        if (!rig_lab_shows_live(content))
            return { content.x, content.y, 0.0f, content.height };
        return { content.x, content.y,
            std::clamp(content.width * rig_lab_combined_live_ratio,
                620.0f, 1400.0f), content.height };
    }

    [[nodiscard]] constexpr Box rig_lab_trainer_panel_box(Box content) noexcept
    {
        const Box live = rig_lab_live_box(content);
        const float x = rig_lab_shows_live(content)
            ? live.x + live.width + panel_gap : content.x;
        return { x, content.y,
            rig_lab_trainer_panel_width(content.width), content.height };
    }

    [[nodiscard]] constexpr Box rig_lab_panel_box(Box content) noexcept
    {
        const Box trainer = rig_lab_trainer_panel_box(content);
        return { trainer.x + trainer.width + panel_gap, content.y,
            rig_lab_panel_width(content.width), content.height };
    }

    [[nodiscard]] constexpr Box rig_lab_world_box(Box content) noexcept
    {
        const Box panel = rig_lab_panel_box(content);
        const float world_x = panel.x + panel.width + panel_gap;
        return { world_x, content.y,
            std::max(0.0f, content.x + content.width - world_x),
            content.height };
    }

    [[nodiscard]] constexpr bool rig_lab_uses_compact_workspace(
        Box content) noexcept
    {
        return content.width < rig_lab_dual_panel_min_content_width;
    }

    [[nodiscard]] constexpr float rig_lab_compact_panel_width(
        float content_width) noexcept
    {
        // 420 keeps the longest compact production status on one readable
        // line at the 1.40 type floor while preserving more than 900 logical
        // pixels for the 1368-wide preview.
        return std::clamp(content_width * 0.28f, 420.0f, 720.0f);
    }

    [[nodiscard]] constexpr Box rig_lab_workspace_bar_box(Box content) noexcept
    {
        if (!rig_lab_uses_compact_workspace(content))
            return { content.x, content.y, 0.0f, 0.0f };
        return { content.x, content.y, content.width,
            std::min(content.height, rig_lab_workspace_bar_height) };
    }

    [[nodiscard]] constexpr RigLabWorkspaceNavigationLayout
        rig_lab_workspace_navigation_layout(Box workspace_bar) noexcept
    {
        if (!(workspace_bar.width > 0.0f)
            || !(workspace_bar.height > 0.0f))
            return {};
        constexpr float outer = 12.0f;
        constexpr float gap = 10.0f;
        constexpr float row_height = 56.0f;
        const float usable_width = std::max(0.0f,
            workspace_bar.width - outer * 2.0f);
        const float lane_width = std::max(0.0f, usable_width - gap * 4.0f);
        const float mission_width = lane_width * 0.24f;
        const float run_width = lane_width * 0.22f;
        const float controls_width = lane_width * 0.16f;
        const float trainer_width = lane_width * 0.18f;
        const float inspector_width = lane_width - mission_width - run_width
            - controls_width - trainer_width;
        const float y = workspace_bar.y + 12.0f;
        float x = workspace_bar.x + outer;
        const Box mission{ x, y, mission_width, row_height };
        x += mission_width + gap;
        const Box run{ x, y, run_width, row_height };
        x += run_width + gap;
        const Box controls{ x, y, controls_width, row_height };
        x += controls_width + gap;
        const Box trainer{ x, y, trainer_width, row_height };
        x += trainer_width + gap;
        const Box inspector{ x, y, inspector_width, row_height };
        return { mission, run, controls, trainer, inspector };
    }

    [[nodiscard]] constexpr RigLabControlDrawerLayout
        rig_lab_control_drawer_layout(Box panel) noexcept
    {
        const Box card = inset(panel, 12.0f);
        const Box heading{ card.x + 16.0f, card.y + 14.0f,
            std::max(0.0f, card.width - 32.0f), 34.0f };
        constexpr float gap = 10.0f;
        constexpr float button_height = 52.0f;
        const float usable = std::max(0.0f, card.width - 32.0f);
        const float first_y = heading.y + heading.height + 18.0f;
        const bool stacked_gaits = usable < 820.0f;
        const float half = std::max(0.0f, (usable - gap) * 0.5f);
        std::array<Box, 4> gaits = stacked_gaits
            ? std::array<Box, 4>{
                Box{ heading.x, first_y, usable, button_height },
                Box{ heading.x, first_y + button_height + gap,
                    usable, button_height },
                Box{ heading.x, first_y + (button_height + gap) * 2.0f,
                    usable, button_height },
                Box{ heading.x, first_y + (button_height + gap) * 3.0f,
                    usable, button_height } }
            : std::array<Box, 4>{
                Box{ heading.x, first_y, half, button_height },
                Box{ heading.x + half + gap, first_y, half, button_height },
                Box{ heading.x, first_y + button_height + gap,
                    half, button_height },
                Box{ heading.x + half + gap,
                    first_y + button_height + gap, half, button_height } };
        const float gait_rows = stacked_gaits ? 4.0f : 2.0f;
        const float authority_y = first_y
            + (button_height + gap) * gait_rows + 18.0f;
        const Box authority{ heading.x, authority_y, usable, button_height };
        const Box profile{ heading.x, authority_y + button_height + gap,
            usable, button_height };
        const float art_y = profile.y + button_height + gap;
        const Box rig_art{ heading.x, art_y, usable, button_height };
        return { card, heading, gaits, authority, profile, rig_art };
    }

    [[nodiscard]] constexpr std::array<Box, 5>
        rig_inspector_page_button_layout(Box panel) noexcept
    {
        constexpr float outer = 18.0f;
        constexpr float gap = 8.0f;
        constexpr float height = 48.0f;
        const float usable = std::max(0.0f, panel.width - outer * 2.0f);
        const float x = panel.x + outer;
        const float first_y = panel.y + 62.0f;
        if (panel.width >= 640.0f)
        {
            const float fifth = std::max(0.0f,
                (usable - gap * 4.0f) / 5.0f);
            return {
                Box{ x, first_y, fifth, height },
                Box{ x + fifth + gap, first_y, fifth, height },
                Box{ x + (fifth + gap) * 2.0f, first_y, fifth, height },
                Box{ x + (fifth + gap) * 3.0f, first_y, fifth, height },
                Box{ x + (fifth + gap) * 4.0f, first_y, fifth, height }
            };
        }
        const float half = std::max(0.0f, (usable - gap) * 0.5f);
        const float second_y = first_y + height + gap;
        const float third_y = second_y + height + gap;
        return {
            Box{ x, first_y, half, height },
            Box{ x + half + gap, first_y, half, height },
            Box{ x, second_y, half, height },
            Box{ x + half + gap, second_y, half, height },
            Box{ x, third_y, usable, height }
        };
    }

    [[nodiscard]] constexpr float rig_inspector_header_height_for_width(
        float panel_width) noexcept
    {
        return panel_width >= 640.0f ? 126.0f : rig_inspector_header_height;
    }

    [[nodiscard]] constexpr RigArtActionLayout rig_art_action_layout(
        Box row) noexcept
    {
        constexpr float gap = 8.0f;
        constexpr float button_height = 52.0f;
        const float half = std::max(0.0f, (row.width - gap) * 0.5f);
        return {
            Box{ row.x, row.y, row.width, button_height },
            Box{ row.x, row.y + button_height + gap,
                half, button_height },
            Box{ row.x + half + gap, row.y + button_height + gap,
                half, button_height }
        };
    }

    [[nodiscard]] constexpr RigPreviewToolbarLayout rig_preview_toolbar_layout(
        Box world) noexcept
    {
        constexpr float margin = 16.0f;
        constexpr float gap = 8.0f;
        constexpr float height = 52.0f;
        const float width = std::min(620.0f,
            std::max(0.0f, world.width - margin * 2.0f));
        const float third = std::max(0.0f, (width - gap * 2.0f) / 3.0f);
        const float x = world.x + std::max(0.0f, world.width - width - margin);
        const float y = world.y + margin;
        return {
            Box{ x, y, third, height },
            Box{ x + third + gap, y, third, height },
            Box{ x + (third + gap) * 2.0f, y, third, height }
        };
    }

    [[nodiscard]] constexpr Box rig_lab_dual_setup_button_layout(
        Box trainer) noexcept
    {
        constexpr float margin = 14.0f;
        constexpr float width = 144.0f;
        constexpr float height = 48.0f;
        return {
            trainer.x + std::max(0.0f, trainer.width - width - margin),
            trainer.y + 8.0f,
            std::min(width, std::max(0.0f, trainer.width - margin * 2.0f)),
            height
        };
    }

    [[nodiscard]] constexpr Box rig_preview_title_box(Box world) noexcept
    {
        const RigPreviewToolbarLayout toolbar =
            rig_preview_toolbar_layout(world);
        constexpr float left = 18.0f;
        constexpr float gap = 12.0f;
        return { world.x + left, world.y + 16.0f,
            std::max(0.0f, toolbar.zoom_out.x - world.x - left - gap),
            toolbar.zoom_out.height };
    }

    [[nodiscard]] constexpr RigLabWorkspaceLayout rig_lab_workspace_layout(
        Box content, RigLabWorkspacePage page) noexcept
    {
        if (!rig_lab_uses_compact_workspace(content))
        {
            return {
                {},
                rig_lab_live_box(content),
                rig_lab_trainer_panel_box(content),
                rig_lab_panel_box(content),
                rig_lab_world_box(content),
                false
            };
        }

        const Box workspace_bar = rig_lab_workspace_bar_box(content);
        const float body_y = workspace_bar.y + workspace_bar.height
            + rig_lab_workspace_bar_gap;
        const Box body{ content.x, body_y, content.width,
            std::max(0.0f, content.y + content.height - body_y) };
        const float panel_width = std::min(body.width,
            rig_lab_compact_panel_width(body.width));
        const Box selected_panel{ body.x, body.y, panel_width, body.height };
        const float world_x = selected_panel.x + selected_panel.width + panel_gap;
        const Box world{ world_x, body.y,
            std::max(0.0f, body.x + body.width - world_x), body.height };
        const Box hidden_panel{ body.x, body.y, 0.0f, 0.0f };
        return {
            workspace_bar,
            { body.x, body.y, 0.0f, 0.0f },
            page == RigLabWorkspacePage::trainer
                ? selected_panel : hidden_panel,
            page == RigLabWorkspacePage::inspector
                ? selected_panel : hidden_panel,
            world,
            true
        };
    }

    [[nodiscard]] constexpr RigLabTestLayout rig_lab_test_layout(Box card) noexcept
    {
        const float inset_width = std::max(0.0f, card.width - 28.0f);
        return {
            { card.x + 14.0f, card.y + 52.0f, inset_width, 48.0f },
            { card.x + 14.0f, card.y + 110.0f, inset_width, 48.0f },
            { card.x + 14.0f, card.y + 168.0f, inset_width, 48.0f },
            { card.x + 14.0f, card.y + 228.0f, inset_width, 26.0f },
            { card.x + 14.0f, card.y + 270.0f, inset_width, 48.0f }
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
        const auto workspace_valid = [content](
            const RigLabWorkspaceLayout& layout,
            RigLabWorkspacePage page) constexpr
        {
            if (layout.compact)
            {
                const Box selected = page == RigLabWorkspacePage::trainer
                    ? layout.trainer : layout.inspector;
                const Box hidden = page == RigLabWorkspacePage::trainer
                    ? layout.inspector : layout.trainer;
                return layout.live.width == 0.0f
                    && selected.width >= 400.0f
                    && hidden.width == 0.0f
                    && layout.world.width >= 760.0f
                    && layout.world.height >= 500.0f
                    && contains(content, layout.workspace_bar)
                    && contains(content, selected)
                    && contains(content, layout.world)
                    && !overlaps(layout.workspace_bar, selected)
                    && !overlaps(layout.workspace_bar, layout.world)
                    && !overlaps(selected, layout.world);
            }

            const bool live_valid = !rig_lab_shows_live(content)
                || (layout.live.width >= 620.0f
                    && contains(content, layout.live)
                    && !overlaps(layout.live, layout.trainer)
                    && !overlaps(layout.live, layout.inspector)
                    && !overlaps(layout.live, layout.world));
            return live_valid && layout.workspace_bar.width == 0.0f
                && layout.trainer.width >= 400.0f
                && layout.inspector.width >= 340.0f
                && layout.world.width >= 400.0f
                && contains(content, layout.trainer)
                && contains(content, layout.inspector)
                && contains(content, layout.world)
                && !overlaps(layout.trainer, layout.inspector)
                && !overlaps(layout.trainer, layout.world)
                && !overlaps(layout.inspector, layout.world);
        };
        return workspace_valid(rig_lab_workspace_layout(
                    content, RigLabWorkspacePage::trainer),
                RigLabWorkspacePage::trainer)
            && workspace_valid(rig_lab_workspace_layout(
                    content, RigLabWorkspacePage::inspector),
                RigLabWorkspacePage::inspector);
    }

    inline constexpr std::array<std::array<float, 2>, 11> validation_sizes{
        std::array<float, 2>{ 1368.0f, 799.0f },
        std::array<float, 2>{ 1280.0f, 820.0f },
        std::array<float, 2>{ 1600.0f, 900.0f },
        std::array<float, 2>{ 1920.0f, 1080.0f },
        std::array<float, 2>{ 2047.0f, 1112.0f },
        std::array<float, 2>{ static_cast<float>(default_window_width),
            static_cast<float>(default_window_height) },
        std::array<float, 2>{ 2560.0f, 1440.0f },
        std::array<float, 2>{ 3840.0f, 2048.0f },
        std::array<float, 2>{ 3840.0f, 2160.0f },
        std::array<float, 2>{ 5120.0f, 2880.0f },
        std::array<float, 2>{ 6144.0f, 3456.0f }
    };
}
