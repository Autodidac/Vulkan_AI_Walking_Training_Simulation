#pragma once

#include <algorithm>
#include <cmath>

namespace runner::view_camera
{
    inline constexpr float default_pixels_per_meter = 42.0f;
    inline constexpr float minimum_pixels_per_meter = 30.24f;
    inline constexpr float maximum_pixels_per_meter = 62.0f;
    inline constexpr float minimum_zoom_factor = 0.72f;
    inline constexpr float maximum_zoom_factor = 1.55f;
    inline constexpr float target_rig_height_fraction = 0.34f;
    inline constexpr float live_ground_fraction = 0.74f;
    inline constexpr float lookahead_screen_fraction = 0.17f;
    inline constexpr float camera_dead_zone_pixels = 18.0f;
    inline constexpr float follow_response_per_second = 6.5f;
    inline constexpr float zoom_response_per_second = 7.5f;
    inline constexpr float pip_minimum_pixels_per_meter = 24.0f;
    inline constexpr float pip_maximum_pixels_per_meter = 56.0f;

    [[nodiscard]] constexpr float clamp_zoom_factor(float value) noexcept
    {
        return std::clamp(value, minimum_zoom_factor, maximum_zoom_factor);
    }

    [[nodiscard]] inline float apply_wheel_zoom(float current, float wheel) noexcept
    {
        if (!std::isfinite(current))
            current = 1.0f;
        if (!std::isfinite(wheel) || std::abs(wheel) < 0.01f)
            return clamp_zoom_factor(current);
        return clamp_zoom_factor(current * std::pow(1.12f, wheel));
    }

    [[nodiscard]] constexpr float automatic_pixels_per_meter(
        float viewport_height, float rig_height) noexcept
    {
        if (!(viewport_height > 0.0f) || !(rig_height > 0.0f))
            return default_pixels_per_meter;
        return std::clamp(
            viewport_height * target_rig_height_fraction / std::max(rig_height, 0.75f),
            default_pixels_per_meter,
            maximum_pixels_per_meter);
    }

    [[nodiscard]] constexpr float fitted_pixels_per_meter(
        float viewport_height, float rig_height, float zoom_factor) noexcept
    {
        return std::clamp(
            automatic_pixels_per_meter(viewport_height, rig_height)
                * clamp_zoom_factor(zoom_factor),
            minimum_pixels_per_meter,
            maximum_pixels_per_meter);
    }

    [[nodiscard]] constexpr float lookahead_meters(
        float viewport_width, float pixels_per_meter) noexcept
    {
        return pixels_per_meter > 0.0f
            ? std::max(2.0f,
                viewport_width * lookahead_screen_fraction / pixels_per_meter)
            : 2.0f;
    }

    [[nodiscard]] inline float exponential_alpha(
        float response_per_second, float dt) noexcept
    {
        if (!(response_per_second > 0.0f) || !(dt > 0.0f)
            || !std::isfinite(response_per_second) || !std::isfinite(dt))
            return 0.0f;
        return std::clamp(
            1.0f - std::exp(-response_per_second * dt), 0.0f, 1.0f);
    }

    [[nodiscard]] inline float smooth_zoom(
        float current, float target, float dt) noexcept
    {
        if (!std::isfinite(current))
            current = default_pixels_per_meter;
        if (!std::isfinite(target))
            target = default_pixels_per_meter;
        const float alpha = exponential_alpha(zoom_response_per_second, dt);
        return std::clamp(
            current + (target - current) * alpha,
            minimum_pixels_per_meter,
            maximum_pixels_per_meter);
    }

    [[nodiscard]] inline float smooth_camera(
        float current, float target, float pixels_per_meter, float dt) noexcept
    {
        if (!std::isfinite(current))
            return target;
        if (!std::isfinite(target) || !(pixels_per_meter > 0.0f))
            return current;
        const float error = target - current;
        if (std::abs(error) * pixels_per_meter <= camera_dead_zone_pixels)
            return current;
        const float dead_zone_world = camera_dead_zone_pixels / pixels_per_meter;
        const float adjusted_target = target
            - std::copysign(dead_zone_world, error);
        const float alpha = exponential_alpha(follow_response_per_second, dt);
        return current + (adjusted_target - current) * alpha;
    }

    [[nodiscard]] constexpr float pip_pixels_per_meter(
        float horizontal_scale, float vertical_scale) noexcept
    {
        return std::clamp(
            std::min(horizontal_scale, vertical_scale),
            pip_minimum_pixels_per_meter,
            pip_maximum_pixels_per_meter);
    }
}
