#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_between(text: str, start: str, end: str, replacement: str, label: str) -> str:
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"{label}: start marker not found")
    finish = text.find(end, begin)
    if finish < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:begin] + replacement + text[finish:]


def patch_small_headers() -> None:
    write("src/ui_layout.hpp", r'''#pragma once

#include <algorithm>
#include <array>
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

    struct SurfaceScale
    {
        float x{ 1.0f };
        float y{ 1.0f };
    };

    inline constexpr float minimum_content_width = 1150.0f;
    inline constexpr float minimum_content_height = 700.0f;
    inline constexpr float top_bar_height = 78.0f;
    inline constexpr float content_margin = 12.0f;
    inline constexpr float panel_gap = 12.0f;
    inline constexpr float card_margin = 14.0f;
    inline constexpr float bottom_telemetry_height = 44.0f;
    inline constexpr float minimum_readable_text_scale = 0.78f;

    enum class DistanceUnits { metric, imperial };

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

    inline constexpr std::array<std::array<float, 2>, 5> validation_sizes{
        std::array<float, 2>{ 1280.0f, 820.0f },
        std::array<float, 2>{ 1600.0f, 900.0f },
        std::array<float, 2>{ 1920.0f, 1080.0f },
        std::array<float, 2>{ 2047.0f, 1112.0f },
        std::array<float, 2>{ 2560.0f, 1440.0f }
    };
}
''')

    write("src/preview_sync.hpp", r'''#pragma once

namespace runner::preview_sync
{
    struct Decision
    {
        bool replace_blueprint{};
        bool replace_course{};
        bool reset_episode{};
        bool adopt_controller{ true };
    };

    [[nodiscard]] constexpr Decision decide(bool rig_changed,
        bool course_changed, bool best_changed) noexcept
    {
        return {
            rig_changed,
            course_changed,
            rig_changed || course_changed,
            best_changed || !rig_changed
        };
    }
}
''')

    write("src/renderer.hpp", r'''#pragma once

#include "math.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

struct SDL_Window;

namespace runner::render
{
    struct Vertex
    {
        Vec2 position{};
        Color color{};
    };

    class Canvas
    {
    public:
        void clear() noexcept
        {
            vertices_.clear();
            clip_stack_.clear();
        }
        void reserve(std::size_t vertex_count) { vertices_.reserve(vertex_count); }
        [[nodiscard]] std::span<const Vertex> vertices() const noexcept { return vertices_; }

        void push_clip(Vec2 minimum, Vec2 maximum);
        void pop_clip() noexcept;
        [[nodiscard]] std::size_t clip_depth() const noexcept { return clip_stack_.size(); }

        void triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
        void quad(Vec2 minimum, Vec2 maximum, Color color);
        void line(Vec2 a, Vec2 b, float thickness, Color color);
        void circle(Vec2 center, float radius, Color color, std::uint32_t segments = 24);
        void capsule(Vec2 a, Vec2 b, float radius, Color color, std::uint32_t segments = 16);
        void polyline(std::span<const Vec2> points, float thickness, Color color);

    private:
        struct ClipRect
        {
            Vec2 minimum{};
            Vec2 maximum{};
        };

        void emit_triangle(Vec2 a, Vec2 b, Vec2 c, Color color);

        std::vector<Vertex> vertices_{};
        std::vector<ClipRect> clip_stack_{};
    };

    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        VulkanRenderer(VulkanRenderer&&) = delete;
        VulkanRenderer& operator=(VulkanRenderer&&) = delete;

        [[nodiscard]] bool initialize(SDL_Window* window,
            const std::filesystem::path& shader_directory, std::string& error);
        void shutdown() noexcept;
        [[nodiscard]] bool render(std::span<const Vertex> vertices,
            int canvas_width, int canvas_height,
            int drawable_width, int drawable_height, std::string& error);
        void wait_idle() noexcept;

    private:
        struct Impl;
        Impl* impl_{};
    };
}
''')

    write("src/runner_icon.rc.in", 'IDI_RUNNER_ICON ICON "@RUNNER_ICON_ICO_RC@"\n')


def patch_canvas() -> None:
    write("src/canvas.cpp", r'''#include "renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace runner::render
{
    namespace
    {
        enum class ClipEdge : std::uint8_t { left, right, top, bottom };

        struct Polygon
        {
            std::array<Vec2, 12> points{};
            std::size_t count{};
        };

        [[nodiscard]] bool inside(Vec2 point, ClipEdge edge, float boundary) noexcept
        {
            switch (edge)
            {
            case ClipEdge::left: return point.x >= boundary;
            case ClipEdge::right: return point.x <= boundary;
            case ClipEdge::top: return point.y >= boundary;
            case ClipEdge::bottom: return point.y <= boundary;
            }
            return false;
        }

        [[nodiscard]] Vec2 intersection(Vec2 a, Vec2 b,
            ClipEdge edge, float boundary) noexcept
        {
            const Vec2 delta = b - a;
            float t{};
            if (edge == ClipEdge::left || edge == ClipEdge::right)
            {
                if (std::abs(delta.x) <= 1.0e-6f)
                    return { boundary, a.y };
                t = (boundary - a.x) / delta.x;
            }
            else
            {
                if (std::abs(delta.y) <= 1.0e-6f)
                    return { a.x, boundary };
                t = (boundary - a.y) / delta.y;
            }
            return a + delta * std::clamp(t, 0.0f, 1.0f);
        }

        [[nodiscard]] Polygon clip_edge(const Polygon& input,
            ClipEdge edge, float boundary) noexcept
        {
            Polygon output{};
            if (input.count == 0u)
                return output;
            Vec2 previous = input.points[input.count - 1u];
            bool previous_inside = inside(previous, edge, boundary);
            for (std::size_t index = 0; index < input.count; ++index)
            {
                const Vec2 current = input.points[index];
                const bool current_inside = inside(current, edge, boundary);
                if (current_inside != previous_inside)
                    output.points[output.count++] = intersection(previous, current, edge, boundary);
                if (current_inside)
                    output.points[output.count++] = current;
                previous = current;
                previous_inside = current_inside;
            }
            return output;
        }
    }

    void Canvas::push_clip(Vec2 minimum, Vec2 maximum)
    {
        ClipRect clip{
            { std::min(minimum.x, maximum.x), std::min(minimum.y, maximum.y) },
            { std::max(minimum.x, maximum.x), std::max(minimum.y, maximum.y) }
        };
        if (!clip_stack_.empty())
        {
            const ClipRect parent = clip_stack_.back();
            clip.minimum.x = std::max(clip.minimum.x, parent.minimum.x);
            clip.minimum.y = std::max(clip.minimum.y, parent.minimum.y);
            clip.maximum.x = std::min(clip.maximum.x, parent.maximum.x);
            clip.maximum.y = std::min(clip.maximum.y, parent.maximum.y);
        }
        clip.maximum.x = std::max(clip.maximum.x, clip.minimum.x);
        clip.maximum.y = std::max(clip.maximum.y, clip.minimum.y);
        clip_stack_.push_back(clip);
    }

    void Canvas::pop_clip() noexcept
    {
        if (!clip_stack_.empty())
            clip_stack_.pop_back();
    }

    void Canvas::emit_triangle(Vec2 a, Vec2 b, Vec2 c, Color color)
    {
        vertices_.push_back({ a, color });
        vertices_.push_back({ b, color });
        vertices_.push_back({ c, color });
    }

    void Canvas::triangle(Vec2 a, Vec2 b, Vec2 c, Color color)
    {
        if (clip_stack_.empty())
        {
            emit_triangle(a, b, c, color);
            return;
        }

        const ClipRect clip = clip_stack_.back();
        const float minimum_x = std::min({ a.x, b.x, c.x });
        const float maximum_x = std::max({ a.x, b.x, c.x });
        const float minimum_y = std::min({ a.y, b.y, c.y });
        const float maximum_y = std::max({ a.y, b.y, c.y });
        if (maximum_x < clip.minimum.x || minimum_x > clip.maximum.x
            || maximum_y < clip.minimum.y || minimum_y > clip.maximum.y)
            return;
        if (minimum_x >= clip.minimum.x && maximum_x <= clip.maximum.x
            && minimum_y >= clip.minimum.y && maximum_y <= clip.maximum.y)
        {
            emit_triangle(a, b, c, color);
            return;
        }

        Polygon polygon{};
        polygon.points[0] = a;
        polygon.points[1] = b;
        polygon.points[2] = c;
        polygon.count = 3u;
        polygon = clip_edge(polygon, ClipEdge::left, clip.minimum.x);
        polygon = clip_edge(polygon, ClipEdge::right, clip.maximum.x);
        polygon = clip_edge(polygon, ClipEdge::top, clip.minimum.y);
        polygon = clip_edge(polygon, ClipEdge::bottom, clip.maximum.y);
        if (polygon.count < 3u)
            return;
        for (std::size_t index = 1u; index + 1u < polygon.count; ++index)
            emit_triangle(polygon.points[0], polygon.points[index],
                polygon.points[index + 1u], color);
    }

    void Canvas::quad(Vec2 minimum, Vec2 maximum, Color color)
    {
        triangle({ minimum.x, minimum.y }, { maximum.x, minimum.y },
            { maximum.x, maximum.y }, color);
        triangle({ minimum.x, minimum.y }, { maximum.x, maximum.y },
            { minimum.x, maximum.y }, color);
    }

    void Canvas::line(Vec2 a, Vec2 b, float thickness, Color color)
    {
        const Vec2 direction = normalized(b - a, { 1.0f, 0.0f });
        const Vec2 normal = perpendicular(direction) * (thickness * 0.5f);
        triangle(a - normal, b - normal, b + normal, color);
        triangle(a - normal, b + normal, a + normal, color);
    }

    void Canvas::circle(Vec2 center, float radius, Color color, std::uint32_t segments)
    {
        segments = std::clamp<std::uint32_t>(segments, 6u, 96u);
        Vec2 previous = center + Vec2{ radius, 0.0f };
        for (std::uint32_t index = 1u; index <= segments; ++index)
        {
            const float angle = 2.0f * pi * static_cast<float>(index)
                / static_cast<float>(segments);
            const Vec2 current = center
                + Vec2{ std::cos(angle) * radius, std::sin(angle) * radius };
            triangle(center, previous, current, color);
            previous = current;
        }
    }

    void Canvas::capsule(Vec2 a, Vec2 b, float radius, Color color, std::uint32_t segments)
    {
        line(a, b, radius * 2.0f, color);
        circle(a, radius, color, segments);
        circle(b, radius, color, segments);
    }

    void Canvas::polyline(std::span<const Vec2> points, float thickness, Color color)
    {
        if (points.size() < 2u)
            return;
        for (std::size_t index = 1u; index < points.size(); ++index)
            line(points[index - 1u], points[index], thickness, color);
    }
}
''')


def patch_icon_generator() -> None:
    write("tools/generate_runner_icon.py", r'''#!/usr/bin/env python3
from __future__ import annotations

import math
import struct
import sys
import zlib
from pathlib import Path

RGBA = tuple[int, int, int, int]


def blend(pixels: bytearray, size: int, x: int, y: int, color: RGBA) -> None:
    if x < 0 or y < 0 or x >= size or y >= size:
        return
    offset = (y * size + x) * 4
    alpha = color[3] / 255.0
    inverse = 1.0 - alpha
    pixels[offset] = round(color[0] * alpha + pixels[offset] * inverse)
    pixels[offset + 1] = round(color[1] * alpha + pixels[offset + 1] * inverse)
    pixels[offset + 2] = round(color[2] * alpha + pixels[offset + 2] * inverse)
    pixels[offset + 3] = min(255, round(color[3] + pixels[offset + 3] * inverse))


def circle(pixels: bytearray, size: int, cx: float, cy: float,
           radius: float, color: RGBA) -> None:
    minimum_x = max(0, math.floor(cx - radius - 1))
    maximum_x = min(size - 1, math.ceil(cx + radius + 1))
    minimum_y = max(0, math.floor(cy - radius - 1))
    maximum_y = min(size - 1, math.ceil(cy + radius + 1))
    radius_squared = radius * radius
    for y in range(minimum_y, maximum_y + 1):
        for x in range(minimum_x, maximum_x + 1):
            dx = x + 0.5 - cx
            dy = y + 0.5 - cy
            if dx * dx + dy * dy <= radius_squared:
                blend(pixels, size, x, y, color)


def line(pixels: bytearray, size: int, ax: float, ay: float,
         bx: float, by: float, thickness: float, color: RGBA) -> None:
    minimum_x = max(0, math.floor(min(ax, bx) - thickness - 1))
    maximum_x = min(size - 1, math.ceil(max(ax, bx) + thickness + 1))
    minimum_y = max(0, math.floor(min(ay, by) - thickness - 1))
    maximum_y = min(size - 1, math.ceil(max(ay, by) + thickness + 1))
    dx = bx - ax
    dy = by - ay
    length_squared = max(1.0e-6, dx * dx + dy * dy)
    radius_squared = (thickness * 0.5) ** 2
    for y in range(minimum_y, maximum_y + 1):
        for x in range(minimum_x, maximum_x + 1):
            px = x + 0.5
            py = y + 0.5
            t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared))
            qx = ax + dx * t
            qy = ay + dy * t
            if (px - qx) ** 2 + (py - qy) ** 2 <= radius_squared:
                blend(pixels, size, x, y, color)


def polygon(pixels: bytearray, size: int,
            points: list[tuple[float, float]], color: RGBA) -> None:
    minimum_y = max(0, math.floor(min(point[1] for point in points)))
    maximum_y = min(size - 1, math.ceil(max(point[1] for point in points)))
    for y in range(minimum_y, maximum_y + 1):
        scan_y = y + 0.5
        intersections: list[float] = []
        previous = points[-1]
        for current in points:
            if (current[1] > scan_y) != (previous[1] > scan_y):
                t = (scan_y - previous[1]) / (current[1] - previous[1])
                intersections.append(previous[0] + (current[0] - previous[0]) * t)
            previous = current
        intersections.sort()
        for index in range(0, len(intersections) - 1, 2):
            start = max(0, math.ceil(intersections[index]))
            finish = min(size - 1, math.floor(intersections[index + 1]))
            for x in range(start, finish + 1):
                blend(pixels, size, x, y, color)


def rounded_background(pixels: bytearray, size: int) -> None:
    radius = size * 0.18
    for y in range(size):
        for x in range(size):
            nearest_x = min(max(x + 0.5, radius), size - radius)
            nearest_y = min(max(y + 0.5, radius), size - radius)
            dx = x + 0.5 - nearest_x
            dy = y + 0.5 - nearest_y
            if dx * dx + dy * dy > radius * radius:
                continue
            fraction = y / max(1, size - 1)
            blend(pixels, size, x, y,
                  (7 + round(8 * fraction), 19 + round(15 * fraction),
                   31 + round(22 * fraction), 255))


def render(size: int) -> bytes:
    scale = 4
    high = size * scale
    pixels = bytearray(high * high * 4)
    rounded_background(pixels, high)
    cyan = (40, 205, 255, 245)
    cyan_dim = (14, 110, 148, 210)
    gold = (255, 194, 61, 255)
    gold_light = (255, 231, 154, 255)
    amber = (244, 116, 31, 245)

    border = high * 0.035
    line(pixels, high, high * 0.15, high * 0.055,
         high * 0.85, high * 0.055, border, cyan)
    line(pixels, high, high * 0.055, high * 0.15,
         high * 0.055, high * 0.85, border, cyan_dim)
    line(pixels, high, high * 0.15, high * 0.945,
         high * 0.85, high * 0.945, border, cyan_dim)
    line(pixels, high, high * 0.945, high * 0.15,
         high * 0.945, high * 0.85, border, cyan)

    for index, length in enumerate((0.34, 0.46, 0.28, 0.40)):
        y = high * (0.25 + index * 0.12)
        line(pixels, high, high * 0.08, y,
             high * (0.08 + length), y, high * 0.020, cyan_dim)
    polygon(pixels, high, [
        (high * 0.18, high * 0.71), (high * 0.42, high * 0.65),
        (high * 0.36, high * 0.75), (high * 0.64, high * 0.69),
        (high * 0.58, high * 0.80), (high * 0.82, high * 0.73),
        (high * 0.75, high * 0.88), (high * 0.22, high * 0.88)
    ], (26, 83, 104, 235))
    line(pixels, high, high * 0.15, high * 0.88,
         high * 0.86, high * 0.88, high * 0.028, amber)

    head = (high * 0.61, high * 0.22)
    neck = (high * 0.57, high * 0.31)
    hip = (high * 0.49, high * 0.52)
    left_hand = (high * 0.30, high * 0.43)
    right_hand = (high * 0.76, high * 0.37)
    left_knee = (high * 0.34, high * 0.65)
    left_foot = (high * 0.20, high * 0.82)
    right_knee = (high * 0.64, high * 0.63)
    right_foot = (high * 0.82, high * 0.75)
    limb = high * 0.052
    line(pixels, high, *neck, *hip, limb * 1.15, gold)
    line(pixels, high, neck[0], neck[1], high * 0.43, high * 0.37, limb, gold)
    line(pixels, high, high * 0.43, high * 0.37, *left_hand, limb * 0.82, gold)
    line(pixels, high, neck[0], neck[1], high * 0.68, high * 0.32, limb, gold_light)
    line(pixels, high, high * 0.68, high * 0.32, *right_hand, limb * 0.82, gold_light)
    line(pixels, high, *hip, *left_knee, limb * 1.05, gold)
    line(pixels, high, *left_knee, *left_foot, limb * 0.92, gold)
    line(pixels, high, *hip, *right_knee, limb * 1.05, gold_light)
    line(pixels, high, *right_knee, *right_foot, limb * 0.92, gold_light)
    line(pixels, high, left_foot[0] - high * 0.03, left_foot[1],
         left_foot[0] + high * 0.10, left_foot[1], limb * 0.50, amber)
    line(pixels, high, right_foot[0] - high * 0.03, right_foot[1],
         right_foot[0] + high * 0.10, right_foot[1], limb * 0.50, amber)
    circle(pixels, high, *head, high * 0.073, gold_light)
    circle(pixels, high, hip[0], hip[1], high * 0.040, amber)
    polygon(pixels, high, [
        (high * 0.74, high * 0.18), (high * 0.86, high * 0.18),
        (high * 0.80, high * 0.29), (high * 0.89, high * 0.29),
        (high * 0.72, high * 0.49), (high * 0.77, high * 0.34),
        (high * 0.68, high * 0.34)
    ], cyan)

    result = bytearray(size * size * 4)
    sample_count = scale * scale
    for y in range(size):
        for x in range(size):
            totals = [0, 0, 0, 0]
            for sy in range(scale):
                for sx in range(scale):
                    source = ((y * scale + sy) * high + x * scale + sx) * 4
                    for channel in range(4):
                        totals[channel] += pixels[source + channel]
            destination = (y * size + x) * 4
            for channel in range(4):
                result[destination + channel] = totals[channel] // sample_count
    return bytes(result)


def png_bytes(size: int, rgba: bytes) -> bytes:
    raw = b"".join(b"\x00" + rgba[y * size * 4:(y + 1) * size * 4]
                   for y in range(size))
    signature = b"\x89PNG\r\n\x1a\n"

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(
            ">I", zlib.crc32(kind + data) & 0xffffffff)

    return signature + chunk(b"IHDR", struct.pack(
        ">IIBBBBB", size, size, 8, 6, 0, 0, 0)) + chunk(
            b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def bmp_bytes(size: int, rgba: bytes) -> bytes:
    row_bytes = size * 4
    pixels = bytearray()
    for y in range(size - 1, -1, -1):
        for x in range(size):
            offset = (y * size + x) * 4
            r, g, b, a = rgba[offset:offset + 4]
            pixels.extend((b, g, r, a))
    file_size = 14 + 40 + len(pixels)
    return (b"BM" + struct.pack("<IHHI", file_size, 0, 0, 54)
            + struct.pack("<IIIHHIIIIII", 40, size, size, 1, 32, 0,
                          len(pixels), 3780, 3780, 0, 0) + pixels)


def ico_bytes(sizes: tuple[int, ...]) -> bytes:
    images = [png_bytes(size, render(size)) for size in sizes]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + len(images) * 16
    entries = bytearray()
    for size, image in zip(sizes, images, strict=True):
        dimension = 0 if size == 256 else size
        entries.extend(struct.pack("<BBBBHHII", dimension, dimension,
                                   0, 0, 1, 32, len(image), offset))
        offset += len(image)
    return header + entries + b"".join(images)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: generate_runner_icon.py <output-directory>", file=sys.stderr)
        return 2
    output = Path(sys.argv[1]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    rgba_256 = render(256)
    (output / "runner_icon.png").write_bytes(png_bytes(256, rgba_256))
    (output / "runner_icon_512.png").write_bytes(png_bytes(512, render(512)))
    (output / "runner_icon.bmp").write_bytes(bmp_bytes(64, render(64)))
    (output / "runner.ico").write_bytes(ico_bytes((16, 20, 24, 32, 40, 48, 64, 128, 256)))
    print(f"Runner icon assets generated in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
''')


def patch_autonomy() -> None:
    path = "src/autonomy_curriculum.cpp"
    text = read(path)
    text = replace_once(text, r'''            if (catastrophic_invalid && !worker_.has_best_policy()
                && metrics.evaluation_count % 3u == 0u)
            {
                worker_.reset_policy(0x715000u
                    + metrics.evaluation_count * 0x9E3779B97F4A7C15ULL);
                worker_.set_course(stage_, difficulty_, false);
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                worker_message_ = "NO VALID CHAMPION AFTER THREE EVALUATIONS - RESET POLICY NURSERY";
                queue_autosave();
                return;
            }''', r'''            if (catastrophic_invalid && !worker_.has_best_policy()
                && nursery_policy_reset_allowed(stage_, fresh_updates, fresh_evaluations))
            {
                worker_.reset_policy(0x720000u
                    + metrics.total_updates * 0x9E3779B97F4A7C15ULL);
                worker_.set_course(stage_, difficulty_, false);
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                const TrainingMetrics& restarted = worker_.metrics();
                stage_entry_total_updates_ = restarted.total_updates;
                stage_entry_total_episodes_ = restarted.total_episodes;
                stage_entry_evaluation_count_ = restarted.evaluation_count;
                stage_entry_baseline_initialized_ = true;
                worker_message_ = "EXTENDED NURSERY BUDGET EXHAUSTED - FRESH POLICY STARTED; TOTALS PRESERVED";
                queue_autosave();
                return;
            }''', "nursery reset")
    write(path, text)

    path = "src/autonomy_persistence.cpp"
    text = read(path)
    text = text.replace('output << "RUNAUTONOMY 13\\n";',
        'output << "RUNAUTONOMY 15\\n";')
    text = text.replace('version != 13', 'version != 15')
    text = replace_once(text, r'''        snapshot.status.pending_commands = pending_command_count();
        snapshot.status.updates_per_second = worker_updates_per_second_;
''', r'''        snapshot.status.pending_commands = pending_command_count();
        const TrainingMetrics& stage_metrics = worker_.metrics();
        snapshot.status.stage_fresh_updates = stage_metrics.total_updates >= stage_entry_total_updates_
            ? stage_metrics.total_updates - stage_entry_total_updates_ : 0u;
        snapshot.status.stage_required_updates = stage_minimum_fresh_updates(stage_);
        snapshot.status.stage_fresh_episodes = stage_metrics.total_episodes >= stage_entry_total_episodes_
            ? stage_metrics.total_episodes - stage_entry_total_episodes_ : 0u;
        snapshot.status.stage_required_episodes = stage_minimum_fresh_episodes(stage_);
        snapshot.status.stage_fresh_evaluations = stage_metrics.evaluation_count >= stage_entry_evaluation_count_
            ? stage_metrics.evaluation_count - stage_entry_evaluation_count_ : 0u;
        snapshot.status.stage_required_evaluations = static_cast<std::uint64_t>(
            required_mastery_confirmations(stage_));
        snapshot.status.updates_per_second = worker_updates_per_second_;
''', "stage progress publication")
    write(path, text)

    path = "src/autonomy_commands.cpp"
    text = read(path)
    text = text.replace("NO V0.7.6 AUTOSAVE FOUND", "NO V0.7.20 AUTOSAVE FOUND")
    text = text.replace("V0.7.6 AUTOSAVE RESUMED ASYNCHRONOUSLY",
        "V0.7.20 AUTOSAVE RESUMED ASYNCHRONOUSLY")
    write(path, text)

    path = "src/autonomy_runtime.cpp"
    text = read(path)
    text = replace_once(text, '#include "autonomy.hpp"\n',
        '#include "autonomy.hpp"\n#include "preview_sync.hpp"\n',
        "preview sync include")
    old = r'''        if (rig_changed)
        {
            live_blueprint_ = snapshot.blueprint;
            live_.set_blueprint(live_blueprint_, false);
        }
        live_.set_course(snapshot.status.stage, snapshot.status.difficulty, false);
        live_.policy().parameters() = snapshot.parameters;
        if (rig_changed || best_changed || course_changed)
            live_.reset_preview(0xDEADBEEFu + snapshot.metrics.update + snapshot.metrics.best_update);
'''
    new = r'''        const preview_sync::Decision decision = preview_sync::decide(
            rig_changed, course_changed, best_changed);
        if (decision.replace_blueprint)
        {
            live_blueprint_ = snapshot.blueprint;
            live_.set_blueprint(live_blueprint_, false);
        }
        if (decision.replace_course)
            live_.set_course(snapshot.status.stage, snapshot.status.difficulty, false);
        if (decision.adopt_controller)
            live_.policy().parameters() = snapshot.parameters;
        if (decision.reset_episode)
            live_.reset_preview(0xDEADBEEFu
                + snapshot.metrics.update + snapshot.metrics.best_update);
'''
    text = replace_once(text, old, new, "preview publication continuity")
    write(path, text)

    path = "src/ppo.hpp"
    text = read(path)
    text, count = re.subn(
        r"inline constexpr std::uint32_t training_semantics_version = 0x[0-9A-Fa-f']+u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'2001u;",
        text, count=1)
    if count != 1:
        raise RuntimeError("training semantics version marker missing")
    write(path, text)


def patch_renderer() -> None:
    path = "src/renderer.cpp"
    text = read(path)
    text = replace_once(text,
        'bool VulkanRenderer::render(std::span<const Vertex> vertices, int drawable_width, int drawable_height, std::string& error)',
        'bool VulkanRenderer::render(std::span<const Vertex> vertices,\n        int canvas_width, int canvas_height,\n        int drawable_width, int drawable_height, std::string& error)',
        "renderer signature")
    text = replace_once(text,
        '''        if (drawable_width <= 0 || drawable_height <= 0)
            return true;''',
        '''        if (canvas_width <= 0 || canvas_height <= 0
            || drawable_width <= 0 || drawable_height <= 0)
            return true;''',
        "renderer dimensions")
    text = replace_once(text,
        '''            struct PushConstants { float width; float height; } push{ static_cast<float>(impl.extent.width), static_cast<float>(impl.extent.height) };''',
        '''            struct PushConstants { float width; float height; } push{
                static_cast<float>(canvas_width), static_cast<float>(canvas_height) };''',
        "logical canvas push constants")
    write(path, text)


def patch_main() -> None:
    path = "src/main.cpp"
    text = read(path)
    text = replace_once(text, '#include "renderer.hpp"\n',
        '#include "renderer.hpp"\n#include "ui_layout.hpp"\n', "ui layout include")
    text = replace_once(text, r'''    [[nodiscard]] bool wants_camera_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-camera";
    }
''', r'''    [[nodiscard]] bool wants_camera_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-camera";
    }

    [[nodiscard]] bool wants_ui_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-ui";
    }
''', "ui diagnostic argument")
    diagnostic = r'''    if (wants_ui_diagnostic(argc, argv))
    {
        bool valid = true;
        for (const auto& size : runner::ui_layout::validation_sizes)
            valid = valid && runner::ui_layout::live_layout_valid(size[0], size[1]);
        const runner::ui_layout::SurfaceScale dpi =
            runner::ui_layout::logical_surface_scale(1600.0f, 900.0f,
                2400.0f, 1350.0f);
        valid = valid && std::abs(dpi.x - 1.5f) < 1.0e-5f
            && std::abs(dpi.y - 1.5f) < 1.0e-5f;
        std::printf("Runner %s UI diagnostic: %s; layouts=%zu dpi=%.2fx%.2f\\n",
            RUNNER_VERSION, valid ? "passed" : "failed",
            runner::ui_layout::validation_sizes.size(), dpi.x, dpi.y);
        return valid ? 0 : 1;
    }

'''
    text = replace_once(text,
        '    if (wants_acceptance_diagnostic(argc, argv))\n',
        diagnostic + '    if (wants_acceptance_diagnostic(argc, argv))\n',
        "ui diagnostic block")
    text = replace_once(text, r'''            std::filesystem::path{ "docs" } / "SANDHYBRID_INTEGRATION_BRIDGE.md",
            std::filesystem::path{ "docs" } / "SandHybrid-missioncache.md"
''', r'''            std::filesystem::path{ "docs" } / "SANDHYBRID_INTEGRATION_BRIDGE.md",
            std::filesystem::path{ "docs" } / "SandHybrid-missioncache.md",
            std::filesystem::path{ "assets" } / "ui" / "runner_icon.png",
            std::filesystem::path{ "assets" } / "ui" / "runner_icon.bmp",
            std::filesystem::path{ "assets" } / "ui" / "runner.ico"
''', "package icon requirements")
    text = replace_once(text, r'''    if (window == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 1280, 820);
''', r'''    if (window == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 1280, 820);
    const std::filesystem::path icon_path = base_directory
        / RUNNER_ASSET_DIRECTORY / "ui" / "runner_icon.bmp";
    if (SDL_Surface* icon = SDL_LoadBMP(icon_path.string().c_str()); icon != nullptr)
    {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }
''', "runtime icon")
    text = replace_once(text, r'''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''', r'''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_TAB: input.tab_pressed = true; break;
                    case SDL_SCANCODE_T: input.totals_pressed = true; break;
                    case SDL_SCANCODE_U: input.units_pressed = true; break;
                    case SDL_SCANCODE_A: input.art_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''', "keyboard mappings")
    old_mouse = r'''        SDL_GetWindowSize(window, &logical_width, &logical_height);
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        const float mouse_scale_x = logical_width > 0
            ? static_cast<float>(drawable_width) / static_cast<float>(logical_width) : 1.0f;
        const float mouse_scale_y = logical_height > 0
            ? static_cast<float>(drawable_height) / static_cast<float>(logical_height) : 1.0f;
        input.mouse = { mouse_x * mouse_scale_x, mouse_y * mouse_scale_y };
'''
    new_mouse = r'''        SDL_GetWindowSize(window, &logical_width, &logical_height);
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        input.mouse = { mouse_x, mouse_y };
'''
    text = replace_once(text, old_mouse, new_mouse, "logical mouse coordinates")
    text = replace_once(text, r'''        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        application.frame(input, dt, drawable_width, drawable_height);
        if (!renderer.render(application.vertices(), drawable_width, drawable_height, error))''', r'''        SDL_GetWindowSize(window, &logical_width, &logical_height);
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        application.frame(input, dt, logical_width, logical_height);
        if (!renderer.render(application.vertices(), logical_width, logical_height,
            drawable_width, drawable_height, error))''', "logical render canvas")
    write(path, text)


def patch_app() -> None:
    path = "src/app.cpp"
    text = read(path)
    text = text.replace('bool optional_art_enabled{ true };',
        'bool optional_art_enabled{ false };')
    text = text.replace('runner-v0717-gait-autosave.eppo',
        'runner-v0720-ui-autosave.eppo')
    text = text.replace('runner-v0717-gait-evolved.rig',
        'runner-v0720-ui-evolved.rig')
    text = text.replace('runner-v0717-gait-autonomy.state',
        'runner-v0720-ui-autonomy.state')
    text = text.replace('if (optional_art_enabled && optional_foot_art.loaded())',
        'if (optional_foot_art.loaded())', 1)
    text = text.replace('float minimum_scale = 1.05f) noexcept',
        'float minimum_scale = ui_layout::minimum_readable_text_scale) noexcept', 1)
    text = text.replace('float maximum_width, float minimum_scale = 1.05f)',
        'float maximum_width, float minimum_scale = ui_layout::minimum_readable_text_scale)', 1)
    text = replace_once(text, r'''            float scale = 1.55f;
            Vec2 measured = font::measure_text(label, scale * ui_font_scale);
            while (measured.x > rect.size.x - 12.0f && scale > 1.05f)
            {
                scale -= 0.08f;
                measured = font::measure_text(label, scale * ui_font_scale);
            }''', r'''            float scale = 1.28f;
            Vec2 measured = font::measure_text(label, scale * ui_font_scale);
            while (measured.x > rect.size.x - 14.0f
                && scale > ui_layout::minimum_readable_text_scale)
            {
                scale -= 0.06f;
                measured = font::measure_text(label, scale * ui_font_scale);
            }''', "button typography")

    top_bar = r'''        void draw_top_bar(const InputState& input, int width)
        {
            const ui_layout::Box top_bar = ui_layout::top_bar_box(static_cast<float>(width));
            canvas.quad({ top_bar.x, top_bar.y },
                { top_bar.x + top_bar.width, top_bar.y + top_bar.height }, rgb(0x0b1119));
            canvas.line({ 0.0f, top_bar.height - 1.0f },
                { static_cast<float>(width), top_bar.height - 1.0f }, 2.0f, border);
            add_text(canvas, { 18.0f, 12.0f }, "RUNNER v" RUNNER_VERSION, 1.68f, white);
            add_text(canvas, { 19.0f, 46.0f },
                "AUTONOMOUS PHYSICS LOCOMOTION LAB", 0.86f, muted);

            const float tab_width = width >= 1500 ? 164.0f : 148.0f;
            const float start_x = static_cast<float>(width) - tab_width * 2.0f - 16.0f;
            if (start_x > 620.0f)
            {
                add_text_fit(canvas, { 330.0f, 20.0f },
                    "TAB VIEW  SPACE TRAIN  1/2/3 SPEED  T TOTALS  U UNITS  A ART  R RESET",
                    0.82f, muted, start_x - 352.0f, 0.78f);
            }
            if (button({ { start_x, 17.0f }, { tab_width - 6.0f, 42.0f } },
                "LIVE AUTOPILOT", input, mode == Mode::live))
                mode = Mode::live;
            if (button({ { start_x + tab_width, 17.0f }, { tab_width - 6.0f, 42.0f } },
                "RIG LAB", input, mode == Mode::rig_lab))
                mode = Mode::rig_lab;
        }

'''
    text = replace_between(text, '        void draw_top_bar(',
        '        void draw_course_ground(', top_bar, "top bar")

    course_reference = r'''        void draw_course_reference(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const float progress = environment.course_progress();
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - 2.0f;
            const float right = camera + half_view + 2.0f;
            const float marker_spacing = ui_layout::course_reference_marker_spacing_m(distance_units);
            const int first_marker = static_cast<int>(std::floor((left + progress) / marker_spacing));
            const int last_marker = static_cast<int>(std::ceil((right + progress) / marker_spacing));
            for (int index = first_marker; index <= last_marker; ++index)
            {
                if (index < 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;
                const float x = distance - progress;
                const float ground = environment.ground_height_at(x);
                const Vec2 base = world_to_screen({ x, ground }, viewport, camera, scale);
                const Vec2 top = world_to_screen({ x, ground + 0.66f }, viewport, camera, scale);
                canvas.line(base, top, 3.0f, accent_dim);
                const Rect sign{ top + Vec2{ -46.0f, -25.0f }, { 92.0f, 25.0f } };
                add_rounded_rect(canvas, sign, 5.0f, rgb(0x102431, 0.97f), accent, 1.0f);
                const std::string marker_label = index == 0 ? "START"
                    : distance_units == ui_layout::DistanceUnits::metric
                        ? (distance >= 1000.0f
                            ? std::format("{:.2f} KM", distance / 1000.0f)
                            : std::format("{:.0f} M", distance))
                        : (distance >= 1609.344f
                            ? std::format("{:.2f} MI", distance / 1609.344f)
                            : std::format("{:.0f} FT", distance * 3.2808399f));
                add_text_fit(canvas, sign.position + Vec2{ 6.0f, 5.0f },
                    marker_label, 0.88f, white, sign.size.x - 12.0f, 0.78f);
            }
        }

'''
    text = replace_between(text, '        void draw_course_reference(',
        '        void draw_course_features(', course_reference, "course reference")

    pip = r'''        void draw_training_pip(Rect rect)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019, 0.99f), accent_dim, 1.5f);
            add_text(canvas, rect.position + Vec2{ 12.0f, 9.0f },
                "LIVE TRAINING ENVIRONMENT", 0.88f, accent);
            if (!trainer.has_training_preview())
            {
                add_text_fit(canvas, rect.position + Vec2{ 12.0f, 42.0f },
                    "WAITING FOR FIRST INTACT TRAINING FRAME", 0.90f, muted,
                    rect.size.x - 24.0f);
                return;
            }

            const sim::Environment& environment = trainer.training_preview();
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty() || rig.root_node >= particles.size())
            {
                add_text_fit(canvas, rect.position + Vec2{ 12.0f, 42.0f },
                    "TRAINING FRAME HAS NO COMPLETE RIG", 0.90f, danger,
                    rect.size.x - 24.0f);
                return;
            }

            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(environment.course_stage(), environment);
            const bool foot_only = !environment.non_foot_grounded();
            const bool intact = environment.body_integrity_valid();
            const Color state_color = qualification.valid ? green
                : intact && foot_only ? yellow : danger;
            const std::string_view state_text = qualification.valid
                ? "STAGE VALID" : !intact ? "BROKEN RIG"
                : !foot_only ? "BODY CONTACT"
                : rl::primary_motion_rejection_name(qualification.rejection_mask);
            add_text_fit(canvas, rect.position + Vec2{ rect.size.x - 132.0f, 9.0f },
                state_text, 0.76f, state_color, 120.0f, 0.68f);

            const Rect inner{ rect.position + Vec2{ 8.0f, 34.0f },
                { rect.size.x - 16.0f, rect.size.y - 64.0f } };
            const float root_x = particles[rig.root_node].position.x;
            float body_min_x = std::numeric_limits<float>::infinity();
            float body_max_x = -std::numeric_limits<float>::infinity();
            float body_min_y = std::numeric_limits<float>::infinity();
            float body_max_y = -std::numeric_limits<float>::infinity();
            for (const sim::Particle& particle : particles)
            {
                body_min_x = std::min(body_min_x, particle.position.x - particle.radius);
                body_max_x = std::max(body_max_x, particle.position.x + particle.radius);
                body_min_y = std::min(body_min_y, particle.position.y - particle.radius);
                body_max_y = std::max(body_max_y, particle.position.y + particle.radius);
            }
            const float view_min_x = std::min(root_x - 1.55f, body_min_x - 0.28f);
            const float view_max_x = std::max(root_x + 3.35f, body_max_x + 0.42f);
            const float view_min_y = std::min(body_min_y - 0.18f,
                environment.ground_height_at(root_x) - 0.18f);
            const float view_max_y = body_max_y + 0.32f;
            const float world_width = std::max(3.8f, view_max_x - view_min_x);
            const float world_height = std::max(1.5f, view_max_y - view_min_y);
            const float scale = view_camera::pip_pixels_per_meter(
                (inner.size.x - 12.0f) / world_width,
                (inner.size.y * 0.78f) / world_height);
            const float camera = (view_min_x + view_max_x) * 0.5f;

            canvas.push_clip(inner.position, inner.position + inner.size);
            std::vector<Vec2> ground_points{};
            ground_points.reserve(81);
            for (int sample = 0; sample <= 80; ++sample)
            {
                const float fraction = static_cast<float>(sample) / 80.0f;
                const float world_x = camera + (fraction - 0.5f) * inner.size.x / scale;
                ground_points.push_back(world_to_screen(
                    { world_x, environment.ground_height_at(world_x) },
                    inner, camera, scale, 0.82f));
            }
            canvas.polyline(ground_points, 3.0f, rgb(0x65727d));
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const Vec2 point = world_to_screen(feature.center,
                    inner, camera, scale, 0.82f);
                if (feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    canvas.circle(point, std::max(3.0f, feature.radius * scale),
                        feature.kind == sim::CourseFeatureKind::projectile ? danger : yellow, 18);
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        inner, camera, scale, 0.82f);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        inner, camera, scale, 0.82f);
                    add_rounded_rect(canvas,
                        { { minimum.x, maximum.y },
                          { maximum.x - minimum.x, minimum.y - maximum.y } },
                        3.0f, accent_dim, accent, 1.0f);
                }
            }
            draw_creature(environment, inner, camera, scale);
            canvas.pop_clip();

            const std::string pip_metrics = std::format(
                "TOTAL {}  STAGE {}  {:.1f}M  STEPS {}",
                trainer.metrics().total_updates, trainer.metrics().update,
                environment.distance_travelled(), environment.gait_cycles());
            add_text_fit(canvas, rect.position + Vec2{ 12.0f, rect.size.y - 23.0f },
                pip_metrics, 0.70f, state_color, rect.size.x - 24.0f, 0.64f);
            add_rounded_rect(canvas, rect, 10.0f, Color{}, accent_dim, 1.5f);
        }

'''
    text = replace_between(text, '        void draw_training_pip(',
        '        void draw_live_panel(', pip, "training PIP")

    panel_start = text.find('        void draw_live_panel(')
    panel_end = text.find('        void draw_live_world(', panel_start)
    if panel_start < 0 or panel_end < 0:
        raise RuntimeError("live panel markers missing")
    panel = text[panel_start:panel_end]
    panel = replace_once(panel,
        '            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);\n',
        '            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);\n'
        '            canvas.push_clip(rect.position + Vec2{ 1.0f, 1.0f },\n'
        '                rect.position + rect.size - Vec2{ 1.0f, 1.0f });\n',
        "panel clipping start")
    panel = panel.replace('            add_text(canvas, cursor, "AUTONOMOUS RIG TRAINER", 1.72f, white);',
        '            add_text_fit(canvas, cursor, "AUTONOMOUS RIG TRAINER", 1.54f,\n'
        '                white, usable_width, 1.08f);')
    panel = panel.replace('rl::mastery_lock_confirmations), 1.12f, white, usable_width);',
        'rl::required_mastery_confirmations(autonomy.stage)),\n'
        '                1.02f, white, usable_width);')
    difficulty_anchor = '            cursor.y += 29.0f;\n            cursor.y += add_wrapped_text(canvas, cursor, autonomy.message, 1.00f,\n'
    if difficulty_anchor not in panel:
        raise RuntimeError("panel stage work anchor missing")
    panel = panel.replace(difficulty_anchor,
        '            cursor.y += 25.0f;\n'
        '            add_text_fit(canvas, cursor,\n'
        '                std::format("STAGE WORK  UPD {}/{}  EPS {}/{}  EVAL {}/{}",\n'
        '                    autonomy.stage_fresh_updates, autonomy.stage_required_updates,\n'
        '                    autonomy.stage_fresh_episodes, autonomy.stage_required_episodes,\n'
        '                    autonomy.stage_fresh_evaluations, autonomy.stage_required_evaluations),\n'
        '                0.78f, accent, usable_width, 0.70f);\n'
        '            cursor.y += 22.0f;\n'
        '            cursor.y += add_wrapped_text(canvas, cursor, autonomy.message, 0.88f,\n', 1)
    panel = panel.replace('"METRIC / 0.25 KM"', '"METRIC / 10 M"')
    panel = panel.replace('"IMPERIAL / 0.25 MI"', '"IMPERIAL / 50 FT"')
    panel = panel.replace('std::format("UPDATE {}   ENV STEPS {}",\n                    metrics.update, metrics.environment_steps)',
        'std::format("TOTAL {}  STAGE {}  ENV {}",\n                    metrics.total_updates, metrics.update, metrics.environment_steps)')
    close = panel.rfind('        }\n')
    if close < 0:
        raise RuntimeError("live panel closing brace missing")
    panel = panel[:close] + r'''            canvas.pop_clip();
            add_rounded_rect(canvas, rect, 11.0f, Color{}, border, 1.0f);
''' + panel[close:]
    text = text[:panel_start] + panel + text[panel_end:]

    live_world = r'''        void draw_live_world(Rect viewport, float dt, const InputState& input)
        {
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
            const auto& particles = environment.particles();
            if (contains(viewport, input.mouse) && std::abs(input.wheel) >= 0.01f)
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(
                    live_zoom_factor, input.wheel);
                live_zoom_auto = false;
            }

            float rig_height = 2.4f;
            if (!particles.empty())
            {
                float minimum_y = std::numeric_limits<float>::infinity();
                float maximum_y = -std::numeric_limits<float>::infinity();
                for (const sim::Particle& particle : particles)
                {
                    minimum_y = std::min(minimum_y, particle.position.y - particle.radius);
                    maximum_y = std::max(maximum_y, particle.position.y + particle.radius);
                }
                if (std::isfinite(minimum_y) && std::isfinite(maximum_y))
                    rig_height = std::max(0.75f, maximum_y - minimum_y);
            }
            const float target_pixels_per_meter = view_camera::fitted_pixels_per_meter(
                viewport.size.y, rig_height, live_zoom_factor);
            live_pixels_per_meter = view_camera::smooth_zoom(
                live_pixels_per_meter, target_pixels_per_meter, dt);
            if (!particles.empty())
            {
                const std::size_t root = environment.blueprint().root_node;
                if (root < particles.size())
                {
                    const float target_camera = particles[root].position.x
                        + view_camera::lookahead_meters(
                            viewport.size.x, live_pixels_per_meter);
                    camera_x = view_camera::smooth_camera(
                        camera_x, target_camera, live_pixels_per_meter, dt);
                }
            }

            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            canvas.push_clip(viewport.position + Vec2{ 1.0f, 1.0f },
                viewport.position + viewport.size - Vec2{ 1.0f, 1.0f });
            draw_course_ground(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_reference(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_features(environment, viewport, camera_x, live_pixels_per_meter);
            draw_creature(environment, viewport, camera_x, live_pixels_per_meter);

            const ui_layout::Box world_box{
                viewport.position.x, viewport.position.y,
                viewport.size.x, viewport.size.y };
            const ui_layout::Box telemetry_box = ui_layout::primary_telemetry_box(world_box);
            const ui_layout::Box pip_box = ui_layout::training_pip_box(world_box);
            const ui_layout::Box bottom_box = ui_layout::bottom_telemetry_box(world_box);
            const Rect telemetry{ { telemetry_box.x, telemetry_box.y },
                { telemetry_box.width, telemetry_box.height } };
            const Rect bottom{ { bottom_box.x, bottom_box.y },
                { bottom_box.width, bottom_box.height } };
            add_rounded_rect(canvas, telemetry, 9.0f,
                rgb(0x07111b, 0.95f), border, 1.0f);
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const float text_width = telemetry.size.x - 24.0f;
            Vec2 line = telemetry.position + Vec2{ 12.0f, 11.0f };
            add_text_fit(canvas, line,
                std::format("{}  /  {:.0f}%", sim::course_stage_name(autonomy.stage),
                    autonomy.difficulty * 100.0f),
                1.42f, white, text_width, 1.00f);
            line.y += 31.0f;
            add_text_fit(canvas, line,
                std::format("SPEED {}   DIST {}   COURSE {}",
                    format_speed(environment.forward_speed()),
                    format_distance(environment.distance_travelled()),
                    format_distance(environment.course_progress())),
                0.92f, environment.valid_motion() ? green : danger, text_width);
            line.y += 24.0f;
            add_text_fit(canvas, line,
                std::format("STEPS {}  CROSS {}  HEEL {}  TOE {}  SLIP {:.2f}",
                    environment.alternating_steps(), environment.limb_crossings(),
                    environment.heel_strikes(), environment.toe_offs(),
                    environment.stance_slip_speed()),
                0.84f, environment.recovering() ? yellow : muted, text_width);
            line.y += 23.0f;
            add_text_fit(canvas, line,
                std::format("LEFT {}   RIGHT {}   PASSED {}",
                    sim::foot_contact_phase_name(environment.left_foot_phase()),
                    sim::foot_contact_phase_name(environment.right_foot_phase()),
                    environment.obstacles_passed()),
                0.82f, muted, text_width);
            line.y += 23.0f;
            add_text_fit(canvas, line,
                std::format("{}   TOTAL UPDATES {}   STAGE {}",
                    sim::invalid_motion_name(environment.invalid_reason()),
                    trainer.metrics().total_updates, trainer.metrics().update),
                0.80f, environment.valid_motion() ? accent : danger, text_width);

            draw_training_pip({ { pip_box.x, pip_box.y },
                { pip_box.width, pip_box.height } });
            add_rounded_rect(canvas, bottom, 8.0f,
                rgb(0x07111b, 0.96f), border, 1.0f);
            add_text_fit(canvas, bottom.position + Vec2{ 11.0f, 10.0f },
                std::format("{}   v{}   VIEW {:.0f} PX/M {}   {}",
                    trainer.has_best_policy()
                        ? "RETAINED CHAMPION PREVIEW"
                        : "CURRENT EXPLORATORY POLICY",
                    RUNNER_VERSION, live_pixels_per_meter,
                    live_zoom_auto ? "AUTO" : "MANUAL",
                    trainer.background_enabled() ? "TRAINING" : "PAUSED"),
                0.86f, trainer.has_best_policy() ? green : yellow,
                bottom.size.x - 22.0f, 0.76f);
            canvas.pop_clip();
            add_rounded_rect(canvas, viewport, 11.0f, Color{}, border, 1.0f);
        }

'''
    text = replace_between(text, '        void draw_live_world(',
        '        void draw_joint_lab(', live_world, "live world")

    text = replace_once(text, r'''        void process_shortcuts(const InputState& input)
        {
            if (input.key_1_pressed) mode = Mode::live;
            if (input.key_2_pressed || input.key_3_pressed) mode = Mode::rig_lab;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
''', r'''        void process_shortcuts(const InputState& input)
        {
            if (input.tab_pressed)
                mode = mode == Mode::live ? Mode::rig_lab : Mode::live;
            if (input.key_1_pressed) trainer.set_updates_per_cycle(1);
            if (input.key_2_pressed) trainer.set_updates_per_cycle(2);
            if (input.key_3_pressed) trainer.set_updates_per_cycle(4);
            if (input.totals_pressed)
                live_panel_page = live_panel_page == LivePanelPage::results
                    ? LivePanelPage::totals : LivePanelPage::results;
            if (input.units_pressed)
                distance_units = distance_units == ui_layout::DistanceUnits::metric
                    ? ui_layout::DistanceUnits::imperial : ui_layout::DistanceUnits::metric;
            if (input.art_pressed)
                optional_art_enabled = !optional_art_enabled;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
''', "shortcut semantics")
    text = text.replace('"WINDOW TOO SMALL - MINIMUM CONTENT 1080 X 800"',
        '"WINDOW TOO SMALL - MINIMUM WINDOW 1280 X 820"')
    write(path, text)


def patch_cmake() -> None:
    path = "CMakeLists.txt"
    text = read(path)
    text = text.replace('project(Runner VERSION 0.7.19 LANGUAGES CXX)',
        'project(Runner VERSION 0.7.20 LANGUAGES CXX)')
    start = 'set(RUNNER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-v0719")'
    end = 'include(FetchContent)'
    asset_block = r'''set(RUNNER_GENERATED_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-assets")
file(MAKE_DIRECTORY "${RUNNER_GENERATED_ASSET_DIR}")
execute_process(
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_runner_icon.py"
        "${RUNNER_GENERATED_ASSET_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE RUNNER_ICON_RESULT)
if(NOT RUNNER_ICON_RESULT EQUAL 0)
    message(FATAL_ERROR "Runner v0.7.20 icon generation failed")
endif()
set(RUNNER_ICON_PNG "${RUNNER_GENERATED_ASSET_DIR}/runner_icon.png")
set(RUNNER_ICON_PNG_512 "${RUNNER_GENERATED_ASSET_DIR}/runner_icon_512.png")
set(RUNNER_ICON_BMP "${RUNNER_GENERATED_ASSET_DIR}/runner_icon.bmp")
set(RUNNER_ICON_ICO "${RUNNER_GENERATED_ASSET_DIR}/runner.ico")

'''
    text = replace_between(text, start, end, asset_block, "generated source removal")
    text = text.replace('    "${RUNNER_GENERATED_DIR}/autonomy_commands.cpp"\n'
        '    "${RUNNER_GENERATED_DIR}/autonomy_curriculum.cpp"\n'
        '    "${RUNNER_GENERATED_DIR}/autonomy_persistence.cpp")',
        '    src/autonomy_commands.cpp\n'
        '    src/autonomy_curriculum.cpp\n'
        '    src/autonomy_persistence.cpp)')
    text = text.replace('        "${RUNNER_GENERATED_DIR}/main.cpp"\n'
        '        "${RUNNER_GENERATED_DIR}/app.cpp"',
        '        src/main.cpp\n        src/app.cpp')
    text = text.replace('        src/ui_font.hpp src/view_camera.hpp src/locomotion_strategy.hpp)',
        '        src/ui_font.hpp src/ui_layout.hpp src/view_camera.hpp\n'
        '        src/locomotion_strategy.hpp src/preview_sync.hpp)')
    resource = r'''    if(WIN32)
        string(REPLACE "\\" "/" RUNNER_ICON_ICO_RC "${RUNNER_ICON_ICO}")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/runner_icon.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/runner_icon.rc" @ONLY)
        target_sources(Runner PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/runner_icon.rc")
    endif()
'''
    text = replace_once(text, '    runner_enable_warnings(Runner)\n',
        '    runner_enable_warnings(Runner)\n\n' + resource,
        "Windows icon resource")
    text = replace_once(text,
        '        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Runner>/assets"\n',
        '        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Runner>/assets"\n'
        '        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Runner>/assets/ui"\n'
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_PNG}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon.png"\n'
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_PNG_512}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon_512.png"\n'
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_BMP}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon.bmp"\n'
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_ICO}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner.ico"\n',
        "post-build icons")
    text = replace_once(text,
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"\n',
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"\n'
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"\n',
        "post-build v0720 doc")
    text = replace_once(text,
        '    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)\n',
        '    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)\n'
        '    install(FILES "${RUNNER_ICON_PNG}" "${RUNNER_ICON_PNG_512}"\n'
        '        "${RUNNER_ICON_BMP}" "${RUNNER_ICON_ICO}" DESTINATION assets/ui)\n',
        "install icons")
    text = replace_once(text,
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"\n        DESTINATION docs)',
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"\n'
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"\n'
        '        DESTINATION docs)',
        "install v0720 doc")
    test_block = r'''    add_executable(RunnerV0720UiTests
        tests/v0720_ui_tests.cpp src/canvas.cpp)
    target_include_directories(RunnerV0720UiTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0720UiTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0720UiTests PRIVATE
        RUNNER_GENERATED_ASSET_DIRECTORY="${RUNNER_GENERATED_ASSET_DIR}")
    runner_enable_warnings(RunnerV0720UiTests)
    add_test(NAME Runner.V0720Ui COMMAND RunnerV0720UiTests)
    set_tests_properties(Runner.V0720Ui PROPERTIES TIMEOUT 30)

'''
    text = replace_once(text,
        '    add_executable(RunnerCoreTests tests/core_tests.cpp)\n',
        test_block + '    add_executable(RunnerCoreTests tests/core_tests.cpp)\n',
        "UI test target")
    text = replace_once(text,
        '        add_test(NAME Runner.CameraDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-camera)\n'
        '        set_tests_properties(Runner.CameraDiagnostic PROPERTIES TIMEOUT 30)\n',
        '        add_test(NAME Runner.CameraDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-camera)\n'
        '        set_tests_properties(Runner.CameraDiagnostic PROPERTIES TIMEOUT 30)\n'
        '        add_test(NAME Runner.UiDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-ui)\n'
        '        set_tests_properties(Runner.UiDiagnostic PROPERTIES TIMEOUT 30)\n',
        "UI diagnostic test")
    write(path, text)


def patch_tests_docs() -> None:
    write("tests/v0720_ui_tests.cpp", r'''#include "preview_sync.hpp"
#include "renderer.hpp"
#include "ui_layout.hpp"

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
    require(canvas.clip_depth() == 0u, "canvas clip stack did not unwind");
    for (const runner::render::Vertex& vertex : canvas.vertices())
    {
        require(vertex.position.x >= 10.0f && vertex.position.x <= 40.0f
                && vertex.position.y >= 20.0f && vertex.position.y <= 50.0f,
            "clipped canvas vertex escaped its viewport");
    }

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
''')

    write("docs/RUNNER_V0720_UI_PREVIEW_ICON.md", r'''# Runner v0.7.20 UI, preview continuity, and application identity

Runner v0.7.20 uses logical SDL window coordinates for application layout and input while Vulkan maps the same canvas into the drawable swapchain. This removes the high-DPI half-size interface seen on scaled Windows displays.

The world viewport and training PIP now use CPU-side rectangle clipping. Terrain cells, course markers, hazards, rigs, and particles cannot escape behind the side panel, through the panel gap, or outside their cards.

The live interface uses shared layout boxes for the top telemetry card, PIP, bottom controller card, world, and side panel. The required layouts are validated at 1280x820, 1600x900, 1920x1080, 2047x1112, and 2560x1440.

Immutable training publication no longer calls `set_course()` on every synchronization. The large preview keeps running through normal telemetry updates and newly retained champions; it resets only for a real rig/course change, explicit user reset, or terminal episode.

The executable is built from the canonical C++23 source files. The obsolete configure-time source patch generator was removed.

A deterministic high-contrast Runner icon generator produces transparent PNG artwork, an SDL BMP window icon, and a multi-resolution Windows ICO containing 16, 20, 24, 32, 40, 48, 64, 128, and 256 pixel entries. Windows embeds the ICO into `Runner.exe`, and packaged builds include all icon assets under `assets/ui`.
''')

    changelog = read("CHANGELOG.md")
    if not changelog.startswith("## 0.7.20"):
        prefix = r'''## 0.7.20

- Fixed high-DPI coordinate mismatch by separating logical UI dimensions from Vulkan drawable dimensions.
- Added deterministic Canvas clipping for the world viewport and training PIP.
- Rebuilt live telemetry, PIP, bottom status, panel spacing, and text sizing around shared layout boxes.
- Stopped immutable publication from resetting the large preview on every training update.
- Preserved complete live episodes when a better retained champion is published.
- Folded generated v0.7.19 patches into canonical C++23 sources and removed the source rewriter.
- Added transparent PNG, SDL BMP, and multi-resolution embedded Windows application icons.
- Added deterministic UI, DPI, clipping, preview-reset, and icon-format regression tests.

'''
        write("CHANGELOG.md", prefix + changelog)

    readme = read("README.md")
    readme = readme.replace("Runner 0.7.19 is", "Runner 0.7.20 is", 1)
    section = r'''## v0.7.20 UI and preview continuity

- Uses logical SDL coordinates end-to-end for readable Windows high-DPI rendering.
- Clips world and PIP geometry to their cards instead of allowing terrain or markers behind the GUI.
- Keeps the large live preview running across normal training publications and retained champion updates.
- Builds canonical C++23 source directly; the configure-time source patcher is gone.
- Embeds and packages a complete high-contrast Runner icon set.

'''
    marker = "## v0.7.19 general locomotion\n"
    if section not in readme:
        if marker not in readme:
            raise RuntimeError("README v0.7.19 marker missing")
        readme = readme.replace(marker, section + marker, 1)
    doc_line = '- [`docs/RUNNER_V0720_UI_PREVIEW_ICON.md`](docs/RUNNER_V0720_UI_PREVIEW_ICON.md) documents logical DPI, clipping, preview continuity, and application icon integration.\n'
    if doc_line not in readme:
        anchor = '- [`docs/RUNNER_V0719_GENERAL_LOCOMOTION.md`](docs/RUNNER_V0719_GENERAL_LOCOMOTION.md) documents balance reserve, terrain adaptation, running, reversal, flee behavior, and emergency recovery.\n'
        if anchor in readme:
            readme = readme.replace(anchor, anchor + doc_line, 1)
    write("README.md", readme)


def patch_repository_audit() -> None:
    write("tools/repository_audit.cmake", r'''if(NOT DEFINED RUNNER_SOURCE_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR was not provided")
endif()

foreach(required IN ITEMS
        AGENTS.md CHANGELOG.md missioncache.md README.md
        docs/SANDHYBRID_INTEGRATION_BRIDGE.md
        docs/RUNNER_V0718_RUNTIME_RECOVERY.md
        docs/RUNNER_V0719_GENERAL_LOCOMOTION.md
        docs/RUNNER_V0720_UI_PREVIEW_ICON.md
        tools/generate_runner_icon.py
        tests/v0718_runtime_recovery_tests.cpp
        tests/v0719_general_locomotion_tests.cpp
        tests/v0720_ui_tests.cpp
        src/locomotion_strategy.hpp
        src/preview_sync.hpp
        src/runner_icon.rc.in
        src/ui_layout.hpp)
    if(NOT EXISTS "${RUNNER_SOURCE_DIR}/${required}")
        message(FATAL_ERROR "Missing required repository file: ${required}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
foreach(reference IN ITEMS
        "project(Runner VERSION 0.7.20 LANGUAGES CXX)"
        "generate_runner_icon.py"
        "src/autonomy_commands.cpp"
        "src/main.cpp"
        "RunnerV0720UiTests"
        "RUNNER_V0720_UI_PREVIEW_ICON.md"
        "runner_icon.rc")
    string(FIND "${cmake_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "CMake v0.7.20 contract missing: ${reference}")
    endif()
endforeach()
foreach(stale IN ITEMS
        "RUNNER_GENERATED_DIR"
        "generate_v0719_sources.py"
        "generated-v0719")
    string(FIND "${cmake_text}" "${stale}" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR "Stale generated-source contract remains: ${stale}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/missioncache.md" mission_text)
foreach(reference IN ITEMS
        "WALK-DPI-253"
        "WALK-CLIP-254"
        "WALK-PREVIEW-CONTINUITY-257"
        "WALK-ICON-260"
        "WALK-RELEASE-262")
    string(FIND "${mission_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Mission cache v0.7.20 contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/autonomy_runtime.cpp" runtime_text)
foreach(reference IN ITEMS
        "preview_sync::decide"
        "if (decision.replace_course)"
        "if (decision.reset_episode)")
    string(FIND "${runtime_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Preview continuity contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/main.cpp" main_text)
foreach(reference IN ITEMS
        "--diagnose-ui"
        "SDL_SetWindowIcon"
        "application.frame(input, dt, logical_width, logical_height)"
        "logical_width, logical_height")
    string(FIND "${main_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "DPI/icon runtime contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/renderer.hpp" renderer_text)
foreach(reference IN ITEMS "push_clip" "canvas_width" "drawable_width")
    string(FIND "${renderer_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Renderer clipping/DPI contract missing: ${reference}")
    endif()
endforeach()

file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")
if(release_notes)
    message(FATAL_ERROR "Per-release note files remain; CHANGELOG.md is canonical")
endif()

foreach(stale IN ITEMS
        tools/generate_v0719_sources.py
        tools/apply_v0720_release.py
        .github/workflows/apply-v0720-release.yml)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Temporary or stale source generator remains: ${stale}")
    endif()
endforeach()

message(STATUS "Runner v0.7.20 repository hygiene passed")
''')


def main() -> int:
    patch_small_headers()
    patch_canvas()
    patch_icon_generator()
    patch_autonomy()
    patch_renderer()
    patch_main()
    patch_app()
    patch_cmake()
    patch_tests_docs()
    patch_repository_audit()
    stale = ROOT / "tools/generate_v0719_sources.py"
    if stale.exists():
        stale.unlink()
    print("Runner v0.7.20 UI, preview continuity, canonical source, and icon refinement applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
