#include "presentation_session.hpp"
#include "ui_font.hpp"
#include "ui_layout.hpp"
#include "view_camera.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

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

    [[nodiscard]] float wrapped_text_height(std::string_view text,
        float scale, float maximum_width, float line_gap = 2.0f)
    {
        const runner::ui_font::FontSize size{
            runner::ui_font::default_logical_height * scale, 1.0f };
        const float line_height = runner::ui_font::make_bitmap_font_metrics(
            size).line_advance + line_gap;
        std::size_t lines{};
        std::string line{};
        std::size_t cursor{};
        while (cursor < text.size())
        {
            if (text[cursor] == '\n')
            {
                lines += !line.empty();
                line.clear();
                ++cursor;
                continue;
            }
            while (cursor < text.size() && text[cursor] == ' ')
                ++cursor;
            if (cursor >= text.size())
                break;
            const std::size_t end = text.find_first_of(" \n", cursor);
            const std::size_t word_end = end == std::string_view::npos
                ? text.size() : end;
            const std::string_view word = text.substr(cursor, word_end - cursor);
            std::string candidate = line;
            if (!candidate.empty())
                candidate.push_back(' ');
            candidate.append(word);
            if (!line.empty()
                && runner::ui_font::measure_text(candidate, size).x
                    > maximum_width)
            {
                ++lines;
                line.assign(word);
            }
            else
                line = std::move(candidate);
            cursor = word_end;
        }
        lines += !line.empty();
        return static_cast<float>(lines) * line_height;
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
            && view_camera::maximum_pixels_per_meter >= 150.0f
            && view_camera::live_maximum_pixels_per_meter
                >= view_camera::automatic_maximum_pixels_per_meter
                    * view_camera::maximum_zoom_factor,
        "manual zoom range is still too narrow for contact and long-course inspection");
    require(close(view_camera::automatic_pixels_per_meter(820.0f, 0.1f),
            view_camera::automatic_maximum_pixels_per_meter),
        "expanded manual zoom changed automatic full-body framing");
    require(view_camera::automatic_pixels_per_meter(2160.0f, 4.8f) > 100.0f,
        "4K automatic framing is still clamped to the rejected distant view");
    require(view_camera::fitted_pixels_per_meter(820.0f, 3.0f, 1.0f)
            >= view_camera::default_pixels_per_meter,
        "automatic rig fit moves farther away than the corrected default");
    require(close(view_camera::fitted_pixels_per_meter(
            820.0f, 100.0f, 0.01f),
        view_camera::minimum_pixels_per_meter),
        "minimum zoom clamp is not enforced");
    require(close(view_camera::fitted_pixels_per_meter(
            820.0f, 0.1f, 100.0f),
        view_camera::live_maximum_pixels_per_meter),
        "maximum Live zoom clamp is not enforced");
    constexpr std::array<std::pair<int, int>, 3> live_surface_sizes{{
        { 1368, 799 }, { 2048, 1152 }, { 3840, 2160 } }};
    for (const auto [surface_width, surface_height] : live_surface_sizes)
    {
        const ui_layout::PresentationSurface surface =
            ui_layout::presentation_surface(surface_width, surface_height);
        const ui_layout::Box content = ui_layout::content_box(
            static_cast<float>(surface.canvas_width),
            static_cast<float>(surface.canvas_height));
        const float viewport_height = ui_layout::live_world_box(content).height;
        const float automatic_scale = view_camera::automatic_pixels_per_meter(
            viewport_height, view_camera::human_reference_height_m);
        float zoom = view_camera::minimum_zoom_factor;
        float rendered_scale = view_camera::fitted_pixels_per_meter(
            viewport_height, view_camera::human_reference_height_m, zoom);
        float projected_height = rendered_scale
            * view_camera::human_reference_height_m;
        for (int button_step = 0; button_step < 16
            && zoom < view_camera::maximum_zoom_factor; ++button_step)
        {
            const float next_zoom = view_camera::apply_wheel_zoom(zoom, 1.0f);
            const float target_scale = view_camera::fitted_pixels_per_meter(
                viewport_height, view_camera::human_reference_height_m,
                next_zoom);
            float next_rendered_scale = rendered_scale;
            for (int frame = 0; frame < 120; ++frame)
            {
                next_rendered_scale = view_camera::smooth_zoom(
                    next_rendered_scale, target_scale, 1.0f / 60.0f);
            }
            const float next_projected_height = next_rendered_scale
                * view_camera::human_reference_height_m;
            require(next_zoom > zoom && target_scale > rendered_scale
                    && next_rendered_scale > rendered_scale
                    && next_projected_height > projected_height,
                "a positive Live button/wheel step changes its readout without changing rendered geometry");
            zoom = next_zoom;
            rendered_scale = next_rendered_scale;
            projected_height = next_projected_height;
        }
        require(close(zoom, view_camera::maximum_zoom_factor)
                && close(view_camera::fitted_pixels_per_meter(
                        viewport_height,
                        view_camera::human_reference_height_m, zoom),
                    automatic_scale * view_camera::maximum_zoom_factor,
                    0.01f),
            "the displayed 3.60x Live endpoint is not the actual fitted projection");

        const float course_width = ui_layout::live_world_box(content).width;
        constexpr float subject_x = 7.25f;
        constexpr float course_span = 48.0f;
        const float course_scale = view_camera::course_pixels_per_meter(
            course_width, course_span, 1.0f);
        const float right_camera = view_camera::course_eye_camera_center(
            subject_x, 1.0f, course_span);
        const float left_camera = view_camera::course_eye_camera_center(
            subject_x, -1.0f, course_span);
        const float right_screen = (subject_x - right_camera) * course_scale
            + course_width * 0.5f;
        const float left_screen = (subject_x - left_camera) * course_scale
            + course_width * 0.5f;
        require(close(right_screen / course_width,
                    view_camera::course_eye_subject_margin_fraction, 0.002f)
                && close(left_screen / course_width,
                    1.0f - view_camera::course_eye_subject_margin_fraction,
                    0.002f),
            "Course eye framing clips the subject or ignores facing");
    }
    require(close(view_camera::course_eye_camera_center(
                3.0f, 1.0f, -4.0f), 3.0f)
            && close(view_camera::course_eye_camera_center(
                std::numeric_limits<float>::quiet_NaN(), 1.0f, 48.0f), 0.0f),
        "Course eye framing does not fail closed on malformed inputs");
    const float human_inspection = view_camera::inspection_pixels_per_meter(
        1000.0f, 1000.0f, 1.0f, 4.0f, 1.0f);
    const float dog_inspection = view_camera::inspection_pixels_per_meter(
        1000.0f, 1000.0f, 2.0f, 1.25f, 1.0f);
    const float wide_inspection = view_camera::inspection_pixels_per_meter(
        1000.0f, 1000.0f, 4.0f, 1.0f, 1.0f);
    require(close(human_inspection * 4.0f, 520.0f)
            && close(dog_inspection * 2.0f, 780.0f)
            && close(wide_inspection * 4.0f, 780.0f),
        "physical inspection fit ignores height or wide-rig bounds");
    require(close(view_camera::inspection_pixels_per_meter(
                1000.0f, 1000.0f, 2.0f, 1.25f, 1.2f),
            dog_inspection * 1.2f)
            && close(view_camera::inspection_pixels_per_meter(
                1000.0f, 1000.0f, 2.0f, 1.25f,
                std::numeric_limits<float>::quiet_NaN()),
            view_camera::default_pixels_per_meter),
        "inspection zoom is not proportional, bounded, and finite");
    require(dog_inspection * 2.0f <= 1000.0f
                * view_camera::inspection_rig_width_fraction + 0.001f
            && dog_inspection * 1.25f <= 1000.0f
                * view_camera::inspection_rig_height_fraction + 0.001f,
        "compact-species inspection fit can clip its physical bounds");
    const float oversized_art_fit =
        view_camera::inspection_envelope_scale_multiplier(
            1000.0f, 600.0f, 100.0f,
            2000.0f, 1200.0f, 50.0f);
    require(close(oversized_art_fit, 0.39f)
            && close(view_camera::inspection_envelope_scale_multiplier(
                    1000.0f, 600.0f, 100.0f,
                    500.0f, 300.0f, 0.0f), 1.0f)
            && close(view_camera::inspection_envelope_scale_multiplier(
                    1000.0f, 600.0f, 100.0f,
                    500.0f, 1200.0f, 0.0f), 0.5f)
            && close(view_camera::inspection_envelope_scale_multiplier(
                    1000.0f, 600.0f, 80.0f,
                    500.0f, 300.0f, 200.0f), 0.4f),
        "authored opaque envelope fit ignores width, top, or ground bounds");
    require(close(view_camera::inspection_envelope_scale_multiplier(
                1000.0f, 600.0f, 100.0f,
                1.0e9f, 300.0f, 0.0f),
            view_camera::inspection_art_minimum_scale)
            && close(view_camera::inspection_envelope_scale_multiplier(
                std::numeric_limits<float>::quiet_NaN(),
                600.0f, 100.0f, 500.0f, 300.0f, 0.0f), 1.0f)
            && close(view_camera::inspection_envelope_scale_multiplier(
                1000.0f, 600.0f, 100.0f, -1.0f, 300.0f, 0.0f), 1.0f),
        "authored envelope fit is not finite and fail-closed on malformed bounds");
    require(close(view_camera::inspection_camera_delta_meters(
                700.0f, 500.0f, 100.0f), 2.0f)
            && close(view_camera::inspection_camera_delta_meters(
                300.0f, 500.0f, 100.0f), -2.0f)
            && close(view_camera::inspection_camera_delta_meters(
                700.0f, 500.0f, 0.0f), 0.0f),
        "authored envelope camera recenter is reversed or accepts invalid scale");
    for (int repeat = 0; repeat < 64; ++repeat)
        require(close(view_camera::inspection_envelope_scale_multiplier(
                    1000.0f, 600.0f, 100.0f,
                    2000.0f, 1200.0f, 50.0f), oversized_art_fit),
            "authored envelope fit changes across identical repetitions");
    require(view_camera::apply_wheel_zoom(1.0f, 1.0f) > 1.0f,
        "positive wheel input does not zoom in");
    require(view_camera::apply_wheel_zoom(1.0f, -1.0f) < 1.0f,
        "negative wheel input does not zoom out");
    require(close(view_camera::normalized_wheel_delta(2.0f, false), 2.0f)
            && close(view_camera::normalized_wheel_delta(2.0f, true), -2.0f)
            && close(view_camera::normalized_wheel_delta(
                std::numeric_limits<float>::infinity(), false), 0.0f),
        "SDL natural-scroll direction is not normalized before camera routing");
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
    const float rig_zoomed_in = ui_layout::rig_lab_zoom_step(
        ui_layout::rig_lab_default_zoom, 1.0f);
    static_assert(ui_layout::rig_lab_default_zoom == 1.0f
        && ui_layout::rig_lab_minimum_zoom == 0.28f
        && ui_layout::rig_lab_maximum_zoom == 3.60f);
    static_assert(ui_layout::rig_lab_minimum_zoom
            == view_camera::minimum_zoom_factor
        && ui_layout::rig_lab_maximum_zoom
            == view_camera::maximum_zoom_factor);
    require(rig_zoomed_in > ui_layout::rig_lab_default_zoom
            && close(ui_layout::rig_lab_zoom_step(rig_zoomed_in, -1.0f),
                ui_layout::rig_lab_default_zoom)
            && close(ui_layout::rig_lab_zoom_step(
                ui_layout::rig_lab_maximum_zoom, 100.0f),
                ui_layout::rig_lab_maximum_zoom)
            && close(ui_layout::rig_lab_zoom_step(
                ui_layout::rig_lab_minimum_zoom, -100.0f),
                ui_layout::rig_lab_minimum_zoom)
            && close(ui_layout::rig_lab_zoom_step(
                std::numeric_limits<float>::quiet_NaN(), 0.0f),
                ui_layout::rig_lab_default_zoom)
            && close(ui_layout::rig_lab_zoom_step(
                ui_layout::rig_lab_default_zoom,
                std::numeric_limits<float>::infinity()),
                ui_layout::rig_lab_default_zoom),
        "Rig Lab wheel/button zoom is not shared, reversible, bounded, and finite");
    const std::array inspection_zooms{ 0.28f, 0.50f, 1.0f, 2.0f, 3.60f };
    float prior_inspection_scale = 0.0f;
    for (const float zoom : inspection_zooms)
    {
        const float actual_scale = view_camera::inspection_pixels_per_meter(
            1000.0f, 1000.0f, 7.8f, 5.2f, zoom);
        require(actual_scale > prior_inspection_scale,
            "Rig Lab declared zoom does not monotonically change actual scale");
        prior_inspection_scale = actual_scale;
    }
    require(close(view_camera::inspection_pixels_per_meter(
                1000.0f, 1000.0f, 7.8f, 5.2f,
                ui_layout::rig_lab_default_zoom), 100.0f)
            && close(view_camera::inspection_pixels_per_meter(
                1000.0f, 1000.0f, 7.8f, 5.2f, -100.0f), 28.0f)
            && close(view_camera::inspection_pixels_per_meter(
                1000.0f, 1000.0f, 7.8f, 5.2f, 100.0f), 360.0f),
        "Fit 1, minimum, and maximum zoom do not map to actual camera scale");
    static_assert(view_camera::course_authored_span_meters
        == course_geometry::authored_span_meters);
    static_assert(view_camera::course_authored_center_meters
        == course_geometry::authored_center_meters);
    const float course_auto = view_camera::course_pixels_per_meter(
        1680.0f, view_camera::course_authored_span_meters, 1.0f);
    const float course_zoomed = view_camera::course_pixels_per_meter(
        1680.0f, view_camera::course_authored_span_meters, 1.25f);
    require(close(course_auto, 1680.0f / 90.0f)
            && close(course_zoomed, (1680.0f / 90.0f) * 1.25f)
            && close(view_camera::course_authored_center_meters, 45.0f),
        "fixed Course view does not fit the complete authored 90 m terrain");
    require(close(view_camera::course_pixels_per_meter(
            0.0f, 0.0f, 100.0f),
        view_camera::maximum_pixels_per_meter),
        "invalid Course fit escapes the shared finite zoom bounds");
    require(view_camera::lookahead_meters(900.0f, 42.0f) > 3.0f,
        "camera no longer preserves useful course lookahead");
    static_assert(ui_layout::course_reference_marker_visible(0, false)
        && ui_layout::course_reference_marker_visible(
            ui_layout::sparse_course_marker_stride, false)
        && !ui_layout::course_reference_marker_visible(1, false)
        && !ui_layout::course_reference_marker_visible(-1, false)
        && ui_layout::course_reference_marker_visible(1, true));
    for (int repeat = 0; repeat < 64; ++repeat)
        require(ui_layout::course_reference_marker_visible(0, false)
                && ui_layout::course_reference_marker_visible(4, false)
                && !ui_layout::course_reference_marker_visible(3, false)
                && ui_layout::course_reference_marker_visible(3, true),
            "default sparse and explicit detailed course labels are not deterministic");

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
    require(close(view_camera::pip_pixels_per_meter(180.0f, 190.0f),
            view_camera::pip_maximum_pixels_per_meter),
        "PIP maximum scale is not enforced");
    require(view_camera::pip_pixels_per_meter(104.0f, 118.0f) > 96.0f,
        "compact-species PIP framing still collapses to a tiny icon");

    static_assert(presentation::replay_may_advance(
        presentation::Session::walk_eye, false, false));
    static_assert(!presentation::replay_may_advance(
        presentation::Session::walk_eye, true, false));
    static_assert(!presentation::replay_may_advance(
        presentation::Session::walk_eye, false, true));
    static_assert(!presentation::replay_may_advance(
        presentation::Session::interactive, false, false));
    static_assert(presentation::replay_retry_available(
        presentation::Session::walk_eye, true));
    static_assert(!presentation::replay_retry_available(
        presentation::Session::walk_eye, false));
    static_assert(!presentation::replay_retry_available(
        presentation::Session::course_eye, true));

    static_assert(presentation::parse_equipment_eye_request(
        "--equipment-eye-test")
        == presentation::EquipmentEyeRequest::exact);
    static_assert(presentation::parse_equipment_eye_request(
        "--equipment-eye-test=human")
        == presentation::EquipmentEyeRequest::invalid);
    static_assert(presentation::parse_equipment_eye_request(
        "--equipment-eye-test-extra")
        == presentation::EquipmentEyeRequest::invalid);
    static_assert(presentation::parse_equipment_eye_request("--art-eye-test")
        == presentation::EquipmentEyeRequest::none);
    static_assert(presentation::isolated(
        presentation::Session::equipment_eye));
    static_assert(presentation::equipment_eye(
        presentation::Session::equipment_eye));
    static_assert(!presentation::permits_training_controls(
        presentation::Session::equipment_eye));
    static_assert(!presentation::permits_replay_controls(
        presentation::Session::equipment_eye));
    static_assert(!presentation::tracks_subject(
        presentation::Session::equipment_eye));

    constexpr float eye_plant =
        presentation::equipment_eye_minimum_plant_seconds;
    constexpr float eye_settle =
        presentation::equipment_eye_minimum_settle_seconds;
    constexpr float eye_rate =
        presentation::equipment_eye_maximum_aim_rate;
    const auto ready_equipment_frame = [&](bool physical_ready = true,
        bool valid_motion = true, bool course_motion = false,
        bool target_visible = true,
        float plant = presentation::equipment_eye_minimum_plant_seconds,
        float settle = presentation::equipment_eye_minimum_settle_seconds,
        float aim_error = 0.03f, float aim_tolerance = 0.04f,
        float aim_rate = presentation::equipment_eye_maximum_aim_rate,
        std::uint32_t shots = 0u)
    {
        return presentation::equipment_eye_frame_ready(
            physical_ready, valid_motion, course_motion, target_visible,
            plant, settle, aim_error, aim_tolerance, aim_rate, shots);
    };
    require(ready_equipment_frame(),
        "settled no-shot planted physical READY frame was rejected");
    require(!ready_equipment_frame(false)
            && !ready_equipment_frame(true, false)
            && !ready_equipment_frame(true, true, true)
            && !ready_equipment_frame(true, true, false, false)
            && !ready_equipment_frame(true, true, false, true,
                eye_plant - 0.001f)
            && !ready_equipment_frame(true, true, false, true,
                eye_plant, eye_settle - 0.001f)
            && !ready_equipment_frame(true, true, false, true,
                eye_plant, eye_settle, 0.041f)
            && !ready_equipment_frame(true, true, false, true,
                eye_plant, eye_settle, 0.03f, 0.04f, eye_rate + 0.001f)
            && !ready_equipment_frame(true, true, false, true,
                eye_plant, eye_settle, 0.03f, 0.04f, eye_rate, 1u),
        "equipment eye frame accepted an unready, assisted, or fired pose");
    require(!ready_equipment_frame(true, true, false, true,
                std::numeric_limits<float>::quiet_NaN())
            && !ready_equipment_frame(true, true, false, true,
                eye_plant, eye_settle,
                std::numeric_limits<float>::infinity())
            && !ready_equipment_frame(true, true, false, true,
                eye_plant, eye_settle, 0.03f, 0.04f,
                std::numeric_limits<float>::quiet_NaN()),
        "equipment eye frame accepted non-finite physical evidence");
    for (int repeat = 0; repeat < 64; ++repeat)
        require(ready_equipment_frame(),
            "equipment eye frame gate changed across identical repetitions");

    static_assert(ui_layout::default_window_width == 2048
        && ui_layout::default_window_height == 1152);
    require(ui_layout::live_layout_valid(
            static_cast<float>(ui_layout::default_window_width),
            static_cast<float>(ui_layout::default_window_height)),
        "default window layout is invalid after PIP enlargement");
    const ui_layout::Box content = ui_layout::content_box(
        static_cast<float>(ui_layout::default_window_width),
        static_cast<float>(ui_layout::default_window_height));
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
    // Exercise the exact client area reported by the operator, not a nearby
    // logical height inferred from its DPI scale.
    require(ui_layout::live_layout_valid(1368.0f, 799.0f)
            && ui_layout::rig_lab_layout_valid(1368.0f, 799.0f),
        "exact 1368x799 layout overlaps or clips a content pane");
    require(!ui_layout::live_layout_valid(1279.0f, 799.0f)
            && !ui_layout::live_layout_valid(1368.0f, 798.0f)
            && !ui_layout::rig_lab_layout_valid(1279.0f, 799.0f)
            && !ui_layout::rig_lab_layout_valid(1368.0f, 798.0f),
        "unsupported near-miss client sizes bypass the minimum layout gate");
    for (const auto& size : ui_layout::validation_sizes)
        require(ui_layout::live_layout_valid(size[0], size[1])
                && ui_layout::rig_lab_layout_valid(size[0], size[1]),
            "declared validation size does not satisfy both layouts");

    const ui_layout::Box compact_content =
        ui_layout::content_box(1368.0f, 799.0f);
    const ui_layout::Box compact_world =
        ui_layout::live_world_box(compact_content);
    const ui_layout::Box compact_live_panel =
        ui_layout::live_panel_box(compact_content);
    const ui_layout::Box compact_footer =
        ui_layout::status_footer_box(1368.0f, 799.0f);
    const ui_layout::Box compact_bottom =
        ui_layout::bottom_telemetry_box(compact_world);
    const ui_layout::Box compact_pip =
        ui_layout::training_pip_box(compact_world);
    const ui_layout::Box compact_pip_title =
        ui_layout::training_pip_title_box(compact_pip);
    const ui_layout::Box compact_pip_state =
        ui_layout::training_pip_state_box(compact_pip);
    const ui_layout::Box compact_pip_world =
        ui_layout::training_pip_world_box(compact_pip);
    const ui_layout::Box compact_pip_footer =
        ui_layout::training_pip_footer_box(compact_pip);
    const ui_layout::Box concise_pip_world =
        ui_layout::training_pip_world_box(compact_pip, false);
    const ui_layout::Box concise_pip_footer =
        ui_layout::training_pip_footer_box(compact_pip, false);
    const ui_layout::Box concise_primary_telemetry =
        ui_layout::primary_telemetry_display_box(compact_world, false);
    const ui_layout::Box expanded_primary_telemetry =
        ui_layout::primary_telemetry_display_box(compact_world, true);
    require(compact_footer.height >= 76.0f
            && ui_layout::visual_session_stacks_authority_controls(
                compact_live_panel.width - 36.0f)
            && !ui_layout::visual_session_stacks_authority_controls(680.0f),
        "minimum proof surface clips its two-line receipt or authority controls");
    require(compact_world.width / compact_content.width >= 0.70f
            && compact_live_panel.width / compact_content.width <= 0.30f
            && !ui_layout::overlaps(compact_bottom, compact_footer)
            && !ui_layout::overlaps(compact_pip_title, compact_pip_state)
            && !ui_layout::overlaps(compact_pip_state, compact_pip_world)
            && !ui_layout::overlaps(compact_pip_world, compact_pip_footer)
            && ui_layout::contains(compact_pip, compact_pip_title)
            && ui_layout::contains(compact_pip, compact_pip_state)
            && ui_layout::contains(compact_pip, compact_pip_world)
            && ui_layout::contains(compact_pip, compact_pip_footer)
            && ui_layout::contains(compact_pip, concise_pip_world)
            && ui_layout::contains(compact_pip, concise_pip_footer)
            && !ui_layout::overlaps(concise_pip_world, concise_pip_footer)
            && concise_pip_world.height > compact_pip_world.height + 60.0f
            && concise_pip_footer.height >= 48.0f
            && concise_primary_telemetry.height
                == ui_layout::primary_telemetry_compact_height
            && concise_primary_telemetry.height
                < expanded_primary_telemetry.height
            && ui_layout::minimum_readable_text_scale >= 0.92f
            && ui_layout::persistent_text_minimum_scale >= 1.00f,
        "compact footer or wrapped PIP telemetry can overlap its reserved world");

    const ui_layout::TrainerControlGroupLayout compact_live_controls =
        ui_layout::trainer_control_group_layout(
            compact_live_panel, 0.0f, true);
    const auto compact_live_with_replay = ui_layout::trainer_control_group_layout(
        compact_live_panel, 0.0f, true, true);
    require(compact_live_with_replay.primary.height
                == compact_live_controls.primary.height
                    + ui_layout::trainer_primary_button_height + 12.0f
            && !ui_layout::overlaps(compact_live_with_replay.primary,
                compact_live_with_replay.page_tabs)
            && ui_layout::contains(compact_live_panel, compact_live_with_replay.page_tabs),
        "Live replay controls overlap the session navigation at minimum size");
    const std::array<ui_layout::Box, 4> compact_quick_actions =
        ui_layout::trainer_quick_action_layout(
            compact_live_controls.page_tabs);
    require(compact_live_controls.primary.height
                == ui_layout::trainer_primary_card_height
            && ui_layout::contains(compact_live_panel,
                compact_live_controls.primary)
            && ui_layout::contains(compact_live_panel,
                compact_live_controls.page_tabs)
            && compact_quick_actions[0].height
                >= ui_layout::trainer_primary_button_height,
        "minimum Live surface lost its primary Session or quick-action lane");
    for (std::size_t index = 0; index < compact_quick_actions.size(); ++index)
    {
        require(ui_layout::contains(compact_live_controls.page_tabs,
                    compact_quick_actions[index])
                && compact_quick_actions[index].width >= 110.0f,
            "Live Mission/View/Details/Art route escaped or collapsed");
        for (std::size_t later = index + 1u;
             later < compact_quick_actions.size(); ++later)
            require(!ui_layout::overlaps(compact_quick_actions[index],
                    compact_quick_actions[later]),
                "Live Mission/View/Details/Art routes overlap");
    }
    constexpr std::array<std::string_view, 4> compact_quick_labels{
        "MISSION", "VIEW", "DETAILS", "ART ON" };
    const ui_font::FontSize compact_quick_text{
        ui_font::default_logical_height
            * ui_layout::persistent_text_minimum_scale, 1.0f };
    for (std::size_t index = 0; index < compact_quick_actions.size(); ++index)
        require(ui_font::measure_text(compact_quick_labels[index],
                    compact_quick_text).x
                <= compact_quick_actions[index].width - 14.0f,
            "compact quick-action label clips its four-action grid target");
    const ui_layout::TrainerControlGroupLayout compact_live_details =
        ui_layout::trainer_control_group_layout(
            compact_live_panel, 0.0f, false, true);
    const std::array<ui_layout::Box, 3> compact_live_pages =
        ui_layout::trainer_live_page_button_layout(
            compact_live_details.page_tabs);
    const std::array<ui_layout::Box, 3> compact_view_choices =
        ui_layout::trainer_three_choice_layout(
            { compact_live_details.page_body.x + 7.0f,
                compact_live_details.page_body.y + 80.0f,
                compact_live_details.page_body.width - 14.0f, 0.0f });
    const ui_font::FontSize compact_button_text{
        ui_font::default_logical_height
            * ui_layout::persistent_text_minimum_scale, 1.0f };
    constexpr std::array<std::string_view, 3> compact_page_labels{
        "MISSION", "VIEW", "DETAILS" };
    constexpr std::array<std::string_view, 3> compact_view_labels{
        "NORMAL", "FASTER", "MAXIMUM" };
    constexpr std::array<std::string_view, 3> compact_camera_labels{
        "ZOOM OUT", "AUTO VIEW", "ZOOM IN" };
    require(compact_live_details.page_tabs.height
                == ui_layout::trainer_page_tab_button_height * 2.0f
                    + ui_layout::trainer_page_tab_gap
            && ui_layout::contains(compact_live_panel,
                compact_live_details.page_tabs)
            && !ui_layout::overlaps(compact_live_details.page_tabs,
                compact_live_details.page_body),
        "compact Live three-route navigation lost its responsive hierarchy");
    for (std::size_t index = 0; index < compact_live_pages.size(); ++index)
    {
        require(ui_layout::contains(compact_live_details.page_tabs,
                    compact_live_pages[index])
                && ui_font::measure_text(compact_page_labels[index],
                    compact_button_text).x
                    <= compact_live_pages[index].width - 14.0f,
            "compact Mission/View/Details label clips its target");
        require(ui_font::measure_text(compact_view_labels[index],
                    compact_button_text).x
                    <= compact_view_choices[index].width - 14.0f,
            "compact responsive View choice clips its target");
        require(ui_font::measure_text(compact_camera_labels[index],
                    compact_button_text).x
                    <= compact_view_choices[index].width - 14.0f,
            "compact responsive camera choice clips its target");
        for (std::size_t later = index + 1u;
             later < compact_live_pages.size(); ++later)
            require(!ui_layout::overlaps(compact_live_pages[index],
                    compact_live_pages[later]),
                "compact Mission/View/Details routes overlap");
    }
    require(ui_layout::trainer_three_choice_height(
                compact_live_details.page_body.width - 14.0f)
                >= ui_layout::trainer_secondary_button_height * 3.0f,
        "compact three-choice control did not reflow to three readable rows");
    constexpr std::array<std::string_view, 5> compact_detail_values{
        "MODE ASSISTED\nCONTACT 1.00",
        "FIRM GROUND\nWATER 0.00 M\nSLIP 0.000 M/S",
        "UPDATE 123456\nRUNS 123 / TESTS 123",
        "LOSS -0.1234\nRATE 0.0001000",
        "CARBINE READY\nAIM +12.3 DEG\nMISSES 123" };
    for (const std::string_view value : compact_detail_values)
        require(wrapped_text_height(value,
                    ui_layout::persistent_text_minimum_scale,
                    compact_live_details.page_body.width - 42.0f, 3.0f)
                <= ui_layout::trainer_detail_card_height
                    - ui_layout::trainer_detail_value_top
                    - ui_layout::trainer_detail_value_bottom,
            "compact Details value card needs more than its three readable rows");
    constexpr std::string_view longest_state{
        "CRAB WALK - NO SAGITTAL CROSSING" };
    const ui_font::FontSize persistent_readable{
        ui_font::default_logical_height
            * ui_layout::persistent_text_minimum_scale,
        1.0f };
    require(ui_font::measure_text(longest_state, persistent_readable).x > 120.0f
            && wrapped_text_height(longest_state,
                ui_layout::persistent_text_minimum_scale,
                compact_pip_state.width) <= compact_pip_state.height,
        "longest PIP state still depends on the clipped fixed 120-pixel lane");
    constexpr std::string_view longest_footer{
        "FAILED: COLLAPSED / UNSUPPORTED POSTURE   R RETRY" };
    require(wrapped_text_height(longest_footer,
            ui_layout::persistent_text_minimum_scale,
            compact_pip_footer.width) <= compact_pip_footer.height,
        "longest terminal PIP footer still clips its final retry/evidence line");
    require(wrapped_text_height("TRAINING SNAPSHOT",
                ui_layout::persistent_text_minimum_scale,
                concise_pip_footer.width, 2.0f)
                <= concise_pip_footer.height
            && wrapped_text_height("WALK / RUN   ZOOM 3.60X",
                ui_layout::persistent_text_minimum_scale,
                concise_pip_footer.width, 2.0f)
                <= concise_pip_footer.height,
        "concise PIP footer cannot wrap at the readable font floor");
    static_assert(ui_layout::training_pip_evaluation_waiting(
        0u, false, false));
    static_assert(!ui_layout::training_pip_evaluation_waiting(
        1u, false, false));
    static_assert(!ui_layout::training_pip_evaluation_waiting(
        0u, true, false));
    static_assert(!ui_layout::training_pip_evaluation_waiting(
        0u, false, true));

    const ui_layout::RigLabWorkspaceLayout compact_trainer =
        ui_layout::rig_lab_workspace_layout(compact_content,
            ui_layout::RigLabWorkspacePage::trainer);
    const ui_layout::RigLabWorkspaceLayout compact_inspector =
        ui_layout::rig_lab_workspace_layout(compact_content,
            ui_layout::RigLabWorkspacePage::inspector);
    const ui_layout::RigLabWorkspaceNavigationLayout compact_navigation =
        ui_layout::rig_lab_workspace_navigation_layout(
            compact_inspector.workspace_bar);
    require(ui_layout::rig_lab_uses_compact_workspace(compact_content)
            && compact_trainer.compact && compact_inspector.compact
            && compact_trainer.trainer.width >= 400.0f
            && compact_trainer.inspector.width == 0.0f
            && compact_inspector.inspector.width >= 400.0f
            && compact_inspector.trainer.width == 0.0f
            && compact_trainer.world.width >= 800.0f
            && compact_inspector.world.width >= 800.0f,
        "1368x799 Rig Lab did not collapse the trainer and inspector into separate workspaces");
    require(ui_layout::contains(compact_content,
                compact_trainer.workspace_bar)
            && ui_layout::contains(compact_content, compact_trainer.trainer)
            && ui_layout::contains(compact_content, compact_trainer.world)
            && ui_layout::contains(compact_content, compact_inspector.inspector)
            && ui_layout::contains(compact_content, compact_inspector.world)
            && !ui_layout::overlaps(compact_trainer.workspace_bar,
                compact_trainer.trainer)
            && !ui_layout::overlaps(compact_trainer.workspace_bar,
                compact_trainer.world)
            && !ui_layout::overlaps(compact_trainer.trainer,
                compact_trainer.world)
            && !ui_layout::overlaps(compact_inspector.inspector,
                compact_inspector.world)
            && ui_layout::contains(compact_inspector.workspace_bar,
                compact_navigation.mission)
            && ui_layout::contains(compact_inspector.workspace_bar,
                compact_navigation.run_button)
            && ui_layout::contains(compact_inspector.workspace_bar,
                compact_navigation.controls_button)
            && ui_layout::contains(compact_inspector.workspace_bar,
                compact_navigation.trainer_button)
            && ui_layout::contains(compact_inspector.workspace_bar,
                compact_navigation.inspector_button)
            && !ui_layout::overlaps(compact_navigation.mission,
                compact_navigation.run_button)
            && !ui_layout::overlaps(compact_navigation.run_button,
                compact_navigation.controls_button)
            && !ui_layout::overlaps(compact_navigation.controls_button,
                compact_navigation.trainer_button)
            && !ui_layout::overlaps(compact_navigation.trainer_button,
                compact_navigation.inspector_button)
            && compact_navigation.run_button.height >= 56.0f
            && compact_navigation.controls_button.height >= 56.0f
            && compact_trainer.world.x
                - (compact_trainer.trainer.x
                    + compact_trainer.trainer.width)
                >= ui_layout::panel_gap
            && compact_inspector.world.x
                - (compact_inspector.inspector.x
                    + compact_inspector.inspector.width)
                >= ui_layout::panel_gap,
        "compact workspace navigation, controls, or preview overlap and can share an interaction lane");
    require(close(compact_trainer.world.x, compact_inspector.world.x)
            && close(compact_trainer.world.y, compact_inspector.world.y)
            && close(compact_trainer.world.width,
                compact_inspector.world.width)
            && close(compact_trainer.world.height,
                compact_inspector.world.height),
        "switching compact workspaces moves or resizes the shared Rig Lab preview");
    require(ui_font::measure_text("PAUSE TRAIN", {
                ui_font::default_logical_height
                    * ui_layout::persistent_text_minimum_scale, 1.0f }).x
                <= compact_navigation.run_button.width - 14.0f,
        "compact Start/Pause Train command cannot fit one readable line");
    const ui_layout::RigLabControlDrawerLayout compact_drawer =
        ui_layout::rig_lab_control_drawer_layout(compact_inspector.inspector);
    require(ui_layout::contains(compact_inspector.inspector,
                compact_drawer.card)
            && compact_drawer.gait_buttons[0].height >= 52.0f
            && compact_drawer.authority_button.height >= 52.0f
            && !ui_layout::overlaps(compact_drawer.gait_buttons[0],
                compact_drawer.gait_buttons[1])
            && !ui_layout::overlaps(compact_drawer.authority_button,
                compact_drawer.profile_button)
            && compact_drawer.authority_button.width
                == compact_drawer.heading.width
            && compact_drawer.profile_button.width
                == compact_drawer.heading.width
            && compact_drawer.profile_button.y
                > compact_drawer.authority_button.y
            && compact_drawer.rig_art_button.width
                == compact_drawer.heading.width,
        "compact contextual Training Setup drawer is clipped or overlapping");
    const ui_layout::RigArtActionLayout compact_art_actions =
        ui_layout::rig_art_action_layout({
            compact_inspector.inspector.x + 18.0f, 0.0f,
            compact_inspector.inspector.width - 36.0f, 0.0f });
    const std::array compact_art_boxes{
        compact_art_actions.skeleton,
        compact_art_actions.freeze,
        compact_art_actions.backdrop };
    constexpr std::array<std::string_view, 3> compact_art_labels{
        "SHOW SKELETON", "RESUME", "BACKDROP" };
    const ui_font::FontSize production_button_text{
        ui_font::default_logical_height
            * ui_layout::persistent_text_minimum_scale, 1.0f };
    for (std::size_t index = 0; index < compact_art_boxes.size(); ++index)
    {
        require(ui_font::measure_text(compact_art_labels[index],
                    production_button_text).x
                    <= compact_art_boxes[index].width - 14.0f,
            "1368 Rig Art action clips at the production type floor");
        for (std::size_t later = index + 1u;
             later < compact_art_boxes.size(); ++later)
            require(!ui_layout::overlaps(compact_art_boxes[index],
                    compact_art_boxes[later]),
                "1368 Rig Art two-row actions overlap");
    }
    const ui_layout::RigPreviewToolbarLayout compact_preview_toolbar =
        ui_layout::rig_preview_toolbar_layout(compact_inspector.world);
    const ui_layout::Box compact_preview_title =
        ui_layout::rig_preview_title_box(compact_inspector.world);
    const std::array compact_preview_buttons{
        compact_preview_toolbar.zoom_out, compact_preview_toolbar.fit,
        compact_preview_toolbar.zoom_in };
    constexpr std::array<std::string_view, 3> preview_labels{
        "ZOOM OUT", "FIT", "ZOOM IN" };
    require(ui_layout::contains(compact_inspector.world,
                compact_preview_title)
            && !ui_layout::overlaps(compact_preview_title,
                compact_preview_toolbar.zoom_out)
            && ui_font::measure_text("MOTOR EDITOR",
                production_button_text).x <= compact_preview_title.width,
        "1368 preview title overlaps its persistent zoom toolbar");
    for (std::size_t index = 0; index < compact_preview_buttons.size(); ++index)
    {
        require(ui_layout::contains(compact_inspector.world,
                    compact_preview_buttons[index])
                && ui_font::measure_text(preview_labels[index],
                    production_button_text).x
                    <= compact_preview_buttons[index].width - 14.0f,
            "1368 persistent preview toolbar clips or escapes the world");
        for (std::size_t later = index + 1u;
             later < compact_preview_buttons.size(); ++later)
            require(!ui_layout::overlaps(compact_preview_buttons[index],
                    compact_preview_buttons[later]),
                "1368 persistent preview toolbar overlaps");
    }

    const ui_layout::Box default_content = ui_layout::content_box(
        static_cast<float>(ui_layout::default_window_width),
        static_cast<float>(ui_layout::default_window_height));
    const ui_layout::RigLabWorkspaceLayout default_layout =
        ui_layout::rig_lab_workspace_layout(default_content,
            ui_layout::RigLabWorkspacePage::inspector);
    require(ui_layout::rig_lab_uses_compact_workspace(default_content)
            && default_layout.compact
            && default_layout.workspace_bar.width > 0.0f
            && default_layout.trainer.width == 0.0f
            && default_layout.inspector.width >= 480.0f
            && default_layout.world.width >= 900.0f,
        "default high-DPI desktop reopened the cluttered dual-inspector wall");

    // A host may report the complete 3840x2160 framebuffer instead of the
    // DPI-independent 2560x1440 window. Both paths must land on the same
    // physical typography, control targets, and focused workspace.
    constexpr ui_layout::PresentationSurface fullscreen_surface =
        ui_layout::presentation_surface(3840, 2160);
    const ui_layout::Box fullscreen_content = ui_layout::content_box(
        static_cast<float>(fullscreen_surface.canvas_width),
        static_cast<float>(fullscreen_surface.canvas_height));
    const ui_layout::RigLabWorkspaceLayout fullscreen_trainer =
        ui_layout::rig_lab_workspace_layout(fullscreen_content,
            ui_layout::RigLabWorkspacePage::trainer);
    const ui_layout::RigLabWorkspaceLayout fullscreen_inspector =
        ui_layout::rig_lab_workspace_layout(fullscreen_content,
            ui_layout::RigLabWorkspacePage::inspector);
    const ui_layout::Box physical_trainer = ui_layout::presentation_box(
        fullscreen_trainer.trainer, fullscreen_surface);
    const ui_layout::Box physical_inspector = ui_layout::presentation_box(
        fullscreen_trainer.inspector, fullscreen_surface);
    const ui_layout::Box physical_world = ui_layout::presentation_box(
        fullscreen_trainer.world, fullscreen_surface);
    const ui_layout::Box fullscreen_setup =
        ui_layout::rig_lab_dual_setup_button_layout(
            fullscreen_trainer.trainer);
    const ui_layout::Box physical_setup = ui_layout::presentation_box(
        fullscreen_setup, fullscreen_surface);
    require(fullscreen_surface.density_scaled
            && fullscreen_surface.canvas_width == 2560
            && fullscreen_surface.canvas_height == 1440
            && close(fullscreen_surface.scale_x, 1.5f)
            && close(fullscreen_surface.scale_y, 1.5f)
            && ui_font::default_logical_height
                * ui_layout::persistent_text_minimum_scale
                * fullscreen_surface.scale_y >= 30.0f
            && !ui_layout::rig_lab_uses_compact_workspace(fullscreen_content)
            && !ui_layout::rig_lab_shows_live(fullscreen_content)
            && !fullscreen_trainer.compact && !fullscreen_inspector.compact
            && fullscreen_trainer.workspace_bar.width == 0.0f
            && fullscreen_trainer.trainer.width >= 600.0f
            && fullscreen_trainer.inspector.width >= 540.0f
            && close(fullscreen_trainer.trainer.width,
                fullscreen_inspector.trainer.width)
            && close(fullscreen_trainer.inspector.width,
                fullscreen_inspector.inspector.width)
            && fullscreen_trainer.live.width == 0.0f
            && fullscreen_inspector.live.width == 0.0f
            && physical_trainer.width >= 900.0f
            && physical_inspector.width >= 800.0f
            && physical_world.width >= 1900.0f
            && physical_world.width > physical_trainer.width
            && ui_layout::contains(fullscreen_trainer.trainer,
                fullscreen_setup)
            && physical_setup.width >= 200.0f
            && physical_setup.height >= 72.0f
            && !ui_layout::overlaps(fullscreen_trainer.trainer,
                fullscreen_trainer.inspector)
            && !ui_layout::overlaps(fullscreen_trainer.trainer,
                fullscreen_trainer.world)
            && !ui_layout::overlaps(fullscreen_inspector.inspector,
                fullscreen_inspector.world),
        "3840x2160 presentation lost its readable Trainer/Inspector/Setup workspace");
    const ui_layout::RigPreviewToolbarLayout four_k_preview_toolbar =
        ui_layout::rig_preview_toolbar_layout(fullscreen_trainer.world);
    const std::array four_k_preview_buttons{
        four_k_preview_toolbar.zoom_out, four_k_preview_toolbar.fit,
        four_k_preview_toolbar.zoom_in };
    for (std::size_t index = 0; index < four_k_preview_buttons.size(); ++index)
    {
        require(ui_layout::contains(fullscreen_trainer.world,
                    four_k_preview_buttons[index])
                && ui_layout::presentation_box(
                    four_k_preview_buttons[index], fullscreen_surface).height
                    >= 72.0f,
            "4K persistent preview toolbar escaped or lost target size");
        for (std::size_t later = index + 1u;
             later < four_k_preview_buttons.size(); ++later)
            require(!ui_layout::overlaps(four_k_preview_buttons[index],
                    four_k_preview_buttons[later]),
                "4K persistent preview toolbar overlaps");
    }

    constexpr ui_layout::PresentationSurface operator_surface =
        ui_layout::presentation_surface(3832, 2085);
    static_assert(operator_surface.density_scaled
        && operator_surface.scale_y > 1.44f
        && operator_surface.scale_y < 1.46f);

    const ui_layout::Box four_k_content = ui_layout::content_box(
        static_cast<float>(fullscreen_surface.canvas_width),
        static_cast<float>(fullscreen_surface.canvas_height));
    const ui_layout::Box four_k_live_panel = ui_layout::live_panel_box(
        four_k_content);
    const ui_layout::TrainerControlGroupLayout four_k_live_details =
        ui_layout::trainer_control_group_layout(
            four_k_live_panel, 0.0f, false, true);
    const std::array<ui_layout::Box, 3> four_k_live_pages =
        ui_layout::trainer_live_page_button_layout(
            four_k_live_details.page_tabs);
    require(four_k_live_details.page_tabs.height
                == ui_layout::trainer_page_tab_button_height
            && four_k_live_pages.size() == 3u,
        "4K Live navigation did not collapse to three primary routes");
    for (std::size_t index = 0; index < four_k_live_pages.size(); ++index)
    {
        const ui_layout::Box physical = ui_layout::presentation_box(
            four_k_live_pages[index], fullscreen_surface);
        require(ui_layout::contains(four_k_live_details.page_tabs,
                    four_k_live_pages[index])
                && physical.height >= 72.0f,
            "4K Mission/View/Details route missed its physical target floor");
        for (std::size_t later = index + 1u;
             later < four_k_live_pages.size(); ++later)
            require(!ui_layout::overlaps(four_k_live_pages[index],
                    four_k_live_pages[later]),
                "4K Mission/View/Details routes overlap");
    }

    const ui_layout::TrainerControlGroupLayout fullscreen_controls =
        ui_layout::trainer_control_group_layout(
            fullscreen_trainer.trainer, 0.0f, true);
    const ui_layout::TrainerSummaryLayout fullscreen_summary =
        ui_layout::trainer_summary_layout(fullscreen_controls.page_body);
    const std::array<ui_layout::Box, 3> fullscreen_page_buttons =
        ui_layout::trainer_page_button_layout(fullscreen_controls.page_tabs);
    const std::array fullscreen_summary_lanes{
        fullscreen_summary.heading,
        fullscreen_summary.headline,
        fullscreen_summary.evidence,
        fullscreen_summary.retained_controller,
        fullscreen_summary.next_goal_label,
        fullscreen_summary.next_goal };
    require(!fullscreen_summary.wide
            && fullscreen_summary.card.height <= 360.0f
            && ui_layout::persistent_text_minimum_scale >= 1.40f
            && ui_layout::section_heading_text_scale >= 1.70f
            && ui_layout::workspace_heading_text_scale >= 1.90f
            && ui_layout::trainer_primary_minimum_text_scale >= 1.40f
            && ui_layout::trainer_primary_button_height >= 64.0f
            && ui_layout::trainer_gait_button_height >= 56.0f
            && ui_layout::trainer_task_button_height >= 48.0f
            && ui_layout::trainer_secondary_button_height >= 48.0f
            && fullscreen_controls.primary.width > 0.0f
            && fullscreen_controls.primary.height > 0.0f
            && fullscreen_controls.secondary.width == 0.0f
            && fullscreen_controls.secondary.height == 0.0f
            && ui_layout::contains(fullscreen_trainer.trainer,
                fullscreen_controls.page_tabs)
            && ui_layout::contains(fullscreen_trainer.trainer,
                fullscreen_summary.card)
            && !ui_layout::overlaps(fullscreen_controls.page_tabs,
                fullscreen_summary.card),
        "fullscreen embedded Trainer lost session controls or overlaps its evidence");
    for (std::size_t index = 0; index < fullscreen_page_buttons.size(); ++index)
    {
        require(ui_layout::contains(fullscreen_controls.page_tabs,
                fullscreen_page_buttons[index])
                && fullscreen_page_buttons[index].height >= 48.0f
                && ui_layout::presentation_box(
                    fullscreen_page_buttons[index], fullscreen_surface).height
                    >= 72.0f,
            "fullscreen progressive-disclosure page action escaped its lane");
        for (std::size_t later = index + 1u;
             later < fullscreen_page_buttons.size(); ++later)
            require(!ui_layout::overlaps(fullscreen_page_buttons[index],
                    fullscreen_page_buttons[later]),
                "fullscreen progressive-disclosure page actions overlap");
    }
    const ui_layout::RigLabControlDrawerLayout fullscreen_drawer =
        ui_layout::rig_lab_control_drawer_layout(fullscreen_trainer.trainer);
    const std::array<ui_layout::Box, 4> fullscreen_gait_buttons =
        fullscreen_drawer.gait_buttons;
    require(fullscreen_gait_buttons[0].width >= 280.0f
            && ui_layout::presentation_box(
                fullscreen_gait_buttons[0], fullscreen_surface).height
                >= 66.0f,
        "fullscreen persistent gait actions are still cramped chips");
    for (std::size_t index = 0; index < fullscreen_gait_buttons.size(); ++index)
        for (std::size_t later = index + 1u;
             later < fullscreen_gait_buttons.size(); ++later)
            require(!ui_layout::overlaps(fullscreen_gait_buttons[index],
                    fullscreen_gait_buttons[later]),
                "fullscreen gait action grid overlaps");
    for (std::size_t index = 0; index < fullscreen_summary_lanes.size(); ++index)
    {
        require(ui_layout::contains(fullscreen_summary.card,
                fullscreen_summary_lanes[index]),
            "fullscreen Summary lane escaped its status card");
        for (std::size_t later = index + 1u;
             later < fullscreen_summary_lanes.size(); ++later)
            require(!ui_layout::overlaps(fullscreen_summary_lanes[index],
                    fullscreen_summary_lanes[later]),
                "fullscreen Summary status lanes overlap");
    }

    constexpr std::string_view longest_headline{
        "RETRYING FAILED TEST" };
    constexpr std::string_view longest_next_goal{
        "Climb the reachable ledge by hand support, transfer onto it, then descend backward feet first." };
    const ui_font::FontSize fullscreen_headline_size{
        ui_font::default_logical_height
            * ui_layout::persistent_text_minimum_scale, 1.0f };
    require(ui_font::measure_text(longest_headline,
                fullscreen_headline_size).x
            <= fullscreen_summary.headline.width,
        "fullscreen Summary clips the longest headline");
    require(wrapped_text_height(longest_next_goal, 1.04f,
            fullscreen_summary.next_goal.width, 3.0f)
            <= fullscreen_summary.next_goal.height,
        "fullscreen Summary clips the longest next goal");

    const ui_layout::TrainerControlGroupLayout narrow_controls =
        ui_layout::trainer_control_group_layout(
            compact_trainer.trainer, 0.0f, false);
    const ui_layout::TrainerSummaryLayout narrow_summary =
        ui_layout::trainer_summary_layout(narrow_controls.page_body);
    const std::array<ui_layout::Box, 4> narrow_gait_buttons =
        compact_drawer.gait_buttons;
    const std::array<ui_layout::Box, 3> narrow_page_buttons =
        ui_layout::trainer_page_button_layout(narrow_controls.page_tabs);
    require(narrow_controls.primary.width == 0.0f
            && narrow_gait_buttons[0].width >= 170.0f
            && narrow_gait_buttons[0].height >= 44.0f,
        "minimum-width embedded Trainer duplicates commands or cramps gaits");
    constexpr ui_layout::Box below_summary_threshold{
        0.0f, 0.0f,
        ui_layout::trainer_summary_wide_minimum_width - 1.0f, 0.0f };
    constexpr ui_layout::Box at_summary_threshold{
        0.0f, 0.0f,
        ui_layout::trainer_summary_wide_minimum_width, 0.0f };
    static_assert(!ui_layout::trainer_summary_layout(
                      below_summary_threshold).wide
        && ui_layout::trainer_summary_layout(at_summary_threshold).wide);
    require(!narrow_summary.wide,
        "minimum-width Summary incorrectly selected wide text lanes");
    require(narrow_summary.card.height <= 360.0f,
        "minimum-width Summary regressed into a prose wall");
    const ui_font::FontSize narrow_headline_size{
        ui_font::default_logical_height
            * ui_layout::persistent_text_minimum_scale, 1.0f };
    require(ui_font::measure_text(longest_headline,
                narrow_headline_size).x
            <= narrow_summary.headline.width,
        "minimum-width Summary clips the longest headline");
    require(wrapped_text_height(longest_next_goal, 1.04f,
            narrow_summary.next_goal.width, 3.0f)
            <= narrow_summary.next_goal.height,
        "minimum-width Summary clips the longest next goal");
    const std::array narrow_summary_lanes{
        narrow_summary.heading,
        narrow_summary.headline,
        narrow_summary.evidence,
        narrow_summary.retained_controller,
        narrow_summary.next_goal_label,
        narrow_summary.next_goal };
    for (std::size_t index = 0; index < narrow_summary_lanes.size(); ++index)
    {
        require(ui_layout::contains(narrow_summary.card,
                narrow_summary_lanes[index]),
            "minimum-width Summary lane escaped its status card");
        for (std::size_t later = index + 1u;
             later < narrow_summary_lanes.size(); ++later)
            require(!ui_layout::overlaps(narrow_summary_lanes[index],
                    narrow_summary_lanes[later]),
                "minimum-width Summary status lanes overlap");
    }
    for (std::size_t index = 0; index < narrow_page_buttons.size(); ++index)
    {
        require(ui_layout::contains(narrow_controls.page_tabs,
                narrow_page_buttons[index])
                && narrow_page_buttons[index].width >= 140.0f,
            "minimum-width progressive-disclosure page action escaped or collapsed");
        for (std::size_t later = index + 1u;
             later < narrow_page_buttons.size(); ++later)
            require(!ui_layout::overlaps(narrow_page_buttons[index],
                    narrow_page_buttons[later]),
                "minimum-width progressive-disclosure page actions overlap");
    }
    for (int repeat = 0; repeat < 64; ++repeat)
    {
        const auto repeated_pages = ui_layout::trainer_page_button_layout(
            narrow_controls.page_tabs);
        for (std::size_t index = 0; index < repeated_pages.size(); ++index)
            require(close(repeated_pages[index].x,
                        narrow_page_buttons[index].x)
                    && close(repeated_pages[index].y,
                        narrow_page_buttons[index].y)
                    && close(repeated_pages[index].width,
                        narrow_page_buttons[index].width)
                    && close(repeated_pages[index].height,
                        narrow_page_buttons[index].height),
                "progressive-disclosure page layout changed across identical inputs");
    }
    const ui_font::FontSize summary_minimum{
        ui_font::default_logical_height
            * ui_layout::trainer_primary_minimum_text_scale,
        1.0f };
    require(wrapped_text_height("18M  14 STEPS  18S  3/3",
            ui_layout::persistent_text_minimum_scale,
            narrow_summary.evidence.width, 3.0f)
                <= narrow_summary.evidence.height,
        "minimum-width compact current evidence still clips");
    constexpr std::array<std::string_view, 12> stage_evidence_labels{
        "UP 6/6S  SEEDS 6/6",
        "LOW 4S  UP 1  SAFE 6S",
        "59.1 FT  STEP14  SAFE18S",
        "18M  STEP14  SAFE18S  3/3",
        "LOW4S  STEP8  CLEAR4",
        "JUMP3  LAND3  59.1 FT",
        "CLEAR3  LAND3  26.2 FT",
        "FLIP2  TURN3.0  JUMP2",
        "36.1 FT  STEP8  CLEAR4",
        "HAND4  XFER2  UP1/DN1",
        "HIT3/3  HANDLE2",
        "19.7 FT  STEP6  HIT2" };
    for (std::string_view label : stage_evidence_labels)
        require(wrapped_text_height(label,
                    ui_layout::persistent_text_minimum_scale,
                    narrow_summary.evidence.width, 3.0f)
                <= narrow_summary.evidence.height,
            "minimum-width stage evidence label requires microtext");
    require(ui_font::measure_text(
            "CONTROLLER SAVED",
            summary_minimum).x
            <= narrow_summary.retained_controller.width,
        "minimum-width compact retained status still clips horizontally");

    const ui_layout::Box dual_content = ui_layout::content_box(5120.0f, 2880.0f);
    const ui_layout::RigLabWorkspaceLayout dual_layout =
        ui_layout::rig_lab_workspace_layout(dual_content,
            ui_layout::RigLabWorkspacePage::inspector);
    require(!ui_layout::rig_lab_uses_compact_workspace(dual_content)
            && !ui_layout::rig_lab_shows_live(dual_content)
            && !dual_layout.compact
            && dual_layout.workspace_bar.width == 0.0f
            && dual_layout.trainer.width >= 400.0f
            && dual_layout.inspector.width >= 420.0f
            && dual_layout.world.width >= 900.0f
            && !ui_layout::overlaps(dual_layout.trainer,
                dual_layout.inspector)
            && !ui_layout::overlaps(dual_layout.inspector,
                dual_layout.world),
        "genuinely wide Rig Lab no longer retains trainer, inspector, and preview as distinct panes");
    constexpr ui_layout::Box compact_threshold_probe{
        0.0f, 0.0f,
        ui_layout::rig_lab_dual_panel_min_content_width - 1.0f, 645.0f };
    constexpr ui_layout::Box wide_threshold_probe{
        0.0f, 0.0f,
        ui_layout::rig_lab_dual_panel_min_content_width, 645.0f };
    static_assert(ui_layout::rig_lab_uses_compact_workspace(
                      compact_threshold_probe)
        && !ui_layout::rig_lab_uses_compact_workspace(wide_threshold_probe));
    constexpr ui_layout::Box combined_threshold_probe{
        0.0f, 0.0f, ui_layout::rig_lab_combined_min_content_width, 645.0f };
    constexpr ui_layout::Box below_combined_threshold_probe{
        0.0f, 0.0f,
        ui_layout::rig_lab_combined_min_content_width - 1.0f, 645.0f };
    static_assert(ui_layout::rig_lab_shows_live(combined_threshold_probe)
        && !ui_layout::rig_lab_shows_live(below_combined_threshold_probe));
    for (int repeat = 0; repeat < 64; ++repeat)
    {
        const ui_layout::RigLabWorkspaceLayout repeated =
            ui_layout::rig_lab_workspace_layout(compact_content,
                ui_layout::RigLabWorkspacePage::inspector);
        require(close(repeated.inspector.x, compact_inspector.inspector.x)
                && close(repeated.inspector.width,
                    compact_inspector.inspector.width)
                && close(repeated.world.x, compact_inspector.world.x)
                && close(repeated.world.width, compact_inspector.world.width),
            "compact Rig Lab layout changed across identical repeated inputs");
    }
    for (const auto& size : ui_layout::validation_sizes)
    {
        const ui_layout::Box validated_world = ui_layout::live_world_box(
            ui_layout::content_box(size[0], size[1]));
        const ui_layout::Box validated_pip =
            ui_layout::training_pip_box(validated_world);
        const ui_layout::Box validated_title =
            ui_layout::training_pip_title_box(validated_pip);
        const ui_layout::Box validated_state =
            ui_layout::training_pip_state_box(validated_pip);
        const ui_layout::Box validated_pip_world =
            ui_layout::training_pip_world_box(validated_pip);
        const ui_layout::Box validated_pip_footer =
            ui_layout::training_pip_footer_box(validated_pip);
        require(ui_layout::contains(validated_pip, validated_title)
                && ui_layout::contains(validated_pip, validated_state)
                && ui_layout::contains(validated_pip, validated_pip_world)
                && ui_layout::contains(validated_pip, validated_pip_footer)
                && !ui_layout::overlaps(validated_title, validated_state)
                && !ui_layout::overlaps(validated_state, validated_pip_world)
                && !ui_layout::overlaps(validated_pip_world,
                    validated_pip_footer),
            "PIP header, state, world, or footer lanes overlap at a validation size");
    }

    constexpr ui_layout::Box scroll_view{ 100.0f, 200.0f, 420.0f, 645.0f };
    constexpr float scroll_content_height = 1280.0f;
    const float scroll_maximum = ui_layout::maximum_scroll_offset(
        scroll_content_height, scroll_view.height);
    require(close(scroll_maximum, 635.0f)
            && close(ui_layout::panel_scroll_offset(0.0f, -100.0f,
                scroll_content_height, scroll_view.height), scroll_maximum)
            && close(ui_layout::panel_scroll_offset(scroll_maximum, 100.0f,
                scroll_content_height, scroll_view.height), 0.0f)
            && close(ui_layout::panel_scroll_offset(73.0f,
                std::numeric_limits<float>::quiet_NaN(),
                scroll_content_height, scroll_view.height), 73.0f),
        "panel scrolling is not finite, directional, or saturated");
    const ui_layout::ScrollbarLayout scroll_top =
        ui_layout::panel_scrollbar_layout(
            scroll_view, scroll_content_height, 0.0f);
    const ui_layout::ScrollbarLayout scroll_bottom =
        ui_layout::panel_scrollbar_layout(
            scroll_view, scroll_content_height, scroll_maximum);
    const ui_layout::ScrollbarLayout non_scrollable =
        ui_layout::panel_scrollbar_layout(
            scroll_view, scroll_view.height, 0.0f);
    require(scroll_top.scrollable && scroll_bottom.scrollable
            && ui_layout::contains(scroll_view, scroll_top.track)
            && ui_layout::contains(scroll_top.track, scroll_top.thumb)
            && ui_layout::contains(scroll_bottom.track, scroll_bottom.thumb)
            && scroll_top.track.width >= 8.0f
            && scroll_top.thumb.width >= 8.0f
            && scroll_top.thumb.height
                >= ui_layout::scrollbar_minimum_thumb_height
            && scroll_bottom.thumb.y > scroll_top.thumb.y,
        "visible panel scrollbar is missing, too small to grab, escaped, or reversed");
    require(!non_scrollable.scrollable
            && close(non_scrollable.maximum_offset, 0.0f),
        "a fully visible panel still advertises a decorative scroll rail");
    for (int repeat = 0; repeat < 64; ++repeat)
        require(close(ui_layout::panel_scroll_offset(0.0f, -100.0f,
                    scroll_content_height, scroll_view.height), scroll_maximum)
                && close(ui_layout::panel_scrollbar_layout(scroll_view,
                    scroll_content_height, scroll_maximum).thumb.y,
                    scroll_bottom.thumb.y),
            "scroll saturation changes across identical repetitions");
    const ui_layout::Box combined_content =
        ui_layout::content_box(6144.0f, 3456.0f);
    const ui_layout::RigLabWorkspaceLayout combined_layout =
        ui_layout::rig_lab_workspace_layout(combined_content,
            ui_layout::RigLabWorkspacePage::trainer);
    require(ui_layout::rig_lab_shows_live(combined_content)
            && !combined_layout.compact
            && combined_layout.live.width >= 620.0f
            && combined_layout.trainer.width >= 400.0f
            && combined_layout.inspector.width >= 420.0f
            && combined_layout.world.width >= 400.0f
            && !ui_layout::overlaps(combined_layout.live,
                combined_layout.trainer)
            && !ui_layout::overlaps(combined_layout.live,
                combined_layout.inspector)
            && !ui_layout::overlaps(combined_layout.live,
                combined_layout.world),
        "genuinely oversized combined Rig Lab panes overlap or share wheel ownership");

    std::cout << "Runner adaptive view camera tests passed\n";
    return EXIT_SUCCESS;
}
