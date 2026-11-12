#include "app.hpp"
#include "atomic_file.hpp"
#include "simulation.hpp"
#include "software_frame_capture.hpp"
#include "ui_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef RUNNER_VERSION
#define RUNNER_VERSION "development"
#endif

namespace
{
    namespace capture = runner::software_frame_capture;

    constexpr int capture_width = runner::ui_layout::default_window_width;
    constexpr int capture_height = runner::ui_layout::default_window_height;
    constexpr int narrow_capture_width = 1368;
    constexpr int narrow_capture_height = 799;
    constexpr int four_k_capture_width = 3840;
    constexpr int four_k_capture_height = 2160;
    constexpr float fixed_step = 1.0f / 60.0f;
    constexpr std::size_t presentation_settle_steps = 60u;
    constexpr std::size_t maximum_zoom_indicator_changed_pixels = 20'000u;

    struct Frame
    {
        capture::Image image{};
        capture::RasterStats raster{};
        std::size_t vertices{};
    };

    struct SubjectEvidence
    {
        std::size_t changed_pixels{};
        capture::PixelRect bounds{};
        float viewport_width_fraction{};
        float viewport_height_fraction{};
    };

    [[nodiscard]] std::uint64_t canonical_rig_signature(
        std::size_t species_index)
    {
        switch (species_index)
        {
        case 0u: return runner::sim::CreatureBlueprint::humanoid().signature();
        case 1u: return runner::sim::CreatureBlueprint::chicken().signature();
        case 2u: return runner::sim::CreatureBlueprint::crawler4().signature();
        case 3u: return runner::sim::CreatureBlueprint::hexapod().signature();
        default: return 0u;
        }
    }

    [[nodiscard]] capture::PixelRect pixel_rect(
        runner::ui_layout::Box box) noexcept
    {
        const int x = static_cast<int>(std::floor(box.x));
        const int y = static_cast<int>(std::floor(box.y));
        const int ending_x = static_cast<int>(std::ceil(box.x + box.width));
        const int ending_y = static_cast<int>(std::ceil(box.y + box.height));
        return { x, y, std::max(0, ending_x - x),
            std::max(0, ending_y - y) };
    }

    [[nodiscard]] bool contains(capture::PixelRect outer,
        capture::PixelRect inner) noexcept
    {
        return outer.valid() && inner.valid()
            && inner.x >= outer.x && inner.y >= outer.y
            && inner.x + inner.width <= outer.x + outer.width
            && inner.y + inner.height <= outer.y + outer.height;
    }

    [[nodiscard]] bool atomic_write(const std::filesystem::path& destination,
        std::span<const std::uint8_t> bytes, std::string& error)
    {
        std::filesystem::path temporary = destination;
        temporary += ".capture.tmp";
        std::error_code filesystem_error{};
        std::filesystem::remove(temporary, filesystem_error);
        filesystem_error.clear();
        {
            std::ofstream output(temporary,
                std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "could not open temporary capture: "
                    + temporary.string();
                return false;
            }
            output.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            output.flush();
            if (!output)
            {
                output.close();
                std::filesystem::remove(temporary, filesystem_error);
                error = "could not completely write temporary capture: "
                    + temporary.string();
                return false;
            }
        }
        if (!runner::persistence::atomic_replace_file(
                temporary, destination, error))
        {
            std::filesystem::remove(temporary, filesystem_error);
            return false;
        }
        error.clear();
        return true;
    }

    [[nodiscard]] bool initialize(runner::Application& application,
        const std::filesystem::path& assets, std::string& error)
    {
        return application.initialize(assets, error,
            runner::presentation::StartupMode::isolated_visual_proof);
    }

    [[nodiscard]] bool render_frame(runner::Application& application,
        const runner::InputState& input, float dt, Frame& frame,
        std::string& error)
    {
        application.frame(input, dt, capture_width, capture_height);
        const std::span<const runner::render::Vertex> vertices =
            application.vertices();
        if (vertices.empty() || vertices.size() % 3u != 0u
            || vertices.size_bytes()
                > runner::render::maximum_frame_vertex_bytes)
        {
            error = std::format(
                "production frame vertex contract failed: vertices={} bytes={}",
                vertices.size(), vertices.size_bytes());
            return false;
        }
        frame.vertices = vertices.size();
        if (!capture::rasterize(vertices, capture_width, capture_height,
                frame.image, frame.raster, error))
            return false;
        if (frame.raster.non_finite_triangles != 0u
            || frame.raster.trailing_vertices != 0u)
        {
            error = std::format(
                "production frame contained {} non-finite triangles and {} trailing vertices",
                frame.raster.non_finite_triangles,
                frame.raster.trailing_vertices);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool render_frame_at(runner::Application& application,
        const runner::InputState& input, float dt, int width, int height,
        Frame& frame, std::string& error)
    {
        application.frame(input, dt, width, height);
        const std::span<const runner::render::Vertex> vertices =
            application.vertices();
        if (vertices.empty() || vertices.size() % 3u != 0u
            || vertices.size_bytes()
                > runner::render::maximum_frame_vertex_bytes)
        {
            error = std::format(
                "production frame vertex contract failed at {}x{}: vertices={} bytes={}",
                width, height, vertices.size(), vertices.size_bytes());
            return false;
        }
        frame = {};
        frame.vertices = vertices.size();
        if (!capture::rasterize(vertices, width, height,
                frame.image, frame.raster, error))
            return false;
        if (frame.raster.non_finite_triangles != 0u
            || frame.raster.trailing_vertices != 0u)
        {
            error = std::format(
                "production frame at {}x{} contained {} non-finite triangles and {} trailing vertices",
                width, height, frame.raster.non_finite_triangles,
                frame.raster.trailing_vertices);
            return false;
        }
        return true;
    }

    void settle_presentation(runner::Application& application)
    {
        // Exercise the same fixed-step frame entry point as the application so
        // smooth camera/zoom state reaches its deterministic visible target.
        // Isolated startup is paused and persistence-free, so this cannot train
        // or alter a retained controller while the presentation camera settles.
        for (std::size_t step = 0u; step < presentation_settle_steps; ++step)
            application.frame({}, fixed_step, capture_width, capture_height);
    }

    [[nodiscard]] bool require_populated(const Frame& frame,
        runner::ui_layout::Box box, std::string_view label,
        std::string& error)
    {
        const capture::RegionStats stats = capture::analyze_region(
            frame.image, pixel_rect(box));
        if (capture::visibly_populated(stats))
            return true;
        error = std::format(
            "{} is visually empty: samples={} non_clear={} colors={}",
            label, stats.samples, stats.non_clear_samples,
            stats.distinct_color_buckets);
        return false;
    }

    [[nodiscard]] bool validate_live_regions(const Frame& frame,
        std::string& error, bool expect_training_snapshot = true)
    {
        if (!runner::ui_layout::live_layout_valid(
                static_cast<float>(capture_width),
                static_cast<float>(capture_height)))
        {
            error = "2048x1152 live layout contract is invalid";
            return false;
        }
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        const runner::ui_layout::Box pip =
            runner::ui_layout::training_pip_box(
                runner::ui_layout::live_world_box(content));
        const capture::RegionStats pip_stats = capture::analyze_region(
            frame.image, pixel_rect(pip));
        const bool snapshot_contract = expect_training_snapshot
            ? require_populated(frame, pip,
                "live production PIP", error)
            : pip_stats.distinct_color_buckets <= 1u;
        if (!snapshot_contract && !expect_training_snapshot)
        {
            error = std::format(
                "default Live canvas still renders the duplicate Training Snapshot: colors={}",
                pip_stats.distinct_color_buckets);
        }
        return require_populated(frame,
                runner::ui_layout::live_world_box(content),
                "live production world", error)
            && require_populated(frame,
                runner::ui_layout::live_panel_box(content),
                "live trainer/statistics panel", error)
            && snapshot_contract;
    }

    [[nodiscard]] bool validate_rig_lab_regions(const Frame& frame,
        std::string& error)
    {
        if (!runner::ui_layout::rig_lab_layout_valid(
                static_cast<float>(capture_width),
                static_cast<float>(capture_height)))
        {
            error = "2048x1152 Rig Lab pane contract is invalid";
            return false;
        }
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        const runner::ui_layout::RigLabWorkspaceLayout workspace =
            runner::ui_layout::rig_lab_workspace_layout(content,
                runner::ui_layout::RigLabWorkspacePage::inspector);
        if ((workspace.compact && !require_populated(frame,
                workspace.workspace_bar, "Rig Lab workspace selector", error))
            || !require_populated(frame, workspace.inspector,
                "Rig Lab controls pane", error)
            || !require_populated(frame, workspace.world,
                "Rig Lab production viewport", error))
            return false;
        if (workspace.live.width > 0.0f
            && !require_populated(frame, workspace.live,
                "Rig Lab retained live pane", error))
            return false;
        return true;
    }

    [[nodiscard]] bool subject_evidence(const Frame& art,
        const Frame& no_art, runner::ui_layout::Box viewport,
        SubjectEvidence& evidence, std::string& error)
    {
        const capture::PixelRect region = pixel_rect(viewport);
        const capture::DifferenceStats difference = capture::difference(
            art.image, no_art.image, region, 1u);
        const float width_fraction = difference.bounds.valid()
            ? static_cast<float>(difference.bounds.width)
                / std::max(1.0f, viewport.width) : 0.0f;
        const float height_fraction = difference.bounds.valid()
            ? static_cast<float>(difference.bounds.height)
                / std::max(1.0f, viewport.height) : 0.0f;
        const bool clear_margin = difference.bounds.valid()
            && difference.bounds.x >= region.x + 3
            && difference.bounds.y >= region.y + 3
            && difference.bounds.x + difference.bounds.width
                <= region.x + region.width - 3
            && difference.bounds.y + difference.bounds.height
                <= region.y + region.height - 3;
        if (!difference.comparable || difference.changed_pixels < 256u
            || width_fraction < 0.035f || height_fraction < 0.30f
            || width_fraction > 0.94f || height_fraction > 0.84f
            || !contains(region, difference.bounds) || !clear_margin)
        {
            error = std::format(
                "subject art delta is tiny, clipped, or oversized: pixels={} bounds={},{},{},{} fractions={:.5f},{:.5f}",
                difference.changed_pixels, difference.bounds.x,
                difference.bounds.y, difference.bounds.width,
                difference.bounds.height, width_fraction, height_fraction);
            return false;
        }
        evidence.changed_pixels = difference.changed_pixels;
        evidence.bounds = difference.bounds;
        evidence.viewport_width_fraction = width_fraction;
        evidence.viewport_height_fraction = height_fraction;
        return true;
    }

    [[nodiscard]] bool subject_clear_of_live_overlays(
        runner::ui_layout::Box world, const SubjectEvidence& subject,
        std::string& error)
    {
        const runner::ui_layout::Box telemetry =
            runner::ui_layout::primary_telemetry_box(world);
        const runner::ui_layout::Box pip =
            runner::ui_layout::training_pip_box(world);
        const runner::ui_layout::Box bottom =
            runner::ui_layout::bottom_telemetry_box(world);
        const int safe_top = static_cast<int>(std::ceil(std::max(
            telemetry.y + telemetry.height, pip.y + pip.height) + 8.0f));
        const int safe_bottom = static_cast<int>(std::floor(bottom.y - 8.0f));
        if (!subject.bounds.valid() || subject.bounds.y < safe_top
            || subject.bounds.y + subject.bounds.height > safe_bottom)
        {
            error = std::format(
                "isolated subject intersects persistent overlays: bounds={},{},{},{} safe_y={}..{}",
                subject.bounds.x, subject.bounds.y, subject.bounds.width,
                subject.bounds.height, safe_top, safe_bottom);
            return false;
        }
        return true;
    }

    [[nodiscard]] runner::ui_layout::Box live_subject_viewport(
        runner::ui_layout::Box world) noexcept
    {
        const runner::ui_layout::Box telemetry =
            runner::ui_layout::primary_telemetry_box(world);
        const runner::ui_layout::Box pip =
            runner::ui_layout::training_pip_box(world);
        const runner::ui_layout::Box bottom =
            runner::ui_layout::bottom_telemetry_box(world);
        const float top = std::max(telemetry.y + telemetry.height,
            pip.y + pip.height) + 8.0f;
        const float bottom_edge = bottom.y - 8.0f;
        return { world.x, top, world.width,
            std::max(0.0f, bottom_edge - top) };
    }

    [[nodiscard]] bool compact_pip_evidence(const Frame& art,
        const Frame& no_art, runner::ui_layout::Box viewport,
        SubjectEvidence& evidence, std::string& error)
    {
        const capture::PixelRect region = pixel_rect(viewport);
        const capture::DifferenceStats difference = capture::difference(
            art.image, no_art.image, region, 1u);
        const float width_fraction = difference.bounds.valid()
            ? static_cast<float>(difference.bounds.width)
                / std::max(1.0f, viewport.width) : 0.0f;
        const float height_fraction = difference.bounds.valid()
            ? static_cast<float>(difference.bounds.height)
                / std::max(1.0f, viewport.height) : 0.0f;
        // A grounded terminal sprite is expected to meet the projected ground
        // at the PIP's lower edge. Keep real clearance on left/right/top and
        // reject any escaped pixel, while allowing that one physical contact
        // edge instead of treating it as cropped anatomy.
        const bool ground_contact_margin = difference.bounds.valid()
            && difference.bounds.x >= region.x + 3
            && difference.bounds.y >= region.y + 3
            && difference.bounds.x + difference.bounds.width
                <= region.x + region.width - 3
            && difference.bounds.y + difference.bounds.height
                <= region.y + region.height;
        if (!difference.comparable || difference.changed_pixels < 256u
            || width_fraction < 0.16f || height_fraction < 0.34f
            || width_fraction > 0.90f || height_fraction > 0.88f
            || !contains(region, difference.bounds)
            || !ground_contact_margin)
        {
            error = std::format(
                "compact-species PIP is tiny, oversized, or clipped away from physical ground: pixels={} bounds={},{},{},{} fractions={:.5f},{:.5f}",
                difference.changed_pixels, difference.bounds.x,
                difference.bounds.y, difference.bounds.width,
                difference.bounds.height, width_fraction, height_fraction);
            return false;
        }
        evidence.changed_pixels = difference.changed_pixels;
        evidence.bounds = difference.bounds;
        evidence.viewport_width_fraction = width_fraction;
        evidence.viewport_height_fraction = height_fraction;
        return true;
    }

    [[nodiscard]] bool validate_zoom_locality(const Frame& before,
        const Frame& after, runner::ui_layout::Box viewport,
        std::size_t& changed_pixels, std::string& error)
    {
        const capture::DifferenceStats changed = capture::difference(
            before.image, after.image, 1u);
        const capture::DifferenceStats editor_changed = capture::difference(
            before.image, after.image, pixel_rect(viewport), 1u);
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        const runner::ui_layout::RigLabWorkspaceLayout workspace =
            runner::ui_layout::rig_lab_workspace_layout(content,
                runner::ui_layout::RigLabWorkspacePage::inspector);
        const capture::DifferenceStats trainer_indicator_changed =
            capture::difference(before.image, after.image,
                pixel_rect(workspace.trainer), 1u);
        const capture::DifferenceStats controls_indicator_changed =
            capture::difference(before.image, after.image,
                pixel_rect(workspace.inspector), 1u);
        const runner::ui_layout::Box live_box = workspace.live;
        const capture::DifferenceStats retained_live_changed =
            capture::difference(before.image, after.image,
                pixel_rect(live_box), 1u);
        const std::size_t indicator_changed_pixels =
            trainer_indicator_changed.changed_pixels
            + controls_indicator_changed.changed_pixels;
        const std::size_t classified_changed_pixels =
            editor_changed.changed_pixels + indicator_changed_pixels
            + retained_live_changed.changed_pixels;
        const std::size_t proportional_indicator_limit = std::max<std::size_t>(
            4'096u, editor_changed.changed_pixels / 3u);
        if (!changed.comparable || !editor_changed.comparable
            || editor_changed.changed_pixels < 64u
            || retained_live_changed.changed_pixels != 0u
            || changed.changed_pixels != classified_changed_pixels
            || indicator_changed_pixels
                > maximum_zoom_indicator_changed_pixels
            || indicator_changed_pixels > proportional_indicator_limit)
        {
            error = std::format(
                "Rig Lab wheel zoom was inert or escaped the hovered editor/declared zoom indicators: total={} editor={} indicators={} retained_live={} classified={} bounds={},{},{},{} world={},{},{},{}",
                changed.changed_pixels, editor_changed.changed_pixels,
                indicator_changed_pixels,
                retained_live_changed.changed_pixels,
                classified_changed_pixels,
                changed.bounds.x, changed.bounds.y,
                changed.bounds.width, changed.bounds.height,
                static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                static_cast<int>(viewport.width),
                static_cast<int>(viewport.height));
            return false;
        }
        changed_pixels = editor_changed.changed_pixels;
        return true;
    }

    [[nodiscard]] bool matching_subject_bounds(const SubjectEvidence& first,
        const SubjectEvidence& second, std::string& error)
    {
        constexpr int pixel_tolerance = 4;
        constexpr float fraction_tolerance = 0.01f;
        const bool matches = std::abs(first.bounds.x - second.bounds.x)
                <= pixel_tolerance
            && std::abs(first.bounds.y - second.bounds.y) <= pixel_tolerance
            && std::abs(first.bounds.width - second.bounds.width)
                <= pixel_tolerance
            && std::abs(first.bounds.height - second.bounds.height)
                <= pixel_tolerance
            && std::abs(first.viewport_width_fraction
                    - second.viewport_width_fraction) <= fraction_tolerance
            && std::abs(first.viewport_height_fraction
                    - second.viewport_height_fraction) <= fraction_tolerance;
        if (matches)
            return true;
        error = std::format(
            "Rig Lab non-Test pages no longer share one physical subject projection: first={},{},{},{} second={},{},{},{}",
            first.bounds.x, first.bounds.y, first.bounds.width,
            first.bounds.height, second.bounds.x, second.bounds.y,
            second.bounds.width, second.bounds.height);
        return false;
    }

    [[nodiscard]] bool materially_zoomed_subject(
        const SubjectEvidence& before, const SubjectEvidence& after,
        std::string& error)
    {
        const float width_growth = static_cast<float>(after.bounds.width)
            / static_cast<float>(std::max(1, before.bounds.width));
        const float height_growth = static_cast<float>(after.bounds.height)
            / static_cast<float>(std::max(1, before.bounds.height));
        if (width_growth >= 1.04f || height_growth >= 1.04f)
            return true;
        error = std::format(
            "Rig Lab wheel changed pixels but did not materially enlarge the physical subject: growth={:.4f},{:.4f}",
            width_growth, height_growth);
        return false;
    }

    [[nodiscard]] bool write_frame(const std::filesystem::path& output_directory,
        std::string_view stem, const Frame& frame,
        const SubjectEvidence* subject, std::size_t zoom_changed_pixels,
        std::string& manifest, std::string& error)
    {
        const std::vector<std::uint8_t> bytes =
            capture::ppm_bytes(frame.image);
        if (bytes.empty())
        {
            error = "software rasterizer produced an invalid PPM payload";
            return false;
        }
        const std::string filename = std::format(
            "{}-{}x{}.ppm", stem, capture_width, capture_height);
        if (!atomic_write(output_directory / filename, bytes, error))
            return false;
        manifest += std::format(
            "frame={} width={} height={} vertices={} triangles={} shaded_samples={} bytes={} fnv1a64={:016x}",
            filename, capture_width, capture_height, frame.vertices,
            frame.raster.rasterized_triangles, frame.raster.shaded_samples,
            bytes.size(), capture::fnv1a64(bytes));
        if (subject != nullptr)
        {
            manifest += std::format(
                " subject_pixels={} subject_bounds={},{},{},{} subject_viewport_fraction={:.5f},{:.5f}",
                subject->changed_pixels, subject->bounds.x, subject->bounds.y,
                subject->bounds.width, subject->bounds.height,
                subject->viewport_width_fraction,
                subject->viewport_height_fraction);
        }
        if (zoom_changed_pixels > 0u)
            manifest += std::format(
                " zoom_local_changed_pixels={}", zoom_changed_pixels);
        manifest.push_back('\n');
        return true;
    }

    [[nodiscard]] bool write_frame_at(
        const std::filesystem::path& output_directory,
        std::string_view stem, int width, int height, const Frame& frame,
        std::string& manifest, std::string& error)
    {
        const std::vector<std::uint8_t> bytes = capture::ppm_bytes(frame.image);
        if (bytes.empty())
        {
            error = "software rasterizer produced an invalid sized PPM payload";
            return false;
        }
        const std::string filename = std::format(
            "{}-{}x{}.ppm", stem, width, height);
        if (!atomic_write(output_directory / filename, bytes, error))
            return false;
        manifest += std::format(
            "frame={} width={} height={} vertices={} triangles={} shaded_samples={} bytes={} fnv1a64={:016x}\n",
            filename, width, height, frame.vertices,
            frame.raster.rasterized_triangles, frame.raster.shaded_samples,
            bytes.size(), capture::fnv1a64(bytes));
        return true;
    }

    [[nodiscard]] bool capture_four_k_ui(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error))
            return false;
        for (std::size_t step = 0u; step < presentation_settle_steps; ++step)
            application.frame({}, fixed_step,
                four_k_capture_width, four_k_capture_height);

        const runner::ui_layout::PresentationSurface presentation =
            runner::ui_layout::presentation_surface(
                four_k_capture_width, four_k_capture_height);
        if (!presentation.density_scaled
            || presentation.canvas_width != 2560
            || presentation.canvas_height != 1440
            || std::abs(presentation.scale_x - 1.5f) > 0.001f
            || std::abs(presentation.scale_y - 1.5f) > 0.001f)
        {
            error = std::format(
                "4K presentation contract failed: canvas={}x{} scale={:.4f},{:.4f}",
                presentation.canvas_width, presentation.canvas_height,
                presentation.scale_x, presentation.scale_y);
            return false;
        }
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(presentation.canvas_width),
            static_cast<float>(presentation.canvas_height));
        const runner::ui_layout::Box live_world =
            runner::ui_layout::presentation_box(
                runner::ui_layout::live_world_box(content), presentation);
        const runner::ui_layout::Box live_panel =
            runner::ui_layout::presentation_box(
                runner::ui_layout::live_panel_box(content), presentation);

        Frame frame{};
        if (!render_frame_at(application, {}, 0.0f,
                four_k_capture_width, four_k_capture_height, frame, error)
            || !require_populated(frame, live_world,
                "4K live world", error)
            || !require_populated(frame, live_panel,
                "4K live command rail", error)
            || !write_frame_at(output_directory, "live-interface",
                four_k_capture_width, four_k_capture_height,
                frame, manifest, error))
            return false;

        runner::InputState next_page{};
        next_page.totals_pressed = true;
        if (!render_frame_at(application, next_page, 0.0f,
                four_k_capture_width, four_k_capture_height, frame, error)
            || !require_populated(frame, live_panel,
                "4K Live Controls page", error)
            || !write_frame_at(output_directory, "live-mission",
                four_k_capture_width, four_k_capture_height,
                frame, manifest, error))
            return false;

        // Three further internal transitions reach the visible Details route
        // through View and its nested History disclosure. Raw rows require a
        // separate explicit disclosure and are not inherited.
        for (std::size_t page = 0u; page < 3u; ++page)
        {
            if (!render_frame_at(application, next_page, 0.0f,
                    four_k_capture_width, four_k_capture_height,
                    frame, error))
                return false;
        }
        if (!render_frame_at(application, {}, 0.0f,
                four_k_capture_width, four_k_capture_height, frame, error)
            || !require_populated(frame, live_panel,
                "4K concise Details page", error)
            || !write_frame_at(output_directory, "live-details",
                four_k_capture_width, four_k_capture_height,
                frame, manifest, error))
            return false;

        if (!application.prepare_rig_lab_page(
                runner::RigLabStartupPage::art, 0u)
            || !render_frame_at(application, {}, fixed_step,
                four_k_capture_width, four_k_capture_height, frame, error))
        {
            if (error.empty())
                error = "4K Rig Lab Art page preparation failed";
            return false;
        }
        const runner::ui_layout::RigLabWorkspaceLayout inspector_layout =
            runner::ui_layout::rig_lab_workspace_layout(content,
                runner::ui_layout::RigLabWorkspacePage::inspector);
        const runner::ui_layout::Box trainer =
            runner::ui_layout::presentation_box(
                inspector_layout.trainer, presentation);
        const runner::ui_layout::Box inspector =
            runner::ui_layout::presentation_box(
                inspector_layout.inspector, presentation);
        const runner::ui_layout::Box preview =
            runner::ui_layout::presentation_box(
                inspector_layout.world, presentation);
        if (inspector_layout.compact
            || !require_populated(frame, trainer,
                "4K simultaneous Trainer", error)
            || !require_populated(frame, inspector,
                "4K Rig Inspector", error)
            || !require_populated(frame, preview,
                "4K Rig preview", error)
            || !write_frame_at(output_directory, "riglab-art-human",
                four_k_capture_width, four_k_capture_height,
                frame, manifest, error))
            return false;

        const runner::ui_layout::Box setup_button =
            runner::ui_layout::presentation_box(
                runner::ui_layout::rig_lab_dual_setup_button_layout(
                    inspector_layout.trainer), presentation);
        runner::InputState open_controls{};
        open_controls.mouse = {
            setup_button.x + setup_button.width * 0.5f,
            setup_button.y + setup_button.height * 0.5f };
        open_controls.left_pressed = true;
        if (!render_frame_at(application, open_controls, 0.0f,
                four_k_capture_width, four_k_capture_height, frame, error)
            || !require_populated(frame, trainer,
                "4K contextual Training Controls", error)
            || !require_populated(frame, inspector,
                "4K Inspector beside Training Controls", error)
            || !require_populated(frame, preview,
                "4K preview beside Training Controls", error)
            || !write_frame_at(output_directory, "riglab-controls",
                four_k_capture_width, four_k_capture_height,
                frame, manifest, error))
            return false;

        runner::InputState close_controls = open_controls;
        if (!render_frame_at(application, close_controls, 0.0f,
                four_k_capture_width, four_k_capture_height, frame, error))
            return false;
        if (!require_populated(frame, trainer,
                "4K embedded full Trainer", error)
            || !require_populated(frame, inspector,
                "4K Inspector beside full Trainer", error)
            || !require_populated(frame, preview,
                "4K preview beside full Trainer", error)
            || !write_frame_at(output_directory, "riglab-trainer",
                four_k_capture_width, four_k_capture_height,
                frame, manifest, error))
            return false;
        return true;
    }

    [[nodiscard]] bool capture_live(const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error))
            return false;
        settle_presentation(application);
        Frame frame{};
        if (!render_frame(application, {}, 0.0f, frame, error)
            || !validate_live_regions(frame, error, false))
            return false;
        if (!write_frame(output_directory, "live-interface", frame,
                nullptr, 0u, manifest, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        const runner::ui_layout::Box panel =
            runner::ui_layout::live_panel_box(content);
        runner::InputState focus_status{};
        focus_status.mouse = { panel.x + panel.width * 0.5f,
            panel.y + panel.height * 0.5f };
        focus_status.wheel = -120.0f;
        if (!render_frame(application, focus_status, 0.0f, frame, error)
            || !validate_live_regions(frame, error, false)
            || !write_frame(output_directory, "live-interface-scrolled", frame,
                nullptr, 0u, manifest, error))
            return false;
        constexpr std::array<std::string_view, 3> page_stems{
            "live-mission", "live-view", "live-details" };
        for (std::size_t page_index = 0u;
            page_index < page_stems.size(); ++page_index)
        {
            const std::string_view stem = page_stems[page_index];
            runner::InputState next_page{};
            next_page.totals_pressed = true;
            const std::size_t transitions = page_index == 2u ? 2u : 1u;
            for (std::size_t transition = 0u;
                transition < transitions; ++transition)
            {
                if (!render_frame(
                        application, next_page, 0.0f, frame, error))
                    return false;
            }
            // Mission and Details deliberately keep telemetry in the rail.
            // Only View owns the in-world snapshot overlay.
            const bool expects_snapshot = page_index == 1u;
            if (!validate_live_regions(frame, error, expects_snapshot)
                || !write_frame(output_directory, stem, frame,
                    nullptr, 0u, manifest, error))
                return false;
            runner::InputState focus_page{};
            focus_page.mouse = { panel.x + panel.width * 0.5f,
                panel.y + panel.height * 0.5f };
            focus_page.wheel = -120.0f;
            if (!render_frame(application, focus_page, 0.0f, frame, error)
                || !validate_live_regions(frame, error, expects_snapshot)
                || !write_frame(output_directory,
                    std::string{ stem } + "-scrolled", frame,
                    nullptr, 0u, manifest, error))
                return false;
        }
        return true;
    }

    [[nodiscard]] bool capture_narrow_live(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error))
            return false;
        for (std::size_t step = 0u; step < presentation_settle_steps; ++step)
            application.frame({}, fixed_step,
                narrow_capture_width, narrow_capture_height);
        Frame frame{};
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(narrow_capture_width),
            static_cast<float>(narrow_capture_height));
        const runner::ui_layout::Box panel =
            runner::ui_layout::live_panel_box(content);
        const auto render_narrow = [&](const runner::InputState& input)
        {
            application.frame(input, 0.0f,
                narrow_capture_width, narrow_capture_height);
            const std::span<const runner::render::Vertex> vertices =
                application.vertices();
            frame = {};
            frame.vertices = vertices.size();
            return !vertices.empty() && vertices.size() % 3u == 0u
                && capture::rasterize(vertices,
                    narrow_capture_width, narrow_capture_height,
                    frame.image, frame.raster, error);
        };
        const auto validate_narrow = [&]()
        {
            return runner::ui_layout::live_layout_valid(
                static_cast<float>(narrow_capture_width),
                static_cast<float>(narrow_capture_height))
                && require_populated(frame,
                runner::ui_layout::live_world_box(content),
                "narrow live production world", error)
                && require_populated(frame, panel,
                    "narrow live trainer panel", error);
        };
        const auto write_narrow = [&](std::string_view stem)
        {
            const std::vector<std::uint8_t> bytes =
                capture::ppm_bytes(frame.image);
            const std::string filename = std::format(
                "{}-{}x{}.ppm", stem,
                narrow_capture_width, narrow_capture_height);
            if (!atomic_write(output_directory / filename, bytes, error))
                return false;
            manifest += std::format(
                "frame={} width={} height={} vertices={} triangles={} shaded_samples={} bytes={} fnv1a64={:016x}\n",
                filename, narrow_capture_width, narrow_capture_height,
                frame.vertices, frame.raster.rasterized_triangles,
                frame.raster.shaded_samples, bytes.size(),
                capture::fnv1a64(bytes));
            return true;
        };
        if (!render_narrow({}) || !validate_narrow()
            || !write_narrow("live-interface"))
            return false;
        runner::InputState focus_status{};
        focus_status.mouse = { panel.x + panel.width * 0.5f,
            panel.y + panel.height * 0.5f };
        focus_status.wheel = -120.0f;
        if (!render_narrow(focus_status) || !validate_narrow()
            || !write_narrow("live-interface-scrolled"))
            return false;
        constexpr std::array<std::string_view, 3> page_stems{
            "live-mission", "live-view", "live-details" };
        for (std::size_t page_index = 0u;
            page_index < page_stems.size(); ++page_index)
        {
            const std::string_view stem = page_stems[page_index];
            runner::InputState next_page{};
            next_page.totals_pressed = true;
            const std::size_t transitions = page_index == 2u ? 2u : 1u;
            for (std::size_t transition = 0u;
                transition < transitions; ++transition)
            {
                if (!render_narrow(next_page))
                    return false;
            }
            if (!validate_narrow()
                || !write_narrow(stem))
                return false;
            runner::InputState focus_page{};
            focus_page.mouse = { panel.x + panel.width * 0.5f,
                panel.y + panel.height * 0.5f };
            focus_page.wheel = -120.0f;
            if (!render_narrow(focus_page) || !validate_narrow()
                || !write_narrow(std::string{ stem } + "-scrolled"))
                return false;
        }
        return true;
    }

    [[nodiscard]] bool capture_course(const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error))
            return false;
        application.prepare_course_eye_test();
        settle_presentation(application);
        Frame frame{};
        if (!render_frame(application, {}, 0.0f, frame, error)
            || !validate_live_regions(frame, error))
            return false;
        runner::InputState toggle_art{};
        toggle_art.art_pressed = true;
        Frame no_art{};
        if (!render_frame(application, toggle_art, 0.0f, no_art, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        const runner::ui_layout::Box world =
            runner::ui_layout::live_world_box(content);
        const runner::ui_layout::Box subject_view =
            live_subject_viewport(world);
        const capture::PixelRect subject_region = pixel_rect(subject_view);
        const capture::DifferenceStats subject = capture::difference(
            frame.image, no_art.image, subject_region, 1u);
        const float subject_center_fraction = subject.bounds.valid()
            ? (static_cast<float>(subject.bounds.x) +
                static_cast<float>(subject.bounds.width) * 0.5f - world.x)
                / std::max(1.0f, world.width)
            : -1.0f;
        if (!subject.comparable || subject.changed_pixels < 128u
            || !subject.bounds.valid()
            || !contains(subject_region, subject.bounds)
            || subject.bounds.x <= subject_region.x + 3
            || subject.bounds.x + subject.bounds.width
                >= subject_region.x + subject_region.width - 3
            || subject_center_fraction < 0.06f
            || subject_center_fraction > 0.24f)
        {
            error = std::format(
                "Course subject is clipped or not placed in its facing-aware lead frame: pixels={} bounds={},{},{},{} center_fraction={:.5f}",
                subject.changed_pixels, subject.bounds.x, subject.bounds.y,
                subject.bounds.width, subject.bounds.height,
                subject_center_fraction);
            return false;
        }
        manifest += std::format(
            "course_subject pixels={} bounds={},{},{},{} center_fraction={:.5f}\n",
            subject.changed_pixels, subject.bounds.x, subject.bounds.y,
            subject.bounds.width, subject.bounds.height,
            subject_center_fraction);
        return write_frame(output_directory, "course", frame,
            nullptr, 0u, manifest, error);
    }

    [[nodiscard]] bool capture_art_subject(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::size_t species_index, std::string_view species_name,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error)
            || !application.prepare_art_diagnostic_rig(species_index))
        {
            if (error.empty())
                error = "art diagnostic rejected species index";
            return false;
        }
        Frame art{};
        if (!render_frame(application, {}, 0.0f, art, error)
            || !validate_live_regions(art, error))
            return false;
        runner::InputState toggle{};
        toggle.art_pressed = true;
        Frame no_art{};
        if (!render_frame(application, toggle, 0.0f, no_art, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        SubjectEvidence subject{};
        const runner::ui_layout::Box live_world =
            runner::ui_layout::live_world_box(content);
        if (!subject_evidence(art, no_art,
                live_subject_viewport(live_world), subject, error)
            || !subject_clear_of_live_overlays(
                live_world, subject, error))
            return false;
        if (species_index > 0u)
        {
            SubjectEvidence pip_subject{};
            const runner::ui_layout::Box pip =
                runner::ui_layout::training_pip_box(live_world);
            // Measure the clipped production-world viewport, not the outer
            // rounded card. Its deliberately transparent corner pixels can
            // reveal the main world's subject and are not PIP clipping.
            const runner::ui_layout::Box pip_world =
                runner::ui_layout::training_pip_world_box(pip);
            if (!compact_pip_evidence(art, no_art,
                    pip_world, pip_subject, error))
                return false;
            manifest += std::format(
                "pip_subject={} pixels={} bounds={},{},{},{} viewport_fraction={:.5f},{:.5f}\n",
                species_name, pip_subject.changed_pixels,
                pip_subject.bounds.x, pip_subject.bounds.y,
                pip_subject.bounds.width, pip_subject.bounds.height,
                pip_subject.viewport_width_fraction,
                pip_subject.viewport_height_fraction);
        }
        return write_frame(output_directory,
            std::format("art-{}", species_name), art,
            &subject, 0u, manifest, error);
    }

    [[nodiscard]] bool capture_equipment(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error)
            || !application.prepare_equipment_eye_test(error))
            return false;
        Frame art{};
        if (!render_frame(application, {}, 0.0f, art, error)
            || !validate_live_regions(art, error))
            return false;
        runner::InputState toggle{};
        toggle.art_pressed = true;
        Frame no_art{};
        if (!render_frame(application, toggle, 0.0f, no_art, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        SubjectEvidence subject{};
        const runner::ui_layout::Box live_world =
            runner::ui_layout::live_world_box(content);
        if (!subject_evidence(art, no_art,
                live_subject_viewport(live_world), subject, error)
            || !subject_clear_of_live_overlays(
                live_world, subject, error))
            return false;
        return write_frame(output_directory, "equipment-assisted-ready", art,
            &subject, 0u, manifest, error);
    }

    [[nodiscard]] bool capture_rig_lab_page(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        runner::RigLabStartupPage page, std::string_view page_name,
        std::size_t species_index, std::string_view species_name,
        std::string& manifest, std::string& error,
        SubjectEvidence* captured_subject = nullptr)
    {
        runner::Application application{};
        if (!initialize(application, assets, error)
            || !application.prepare_rig_lab_page(page, species_index))
        {
            if (error.empty())
                error = "Rig Lab rejected page/species request";
            return false;
        }
        const std::uint64_t expected_signature =
            canonical_rig_signature(species_index);
        const std::uint64_t preview_signature =
            application.rig_preview_signature();
        if (expected_signature == 0u
            || preview_signature != expected_signature)
        {
            error = std::format(
                "Rig Lab request/render species mismatch: subject={} expected={:016x} preview={:016x}",
                species_name, expected_signature, preview_signature);
            return false;
        }
        manifest += std::format(
            "riglab_subject={} page={} preview_signature={:016x}\n",
            species_name, page_name, preview_signature);
        Frame normal{};
        if (!render_frame(application, {}, fixed_step, normal, error)
            || !validate_rig_lab_regions(normal, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        const runner::ui_layout::Box world =
            runner::ui_layout::rig_lab_workspace_layout(content,
                runner::ui_layout::RigLabWorkspacePage::inspector).world;

        runner::InputState toggle{};
        toggle.art_pressed = true;
        Frame no_art{};
        if (!render_frame(application, toggle, 0.0f, no_art, error))
            return false;
        SubjectEvidence subject{};
        if (!subject_evidence(normal, no_art, world, subject, error))
            return false;
        if (captured_subject != nullptr)
            *captured_subject = subject;

        runner::InputState restore_art{};
        restore_art.art_pressed = true;
        Frame restored{};
        if (!render_frame(application, restore_art, 0.0f, restored, error))
            return false;
        Frame before_zoom{};
        if (!render_frame(application, {}, 0.0f, before_zoom, error))
            return false;
        runner::InputState zoom{};
        zoom.wheel = 1.0f;
        zoom.mouse = { world.x + world.width * 0.5f,
            world.y + world.height * 0.5f };
        Frame zoomed{};
        if (!render_frame(application, zoom, 0.0f, zoomed, error)
            || !validate_rig_lab_regions(zoomed, error))
            return false;
        runner::InputState zoom_toggle{};
        zoom_toggle.art_pressed = true;
        Frame zoomed_no_art{};
        if (!render_frame(application, zoom_toggle, 0.0f,
                zoomed_no_art, error))
            return false;
        SubjectEvidence zoomed_subject{};
        if (!subject_evidence(zoomed, zoomed_no_art, world,
                zoomed_subject, error))
            return false;
        if (!materially_zoomed_subject(subject, zoomed_subject, error))
            return false;
        std::size_t zoom_changed_pixels{};
        if (!validate_zoom_locality(before_zoom, zoomed, world,
                zoom_changed_pixels, error))
            return false;

        const std::string base_stem = std::format(
            "riglab-{}-{}", page_name, species_name);
        return write_frame(output_directory, base_stem, normal,
                &subject, 0u, manifest, error)
            && write_frame(output_directory, base_stem + "-zoom-in", zoomed,
                &zoomed_subject, zoom_changed_pixels, manifest, error);
    }

    [[nodiscard]] bool capture_narrow_rig_lab_page(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        runner::RigLabStartupPage page, std::string_view page_name,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error)
            || !application.prepare_rig_lab_page(page, 0u))
        {
            if (error.empty())
                error = "narrow Rig Lab rejected page request";
            return false;
        }
        application.frame({}, fixed_step,
            narrow_capture_width, narrow_capture_height);
        const std::span<const runner::render::Vertex> vertices =
            application.vertices();
        Frame frame{};
        frame.vertices = vertices.size();
        if (vertices.empty() || vertices.size() % 3u != 0u
            || !capture::rasterize(vertices,
                narrow_capture_width, narrow_capture_height,
                frame.image, frame.raster, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(narrow_capture_width),
            static_cast<float>(narrow_capture_height));
        const runner::ui_layout::RigLabWorkspaceLayout workspace =
            runner::ui_layout::rig_lab_workspace_layout(content,
                runner::ui_layout::RigLabWorkspacePage::inspector);
        if (!runner::ui_layout::rig_lab_layout_valid(
                static_cast<float>(narrow_capture_width),
                static_cast<float>(narrow_capture_height))
            || !require_populated(frame, workspace.workspace_bar,
                "narrow Rig Lab workspace selector", error)
            || !require_populated(frame, workspace.inspector,
                "narrow Rig Lab inspector", error)
            || !require_populated(frame, workspace.world,
                "narrow Rig Lab viewport", error))
            return false;
        const auto write_narrow = [&](std::string_view suffix)
        {
            const std::vector<std::uint8_t> bytes =
                capture::ppm_bytes(frame.image);
            const std::string filename = std::format(
                "riglab-{}-human{}-{}x{}.ppm", page_name, suffix,
                narrow_capture_width, narrow_capture_height);
            if (!atomic_write(output_directory / filename, bytes, error))
                return false;
            manifest += std::format(
                "frame={} width={} height={} vertices={} triangles={} shaded_samples={} bytes={} fnv1a64={:016x}\n",
                filename, narrow_capture_width, narrow_capture_height,
                frame.vertices, frame.raster.rasterized_triangles,
                frame.raster.shaded_samples, bytes.size(),
                capture::fnv1a64(bytes));
            return true;
        };
        if (!write_narrow(""))
            return false;
        runner::InputState scroll{};
        scroll.mouse = { workspace.inspector.x
                + workspace.inspector.width * 0.5f,
            workspace.inspector.y + workspace.inspector.height * 0.5f };
        scroll.wheel = -120.0f;
        application.frame(scroll, 0.0f,
            narrow_capture_width, narrow_capture_height);
        const std::span<const runner::render::Vertex> scrolled_vertices =
            application.vertices();
        frame = {};
        frame.vertices = scrolled_vertices.size();
        if (scrolled_vertices.empty() || scrolled_vertices.size() % 3u != 0u
            || !capture::rasterize(scrolled_vertices,
                narrow_capture_width, narrow_capture_height,
                frame.image, frame.raster, error)
            || !require_populated(frame, workspace.inspector,
                "scrolled narrow Rig Lab inspector", error))
            return false;
        return write_narrow("-scrolled");
    }

    [[nodiscard]] bool capture_opposing_physical_test_phase(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error)
            || !application.prepare_rig_lab_page(
                runner::RigLabStartupPage::test, 0u))
        {
            if (error.empty())
                error = "Rig Lab rejected opposing Human Test phase";
            return false;
        }
        // Drive the real deterministic Environment until physical contacts,
        // not an animation-clock estimate, prove one complete heel/ball/toe
        // plate is planted while the opposing plate is airborne. Generation
        // must remain unchanged so a hidden terminal recycle cannot produce a
        // plausible-looking replacement pose. This remains labelled Assisted
        // physical test evidence and is never Raw learned-policy proof.
        constexpr std::size_t maximum_opposing_phase_steps = 360u;
        const std::uint64_t initial_generation =
            application.rig_preview_contact_evidence()
                .physical_test_generation;
        runner::RigPreviewContactEvidence contact{};
        std::size_t opposing_phase_steps{};
        for (std::size_t step = 1u;
            step <= maximum_opposing_phase_steps; ++step)
        {
            application.frame({}, fixed_step, capture_width, capture_height);
            contact = application.rig_preview_contact_evidence();
            if (contact.physical_test_generation != initial_generation)
            {
                error = "physical Rig Lab Test terminated and recycled before "
                    "an opposing planted heel/ball/toe frame";
                return false;
            }
            if (contact.opposing_planted_sole())
            {
                opposing_phase_steps = step;
                break;
            }
        }
        if (opposing_phase_steps == 0u)
        {
            error = std::format(
                "physical Rig Lab Test never reached an opposing planted sole "
                "within {} steps: L={}/{}/{} R={}/{}/{} valid={}",
                maximum_opposing_phase_steps,
                contact.left_heel, contact.left_ball, contact.left_toe,
                contact.right_heel, contact.right_ball, contact.right_toe,
                contact.valid_motion);
            return false;
        }
        Frame phase{};
        if (!render_frame(application, {}, 0.0f, phase, error)
            || !validate_rig_lab_regions(phase, error))
            return false;
        runner::InputState toggle{};
        toggle.art_pressed = true;
        Frame no_art{};
        if (!render_frame(application, toggle, 0.0f, no_art, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        SubjectEvidence subject{};
        if (!subject_evidence(phase, no_art,
                runner::ui_layout::rig_lab_workspace_layout(content,
                    runner::ui_layout::RigLabWorkspacePage::inspector).world,
                subject, error))
            return false;
        manifest += std::format(
            "physical_test_opposing_phase_steps={} generation={} "
            "left_heel_ball_toe={}/{}/{} right_heel_ball_toe={}/{}/{}\n",
            opposing_phase_steps, initial_generation,
            contact.left_heel, contact.left_ball, contact.left_toe,
            contact.right_heel, contact.right_ball, contact.right_toe);
        return write_frame(output_directory,
            "riglab-test-human-opposing-phase", phase,
            &subject, 0u, manifest, error);
    }

    [[nodiscard]] bool capture_species_physical_contact_phase(
        const std::filesystem::path& assets,
        const std::filesystem::path& output_directory,
        std::size_t species_index, std::string_view species_name,
        std::string& manifest, std::string& error)
    {
        runner::Application application{};
        if (!initialize(application, assets, error)
            || !application.prepare_rig_lab_page(
                runner::RigLabStartupPage::test, species_index))
        {
            if (error.empty())
                error = "Rig Lab rejected species contact-phase request";
            return false;
        }
        const runner::RigPreviewContactEvidence initial =
            application.rig_preview_contact_evidence();
        runner::RigPreviewContactEvidence contact{};
        constexpr std::size_t maximum_contact_phase_steps = 240u;
        std::size_t contact_phase_steps{};
        for (std::size_t step = 1u;
            step <= maximum_contact_phase_steps; ++step)
        {
            application.frame({}, fixed_step, capture_width, capture_height);
            contact = application.rig_preview_contact_evidence();
            if (contact.physical_test_generation
                != initial.physical_test_generation)
            {
                error = std::format(
                    "{} physical Test recycled before contact evidence",
                    species_name);
                return false;
            }
            const bool contact_distribution_changed =
                contact.left_support_contacts
                    != initial.left_support_contacts
                || contact.right_support_contacts
                    != initial.right_support_contacts;
            if (step >= 8u && contact.has_physical_support()
                && (contact_distribution_changed || step >= 45u))
            {
                contact_phase_steps = step;
                break;
            }
        }
        if (contact_phase_steps == 0u)
        {
            error = std::format(
                "{} physical Test has no stable contact phase within {} steps",
                species_name, maximum_contact_phase_steps);
            return false;
        }

        Frame phase{};
        if (!render_frame(application, {}, 0.0f, phase, error)
            || !validate_rig_lab_regions(phase, error))
            return false;
        runner::InputState toggle{};
        toggle.art_pressed = true;
        Frame no_art{};
        if (!render_frame(application, toggle, 0.0f, no_art, error))
            return false;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(capture_width),
            static_cast<float>(capture_height));
        SubjectEvidence subject{};
        if (!subject_evidence(phase, no_art,
                runner::ui_layout::rig_lab_workspace_layout(content,
                    runner::ui_layout::RigLabWorkspacePage::inspector).world,
                subject, error))
            return false;
        manifest += std::format(
            "physical_species_contact={} steps={} generation={} "
            "left_supports={} right_supports={}\n",
            species_name, contact_phase_steps,
            initial.physical_test_generation,
            contact.left_support_contacts,
            contact.right_support_contacts);
        return write_frame(output_directory,
            std::format("riglab-test-{}-contact-phase", species_name),
            phase, &subject, 0u, manifest, error);
    }
}

int main(int argc, char** argv)
{
    if (argc != 3 || argv == nullptr || argv[1] == nullptr
        || argv[2] == nullptr)
    {
        std::cerr << "usage: RunnerVisualCapture <asset-directory> <output-directory>\n";
        return 2;
    }
    std::error_code filesystem_error{};
    const std::filesystem::path assets = std::filesystem::absolute(
        std::filesystem::path{ argv[1] }, filesystem_error);
    if (filesystem_error)
    {
        std::cerr << "visual capture asset path could not be resolved: "
            << filesystem_error.message() << '\n';
        return 2;
    }
    filesystem_error.clear();
    const std::filesystem::path output_directory = std::filesystem::absolute(
        std::filesystem::path{ argv[2] }, filesystem_error);
    if (filesystem_error)
    {
        std::cerr << "visual capture output path could not be resolved: "
            << filesystem_error.message() << '\n';
        return 2;
    }
    filesystem_error.clear();
    if (!std::filesystem::is_directory(assets, filesystem_error))
    {
        std::cerr << "visual capture asset directory is unavailable: "
            << assets.string() << '\n';
        return 2;
    }
    filesystem_error.clear();
    std::filesystem::create_directories(output_directory, filesystem_error);
    if (filesystem_error)
    {
        std::cerr << "visual capture output directory could not be created: "
            << filesystem_error.message() << '\n';
        return 1;
    }
    // Application construction probes package-relative authored rigs. Use a
    // verified-empty runtime root so a caller's interactive checkout/autosave
    // cannot alter these canonical factory frames. Isolated initialization
    // performs no persistence writes, so the directory stays empty on repeats.
    const std::filesystem::path isolated_runtime =
        output_directory / ".isolated-runtime";
    std::filesystem::create_directories(isolated_runtime, filesystem_error);
    std::filesystem::directory_iterator isolated_begin{};
    if (!filesystem_error)
        isolated_begin = std::filesystem::directory_iterator(
            isolated_runtime, filesystem_error);
    if (!filesystem_error
        && isolated_begin != std::filesystem::directory_iterator{})
    {
        std::cerr << "visual capture runtime root is not empty: "
            << isolated_runtime.string() << '\n';
        return 1;
    }
    if (!filesystem_error)
        std::filesystem::current_path(isolated_runtime, filesystem_error);
    if (filesystem_error)
    {
        std::cerr << "visual capture isolated runtime directory was rejected: "
            << filesystem_error.message() << '\n';
        return 1;
    }

    std::string manifest = std::format(
        "Epoch2DWalkEngine v{} deterministic production-vertex visual captures "
        "(Runner example)\n"
        "logical_width={}\nlogical_height={}\n"
        "presentation_settle_steps={}\n"
        "raster_contract=top-left-pixel-center/source-over/linear-blend/srgb-output\n"
        "authority=visual-qa-only-not-native-vulkan-not-raw-learned-proof\n",
        RUNNER_VERSION, capture_width, capture_height,
        presentation_settle_steps);
    std::string error{};
    constexpr std::array species_names{
        std::string_view{ "human" }, std::string_view{ "chicken" },
        std::string_view{ "dog" }, std::string_view{ "hexapod" }
    };
    if (!capture_four_k_ui(assets, output_directory, manifest, error)
        || !capture_live(assets, output_directory, manifest, error)
        || !capture_narrow_live(assets, output_directory, manifest, error)
        || !capture_course(assets, output_directory, manifest, error))
    {
        std::cerr << "visual capture failed: " << error << '\n';
        return 1;
    }
    for (std::size_t species = 0u; species < species_names.size(); ++species)
    {
        if (!capture_art_subject(assets, output_directory, species,
                species_names[species], manifest, error))
        {
            std::cerr << "visual capture failed: " << error << '\n';
            return 1;
        }
    }
    if (!capture_equipment(assets, output_directory, manifest, error))
    {
        std::cerr << "visual capture failed: " << error << '\n';
        return 1;
    }

    struct Page
    {
        runner::RigLabStartupPage page{};
        std::string_view name{};
    };
    constexpr std::array pages{
        Page{ runner::RigLabStartupPage::presets, "presets" },
        Page{ runner::RigLabStartupPage::structure, "structure" },
        Page{ runner::RigLabStartupPage::motors, "motors" },
        Page{ runner::RigLabStartupPage::art, "art" },
        Page{ runner::RigLabStartupPage::test, "test" }
    };
    SubjectEvidence shared_human_subject{};
    bool have_shared_human_subject{};
    for (const Page& page : pages)
    {
        SubjectEvidence page_subject{};
        if (!capture_rig_lab_page(assets, output_directory,
                page.page, page.name, 0u, species_names[0], manifest, error,
                &page_subject))
        {
            std::cerr << "visual capture failed: " << error << '\n';
            return 1;
        }
        if (page.page != runner::RigLabStartupPage::test)
        {
            if (have_shared_human_subject
                && !matching_subject_bounds(
                    shared_human_subject, page_subject, error))
            {
                std::cerr << "visual capture failed: " << error << '\n';
                return 1;
            }
            shared_human_subject = page_subject;
            have_shared_human_subject = true;
        }
        if (!capture_narrow_rig_lab_page(assets, output_directory,
                page.page, page.name, manifest, error))
        {
            std::cerr << "visual capture failed: " << error << '\n';
            return 1;
        }
    }
    if (!capture_opposing_physical_test_phase(
            assets, output_directory, manifest, error))
    {
        std::cerr << "visual capture failed: " << error << '\n';
        return 1;
    }
    for (std::size_t species = 1u; species < species_names.size(); ++species)
    {
        if (!capture_rig_lab_page(assets, output_directory,
                runner::RigLabStartupPage::art, "art", species,
                species_names[species], manifest, error))
        {
            std::cerr << "visual capture failed: " << error << '\n';
            return 1;
        }
    }
    for (std::size_t species = 1u; species < species_names.size(); ++species)
    {
        if (!capture_species_physical_contact_phase(
                assets, output_directory, species, species_names[species],
                manifest, error))
        {
            std::cerr << "visual capture failed: " << error << '\n';
            return 1;
        }
    }

    const std::vector<std::uint8_t> manifest_bytes(
        manifest.begin(), manifest.end());
    if (!atomic_write(output_directory / "manifest.txt",
            manifest_bytes, error))
    {
        std::cerr << "visual capture manifest failed: " << error << '\n';
        return 1;
    }
    std::cout << manifest;
    return 0;
}
