#pragma once

#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace runner::ui_render
{
    inline constexpr Color transparent_fill{ 0.0f, 0.0f, 0.0f, 0.0f };

    [[nodiscard]] constexpr bool is_explicitly_transparent(Color color) noexcept
    {
        return color.a == 0.0f;
    }

    inline void fill_rounded_rect(render::Canvas& canvas, Vec2 position,
        Vec2 size, float radius, Color color)
    {
        if (size.x <= 0.0f || size.y <= 0.0f || color.a <= 0.0f)
            return;
        radius = std::clamp(radius, 0.0f, std::min(size.x, size.y) * 0.5f);
        const Vec2 minimum = position;
        const Vec2 maximum = position + size;
        if (radius <= 0.0f)
        {
            canvas.quad(minimum, maximum, color);
            return;
        }
        canvas.quad({ minimum.x + radius, minimum.y },
            { maximum.x - radius, maximum.y }, color);
        canvas.quad({ minimum.x, minimum.y + radius },
            { maximum.x, maximum.y - radius }, color);
        canvas.circle({ minimum.x + radius, minimum.y + radius }, radius, color, 12u);
        canvas.circle({ maximum.x - radius, minimum.y + radius }, radius, color, 12u);
        canvas.circle({ minimum.x + radius, maximum.y - radius }, radius, color, 12u);
        canvas.circle({ maximum.x - radius, maximum.y - radius }, radius, color, 12u);
    }

    namespace detail
    {
        inline void rounded_corner_stroke(render::Canvas& canvas, Vec2 center,
            float outer_radius, float inner_radius, float start_angle, Color color)
        {
            constexpr std::uint32_t segments = 8u;
            Vec2 previous_outer = center + Vec2{
                std::cos(start_angle) * outer_radius,
                std::sin(start_angle) * outer_radius };
            Vec2 previous_inner = center + Vec2{
                std::cos(start_angle) * inner_radius,
                std::sin(start_angle) * inner_radius };
            for (std::uint32_t segment = 1u; segment <= segments; ++segment)
            {
                const float angle = start_angle + (pi * 0.5f)
                    * static_cast<float>(segment) / static_cast<float>(segments);
                const Vec2 current_outer = center + Vec2{
                    std::cos(angle) * outer_radius,
                    std::sin(angle) * outer_radius };
                if (inner_radius > 0.0f)
                {
                    const Vec2 current_inner = center + Vec2{
                        std::cos(angle) * inner_radius,
                        std::sin(angle) * inner_radius };
                    canvas.triangle(previous_outer, current_outer, current_inner, color);
                    canvas.triangle(previous_outer, current_inner, previous_inner, color);
                    previous_inner = current_inner;
                }
                else
                {
                    canvas.triangle(center, previous_outer, current_outer, color);
                }
                previous_outer = current_outer;
            }
        }
    }

    inline void stroke_rounded_rect(render::Canvas& canvas, Vec2 position,
        Vec2 size, float radius, float border_width, Color color)
    {
        if (size.x <= 0.0f || size.y <= 0.0f
            || border_width <= 0.0f || color.a <= 0.0f)
            return;

        const float maximum_border = std::min(size.x, size.y) * 0.5f;
        border_width = std::clamp(border_width, 0.0f, maximum_border);
        radius = std::clamp(radius, 0.0f, std::min(size.x, size.y) * 0.5f);
        const Vec2 minimum = position;
        const Vec2 maximum = position + size;

        if (radius <= 0.0f)
        {
            canvas.quad(minimum, { maximum.x, minimum.y + border_width }, color);
            canvas.quad({ minimum.x, maximum.y - border_width }, maximum, color);
            canvas.quad({ minimum.x, minimum.y + border_width },
                { minimum.x + border_width, maximum.y - border_width }, color);
            canvas.quad({ maximum.x - border_width, minimum.y + border_width },
                { maximum.x, maximum.y - border_width }, color);
            return;
        }

        const float inner_radius = std::max(0.0f, radius - border_width);
        if (maximum.x - minimum.x > radius * 2.0f)
        {
            canvas.quad({ minimum.x + radius, minimum.y },
                { maximum.x - radius, minimum.y + border_width }, color);
            canvas.quad({ minimum.x + radius, maximum.y - border_width },
                { maximum.x - radius, maximum.y }, color);
        }
        if (maximum.y - minimum.y > radius * 2.0f)
        {
            canvas.quad({ minimum.x, minimum.y + radius },
                { minimum.x + border_width, maximum.y - radius }, color);
            canvas.quad({ maximum.x - border_width, minimum.y + radius },
                { maximum.x, maximum.y - radius }, color);
        }

        detail::rounded_corner_stroke(canvas,
            { minimum.x + radius, minimum.y + radius },
            radius, inner_radius, pi, color);
        detail::rounded_corner_stroke(canvas,
            { maximum.x - radius, minimum.y + radius },
            radius, inner_radius, -pi * 0.5f, color);
        detail::rounded_corner_stroke(canvas,
            { maximum.x - radius, maximum.y - radius },
            radius, inner_radius, 0.0f, color);
        detail::rounded_corner_stroke(canvas,
            { minimum.x + radius, maximum.y - radius },
            radius, inner_radius, pi * 0.5f, color);
    }

    inline void rounded_rect(render::Canvas& canvas, Vec2 position, Vec2 size,
        float radius, Color fill, Color outline = transparent_fill,
        float border_width = 0.0f)
    {
        fill_rounded_rect(canvas, position, size, radius, fill);
        stroke_rounded_rect(canvas, position, size, radius, border_width, outline);
    }
}
