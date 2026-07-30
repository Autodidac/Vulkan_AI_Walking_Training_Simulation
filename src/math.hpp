#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace epochrunner
{
    inline constexpr float pi = 3.14159265358979323846f;

    struct Vec2
    {
        float x{};
        float y{};

        constexpr Vec2& operator+=(Vec2 rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
        constexpr Vec2& operator-=(Vec2 rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
        constexpr Vec2& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; return *this; }
        constexpr Vec2& operator/=(float scalar) noexcept { x /= scalar; y /= scalar; return *this; }
    };

    [[nodiscard]] constexpr Vec2 operator+(Vec2 lhs, Vec2 rhs) noexcept { return lhs += rhs; }
    [[nodiscard]] constexpr Vec2 operator-(Vec2 lhs, Vec2 rhs) noexcept { return lhs -= rhs; }
    [[nodiscard]] constexpr Vec2 operator-(Vec2 value) noexcept { return { -value.x, -value.y }; }
    [[nodiscard]] constexpr Vec2 operator*(Vec2 lhs, float rhs) noexcept { return lhs *= rhs; }
    [[nodiscard]] constexpr Vec2 operator*(float lhs, Vec2 rhs) noexcept { return rhs *= lhs; }
    [[nodiscard]] constexpr Vec2 operator/(Vec2 lhs, float rhs) noexcept { return lhs /= rhs; }

    [[nodiscard]] constexpr float dot(Vec2 lhs, Vec2 rhs) noexcept { return lhs.x * rhs.x + lhs.y * rhs.y; }
    [[nodiscard]] constexpr float cross(Vec2 lhs, Vec2 rhs) noexcept { return lhs.x * rhs.y - lhs.y * rhs.x; }
    [[nodiscard]] inline float length_squared(Vec2 value) noexcept { return dot(value, value); }
    [[nodiscard]] inline float length(Vec2 value) noexcept { return std::sqrt(length_squared(value)); }

    [[nodiscard]] inline Vec2 normalized(Vec2 value, Vec2 fallback = { 1.0f, 0.0f }) noexcept
    {
        const float magnitude = length(value);
        return magnitude > 1.0e-6f ? value / magnitude : fallback;
    }

    [[nodiscard]] constexpr Vec2 perpendicular(Vec2 value) noexcept { return { -value.y, value.x }; }

    [[nodiscard]] inline Vec2 rotate(Vec2 value, float radians) noexcept
    {
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        return { value.x * c - value.y * s, value.x * s + value.y * c };
    }

    [[nodiscard]] inline float signed_angle(Vec2 from, Vec2 to) noexcept
    {
        return std::atan2(cross(from, to), dot(from, to));
    }

    [[nodiscard]] inline float wrap_angle(float radians) noexcept
    {
        return std::remainder(radians, 2.0f * pi);
    }

    [[nodiscard]] constexpr float clamp(float value, float minimum, float maximum) noexcept
    {
        return std::clamp(value, minimum, maximum);
    }

    [[nodiscard]] inline float lerp(float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }

    [[nodiscard]] inline Vec2 lerp(Vec2 a, Vec2 b, float t) noexcept
    {
        return a + (b - a) * t;
    }

    struct Color
    {
        float r{};
        float g{};
        float b{};
        float a{ 1.0f };
    };

    [[nodiscard]] constexpr Color with_alpha(Color color, float alpha) noexcept
    {
        color.a = alpha;
        return color;
    }
}
