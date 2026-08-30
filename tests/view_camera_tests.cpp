#include "ui_layout.hpp"
#include "view_camera.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner view camera test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    bool close(float a, float b, float tolerance = 0.001f)
    {
        return std::abs(a - b) <= tolerance;
    }
}

int main()
{
    using namespace runner;

    require(view_camera::default_pixels_per_meter > 22.0f,
        "default view still uses the overly distant v0.7.15 scale");
    require(view_camera::tracks_live_subject(false, false),
        "normal live view stopped tracking its subject");
    require(view_camera::tracks_live_subject(true, true),
        "retained Walk eye replay can leave the primary camera");
    require(!view_camera::tracks_live_subject(true, false),
        "fixed Course/Art eye tests stopped preserving their authored frame");
    require(view_camera::tracks_live_subject(false, true),
        "walk-eye mode disabled normal live subject tracking");

    require(view_camera::minimum_zoom_factor <= 0.28f
            && view_camera::maximum_zoom_factor >= 3.60f
            && view_camera::minimum_pixels_per_meter <= 12.0f
            && view_camera::maximum_pixels_per_meter >= 150.0f,
        "manual zoom range is still too narrow for contact and long-course inspection");
    require(close(view_camera::automatic_pixels_per_meter(820.0f, 0.1f),
            view_camera::automatic_maximum_pixels_per_meter),
        "expanded manual zoom changed automatic full-body framing");
    require(view_camera::fitted_pixels_per_meter(820.0f, 3.0f, 1.0f)
            >= view_camera::default_pixels_per_meter,
        "automatic rig fit moves farther away than the corrected default");
    require(close(view_camera::fitted_pixels_per_meter(
            820.0f, 100.0f, 0.01f),
        view_camera::minimum_pixels_per_meter),
        "minimum zoom clamp is not enforced");
    require(close(view_camera::fitted_pixels_per_meter(
            820.0f, 0.1f, 100.0f),
        view_camera::maximum_pixels_per_meter),
        "maximum zoom clamp is not enforced");
    require(view_camera::apply_wheel_zoom(1.0f, 1.0f) > 1.0f,
        "positive wheel input does not zoom in");
    require(view_camera::apply_wheel_zoom(1.0f, -1.0f) < 1.0f,
        "negative wheel input does not zoom out");
    require(close(view_camera::apply_wheel_zoom(
            view_camera::maximum_zoom_factor, 20.0f),
        view_camera::maximum_zoom_factor),
        "wheel zoom escapes the maximum factor");
    float repeated_out = 1.0f;
    float repeated_in = 1.0f;
    for (int step = 0; step < 40; ++step)
    {
        repeated_out = view_camera::apply_wheel_zoom(repeated_out, -1.0f);
        repeated_in = view_camera::apply_wheel_zoom(repeated_in, 1.0f);
    }
    require(close(repeated_out, view_camera::minimum_zoom_factor)
            && close(repeated_in, view_camera::maximum_zoom_factor),
        "repeated zoom controls do not reach both useful hard boundaries");
    require(close(view_camera::apply_wheel_zoom(
                std::numeric_limits<float>::quiet_NaN(), 1.0f), 1.25f)
            && close(view_camera::apply_wheel_zoom(
                1.0f, std::numeric_limits<float>::infinity()), 1.0f),
        "invalid zoom input escapes finite deterministic sanitization");
    require(view_camera::lookahead_meters(900.0f, 42.0f) > 3.0f,
        "camera no longer preserves useful course lookahead");

    const float still = view_camera::smooth_camera(
        0.0f, 0.1f, 42.0f, 1.0f / 60.0f);
    require(close(still, 0.0f),
        "small root jitter escapes the camera dead zone");
    const float moving = view_camera::smooth_camera(
        0.0f, 4.0f, 42.0f, 1.0f / 60.0f);
    require(moving > 0.0f && moving < 4.0f,
        "camera follow is not bounded and smoothed");

    float sixty_hz = 0.0f;
    for (int frame = 0; frame < 60; ++frame)
        sixty_hz = view_camera::smooth_camera(
            sixty_hz, 5.0f, 42.0f, 1.0f / 60.0f);
    float thirty_hz = 0.0f;
    for (int frame = 0; frame < 30; ++frame)
        thirty_hz = view_camera::smooth_camera(
            thirty_hz, 5.0f, 42.0f, 1.0f / 30.0f);
    require(std::abs(sixty_hz - thirty_hz) < 0.08f,
        "camera smoothing depends materially on frame rate");

    require(close(view_camera::pip_pixels_per_meter(10.0f, 80.0f),
            view_camera::pip_minimum_pixels_per_meter),
        "PIP minimum scale is not enforced");
    require(close(view_camera::pip_pixels_per_meter(80.0f, 90.0f),
            view_camera::pip_maximum_pixels_per_meter),
        "PIP maximum scale is not enforced");

    require(ui_layout::live_layout_valid(1900.0f, 1180.0f),
        "default window layout is invalid after PIP enlargement");
    const ui_layout::Box content = ui_layout::content_box(1900.0f, 1180.0f);
    const ui_layout::Box world = ui_layout::live_world_box(content);
    const ui_layout::Box pip = ui_layout::training_pip_box(world);
    require(ui_layout::contains(world, pip),
        "training PIP escapes the live world");
    require(!ui_layout::overlaps(
            pip, ui_layout::primary_telemetry_box(world)),
        "training PIP overlaps primary telemetry");
    require(!ui_layout::overlaps(
            pip, ui_layout::bottom_telemetry_box(world)),
        "training PIP overlaps bottom telemetry");

    std::cout << "Runner adaptive view camera tests passed\n";
    return EXIT_SUCCESS;
}
