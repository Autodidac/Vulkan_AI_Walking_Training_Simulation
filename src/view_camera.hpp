#pragma once

#include "course_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace runner::view_camera
{
    inline constexpr float default_pixels_per_meter = 42.0f;
    // The application uses logical SDL coordinates on high-DPI displays.  A
    // 62 px/m ceiling made the subject occupy only a small fraction of a 4K
    // window even though the viewport had ample room. Keep automatic framing
    // separate from the Course cap and from the complete declared Live zoom
    // range so the readout can never advance while the projection is clamped.
    inline constexpr float automatic_maximum_pixels_per_meter = 112.0f;
    inline constexpr float minimum_pixels_per_meter = 12.0f;
    // Fixed authored Course views retain this conservative projection cap.
    inline constexpr float maximum_pixels_per_meter = 150.0f;
    inline constexpr float minimum_zoom_factor = 0.28f;
    inline constexpr float maximum_zoom_factor = 3.60f;
    inline constexpr float live_maximum_pixels_per_meter =
        automatic_maximum_pixels_per_meter * maximum_zoom_factor;
    inline constexpr float target_rig_height_fraction = 0.34f;
    inline constexpr float inspection_rig_height_fraction = 0.52f;
    inline constexpr float inspection_rig_width_fraction = 0.78f;
    // Physical nodes establish the canonical projection, but authored opaque
    // pixels can legitimately extend beyond their anchors. Rig Lab measures
    // that production envelope and may only scale it down into this bounded
    // presentation area. User zoom is applied afterwards, so fitting cannot
    // silently consume or cancel a wheel event.
    // Leave enough base-fit room for the first common zoom-in step to remain
    // wholly inside the editor instead of turning inspection into clipping.
    inline constexpr float inspection_art_width_fraction = 0.78f;
    inline constexpr float inspection_art_minimum_scale = 0.08f;
    inline constexpr float inspection_art_top_margin_pixels = 72.0f;
    inline constexpr float inspection_art_bottom_margin_pixels = 4.0f;
    inline constexpr float inspection_minimum_span_meters = 0.35f;
    inline constexpr float inspection_maximum_pixels_per_meter = 720.0f;
    // Human is the immutable world-scale reference. Auto framing may include a
    // smaller species, but it must not enlarge that species to Human height.
    inline constexpr float human_reference_height_m = 4.8f;
    inline constexpr float live_ground_fraction = 0.74f;
    inline constexpr float lookahead_screen_fraction = 0.17f;
    inline constexpr float camera_dead_zone_pixels = 18.0f;
    inline constexpr float follow_response_per_second = 6.5f;
    [[nodiscard]] constexpr bool tracks_live_subject(
        bool fixed_eye_test, bool walk_eye_test) noexcept
    {
        return !fixed_eye_test || walk_eye_test;
    }

    inline constexpr float zoom_response_per_second = 7.5f;
    inline constexpr float pip_minimum_pixels_per_meter = 24.0f;
    // Compact production species need enough scale to remain inspectable in
    // the PIP once their own physical bounds, rather than Human's, own the fit.
    inline constexpr float pip_maximum_pixels_per_meter = 112.0f;
    inline constexpr float course_authored_span_meters =
        course_geometry::authored_span_meters;
    inline constexpr float course_authored_center_meters =
        course_geometry::authored_center_meters;
    inline constexpr float course_eye_subject_margin_fraction = 0.12f;

    [[nodiscard]] inline float course_eye_camera_center(
        float subject_x, float facing, float visible_span_meters) noexcept
    {
        if (!std::isfinite(subject_x) || !std::isfinite(facing)
            || !std::isfinite(visible_span_meters)
            || !(visible_span_meters > 0.0f))
            return std::isfinite(subject_x) ? subject_x : 0.0f;
        const float direction = facing < 0.0f ? -1.0f : 1.0f;
        return subject_x + direction * visible_span_meters
            * (0.5f - course_eye_subject_margin_fraction);
    }

    [[nodiscard]] constexpr float clamp_zoom_factor(float value) noexcept
    {
        return std::clamp(value, minimum_zoom_factor, maximum_zoom_factor);
    }

    [[nodiscard]] inline float normalized_wheel_delta(
        float delta, bool platform_direction_flipped) noexcept
    {
        if (!std::isfinite(delta))
            return 0.0f;
        return platform_direction_flipped ? -delta : delta;
    }

    [[nodiscard]] inline float apply_wheel_zoom(float current, float wheel) noexcept
    {
        if (!std::isfinite(current))
            current = 1.0f;
        if (!std::isfinite(wheel) || std::abs(wheel) < 0.01f)
            return clamp_zoom_factor(current);
        return clamp_zoom_factor(current * std::pow(1.25f, wheel));
    }

    [[nodiscard]] constexpr float automatic_pixels_per_meter(
        float viewport_height, float rig_height) noexcept
    {
        if (!(viewport_height > 0.0f) || !(rig_height > 0.0f))
            return default_pixels_per_meter;
        return std::clamp(
            viewport_height * target_rig_height_fraction / std::max(rig_height, 0.75f),
            default_pixels_per_meter,
            automatic_maximum_pixels_per_meter);
    }

    [[nodiscard]] constexpr float fitted_pixels_per_meter(
        float viewport_height, float rig_height, float zoom_factor) noexcept
    {
        return std::clamp(
            automatic_pixels_per_meter(viewport_height, rig_height)
                * clamp_zoom_factor(zoom_factor),
            minimum_pixels_per_meter,
            live_maximum_pixels_per_meter);
    }

    // Art and Rig Lab are close inspection surfaces, not a shared world-scale
    // comparison. Fit the selected physical particle bounds in both axes so a
    // compact Dog/Chicken/Hexapod remains readable without clipping a wide rig.
    // The caller supplies a zoom relative to its declared "fit" position.
    [[nodiscard]] inline float inspection_pixels_per_meter(
        float viewport_width, float viewport_height,
        float rig_width, float rig_height, float relative_zoom) noexcept
    {
        if (!std::isfinite(viewport_width) || !std::isfinite(viewport_height)
            || !std::isfinite(rig_width) || !std::isfinite(rig_height)
            || !std::isfinite(relative_zoom)
            || !(viewport_width > 0.0f) || !(viewport_height > 0.0f)
            || !(rig_width > 0.0f) || !(rig_height > 0.0f))
            return default_pixels_per_meter;
        const float fitted_width = viewport_width
            * inspection_rig_width_fraction
            / std::max(rig_width, inspection_minimum_span_meters);
        const float fitted_height = viewport_height
            * inspection_rig_height_fraction
            / std::max(rig_height, inspection_minimum_span_meters);
        return std::clamp(std::min(fitted_width, fitted_height)
                * clamp_zoom_factor(relative_zoom),
            minimum_pixels_per_meter,
            inspection_maximum_pixels_per_meter);
    }

    [[nodiscard]] inline float inspection_envelope_scale_multiplier(
        float viewport_width, float available_above_ground,
        float available_below_ground, float envelope_width,
        float envelope_above_ground, float envelope_below_ground) noexcept
    {
        if (!std::isfinite(viewport_width)
            || !std::isfinite(available_above_ground)
            || !std::isfinite(available_below_ground)
            || !std::isfinite(envelope_width)
            || !std::isfinite(envelope_above_ground)
            || !std::isfinite(envelope_below_ground)
            || !(viewport_width > 0.0f)
            || !(available_above_ground > 0.0f)
            || !(available_below_ground >= 0.0f)
            || !(envelope_width > 0.0f)
            || !(envelope_above_ground >= 0.0f)
            || !(envelope_below_ground >= 0.0f))
            return 1.0f;

        float multiplier = std::min(1.0f,
            viewport_width * inspection_art_width_fraction / envelope_width);
        if (envelope_above_ground > 1.0f)
            multiplier = std::min(multiplier,
                available_above_ground / envelope_above_ground);
        if (envelope_below_ground > 1.0f)
            multiplier = std::min(multiplier,
                available_below_ground / envelope_below_ground);
        return std::clamp(multiplier,
            inspection_art_minimum_scale, 1.0f);
    }

    [[nodiscard]] inline float inspection_camera_delta_meters(
        float envelope_center_screen, float viewport_center_screen,
        float pixels_per_meter) noexcept
    {
        if (!std::isfinite(envelope_center_screen)
            || !std::isfinite(viewport_center_screen)
            || !std::isfinite(pixels_per_meter)
            || !(pixels_per_meter > 0.0f))
            return 0.0f;
        return (envelope_center_screen - viewport_center_screen)
            / pixels_per_meter;
    }

    [[nodiscard]] constexpr float course_pixels_per_meter(
        float viewport_width, float authored_span_meters,
        float zoom_factor) noexcept
    {
        const float base = viewport_width > 0.0f && authored_span_meters > 0.0f
            ? viewport_width / authored_span_meters
            : default_pixels_per_meter;
        return std::clamp(base * clamp_zoom_factor(zoom_factor),
            minimum_pixels_per_meter, maximum_pixels_per_meter);
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
            live_maximum_pixels_per_meter);
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
