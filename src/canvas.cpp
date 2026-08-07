#include "renderer.hpp"

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
