#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace runner::render
{
    void Canvas::triangle(Vec2 a, Vec2 b, Vec2 c, Color color)
    {
        vertices_.push_back({ a, color });
        vertices_.push_back({ b, color });
        vertices_.push_back({ c, color });
    }

    void Canvas::quad(Vec2 minimum, Vec2 maximum, Color color)
    {
        triangle({ minimum.x, minimum.y }, { maximum.x, minimum.y }, { maximum.x, maximum.y }, color);
        triangle({ minimum.x, minimum.y }, { maximum.x, maximum.y }, { minimum.x, maximum.y }, color);
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
        segments = std::clamp<std::uint32_t>(segments, 6, 96);
        Vec2 previous = center + Vec2{ radius, 0.0f };
        for (std::uint32_t index = 1; index <= segments; ++index)
        {
            const float angle = 2.0f * pi * static_cast<float>(index) / static_cast<float>(segments);
            const Vec2 current = center + Vec2{ std::cos(angle) * radius, std::sin(angle) * radius };
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
        if (points.size() < 2)
            return;
        for (std::size_t index = 1; index < points.size(); ++index)
            line(points[index - 1], points[index], thickness, color);
    }
}
