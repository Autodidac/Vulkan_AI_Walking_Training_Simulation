#pragma once

#include <algorithm>
#include <cstdint>

namespace runner::ui_layout
{
    struct Box
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    inline constexpr float minimum_content_width = 1080.0f;
    inline constexpr float minimum_content_height = 800.0f;
    inline constexpr float top_bar_height = 92.0f;
    inline constexpr float content_margin = 10.0f;
    inline constexpr float panel_gap = 10.0f;
    inline constexpr float primary_telemetry_bottom = 148.0f;
    inline constexpr float pip_top_padding = 162.0f;
    inline constexpr float bottom_telemetry_height = 52.0f;

    enum class DistanceUnits { metric, imperial };

    [[nodiscard]] constexpr Box top_bar_box(float window_width) noexcept
    {
        return { 0.0f, 0.0f, std::max(0.0f, window_width), top_bar_height };
    }

    [[nodiscard]] constexpr float course_reference_marker_spacing_m(
        DistanceUnits units) noexcept
    {
        return units == DistanceUnits::metric ? 250.0f : 1609.344f * 0.25f;
    }

    [[nodiscard]] constexpr std::uint64_t lifetime_delta(
        std::uint64_t total, std::uint64_t start) noexcept
    {
        return total >= start ? total - start : 0u;
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

    [[nodiscard]] constexpr Box content_box(float window_width, float window_height) noexcept
    {
        return { content_margin, top_bar_height,
            window_width - content_margin * 2.0f,
            window_height - top_bar_height - content_margin };
    }

    [[nodiscard]] constexpr float live_panel_width(float content_width) noexcept
    {
        return std::clamp(content_width * 0.42f, 650.0f, 720.0f);
    }

    [[nodiscard]] constexpr Box live_world_box(Box content) noexcept
    {
        const float panel = live_panel_width(content.width);
        return { content.x, content.y, content.width - panel - panel_gap, content.height };
    }

    [[nodiscard]] constexpr Box live_panel_box(Box content) noexcept
    {
        const Box world = live_world_box(content);
        return { world.x + world.width + panel_gap, content.y,
            live_panel_width(content.width), content.height };
    }

    [[nodiscard]] constexpr Box primary_telemetry_box(Box world) noexcept
    {
        return { world.x + 18.0f, world.y + 12.0f,
            std::max(0.0f, world.width - 36.0f), primary_telemetry_bottom };
    }

    [[nodiscard]] constexpr Box bottom_telemetry_box(Box world) noexcept
    {
        return { world.x + 18.0f,
            world.y + std::max(0.0f, world.height - bottom_telemetry_height),
            std::max(0.0f, world.width - 36.0f), bottom_telemetry_height };
    }

    [[nodiscard]] constexpr Box training_pip_box(Box world) noexcept
    {
        const float width = std::clamp(world.width * 0.46f, 240.0f, 440.0f);
        const float available_height = std::max(150.0f,
            world.height - pip_top_padding - bottom_telemetry_height - 18.0f);
        const float height = std::min(std::clamp(world.height * 0.30f, 190.0f, 270.0f),
            available_height);
        return { world.x + world.width - width - 18.0f,
            world.y + pip_top_padding, width, height };
    }

    [[nodiscard]] constexpr bool supported_window(float width, float height) noexcept
    {
        const Box content = content_box(width, height);
        return content.width >= minimum_content_width
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
        return world.width > 0.0f && panel.width >= 650.0f
            && contains(content, world) && contains(content, panel)
            && contains(world, pip)
            && !overlaps(world, panel)
            && !overlaps(pip, primary_telemetry_box(world))
            && !overlaps(pip, bottom_telemetry_box(world));
    }
}
