#include "app.hpp"
#include "art_authoring.hpp"
#include "autonomy.hpp"
#include "pixel_art.hpp"
#include "presentation_session.hpp"
#include "rig_training_diagnostic.hpp"
#include "simulation.hpp"
#include "species_art_layout.hpp"
#include "training_explainer.hpp"
#include "ui_render_contract.hpp"
#include "ui_layout.hpp"
#include "ui_font.hpp"
#include "view_camera.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


#ifndef RUNNER_VERSION
#define RUNNER_VERSION "development"
#endif

namespace runner
{
    namespace font = ui_font;

    namespace
    {
        [[nodiscard]] constexpr font::FontSize font_size(
            float style_scale) noexcept
        {
            const float readable_scale = std::max(
                style_scale > 0.0f ? style_scale : 1.0f,
                ui_layout::persistent_text_minimum_scale);
            return {
                .logical_height = font::default_logical_height
                    * readable_scale,
                .dpi_scale = 1.0f
            };
        }

        struct Rect
        {
            Vec2 position{};
            Vec2 size{};
        };

        [[nodiscard]] bool contains(Rect rect, Vec2 point) noexcept
        {
            return point.x >= rect.position.x && point.y >= rect.position.y
                && point.x <= rect.position.x + rect.size.x
                && point.y <= rect.position.y + rect.size.y;
        }


        [[nodiscard]] Color rgb(std::uint32_t hex, float alpha = 1.0f) noexcept
        {
            return {
                static_cast<float>((hex >> 16) & 0xffu) / 255.0f,
                static_cast<float>((hex >> 8) & 0xffu) / 255.0f,
                static_cast<float>(hex & 0xffu) / 255.0f,
                alpha
            };
        }

        constexpr Color white{ 0.95f, 0.96f, 0.98f, 1.0f };
        constexpr Color muted{ 0.61f, 0.66f, 0.73f, 1.0f };
        constexpr Color panel{ 0.064f, 0.076f, 0.098f, 0.98f };
        constexpr Color panel_alt{ 0.085f, 0.100f, 0.126f, 1.0f };
        constexpr Color border{ 0.16f, 0.19f, 0.24f, 1.0f };
        constexpr Color accent{ 0.20f, 0.72f, 0.92f, 1.0f };
        constexpr Color accent_dim{ 0.12f, 0.35f, 0.48f, 1.0f };
        constexpr Color danger{ 0.93f, 0.28f, 0.30f, 1.0f };
        constexpr Color yellow{ 0.95f, 0.74f, 0.18f, 1.0f };
        constexpr Color green{ 0.28f, 0.82f, 0.48f, 1.0f };
        constexpr Color body{ 0.82f, 0.59f, 0.24f, 1.0f };
        constexpr Color body_light{ 0.96f, 0.82f, 0.40f, 1.0f };
        constexpr Color leg{ 0.89f, 0.42f, 0.15f, 1.0f };
        // Course Eye is a traversal inspection, not the authored-world atlas.
        // Keep the actor, the lake, and the first recovery challenges large
        // enough to judge; a separate overview capture may still show all
        // ninety authored meters.
        constexpr float course_eye_focus_span_meters = 48.0f;
        // The physical Human graph is about 2.4 m tall while its production
        // art envelope extends farther. A 4.8 m framing proxy reduced the
        // default actor to a distant thumbnail on maximized/4K windows. This
        // conservative reference keeps every species on one world scale while
        // making the complete Human silhouette occupy roughly one third of
        // the Live viewport; wheel zoom remains an independent multiplier.
        constexpr float live_default_reference_height_m = 4.0f;

        [[nodiscard]] constexpr Color telemetry_color(telemetry::Tone tone) noexcept
        {
            switch (tone)
            {
            case telemetry::Tone::information: return accent;
            case telemetry::Tone::caution: return yellow;
            case telemetry::Tone::success: return green;
            case telemetry::Tone::danger: return danger;
            }
            return accent;
        }

        [[nodiscard]] constexpr std::string_view retained_guidance_label(
            sim::GuidanceMode mode) noexcept
        {
            return mode == sim::GuidanceMode::raw_policy_audit
                ? "RAW LEARNED" : "ASSISTED / TEACHER";
        }

        void fill_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color color)
        {
            ui_render::fill_rounded_rect(canvas, rect.position, rect.size, radius, color);
        }

        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill,
            Color outline = {}, float border_width = 0.0f)
        {
            ui_render::rounded_rect(canvas, rect.position, rect.size,
                radius, fill, outline, border_width);
        }

        void add_text(render::Canvas& canvas, Vec2 position,
            std::string_view text, float style_scale, Color color)
        {
            const font::BitmapFontMetrics metrics =
                font::make_bitmap_font_metrics(font_size(style_scale));
            Vec2 cursor = position;
            const float start_x = position.x;
            for (const char character : text)
            {
                if (character == '\n')
                {
                    cursor.x = start_x;
                    cursor.y += metrics.line_advance;
                    continue;
                }
                const font::BitmapGlyph glyph = font::default_glyph(character);
                for (std::uint32_t row = 0; row < font::glyph_height; ++row)
                {
                    std::uint32_t column = 0;
                    while (column < font::glyph_width)
                    {
                        while (column < font::glyph_width
                            && !font::pixel_on(glyph, column, row))
                            ++column;
                        if (column >= font::glyph_width)
                            break;
                        const std::uint32_t run_begin = column;
                        while (column < font::glyph_width
                            && font::pixel_on(glyph, column, row))
                            ++column;
                        const Vec2 minimum{
                            cursor.x + static_cast<float>(run_begin)
                                * metrics.cell_size,
                            cursor.y + static_cast<float>(row)
                                * metrics.cell_size
                        };
                        const Vec2 maximum{
                            cursor.x + static_cast<float>(column)
                                * metrics.cell_size,
                            minimum.y + metrics.cell_size
                        };
                        canvas.quad(minimum, maximum, color);
                    }
                }
                cursor.x += metrics.advance;
            }
        }

        void draw_oriented_pixel_art(render::Canvas& canvas,
            const art::PixelArt& art, Vec2 beginning, Vec2 ending,
            float thickness, float alpha = 1.0f, bool mirror_vertical = false,
            bool mirror_horizontal = false, bool fit_opaque_horizontal = false,
            Color opaque_tint = {}, float opaque_tint_strength = 0.0f)
        {
            const Vec2 delta = ending - beginning;
            const float span = length(delta);
            if (!art.loaded() || span <= 1.0f || thickness <= 1.0f)
                return;
            const art::OpaquePixelBounds opaque = art::opaque_pixel_bounds(art);
            if (!opaque.valid())
                return;

            const Vec2 axis = delta / span;
            Vec2 normal{ -axis.y, axis.x };
            if (mirror_vertical)
                normal = normal * -1.0f;

            auto point = [&](float u, float v) noexcept
            {
                const float opaque_u0 = static_cast<float>(opaque.left)
                    / static_cast<float>(art.width);
                const float opaque_u1 = static_cast<float>(opaque.right)
                    / static_cast<float>(art.width);
                const float visible_u = fit_opaque_horizontal
                    ? (u - opaque_u0) / (opaque_u1 - opaque_u0) : u;
                const float mapped_u = mirror_horizontal ? 1.0f - visible_u : visible_u;
                return beginning + axis * (mapped_u * span)
                    + normal * ((v - 0.5f) * thickness);
            };
            const float inverse_width = 1.0f / static_cast<float>(art.width);
            const float inverse_height = 1.0f / static_cast<float>(art.height);
            const auto presentation_color = [opaque_tint,
                opaque_tint_strength](Color color) noexcept
            {
                const auto quantize = [](float channel) noexcept
                {
                    return std::round(std::clamp(channel, 0.0f, 1.0f) * 7.0f)
                        / 7.0f;
                };
                color = art::opaque_depth_tint(
                    color, opaque_tint, opaque_tint_strength);
                color.r = quantize(color.r);
                color.g = quantize(color.g);
                color.b = quantize(color.b);
                return color;
            };
            const auto same_color = [](Color lhs, Color rhs) noexcept
            {
                return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b
                    && lhs.a == rhs.a;
            };
            const auto emit_rectangle = [&](int x_begin, int x_end,
                int y_begin, int y_end, Color color)
            {
                color.a *= alpha;
                const float u0 = static_cast<float>(x_begin) * inverse_width;
                const float u1 = static_cast<float>(x_end) * inverse_width;
                const float v0 = static_cast<float>(y_begin) * inverse_height;
                const float v1 = static_cast<float>(y_end) * inverse_height;
                const Vec2 p00 = point(u0, v0);
                const Vec2 p10 = point(u1, v0);
                const Vec2 p11 = point(u1, v1);
                const Vec2 p01 = point(u0, v1);
                canvas.triangle(p00, p10, p11, color);
                canvas.triangle(p00, p11, p01, color);
            };
            const std::size_t pixel_count = static_cast<std::size_t>(
                art.width * art.height);
            std::vector<Color> presented(pixel_count);
            std::vector<std::uint8_t> drawable(pixel_count, 0u);
            std::vector<std::uint8_t> consumed(pixel_count, 0u);
            for (int y = 0; y < art.height; ++y)
            {
                for (int x = 0; x < art.width; ++x)
                {
                    const std::size_t index = static_cast<std::size_t>(
                        y * art.width + x);
                    const Color source_color = art.pixels[index];
                    if (art.transparent(source_color))
                        continue;
                    presented[index] = presentation_color(source_color);
                    drawable[index] = 1u;
                }
            }
            for (int y = 0; y < art.height; ++y)
            {
                for (int x = 0; x < art.width; ++x)
                {
                    const std::size_t origin = static_cast<std::size_t>(
                        y * art.width + x);
                    if (drawable[origin] == 0u || consumed[origin] != 0u)
                        continue;
                    const Color color = presented[origin];
                    int maximum_width = 0;
                    while (x + maximum_width < art.width)
                    {
                        const std::size_t candidate = static_cast<std::size_t>(
                            y * art.width + x + maximum_width);
                        if (drawable[candidate] == 0u || consumed[candidate] != 0u
                            || !same_color(presented[candidate], color))
                            break;
                        ++maximum_width;
                    }
                    int best_width = maximum_width;
                    int best_height = 1;
                    int row_width = maximum_width;
                    for (int scan_y = y + 1; scan_y < art.height; ++scan_y)
                    {
                        int compatible_width = 0;
                        while (compatible_width < row_width)
                        {
                            const std::size_t candidate = static_cast<std::size_t>(
                                scan_y * art.width + x + compatible_width);
                            if (drawable[candidate] == 0u || consumed[candidate] != 0u
                                || !same_color(presented[candidate], color))
                                break;
                            ++compatible_width;
                        }
                        row_width = compatible_width;
                        if (row_width == 0)
                            break;
                        const int height = scan_y - y + 1;
                        if (row_width * height > best_width * best_height)
                        {
                            best_width = row_width;
                            best_height = height;
                        }
                    }
                    for (int fill_y = y; fill_y < y + best_height; ++fill_y)
                    {
                        for (int fill_x = x; fill_x < x + best_width; ++fill_x)
                        {
                            consumed[static_cast<std::size_t>(
                                fill_y * art.width + fill_x)] = 1u;
                        }
                    }
                    emit_rectangle(x, x + best_width, y, y + best_height, color);
                }
            }
        }

        [[nodiscard]] float fit_text_scale(std::string_view text, float requested_scale,
            float maximum_width,
            float minimum_scale = ui_layout::persistent_text_minimum_scale) noexcept
        {
            const float readable_minimum = std::max(minimum_scale,
                ui_layout::persistent_text_minimum_scale);
            float scale = std::max(requested_scale, readable_minimum);
            while (scale > readable_minimum
                && font::measure_text(text, font_size(scale)).x > maximum_width)
                scale -= 0.05f;
            return std::max(scale, readable_minimum);
        }

        void add_text_fit(render::Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float maximum_width,
            float minimum_scale = ui_layout::persistent_text_minimum_scale)
        {
            add_text(canvas, position, text,
                fit_text_scale(text, scale, maximum_width, minimum_scale), color);
        }

        float add_wrapped_text(render::Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float maximum_width, float line_gap = 5.0f)
        {
            const float advance = font::make_bitmap_font_metrics(
                font_size(scale)).line_advance + line_gap;
            float y = position.y;
            std::string line{};
            std::size_t cursor = 0;
            auto flush = [&]()
            {
                if (line.empty())
                    return;
                add_text(canvas, { position.x, y }, line, scale, color);
                line.clear();
                y += advance;
            };

            while (cursor < text.size())
            {
                if (text[cursor] == '\n')
                {
                    flush();
                    ++cursor;
                    continue;
                }
                while (cursor < text.size() && text[cursor] == ' ')
                    ++cursor;
                if (cursor >= text.size())
                    break;
                const std::size_t end = text.find_first_of(" \n", cursor);
                const std::size_t word_end = end == std::string_view::npos ? text.size() : end;
                const std::string_view word = text.substr(cursor, word_end - cursor);
                std::string candidate = line;
                if (!candidate.empty())
                    candidate.push_back(' ');
                candidate.append(word);
                if (!line.empty()
                    && font::measure_text(candidate, font_size(scale)).x > maximum_width)
                {
                    flush();
                    line.assign(word);
                }
                else
                {
                    line = std::move(candidate);
                }
                cursor = word_end;
            }
            flush();
            return y - position.y;
        }

        [[nodiscard]] std::string format_work_counter(
            std::string_view label, std::uint64_t completed,
            std::uint64_t required)
        {
            if (required == 0u || completed >= required)
                return std::format("{} READY", label);
            return std::format("{} {}/{}", label, completed, required);
        }

        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera_x,
            float pixels_per_meter,
            float ground_fraction = view_camera::live_ground_fraction) noexcept
        {
            const float ground_y = viewport.position.y + viewport.size.y * ground_fraction;
            return {
                viewport.position.x + viewport.size.x * 0.50f + (world.x - camera_x) * pixels_per_meter,
                ground_y - world.y * pixels_per_meter
            };
        }


    }

    struct Application::Impl
    {
        struct SpeciesArtBundle
        {
            art::PixelArt head{};
            art::PixelArt body{};
            art::PixelArt tail{};
            art::PixelArt upper_leg{};
            art::PixelArt lower_leg{};
            art::PixelArt foot{};

            [[nodiscard]] bool loaded() const noexcept
            {
                return head.loaded() || body.loaded() || tail.loaded()
                    || upper_leg.loaded() || lower_leg.loaded() || foot.loaded();
            }
        };

        enum class Mode : std::uint8_t { live, rig_lab };
        enum class RigPreset : std::uint8_t {
            scaffold, humanoid, biped, chicken, quadruped, crawler4, hexapod, monoped, custom
        };
        enum class RigPanelPage : std::uint8_t { presets, structure, motors, art, test };
        enum class LivePanelPage : std::uint8_t {
            summary, mission, view, totals, advanced
        };
        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };

        inline static const std::filesystem::path legacy_rig_path{ "creature.rig" };
        struct StartupRigSelection
        {
            sim::CreatureBlueprint blueprint{};
            std::string source_note{};
        };

        [[nodiscard]] static StartupRigSelection load_startup_rig()
        {
            const sim::CreatureSpeciesPaths paths = sim::creature_species_paths(
                sim::CreatureSpecies::human);
            StartupRigSelection selection{};
            selection.blueprint = sim::CreatureBlueprint::load_owned_or_default(
                sim::CreatureSpecies::human, paths.rig, legacy_rig_path,
                selection.source_note);
            return selection;
        }

        StartupRigSelection startup_rig{ load_startup_rig() };
        render::Canvas canvas{};
        std::vector<render::Vertex> presented_vertices{};
        bool presentation_scaled{};
        sim::CreatureBlueprint blueprint{ startup_rig.blueprint };
        rl::AutonomousTrainer trainer{ blueprint, 64 };
        std::optional<sim::Environment> course_eye_test_environment{};
        presentation::Session presentation_session{
            presentation::Session::interactive };
        diagnostics::WalkEyeTestProof walk_eye_test_proof{};
        rl::PolicyNetwork walk_eye_test_policy{ 0x7300u };
        std::optional<sim::Environment> walk_eye_replay_start{};
        struct WalkEyeTerminalSnapshot
        {
            bool frozen{};
            sim::TrialTerminalCause cause{ sim::TrialTerminalCause::none };
            sim::InvalidMotion reason{ sim::InvalidMotion::none };
            Vec2 position{};
            sim::TerrainRegion terrain{ sim::TerrainRegion::firm };
            float water_depth{};
        };
        WalkEyeTerminalSnapshot walk_eye_terminal{};
        std::uint64_t walk_eye_trial_id{ 1u };
        double walk_eye_accumulator_seconds{};
        Mode mode{ Mode::live };
        RigPreset rig_preset{ RigPreset::humanoid };
        JointTestGroup joint_test_group{ JointTestGroup::selected };
        RigPanelPage rig_panel_page{ RigPanelPage::presets };
        LivePanelPage live_panel_page{ LivePanelPage::summary };
        LivePanelPage rig_trainer_panel_page{ LivePanelPage::summary };
        ui_layout::RigLabWorkspacePage rig_lab_workspace_page{
            ui_layout::RigLabWorkspacePage::inspector };
        bool rig_lab_controls_expanded{};
        bool live_raw_telemetry_expanded{};
        bool rig_raw_telemetry_expanded{};
        float trainer_scroll_offset{};
        float trainer_content_height{ 1100.0f };
        float rig_trainer_scroll_offset{};
        float rig_trainer_content_height{ 720.0f };
        float rig_panel_scroll_offset{};
        float rig_panel_content_height{ 900.0f };
        int selected_node{ -1 };
        int selected_bone{ -1 };
        int selected_motor{};
        std::uint16_t director_browser_index{};
        bool dragging_node{};
        bool joint_auto_sweep{};
        bool right_leg_near{ true };
        bool rig_test_loose_ground{};
        sim::RigTestPattern rig_test_pattern{ sim::RigTestPattern::manual };
        std::optional<sim::Environment> rig_test_physical_environment{};
        sim::CourseStage rig_test_physical_stage{ sim::CourseStage::uneven };
        double rig_test_physical_accumulator_seconds{};
        std::uint64_t rig_test_physical_generation{};
        RigPreviewTerminalEvidence rig_test_terminal{};
        bool run_paused{};
        ui_layout::DistanceUnits distance_units{ ui_layout::DistanceUnits::imperial };
        float session_runtime_seconds{};
        float rig_lifetime_seconds{};
        struct SessionRigSample
        {
            std::uint64_t signature{};
            ui_layout::TrainingTotals latest{};
        };

        std::uint64_t tracked_rig_signature{};
        std::uint64_t rig_start_total_updates{};
        std::uint64_t rig_start_episodes{};
        std::uint64_t rig_start_valid_episodes{};
        std::uint64_t rig_start_invalid_episodes{};
        std::uint64_t rig_start_steps{};
        std::uint64_t rig_start_falls{};
        std::uint64_t rig_start_collisions{};
        std::uint64_t rig_start_obstacles{};
        double rig_start_distance{};
        std::uint8_t rig_best_stage{};
        ui_layout::TrainingTotals session_totals{};
        std::vector<SessionRigSample> session_rig_samples{};
        bool rig_edit_pending{};
        std::string rig_edit_reason{};
        float joint_test_input{};
        float joint_test_phase{};
        sim::WeaponClass editor_weapon_class{ sim::WeaponClass::carbine };
        float editor_target_distance{ 8.0f };
        float camera_x{};
        float live_pixels_per_meter{ view_camera::default_pixels_per_meter };
        float live_zoom_factor{ 1.0f };
        bool live_zoom_auto{ true };
        bool live_projection_initialized{};
        art::PixelArt original_runner_art{};
        std::filesystem::path art_layout_directory{};
        std::array<art::Layout, 4> art_layouts{
            art::default_layout(sim::CreatureSpecies::human),
            art::default_layout(sim::CreatureSpecies::chicken),
            art::default_layout(sim::CreatureSpecies::dog),
            art::default_layout(sim::CreatureSpecies::hexapod)
        };
        std::array<art::LayoutHistory, 4> art_layout_history{};
        art::Module selected_art_module{ art::Module::body };
        bool art_edit_pending{};
        bool art_preview_frozen{};
        std::optional<sim::Environment> frozen_art_environment{};
        float art_editor_zoom{ ui_layout::rig_lab_default_zoom };
        bool art_editor_high_contrast{};
        art::PixelArt optional_foot_art{};
        art::PixelArt optional_helmet_art{};
        art::PixelArt optional_torso_art{};
        art::PixelArt optional_upper_arm_art{};
        art::PixelArt optional_forearm_art{};
        art::PixelArt optional_hand_art{};
        art::PixelArt optional_thigh_art{};
        art::PixelArt optional_shin_art{};
        art::PixelArt optional_weapon_art{};
        SpeciesArtBundle chicken_art{};
        SpeciesArtBundle dog_art{};
        SpeciesArtBundle hexapod_art{};
        bool live_art_enabled{ false };
        bool rig_lab_art_enabled{ false };
        bool debug_skeleton_overlay{};
        std::string status{ "AUTOPILOT STARTING" };
        float status_time{ 4.0f };
        bool quit{};
        std::filesystem::path custom_rig_path{ "custom.rig" };
        std::filesystem::path persistence_directory{};
        std::filesystem::path autosave_policy_path{
            sim::creature_species_paths(sim::CreatureSpecies::human).autosave_checkpoint };
        std::filesystem::path autosave_rig_path{
            sim::creature_species_paths(sim::CreatureSpecies::human).evolved_rig };
        std::filesystem::path autosave_state_path{
            sim::creature_species_paths(sim::CreatureSpecies::human).autonomy_state };

        [[nodiscard]] bool art_enabled_for_mode() const noexcept
        {
            return mode == Mode::live ? live_art_enabled : rig_lab_art_enabled;
        }

        bool& art_toggle_for_mode() noexcept
        {
            return mode == Mode::live ? live_art_enabled : rig_lab_art_enabled;
        }

        [[nodiscard]] std::string_view preset_name() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::scaffold: return "SCAFFOLD";
            case RigPreset::humanoid: return "HUMAN";
            case RigPreset::biped: return "LEGACY BIPED";
            case RigPreset::chicken: return "CHICKEN";
            case RigPreset::quadruped: return "LEGACY QUADRUPED";
            case RigPreset::crawler4: return "DOG";
            case RigPreset::hexapod: return "HEXAPOD";
            case RigPreset::monoped: return "LEGACY MONOPED";
            case RigPreset::custom: return "CUSTOM / EVOLVED";
            }
            return "CUSTOM / EVOLVED";
        }

        [[nodiscard]] static std::optional<sim::CreatureSpecies> preset_species(
            RigPreset preset) noexcept
        {
            switch (preset)
            {
            case RigPreset::humanoid: return sim::CreatureSpecies::human;
            case RigPreset::chicken: return sim::CreatureSpecies::chicken;
            case RigPreset::crawler4: return sim::CreatureSpecies::dog;
            case RigPreset::hexapod: return sim::CreatureSpecies::hexapod;
            case RigPreset::custom: return sim::CreatureSpecies::custom;
            case RigPreset::scaffold:
            case RigPreset::biped:
            case RigPreset::quadruped:
            case RigPreset::monoped:
                return std::nullopt;
            }
            return std::nullopt;
        }

        [[nodiscard]] static RigPreset preset_for_species(
            sim::CreatureSpecies species) noexcept
        {
            switch (species)
            {
            case sim::CreatureSpecies::human: return RigPreset::humanoid;
            case sim::CreatureSpecies::chicken: return RigPreset::chicken;
            case sim::CreatureSpecies::dog: return RigPreset::crawler4;
            case sim::CreatureSpecies::hexapod: return RigPreset::hexapod;
            case sim::CreatureSpecies::custom: return RigPreset::custom;
            }
            return RigPreset::custom;
        }

        [[nodiscard]] static sim::CreatureBlueprint canonical_blueprint(
            RigPreset preset)
        {
            switch (preset)
            {
            case RigPreset::scaffold: return sim::CreatureBlueprint::scaffold();
            case RigPreset::humanoid: return sim::CreatureBlueprint::humanoid();
            case RigPreset::biped: return sim::CreatureBlueprint::biped();
            case RigPreset::chicken: return sim::CreatureBlueprint::chicken();
            case RigPreset::quadruped: return sim::CreatureBlueprint::quadruped();
            case RigPreset::crawler4: return sim::CreatureBlueprint::crawler4();
            case RigPreset::hexapod: return sim::CreatureBlueprint::hexapod();
            case RigPreset::monoped: return sim::CreatureBlueprint::monoped();
            case RigPreset::custom: return sim::CreatureBlueprint::scaffold();
            }
            return sim::CreatureBlueprint::scaffold();
        }

        [[nodiscard]] std::filesystem::path rig_file_for_preset(RigPreset preset) const
        {
            if (const auto species = preset_species(preset))
                return sim::creature_species_paths(*species).rig;
            return custom_rig_path;
        }

        [[nodiscard]] std::filesystem::path active_rig_path() const
        {
            return rig_file_for_preset(rig_preset);
        }

        void select_autosave_rig_file(const sim::CreatureBlueprint& candidate)
        {
            const sim::CreatureSpeciesPaths paths = sim::creature_species_paths(
                candidate.presentation_species());
            const auto persistent = [&](const std::filesystem::path& filename)
            {
                return persistence_directory.empty()
                    ? filename : persistence_directory / filename;
            };
            autosave_policy_path = persistent(paths.autosave_checkpoint);
            autosave_rig_path = persistent(paths.evolved_rig);
            autosave_state_path = persistent(paths.autonomy_state);
            trainer.set_autosave_paths(autosave_policy_path,
                autosave_rig_path, autosave_state_path);
        }

        [[nodiscard]] std::string format_speed(float meters_per_second) const
        {
            if (distance_units == ui_layout::DistanceUnits::metric)
                return std::format("{:.1f} KM/H", meters_per_second * 3.6f);
            return std::format("{:.1f} MPH", meters_per_second * 2.23693629f);
        }

        [[nodiscard]] std::string format_distance(float meters) const
        {
            if (!std::isfinite(meters))
                meters = 0.0f;
            if (distance_units == ui_layout::DistanceUnits::metric)
            {
                const float display_meters = std::abs(meters) < 0.05f
                    ? 0.0f : meters;
                return display_meters >= 1000.0f
                    ? std::format("{:.2f} KM", display_meters / 1000.0f)
                    : std::format("{:.1f} M", display_meters);
            }
            const float raw_feet = meters * 3.2808399f;
            const float feet = std::abs(raw_feet) < 0.5f
                ? 0.0f : raw_feet;
            return feet >= 5280.0f ? std::format("{:.2f} MI", feet / 5280.0f)
                : std::format("{:.0f} FT", feet);
        }

        [[nodiscard]] static std::string format_duration(float seconds)
        {
            const auto total = static_cast<std::uint64_t>(std::max(0.0f, seconds));
            const std::uint64_t hours = total / 3600u;
            const std::uint64_t minutes = (total / 60u) % 60u;
            const std::uint64_t remaining = total % 60u;
            return std::format("{:02}:{:02}:{:02}", hours, minutes, remaining);
        }

        [[nodiscard]] static ui_layout::TrainingTotals training_totals_from(
            const rl::TrainingMetrics& metrics,
            const rl::AutonomyStatus& autonomy) noexcept
        {
            return {
                metrics.total_updates,
                metrics.total_episodes,
                metrics.total_valid_episodes,
                metrics.total_invalid_episodes,
                metrics.total_resets,
                metrics.total_alternating_steps,
                metrics.total_falls,
                metrics.total_collisions,
                metrics.total_obstacles_passed,
                static_cast<std::uint64_t>(std::max(0, autonomy.rollback_count)),
                metrics.total_training_seconds,
                metrics.total_distance
            };
        }

        [[nodiscard]] static bool blueprint_connected(
            const sim::CreatureBlueprint& rig) noexcept
        {
            if (rig.nodes.empty())
                return false;
            std::vector<bool> visited(rig.nodes.size(), false);
            std::vector<std::uint16_t> stack{ rig.root_node };
            if (rig.root_node >= rig.nodes.size())
                return false;
            visited[rig.root_node] = true;
            while (!stack.empty())
            {
                const std::uint16_t node = stack.back();
                stack.pop_back();
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                    if (bone.a == node) next = bone.b;
                    else if (bone.b == node) next = bone.a;
                    if (next < visited.size() && !visited[next])
                    {
                        visited[next] = true;
                        stack.push_back(next);
                    }
                }
            }
            return std::ranges::all_of(visited, [](bool value) { return value; });
        }

        [[nodiscard]] float test_input_for_motor(std::size_t motor_index) const noexcept
        {
            const float phase = session_runtime_seconds * 2.0f * pi * 1.05f;
            if (rig_test_pattern != sim::RigTestPattern::gait)
                return sim::rig_test_motor_input(rig_test_pattern,
                    motor_index, phase, joint_test_input);
            const float swing = std::sin(phase);
            if (rig_preset == RigPreset::quadruped
                || rig_preset == RigPreset::crawler4)
            {
                const std::size_t support_leg = motor_index / 2u;
                        const bool phase_a = support_leg == 0u || support_leg == 3u;
                const float drive = phase_a ? swing : -swing;
                return (motor_index & 1u) == 0u
                    ? 0.52f * drive
                    : 0.48f * std::max(0.0f, drive)
                        - 0.20f * std::max(0.0f, -drive);
            }
            if (rig_preset == RigPreset::hexapod && motor_index < 6u)
            {
                const bool phase_a = motor_index == 0u
                    || motor_index == 2u || motor_index == 4u;
                return 0.55f * (phase_a ? swing : -swing);
            }
            return sim::rig_test_motor_input(rig_test_pattern,
                motor_index, phase, joint_test_input);
        }

        [[nodiscard]] std::string_view physical_scrub_name() const noexcept
        {
            if (rig_test_physical_stage == sim::CourseStage::equipment_targets)
                return "AIM";
            if (rig_test_pattern == sim::RigTestPattern::crouch)
                return "CROUCH";
            if (rig_test_pattern == sim::RigTestPattern::manual)
                return "MANUAL";
            return "GAIT";
        }

        [[nodiscard]] std::string_view physical_scrub_authority() const noexcept
        {
            return rig_test_pattern == sim::RigTestPattern::manual
                    && rig_test_physical_stage != sim::CourseStage::equipment_targets
                ? "DIRECT JOINT INPUT / ASSISTED STABILITY"
                : "ASSISTED TEACHER";
        }

        [[nodiscard]] static std::uint64_t physical_pose_signature(
            const sim::Environment& environment) noexcept
        {
            std::uint64_t hash = 1469598103934665603ULL;
            const auto mix = [&hash](std::uint64_t value) noexcept
            {
                hash ^= value;
                hash *= 1099511628211ULL;
            };
            mix(environment.blueprint().signature());
            for (const sim::Particle& particle : environment.particles())
            {
                mix(std::bit_cast<std::uint32_t>(particle.position.x));
                mix(std::bit_cast<std::uint32_t>(particle.position.y));
                mix(std::bit_cast<std::uint32_t>(particle.previous.x));
                mix(std::bit_cast<std::uint32_t>(particle.previous.y));
                mix(std::bit_cast<std::uint32_t>(particle.radius));
                mix(static_cast<std::uint64_t>(particle.grounded));
            }
            return hash;
        }

        void start_physical_rig_test(sim::CourseStage stage)
        {
            constexpr std::uint64_t physical_test_seed = 0x751A17u;
            ++rig_test_physical_generation;
            rig_test_terminal = {};
            rig_test_physical_stage = stage;
            rig_test_physical_environment.emplace(blueprint, physical_test_seed);
            sim::Environment& environment = *rig_test_physical_environment;
            environment.set_course(stage, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(sim::GuidanceMode::assisted);
            environment.set_physical_guidance_authority(1.0f);
            if (editor_weapon_class == sim::WeaponClass::none)
                environment.clear_equipment();
            else
                environment.configure_equipment(
                    editor_weapon_class, editor_target_distance);
            rig_test_physical_accumulator_seconds = 0.0;
            art_preview_frozen = false;
            frozen_art_environment.reset();
        }

        void step_physical_rig_test(float dt)
        {
            if (!rig_test_physical_environment.has_value()
                || rig_test_terminal.frozen
                || !std::isfinite(dt) || dt <= 0.0f)
                return;
            constexpr double fixed_step = 1.0 / 60.0;
            rig_test_physical_accumulator_seconds = std::min(0.25,
                rig_test_physical_accumulator_seconds
                    + static_cast<double>(dt));
            while (rig_test_physical_accumulator_seconds + 1.0e-8
                >= fixed_step)
            {
                sim::Environment& environment = *rig_test_physical_environment;
                std::array<float, sim::action_count> action{};
                // Rig Lab's gait/crouch buttons are an explicitly labelled
                // physical motor scrub, not a learned-policy preview.  Give the
                // deterministic teacher full authority so the canonical
                // Environment advances through complete opposing support
                // phases instead of relaxing both feet into the same standing
                // pose.  The production renderer still reads only the live
                // particles, contacts, and equipment anchors from this
                // Environment.
                if (rig_test_pattern == sim::RigTestPattern::manual
                    && rig_test_physical_stage
                        != sim::CourseStage::equipment_targets)
                {
                    // Manual range and sweep are applied to actual Environment
                    // motors. The environment remains explicitly Assisted for
                    // bounded stability, but there is no second pose or
                    // renderer-only command skeleton.
                    const std::size_t active = std::min<std::size_t>(
                        environment.blueprint().active_motor_count,
                        sim::anatomy_action_count);
                    for (std::size_t index = 0u; index < active; ++index)
                        if (test_motor_active(static_cast<int>(index)))
                            action[index] = test_input_for_motor(index);
                }
                else
                {
                    constexpr float authored_authority = 1.0f;
                    action = rl::effective_policy_action(
                        environment, action, rig_test_physical_stage,
                        authored_authority, sim::GuidanceMode::assisted);
                }
                const sim::StepResult result = environment.step(action,
                    static_cast<float>(fixed_step));
                if (result.terminated)
                {
                    const auto& terminal_particles = environment.particles();
                    const std::size_t root = environment.blueprint().root_node;
                    rig_test_terminal.frozen = true;
                    rig_test_terminal.cause = result.terminal_cause;
                    rig_test_terminal.reason = result.invalid_reason;
                    if (root < terminal_particles.size())
                    {
                        rig_test_terminal.position = terminal_particles[root].position;
                        rig_test_terminal.terrain = environment.terrain_region_at(
                            rig_test_terminal.position.x);
                    }
                    rig_test_terminal.water_depth = environment.water_depth();
                    rig_test_terminal.physical_test_generation =
                        rig_test_physical_generation;
                    rig_test_terminal.pose_signature =
                        physical_pose_signature(environment);
                    rig_test_physical_accumulator_seconds = 0.0;
                    set_status(std::format(
                        "RIG LAB TEST {} TERMINAL FROZEN - {} AT X {:.2f} M / CELL {} - RETRY OR R REQUIRED",
                        rig_test_physical_generation,
                        sim::trial_terminal_cause_name(
                            rig_test_terminal.cause, rig_test_terminal.reason),
                        rig_test_terminal.position.x,
                        sim::DeformableTerrain::global_cell_x(
                            rig_test_terminal.position.x)));
                    break;
                }
                rig_test_physical_accumulator_seconds -= fixed_step;
            }
            rig_test_physical_accumulator_seconds = std::max(
                0.0, rig_test_physical_accumulator_seconds);
        }

        [[nodiscard]] bool has_direct_bone(std::uint16_t a, std::uint16_t b) const noexcept
        {
            return std::ranges::any_of(blueprint.bones, [a, b](const sim::DistanceConstraint& bone)
            {
                return (bone.a == a && bone.b == b) || (bone.a == b && bone.b == a);
            });
        }

        [[nodiscard]] bool preview_rig_change()
        {
            sim::CreatureBlueprint candidate = blueprint;
            const std::size_t edited_node = selected_node >= 0
                ? static_cast<std::size_t>(selected_node)
                : std::numeric_limits<std::size_t>::max();
            if (!candidate.enforce_human_paired_segment_lengths(edited_node))
                return false;
            candidate.rebuild_rest_lengths();
            if (!candidate.valid())
                return false;
            return install_canonical_rig_preview(std::move(candidate));
        }

        void queue_rig_change(std::string_view reason)
        {
            rig_edit_pending = true;
            rig_edit_reason = reason;
            if (!preview_rig_change())
            {
                trainer.cancel_blueprint_preview();
                set_status("LIVE MORPHOLOGY REJECTED - CHECK NODE AND MOTOR CHAINS");
            }
        }

        [[nodiscard]] std::array<std::string_view, sim::anatomy_action_count> motor_names() const noexcept
        {
            std::array<std::string_view, sim::anatomy_action_count> names{};
            names.fill("UNUSED");
            switch (rig_preset)
            {
            case RigPreset::quadruped:
                names[0] = "REAR PHASE A HIP"; names[1] = "REAR PHASE A KNEE";
                names[2] = "REAR PHASE B HIP"; names[3] = "REAR PHASE B KNEE";
                names[4] = "FRONT PHASE B HIP"; names[5] = "FRONT PHASE B KNEE";
                names[6] = "FRONT PHASE A HIP"; names[7] = "FRONT PHASE A KNEE";
                break;
            case RigPreset::crawler4:
                names[0] = "REAR PHASE A HIP"; names[1] = "REAR PHASE A KNEE";
                names[2] = "REAR PHASE B HIP"; names[3] = "REAR PHASE B KNEE";
                names[4] = "FRONT PHASE B HIP"; names[5] = "FRONT PHASE B KNEE";
                names[6] = "FRONT PHASE A HIP"; names[7] = "FRONT PHASE A KNEE";
                break;
            case RigPreset::hexapod:
                names[0] = "REAR LEFT HIP"; names[1] = "REAR LEFT KNEE";
                names[2] = "REAR RIGHT HIP"; names[3] = "REAR RIGHT KNEE";
                names[4] = "FRONT LEFT HIP"; names[5] = "FRONT LEFT KNEE";
                names[6] = "FRONT RIGHT HIP"; names[7] = "FRONT RIGHT KNEE";
                break;
            case RigPreset::monoped:
                names[0] = "HIP"; names[1] = "KNEE";
                names[2] = "LEFT FOOT"; names[3] = "RIGHT FOOT";
                break;
            case RigPreset::humanoid:
                names[0] = "LEFT HIP"; names[1] = "LEFT KNEE";
                names[2] = "RIGHT HIP"; names[3] = "RIGHT KNEE";
                names[4] = "LEFT SHOULDER"; names[5] = "LEFT ELBOW";
                names[6] = "RIGHT SHOULDER"; names[7] = "RIGHT ELBOW";
                names[sim::human_left_ankle_action] = "LEFT ANKLE";
                names[sim::human_right_ankle_action] = "RIGHT ANKLE";
                break;
            case RigPreset::scaffold:
            case RigPreset::biped:
            case RigPreset::chicken:
                names[0] = "LEFT HIP"; names[1] = "LEFT KNEE";
                names[2] = "RIGHT HIP"; names[3] = "RIGHT KNEE";
                break;
            case RigPreset::custom:
                for (std::size_t index = 0u; index < names.size(); ++index)
                    names[index] = "CUSTOM MOTOR";
                break;
            }
            return names;
        }

        void set_status(std::string text)
        {
            status = std::move(text);
            status_time = 4.0f;
        }

        [[nodiscard]] InputState panel_input(Rect rect,
            const InputState& input) const noexcept
        {
            InputState result = input;
            result.wheel = 0.0f;
            if (contains(rect, input.mouse))
                return result;
            result.mouse = { -1.0e6f, -1.0e6f };
            result.left_down = false;
            result.left_pressed = false;
            result.left_released = false;
            return result;
        }

        void draw_panel_scrollbar(Rect rect, float content_height,
            float offset)
        {
            const ui_layout::ScrollbarLayout scrollbar =
                ui_layout::panel_scrollbar_layout(
                    { rect.position.x, rect.position.y,
                        rect.size.x, rect.size.y },
                    content_height, offset);
            if (!scrollbar.scrollable)
                return;
            add_rounded_rect(canvas,
                { { scrollbar.track.x, scrollbar.track.y },
                    { scrollbar.track.width, scrollbar.track.height } },
                2.0f, rgb(0x111923, 0.92f), border, 0.5f);
            add_rounded_rect(canvas,
                { { scrollbar.thumb.x, scrollbar.thumb.y },
                    { scrollbar.thumb.width, scrollbar.thumb.height } },
                2.0f, scrollbar.scrollable ? accent : muted);
        }

        void invalidate_rig_preview_snapshots() noexcept
        {
            art_preview_frozen = false;
            frozen_art_environment.reset();
            rig_test_physical_environment.reset();
            rig_test_physical_accumulator_seconds = 0.0;
            rig_test_terminal = {};
            rig_frame_projection.reset();
        }

        [[nodiscard]] bool install_canonical_rig_preview(
            sim::CreatureBlueprint candidate)
        {
            if (!candidate.valid())
                return false;
            invalidate_rig_preview_snapshots();
            if (!trainer.preview_blueprint(candidate))
                return false;
            blueprint = std::move(candidate);
            return trainer.preview().blueprint().signature()
                == blueprint.signature();
        }

        [[nodiscard]] bool commit_canonical_rig(
            sim::CreatureBlueprint candidate, bool preserve_policy)
        {
            if (!install_canonical_rig_preview(std::move(candidate)))
                return false;
            rig_preset = preset_for_species(blueprint.presentation_species());
            select_autosave_rig_file(blueprint);
            trainer.set_blueprint(blueprint, preserve_policy);
            return true;
        }

        void retry_walk_eye_replay(sim::GuidanceMode guidance)
        {
            if (!presentation::walk_eye(presentation_session)
                || !walk_eye_replay_start.has_value())
                return;
            walk_eye_replay_start->set_guidance_mode(guidance);
            if (course_eye_test_environment.has_value())
                *course_eye_test_environment = *walk_eye_replay_start;
            else
                course_eye_test_environment.emplace(*walk_eye_replay_start);
            walk_eye_accumulator_seconds = 0.0;
            walk_eye_terminal = {};
            ++walk_eye_trial_id;
            run_paused = false;

            const auto& replay = *course_eye_test_environment;
            const auto& particles = replay.particles();
            const std::size_t root = replay.blueprint().root_node;
            if (root < particles.size())
                camera_x = particles[root].position.x + 2.5f;
        }

        [[nodiscard]] bool button(Rect rect, std::string_view label, const InputState& input,
            bool active = false, bool enabled = true)
        {
            const bool hovered = contains(rect, input.mouse);
            Color fill = active ? accent_dim : panel_alt;
            if (hovered && enabled)
                fill = active ? rgb(0x1b7998) : rgb(0x1a2531);
            if (!enabled)
                fill = rgb(0x10151d);
            add_rounded_rect(canvas, rect, 8.0f, fill, active ? accent : border, 1.0f);

            float scale = label.find('\n') == std::string_view::npos
                ? 1.40f : ui_layout::persistent_text_minimum_scale;
            Vec2 measured = font::measure_text(label, font_size(scale));
            while ((measured.x > rect.size.x - 14.0f
                    || measured.y > rect.size.y - 6.0f)
                && scale > ui_layout::persistent_text_minimum_scale)
            {
                const float next_scale = std::max(
                    ui_layout::persistent_text_minimum_scale, scale - 0.06f);
                if (next_scale >= scale)
                    break;
                scale = next_scale;
                measured = font::measure_text(label, font_size(scale));
            }
            canvas.push_clip(rect.position + Vec2{ 2.0f, 2.0f },
                rect.position + rect.size - Vec2{ 2.0f, 2.0f });
            add_text(canvas,
                { rect.position.x + (rect.size.x - measured.x) * 0.5f,
                  rect.position.y + (rect.size.y - measured.y) * 0.5f },
                label, scale, enabled ? white : muted);
            canvas.pop_clip();
            return enabled && hovered && input.left_pressed;
        }

        float slider(Rect rect, std::string_view label, float value, float minimum, float maximum,
            const InputState& input, std::string_view suffix = {})
        {
            const float track_height = 16.0f;
            Rect track{ { rect.position.x,
                    rect.position.y + std::max(26.0f,
                        rect.size.y - track_height - 3.0f) },
                { rect.size.x, track_height } };
            add_rounded_rect(canvas, track, 5.0f, rgb(0x101820), border, 1.0f);
            float fraction = (value - minimum) / std::max(0.0001f, maximum - minimum);
            fraction = clamp(fraction, 0.0f, 1.0f);
            add_rounded_rect(canvas, { track.position, { track.size.x * fraction, track.size.y } }, 5.0f, accent);
            canvas.circle({ track.position.x + track.size.x * fraction,
                track.position.y + track.size.y * 0.5f }, 12.0f, white, 20);
            if (input.left_down && contains(rect, input.mouse))
            {
                fraction = clamp((input.mouse.x - track.position.x) / track.size.x, 0.0f, 1.0f);
                value = lerp(minimum, maximum, fraction);
            }
            const std::string value_text = std::format("{:.2f}{}", value, suffix);
            constexpr float value_scale = 1.40f;
            const float value_width = font::measure_text(
                value_text, font_size(value_scale)).x;
            const float label_width = std::max(40.0f,
                rect.size.x - value_width - 16.0f);
            add_text_fit(canvas, rect.position, label, 1.40f, muted,
                label_width, ui_layout::persistent_text_minimum_scale);
            add_text(canvas,
                { rect.position.x + rect.size.x - value_width, rect.position.y },
                value_text, value_scale, white);
            return value;
        }

        float angle_slider(Rect rect, std::string_view label, float radians, float minimum_degrees,
            float maximum_degrees, const InputState& input)
        {
            float degrees = radians * 180.0f / pi;
            degrees = slider(rect, label, degrees, minimum_degrees, maximum_degrees, input, " DEG");
            return degrees * pi / 180.0f;
        }

        void use_preset(RigPreset preset)
        {
            if (preset == RigPreset::custom)
                return;

            sim::CreatureBlueprint candidate = canonical_blueprint(preset);
            const std::string source_note{ "FACTORY DEFAULT" };

            selected_node = -1;
            selected_bone = -1;
            selected_motor = 0;
            dragging_node = false;
            // The worker command below is intentionally asynchronous. Commit
            // the same canonical blueprint to the production preview now so
            // an interactive selection or direct Rig Lab launch cannot label
            // Chicken/Dog/Hexapod controls while still drawing Human physics.
            // When the worker publishes the matching signature,
            // AutonomousTrainer promotes this preview without a second pose.
            if (!commit_canonical_rig(std::move(candidate), false))
            {
                set_status("CANONICAL RIG PREVIEW REJECTED - ACTIVE RIG UNCHANGED");
                return;
            }
            rig_preset = preset;
            set_status(std::format("{} LOADED FROM {} - FRESH BALANCE LESSON",
                preset_name(), source_note));
        }

        void apply_small_rig_change(std::string_view reason)
        {
            const std::size_t edited_node = selected_node >= 0
                ? static_cast<std::size_t>(selected_node)
                : std::numeric_limits<std::size_t>::max();
            if (!blueprint.enforce_human_paired_segment_lengths(edited_node))
            {
                trainer.cancel_blueprint_preview();
                set_status("RIG CHANGE REJECTED - HUMAN LIMB PAIRS MUST REMAIN VALID");
                return;
            }
            blueprint.rebuild_rest_lengths();
            if (!blueprint.valid())
            {
                trainer.cancel_blueprint_preview();
                set_status("RIG CHANGE REJECTED - CONNECT EACH ENABLED MOTOR THROUGH TWO REAL BONES");
                return;
            }
            const sim::CreatureSpecies species = blueprint.presentation_species();
            if (!blueprint.topology_compatible_with_species(species))
            {
                trainer.cancel_blueprint_preview();
                set_status("RIG CHANGE REJECTED - SUPPORT ROLES MUST BE UNIQUE, TERMINAL, AND SPECIES-CORRECT");
                return;
            }
            if (!commit_canonical_rig(blueprint, true))
            {
                set_status("RIG CHANGE REJECTED - CANONICAL PREVIEW COMMIT FAILED");
                return;
            }
            set_status(std::format(
                "{} - LIVE PREVIEW COMMITTED; {} REMAINS SPECIES-OWNED",
                reason, preset_name()));
        }

        [[nodiscard]] bool test_motor_active(int index) const noexcept
        {
            switch (joint_test_group)
            {
            case JointTestGroup::selected: return index == selected_motor;
            case JointTestGroup::pair_a: return index < 4;
            case JointTestGroup::pair_b: return index >= 4 && index < 8;
            case JointTestGroup::all: return true;
            }
            return false;
        }

        void draw_top_bar(const InputState& input, int width)
        {
            const ui_layout::Box top_bar = ui_layout::top_bar_box(static_cast<float>(width));
            canvas.quad({ top_bar.x, top_bar.y },
                { top_bar.x + top_bar.width, top_bar.y + top_bar.height }, rgb(0x0b1119));
            canvas.line({ 0.0f, top_bar.height - 1.0f },
                { static_cast<float>(width), top_bar.height - 1.0f }, 2.0f, border);
            add_text_fit(canvas, { 18.0f, 12.0f },
                "Epoch2DWalkEngine v" RUNNER_VERSION,
                1.62f, white, 420.0f,
                ui_layout::persistent_text_minimum_scale);
            add_text_fit(canvas, { 19.0f, 46.0f },
                "PHYSICAL LOCOMOTION STUDIO",
                1.00f, muted, std::max(365.0f,
                    static_cast<float>(width) - 760.0f),
                ui_layout::persistent_text_minimum_scale);

            const float live_tab_width = width >= 1500 ? 150.0f : 132.0f;
            const float rig_tab_width = width >= 1500 ? 164.0f : 150.0f;
            const float start_x = static_cast<float>(width)
                - live_tab_width - rig_tab_width - 16.0f;
            if (button({ { start_x, 15.0f },
                    { live_tab_width - 6.0f, 48.0f } },
                presentation::isolated(presentation_session)
                    ? "PROOF" : "LIVE",
                input, mode == Mode::live, !presentation::isolated(presentation_session)))
            {
                mode = Mode::live;
                live_panel_page = LivePanelPage::summary;
                trainer_scroll_offset = 0.0f;
                live_raw_telemetry_expanded = false;
            }
            if (button({ { start_x + live_tab_width, 15.0f },
                    { rig_tab_width - 6.0f, 48.0f } },
                "RIG LAB", input, mode == Mode::rig_lab,
                !presentation::isolated(presentation_session)))
                mode = Mode::rig_lab;
        }

        void draw_rig_lab_workspace_bar(
            Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 10.0f, panel, border, 1.0f);
            const ui_layout::RigLabWorkspaceNavigationLayout navigation =
                ui_layout::rig_lab_workspace_navigation_layout({
                    rect.position.x, rect.position.y,
                    rect.size.x, rect.size.y });
            const auto rect_from_box = [](ui_layout::Box box) noexcept
            {
                return Rect{ { box.x, box.y }, { box.width, box.height } };
            };
            const Rect trainer_button = rect_from_box(
                navigation.trainer_button);
            const Rect inspector_button = rect_from_box(
                navigation.inspector_button);
            const Rect controls_button = rect_from_box(
                navigation.controls_button);

            // Resolve the workspace before drawing its active state and before
            // the selected panel is laid out. One click therefore cannot land
            // on, or mutate, a control in the other workspace underneath it.
            if (input.left_pressed && contains(trainer_button, input.mouse))
            {
                rig_lab_workspace_page = ui_layout::RigLabWorkspacePage::trainer;
                rig_lab_controls_expanded = false;
            }
            else if (input.left_pressed && contains(inspector_button, input.mouse))
            {
                rig_lab_workspace_page = ui_layout::RigLabWorkspacePage::inspector;
                rig_lab_controls_expanded = false;
            }

            const rl::AutonomyStatus autonomy = trainer.autonomy_status();
            const telemetry::LessonProgress progress =
                telemetry::lesson_progress(autonomy);
            const std::string lesson_name = autonomy.stage
                    == sim::CourseStage::uneven
                ? std::format("{} / {}",
                    sim::course_stage_name(autonomy.stage),
                    sim::gait_task_name(autonomy.gait_task))
                : std::string{ sim::course_stage_name(autonomy.stage) };
            add_rounded_rect(canvas, rect_from_box(navigation.mission), 8.0f,
                panel_alt, border, 1.0f);
            add_text_fit(canvas,
                { navigation.mission.x + 10.0f,
                    navigation.mission.y + 12.0f },
                std::format("{} / {:.0f}%", lesson_name,
                    progress.overall * 100.0f),
                ui_layout::persistent_text_minimum_scale, accent,
                std::max(0.0f, navigation.mission.width - 20.0f),
                ui_layout::persistent_text_minimum_scale);

            if (button(rect_from_box(navigation.run_button),
                trainer.background_enabled() ? "PAUSE TRAIN" : "START TRAIN",
                input, trainer.background_enabled()))
            {
                trainer.set_background_enabled(!trainer.background_enabled());
                set_status(trainer.background_enabled()
                    ? "BACKGROUND TRAINING RESUMED"
                    : "BACKGROUND TRAINING PAUSED");
            }
            if (button(controls_button,
                rig_lab_controls_expanded ? "CLOSE" : "SETUP",
                input, rig_lab_controls_expanded))
                rig_lab_controls_expanded = !rig_lab_controls_expanded;

            InputState display_input = input;
            display_input.left_pressed = false;
            static_cast<void>(button(trainer_button, "TRAINER", display_input,
                rig_lab_workspace_page
                    == ui_layout::RigLabWorkspacePage::trainer));
            static_cast<void>(button(inspector_button, "INSPECTOR",
                display_input, rig_lab_workspace_page
                    == ui_layout::RigLabWorkspacePage::inspector));
        }

        void draw_rig_lab_control_drawer(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            const ui_layout::RigLabControlDrawerLayout layout =
                ui_layout::rig_lab_control_drawer_layout({
                    rect.position.x, rect.position.y, rect.size.x, rect.size.y });
            const auto rect_from_box = [](ui_layout::Box box) noexcept
            {
                return Rect{ { box.x, box.y }, { box.width, box.height } };
            };
            add_rounded_rect(canvas, rect_from_box(layout.card), 12.0f,
                panel_alt, accent_dim, 1.0f);
            add_text_fit(canvas, { layout.heading.x, layout.heading.y },
                "TRAIN SETUP", ui_layout::workspace_heading_text_scale,
                white, layout.heading.width,
                ui_layout::persistent_text_minimum_scale);

            const rl::AutonomyStatus autonomy = trainer.autonomy_status();
            constexpr director::TaskGraph task_graph =
                director::default_training_graph();
            constexpr std::array gait_tasks{
                sim::GaitTask::walk, sim::GaitTask::speed_walk,
                sim::GaitTask::walk_run_transition, sim::GaitTask::run };
            constexpr std::array<std::uint16_t, 4> task_indices{
                1u, 7u, 8u, 9u };
            constexpr std::array<std::string_view, 4> gait_labels{
                "WALK", "SPEED WALK", "WALK / RUN", "RUN" };
            for (std::size_t index = 0; index < gait_tasks.size(); ++index)
            {
                const std::uint16_t task_index = task_indices[index];
                const director::TaskNode& task = task_graph.nodes[task_index];
                if (button(rect_from_box(layout.gait_buttons[index]),
                    gait_labels[index], input,
                    autonomy.director_stable_task_id == task.stable_id))
                {
                    director_browser_index = task_index;
                    trainer.select_director_task(task_index);
                    set_status(std::format("GAIT REQUEST - {}",
                        director::task_name(task.task)));
                }
            }

            const sim::GuidanceMode active_guidance =
                trainer.preview_guidance_mode();
            const bool raw_audit = active_guidance
                == sim::GuidanceMode::raw_policy_audit;
            if (button(rect_from_box(layout.authority_button),
                raw_audit ? "AUTHORITY: RAW AUDIT" : "AUTHORITY: ASSISTED",
                input, raw_audit))
            {
                const sim::GuidanceMode new_mode = raw_audit
                    ? sim::GuidanceMode::assisted
                    : sim::GuidanceMode::raw_policy_audit;
                trainer.set_preview_guidance_mode(new_mode);
                trainer.reset_preview();
                set_status(new_mode == sim::GuidanceMode::raw_policy_audit
                    ? "RAW POLICY AUDIT ENABLED"
                    : "ASSISTED GUIDANCE ENABLED");
            }

            const auto profile_label = [](director::Profile profile) noexcept
                -> std::string_view
            {
                switch (profile)
                {
                case director::Profile::autonomous_npc: return "AUTONOMOUS NPC";
                case director::Profile::player_guided: return "PLAYER GUIDED";
                case director::Profile::curriculum: return "CURRICULUM";
                }
                return "CURRICULUM";
            };
            const director::Profile next_profile = autonomy.director_profile
                    == director::Profile::curriculum
                ? director::Profile::autonomous_npc
                : autonomy.director_profile == director::Profile::autonomous_npc
                    ? director::Profile::player_guided
                    : director::Profile::curriculum;
            if (button(rect_from_box(layout.profile_button),
                std::format("PROFILE: {}", profile_label(autonomy.director_profile)),
                input, true))
            {
                trainer.set_director_profile(next_profile);
                set_status(std::format("DIRECTOR PROFILE - {}",
                    profile_label(next_profile)));
            }
            if (button(rect_from_box(layout.rig_art_button),
                rig_lab_art_enabled ? "RIG ART ON" : "RIG ART OFF",
                input, rig_lab_art_enabled))
                rig_lab_art_enabled = !rig_lab_art_enabled;
        }

        void draw_course_ground(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - sim::DeformableTerrain::macro_tile_size;
            const float right = camera + half_view + sim::DeformableTerrain::macro_tile_size;

            auto material_color = [](sandhybrid::Material material)
            {
                const sandhybrid::Rgb8 source = sandhybrid::material_editor_color(
                    static_cast<std::uint32_t>(material));
                const std::uint32_t packed = (static_cast<std::uint32_t>(source.r) << 16u)
                    | (static_cast<std::uint32_t>(source.g) << 8u)
                    | static_cast<std::uint32_t>(source.b);
                return rgb(packed);
            };
            auto draw_world_color = [&](float x0, float y0, float x1, float y1,
                Color color)
            {
                const Vec2 minimum = world_to_screen({ x0, y0 }, viewport, camera, scale);
                const Vec2 maximum = world_to_screen({ x1, y1 }, viewport, camera, scale);
                canvas.quad({ minimum.x, maximum.y }, { maximum.x, minimum.y }, color);
            };

            if (!sim::stage_uses_deformable_terrain(environment.course_stage()))
            {
                const Vec2 surface = world_to_screen({ camera, 0.0f }, viewport, camera, scale);
                canvas.quad({ viewport.position.x, surface.y },
                    viewport.position + viewport.size, rgb(0x4d392c));

                const float cell = sim::DeformableTerrain::fine_cell_spacing;
                const int first_cell = static_cast<int>(std::floor(left / cell));
                const int last_cell = static_cast<int>(std::ceil(right / cell));
                for (int column = first_cell; column <= last_cell; ++column)
                {
                    const float x0 = static_cast<float>(column) * cell;
                    const std::uint32_t hash = static_cast<std::uint32_t>(column) * 2654435761u;
                    const Color upper = (hash & 1u) == 0u
                        ? rgb(0x77543a) : rgb(0x6b4a34);
                    const Color lower = (hash & 2u) == 0u
                        ? rgb(0x604330) : rgb(0x593d2d);
                    draw_world_color(x0, -cell, x0 + cell, 0.0f, upper);
                    draw_world_color(x0, -cell * 2.0f, x0 + cell, -cell, lower);
                }
                return;
            }

            const float surface_step = sim::DeformableTerrain::fine_cell_spacing;
            const float first_surface_x =
                (std::floor(left / surface_step + 0.5f) - 0.5f) * surface_step;
            const float viewport_bottom = viewport.position.y + viewport.size.y;
            const Color underground_open_air = rgb(0x09101a);
            for (float x = first_surface_x; x < right; x += surface_step)
            {
                const float next_x = x + surface_step;
                const float center_x = x + 0.5f * surface_step;
                const float ground = environment.ground_height_at(center_x);
                const Vec2 surface_a = world_to_screen(
                    { x, ground }, viewport, camera, scale);
                const Vec2 surface_b = world_to_screen(
                    { next_x, ground }, viewport, camera, scale);
                const Vec2 bottom_a{ surface_a.x, viewport_bottom };
                const Vec2 bottom_b{ surface_b.x, viewport_bottom };
                // Canonical Empty and Atmosphere are open space. Paint the
                // deep fallback with the same dark air as the world, then let
                // only occupied SceneCells cover it below. The former brown
                // slab made every authored room, pipe and cavern look filled.
                canvas.triangle(surface_a, surface_b, bottom_b,
                    underground_open_air);
                canvas.triangle(surface_a, bottom_b, bottom_a,
                    underground_open_air);

                // The visible material band is clipped to the exact sampled
                // collision surface. Raw fine-cell bottoms never render.
                const float band_depth = 0.30f;
                const Vec2 band_a = world_to_screen(
                    { x, ground - band_depth }, viewport, camera, scale);
                const Vec2 band_b = world_to_screen(
                    { next_x, ground - band_depth }, viewport, camera, scale);
                const Color band_color = material_color(
                    environment.terrain_surface_material_at(center_x));
                canvas.triangle(surface_a, surface_b, band_b, band_color);
                canvas.triangle(surface_a, band_b, band_a, band_color);
            }

            // Draw the canonical 16-byte SandHybrid cells, not a decorative
            // surface ribbon. Only the camera-visible rows and columns are
            // visited; physics and presentation read the same cell storage.
            const float ground_screen_y = viewport.position.y
                + viewport.size.y * view_camera::live_ground_fraction;
            const float visible_world_top =
                (ground_screen_y - viewport.position.y) / scale;
            const float visible_world_bottom =
                (ground_screen_y - (viewport.position.y + viewport.size.y)) / scale;
            const int first_row = std::clamp(static_cast<int>(std::floor(
                (visible_world_bottom - sim::DeformableTerrain::world_bottom)
                    / surface_step)), 0,
                static_cast<int>(sim::DeformableTerrain::vertical_cell_count) - 1);
            const int last_row = std::clamp(static_cast<int>(std::ceil(
                (visible_world_top - sim::DeformableTerrain::world_bottom)
                    / surface_step)), 0,
                static_cast<int>(sim::DeformableTerrain::vertical_cell_count) - 1);
            const int first_column = static_cast<int>(std::floor(left / surface_step));
            const int last_column = static_cast<int>(std::ceil(right / surface_step));
            for (int row = first_row; row <= last_row; ++row)
            {
                bool run_active = false;
                int run_begin = first_column;
                float run_fill = 0.0f;
                sandhybrid::Material run_material = sandhybrid::Material::empty;
                for (int signed_column = first_column;
                    signed_column <= last_column + 1; ++signed_column)
                {
                    float fill = 0.0f;
                    sandhybrid::Material material = sandhybrid::Material::empty;
                    bool occupied = false;
                    if (signed_column <= last_column)
                    {
                        const std::size_t column =
                            sim::DeformableTerrain::wrap_column(
                                static_cast<std::ptrdiff_t>(signed_column));
                        const sim::DeformableTerrain::FineCell& cell =
                            environment.terrain().fine_cell(column,
                                static_cast<std::size_t>(row));
                        fill = cell.fill;
                        material = cell.material();
                        occupied = fill > 1.0e-6f
                            && material != sandhybrid::Material::empty
                            && material != sandhybrid::Material::atmosphere;
                    }
                    if (run_active && occupied && material == run_material
                        && std::abs(fill - run_fill) <= 1.0e-6f)
                        continue;
                    if (run_active)
                    {
                        const float x0 = static_cast<float>(run_begin) * surface_step;
                        const float x1 = static_cast<float>(signed_column) * surface_step;
                        const float y0 = sim::DeformableTerrain::row_world_bottom(
                            static_cast<std::size_t>(row));
                        draw_world_color(x0, y0, x1,
                            y0 + surface_step * run_fill,
                            material_color(run_material));
                    }
                    run_active = occupied;
                    run_begin = signed_column;
                    run_fill = fill;
                    run_material = material;
                }
            }

            for (float x = first_surface_x; x < right; x += surface_step)
            {
                const float next_x = x + surface_step;
                const float center_x = x + 0.5f * surface_step;
                const float ground = environment.ground_height_at(center_x);
                const Vec2 surface_a = world_to_screen(
                    { x, ground }, viewport, camera, scale);
                const Vec2 surface_b = world_to_screen(
                    { next_x, ground }, viewport, camera, scale);
                canvas.line(surface_a, surface_b, 1.25f, rgb(0xc59a65, 0.92f));
            }

            const float water_step = sim::DeformableTerrain::fine_cell_spacing;
            for (float x = first_surface_x; x <= right; x += water_step)
            {
                const float next_x = x + water_step;
                const float center_x = x + 0.5f * water_step;
                const float depth = environment.water_depth_at(center_x);
                if (depth <= 0.002f)
                    continue;
                const float ground = environment.ground_height_at(center_x);
                const float surface = environment.water_surface_at(center_x);
                draw_world_color(x, ground, next_x, surface,
                    rgb(0x2479a8, 0.62f));
                const Vec2 a = world_to_screen({ x, surface }, viewport, camera, scale);
                const Vec2 b = world_to_screen({ next_x, surface }, viewport, camera, scale);
                canvas.line(a, b, 1.5f, rgb(0x66d4ef, 0.90f));
            }
        }

        void draw_terrain_challenges(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            if (!sim::stage_uses_deformable_terrain(environment.course_stage())
                || viewport.size.y < 260.0f || scale < 12.0f)
                return;
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view;
            const float right = camera + half_view;
            const int first_cycle = static_cast<int>(std::floor(
                left / sim::DeformableTerrain::period));
            const int last_cycle = static_cast<int>(std::floor(
                right / sim::DeformableTerrain::period));
            const auto challenges = environment.terrain().terrain_challenges();
            std::vector<Rect> placed_labels{};
            placed_labels.reserve(challenges.size()
                * static_cast<std::size_t>(std::max(1,
                    last_cycle - first_cycle + 1)));
            for (int cycle = first_cycle; cycle <= last_cycle; ++cycle)
            {
                const float cycle_offset = static_cast<float>(cycle)
                    * sim::DeformableTerrain::period;
                for (const sim::TerrainChallenge& challenge : challenges)
                {
                    const float begin = challenge.begin + cycle_offset;
                    const float end = challenge.end + cycle_offset;
                    if (end < left || begin > right)
                        continue;
                    const float visible_begin = std::max(begin, left);
                    const float visible_end = std::min(end, right);
                    if ((visible_end - visible_begin) * scale < 72.0f)
                        continue;
                    const float anchor_x = std::clamp(challenge.center() + cycle_offset,
                        visible_begin + sim::DeformableTerrain::fine_cell_spacing,
                        visible_end - sim::DeformableTerrain::fine_cell_spacing);
                    const float water = environment.water_depth_at(anchor_x);
                    const float anchor_y = water > 0.002f
                        ? environment.water_surface_at(anchor_x)
                        : environment.ground_height_at(anchor_x);
                    const Vec2 anchor = world_to_screen(
                        { anchor_x, anchor_y }, viewport, camera, scale);
                    constexpr float label_width = 154.0f;
                    constexpr float label_height = 21.0f;
                    constexpr std::size_t label_lane_count = 6u;
                    constexpr float label_lane_step = 27.0f;
                    constexpr float label_gap = 5.0f;
                    Rect sign{};
                    bool label_placed{};
                    for (std::size_t attempt = 0u;
                        attempt < label_lane_count; ++attempt)
                    {
                        const std::size_t lane = (static_cast<std::size_t>(
                            challenge.sequence) + attempt) % label_lane_count;
                        const float horizontal_nudge = (lane & 1u) == 0u
                            ? -8.0f : 8.0f;
                        const float label_x = clamp(anchor.x
                                - label_width * 0.5f + horizontal_nudge,
                            viewport.position.x + 6.0f,
                            viewport.position.x + viewport.size.x
                                - label_width - 6.0f);
                        const float lift = 38.0f
                            + static_cast<float>(lane) * label_lane_step;
                        const float label_y = clamp(
                            anchor.y - lift - label_height,
                            viewport.position.y + 8.0f,
                            viewport.position.y + viewport.size.y
                                - label_height - 8.0f);
                        const Rect candidate{ { label_x, label_y },
                            { label_width, label_height } };
                        const bool conflict = std::ranges::any_of(
                            placed_labels, [&](const Rect& placed)
                            {
                                return candidate.position.x
                                        < placed.position.x
                                            + placed.size.x + label_gap
                                    && candidate.position.x
                                            + candidate.size.x + label_gap
                                        > placed.position.x
                                    && candidate.position.y
                                        < placed.position.y
                                            + placed.size.y + label_gap
                                    && candidate.position.y
                                            + candidate.size.y + label_gap
                                        > placed.position.y;
                            });
                        if (conflict)
                            continue;
                        sign = candidate;
                        label_placed = true;
                        break;
                    }
                    // Keep the physical anchor in world space, but omit a
                    // bubble when every deterministic lane is occupied. A
                    // crossed/floating tag is worse evidence than no tag.
                    if (!label_placed)
                        continue;
                    placed_labels.push_back(sign);
                    const Color tone = challenge.region == sim::TerrainRegion::firm
                        ? accent : challenge.region == sim::TerrainRegion::shallow_water
                            ? rgb(0x66d4ef) : yellow;
                    const Vec2 tether{ clamp(anchor.x, sign.position.x + 5.0f,
                        sign.position.x + sign.size.x - 5.0f),
                        sign.position.y + sign.size.y };
                    canvas.line(anchor, tether, 1.15f, tone);
                    add_rounded_rect(canvas, sign, 4.0f,
                        rgb(0x102431, 0.92f), tone, 1.0f);
                    add_text_fit(canvas, sign.position + Vec2{ 5.0f, 4.0f },
                        std::format("C{:02}  {}", challenge.sequence + 1u,
                            sim::terrain_region_name(challenge.region)),
                        0.68f, tone, sign.size.x - 10.0f, 0.58f);
                }
            }
        }

        void draw_course_reference(const sim::Environment& environment, Rect viewport,
            float camera, float scale, bool detailed)
        {
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - 2.0f;
            const float right = camera + half_view + 2.0f;
            if (environment.shuttle_enabled())
            {
                constexpr std::array boundaries{
                    sim::shuttle_left_boundary, sim::shuttle_right_boundary };
                for (std::size_t index = 0; index < boundaries.size(); ++index)
                {
                    const float x = boundaries[index];
                    const float ground = environment.ground_height_at(x);
                    const Vec2 base = world_to_screen(
                        { x, ground }, viewport, camera, scale);
                    const Vec2 top = world_to_screen(
                        { x, ground + 0.82f }, viewport, camera, scale);
                    canvas.line(base, top, 3.5f, accent_dim);
                    if (!detailed)
                        continue;
                    const Rect sign{ top + Vec2{ -58.0f, -26.0f },
                        { 116.0f, 26.0f } };
                    add_rounded_rect(canvas, sign, 5.0f,
                        rgb(0x102431, 0.97f), accent, 1.0f);
                    add_text_fit(canvas, sign.position + Vec2{ 6.0f, 5.0f },
                        index == 0u ? "TURN A" : "TURN B",
                        0.82f, white, sign.size.x - 12.0f, 0.72f);
                }
                if (detailed)
                    return;
            }
            const float marker_spacing = ui_layout::course_reference_marker_spacing_m(distance_units);
            const int first_marker = static_cast<int>(std::floor(left / marker_spacing));
            const int last_marker = static_cast<int>(std::ceil(right / marker_spacing));
            for (int index = first_marker; index <= last_marker; ++index)
            {
                if (!ui_layout::course_reference_marker_visible(
                        index, detailed))
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;
                const float x = distance;
                const float ground = environment.ground_height_at(x);
                const Vec2 base = world_to_screen({ x, ground }, viewport, camera, scale);
                const Vec2 tick = world_to_screen({ x, ground + 0.18f },
                    viewport, camera, scale);
                canvas.line(base, tick, 2.0f, accent_dim);
                Vec2 sign_position = base + Vec2{
                    (index == 0 ? -88.0f : -41.0f)
                        + ui_layout::course_reference_marker_label_offset_pixels(index),
                    -23.0f };
                sign_position.x = clamp(sign_position.x,
                    viewport.position.x + 6.0f,
                    viewport.position.x + viewport.size.x - 88.0f);
                const Rect sign{ sign_position, { 82.0f, 22.0f } };
                canvas.line(base, sign.position + Vec2{
                    index == 0 ? sign.size.x : sign.size.x * 0.5f,
                    sign.size.y }, 1.25f, accent_dim);
                add_rounded_rect(canvas, sign, 4.0f,
                    rgb(0x102431, 0.97f), accent, 1.0f);
                const std::string marker_label = index == 0 ? "START"
                    : distance_units == ui_layout::DistanceUnits::metric
                        ? (distance >= 1000.0f
                            ? std::format("{:.2f} KM", distance / 1000.0f)
                            : std::format("{:.0f} M", distance))
                        : (distance >= 1609.344f
                            ? std::format("{:.2f} MI", distance / 1609.344f)
                            : std::format("{:.0f} FT", distance * 3.2808399f));
                add_text_fit(canvas, sign.position + Vec2{ 5.0f, 4.0f },
                    marker_label, 0.72f, white, sign.size.x - 10.0f, 0.64f);
            }
        }

        [[nodiscard]] static std::size_t art_species_index(
            sim::CreatureSpecies species) noexcept
        {
            switch (species)
            {
            case sim::CreatureSpecies::human: return 0u;
            case sim::CreatureSpecies::chicken: return 1u;
            case sim::CreatureSpecies::dog: return 2u;
            case sim::CreatureSpecies::hexapod: return 3u;
            case sim::CreatureSpecies::custom: return 0u;
            }
            return 0u;
        }

        [[nodiscard]] art::Layout& active_art_layout() noexcept
        {
            return art_layouts[art_species_index(
                blueprint.presentation_species())];
        }

        [[nodiscard]] art::LayoutHistory& active_art_history() noexcept
        {
            return art_layout_history[art_species_index(
                blueprint.presentation_species())];
        }

        [[nodiscard]] std::filesystem::path active_art_layout_path() const
        {
            std::string_view stem = "human";
            switch (blueprint.presentation_species())
            {
            case sim::CreatureSpecies::chicken: stem = "chicken"; break;
            case sim::CreatureSpecies::dog: stem = "dog"; break;
            case sim::CreatureSpecies::hexapod: stem = "hexapod"; break;
            case sim::CreatureSpecies::human:
            case sim::CreatureSpecies::custom: break;
            }
            return art_layout_directory / std::format("{}.artlayout", stem);
        }

        void draw_authored_pixel_art(sim::CreatureSpecies species,
            art::Module module, const art::PixelArt& sprite,
            Vec2 beginning, Vec2 ending, float thickness, float alpha = 1.0f,
            bool flip_vertical = false, bool flip_horizontal = false,
            bool fit_opaque_horizontal = false,
            Color opaque_tint = {}, float opaque_tint_strength = 0.0f)
        {
            const art::Layout& layout = art_layouts[art_species_index(species)];
            const art::AdjustedTransform adjusted = art::adjust_transform(
                beginning, ending, thickness, layout.at(module),
                flip_vertical, flip_horizontal);
            float depth_alpha = alpha;
            if (adjusted.layer < 0)
                depth_alpha *= adjusted.layer == -1 ? 0.80f : 0.62f;
            else if (adjusted.layer > 0)
                depth_alpha = std::min(1.0f, depth_alpha
                    + static_cast<float>(adjusted.layer) * 0.03f);
            draw_oriented_pixel_art(canvas, sprite,
                adjusted.beginning, adjusted.ending, adjusted.thickness,
                depth_alpha, adjusted.flip_vertical,
                adjusted.flip_horizontal, fit_opaque_horizontal,
                opaque_tint, opaque_tint_strength);
        }

        void draw_equipment(const sim::Environment& environment, Rect viewport,
            float camera, float scale, bool held_pass)
        {
            if (environment.weapon_class() == sim::WeaponClass::none)
                return;
            const bool loose = environment.equipment_state()
                == sim::EquipmentState::dropped
                || environment.equipment_state() == sim::EquipmentState::disarmed;
            if (held_pass == loose)
                return;
            const Vec2 mount = environment.equipment_display_position();
            const Vec2 direction = environment.equipment_display_direction();
            const float alpha = loose ? 0.86f
                : environment.equipment_state() == sim::EquipmentState::ready
                    ? 0.98f : 0.90f;
            if (art_enabled_for_mode() && optional_weapon_art.loaded())
            {
                const sim::WeaponGeometry geometry = sim::weapon_geometry(
                    environment.weapon_class());
                const float weapon_length = geometry.overall_length;
                const float rear_grip_fraction = weapon_length > 1.0e-5f
                    ? geometry.butt_to_rear_grip / weapon_length : 0.0f;
                const float art_pixel_scale = art::presentation_pixel_scale(scale);
                const float thickness = std::clamp(scale * 0.24f,
                    art::scaled_pixels(16.0f, art_pixel_scale),
                    art::scaled_pixels(28.0f, art_pixel_scale));
                const Vec2 grip = world_to_screen(mount, viewport, camera, scale);
                const Vec2 aim = world_to_screen(
                    mount + direction, viewport, camera, scale) - grip;
                const bool mirror_weapon_vertical =
                    environment.facing_direction() < 0.0f;
                const art::OrientedArtTransform transform = loose
                    ? art::oriented_box_transform(grip, aim,
                        weapon_length * scale, thickness)
                    : art::grip_anchored_art_transform(grip, aim,
                        weapon_length * scale, thickness,
                        rear_grip_fraction,
                        art::equipment_primary_grip_normal_fraction,
                        mirror_weapon_vertical);
                // The accepted carbine pixels are deliberately pale. Give the
                // receiver a dark, physically anchored backbone so it cannot
                // merge into a hand/forearm in strict side view. This follows
                // the same grip and barrel transform; it is not a second pose.
                canvas.line(transform.beginning, transform.ending,
                    std::max(3.0f, transform.thickness * 0.52f),
                    rgb(0x152431, alpha * 0.96f));
                draw_authored_pixel_art(
                    environment.blueprint().presentation_species(),
                    art::Module::equipment, optional_weapon_art,
                    transform.beginning, transform.ending,
                    transform.thickness, alpha,
                    mirror_weapon_vertical, false, true);
                // Retain the authored silhouette, then articulate its material
                // regions on the exact physical grip/barrel axis. The pale
                // source pixels alone merged with two armored hands into one
                // featureless bar; these narrow stock/receiver/barrel accents
                // make the held object readable without moving a single pose
                // anchor or substituting generated art.
                const Vec2 weapon_delta = transform.ending
                    - transform.beginning;
                const float weapon_span = length(weapon_delta);
                if (weapon_span > 1.0f)
                {
                    const Vec2 weapon_axis = weapon_delta / weapon_span;
                    const Vec2 weapon_normal = perpendicular(weapon_axis);
                    const Vec2 stock_end = transform.beginning
                        + weapon_axis * (weapon_span * 0.14f);
                    const Vec2 receiver_begin = transform.beginning
                        + weapon_axis * (weapon_span * 0.11f);
                    const Vec2 receiver_end = transform.beginning
                        + weapon_axis * (weapon_span * 0.47f);
                    canvas.line(transform.beginning, stock_end,
                        std::max(2.0f, transform.thickness * 0.18f),
                        rgb(0x425a67, alpha * 0.92f));
                    canvas.line(receiver_begin, receiver_end,
                        std::max(3.0f, transform.thickness * 0.34f),
                        rgb(0x10232e, alpha));
                    canvas.line(receiver_end, transform.ending,
                        std::max(2.0f, transform.thickness * 0.11f),
                        rgb(0x203946, alpha * 0.98f));
                    canvas.line(receiver_begin, receiver_end,
                        std::max(1.0f, transform.thickness * 0.065f),
                        rgb(0x4eb3c7, alpha * 0.80f));
                    // Mark the actual physical grip and muzzle on the same
                    // transform. These small material breaks keep the two
                    // exact-anchor hands, receiver, and barrel from becoming
                    // one pale horizontal bar in a planted aiming pose.
                    canvas.line(
                        grip - weapon_normal * (transform.thickness * 0.10f),
                        grip + weapon_normal * (transform.thickness * 0.30f),
                        std::max(2.5f, transform.thickness * 0.16f),
                        rgb(0x0c1c25, alpha));
                    canvas.circle(grip,
                        std::max(1.8f, transform.thickness * 0.075f),
                        rgb(0x62c5d1, alpha * 0.96f), 14);
                    canvas.circle(transform.ending,
                        std::max(1.5f, transform.thickness * 0.055f),
                        rgb(0x09151c, alpha), 14);
                }
            }
            else
            {
                canvas.line(world_to_screen(mount, viewport, camera, scale),
                    world_to_screen(mount + direction * 0.55f,
                        viewport, camera, scale),
                    5.0f, loose ? danger : environment.equipment_state()
                        == sim::EquipmentState::ready ? accent : muted);
            }
        }

        void draw_course_features(const sim::Environment& environment, Rect viewport,
            float camera, float scale, bool annotations = true)
        {
            int label_lane = 0;
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const Vec2 feature_screen = world_to_screen(feature.center, viewport, camera, scale);
                const bool granular_block = feature.marker_sequence >= 50'000;
                if (feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    const Color fill = feature.kind == sim::CourseFeatureKind::rock
                        ? rgb(0x6c747d)
                        : feature.kind == sim::CourseFeatureKind::projectile ? rgb(0xf06a3e) : danger;
                    canvas.circle(feature_screen, feature.radius * scale, fill, 24);
                    if (feature.kind == sim::CourseFeatureKind::projectile)
                        canvas.line(feature_screen - feature.velocity * (scale * 0.20f),
                            feature_screen, 3.0f, yellow);
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        viewport, camera, scale);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        viewport, camera, scale);
                    const Rect rect{ { minimum.x, maximum.y },
                        { maximum.x - minimum.x, minimum.y - maximum.y } };
                    const bool ledge = feature.kind == sim::CourseFeatureKind::ledge;
                    const Color fill = granular_block ? rgb(0x70533f)
                        : ledge ? rgb(0x596b75)
                        : feature.kind == sim::CourseFeatureKind::hurdle ? yellow
                        : feature.kind == sim::CourseFeatureKind::duck_press
                            ? rgb(0x315b70) : accent_dim;
                    add_rounded_rect(canvas, rect, ledge ? 1.0f : 4.0f,
                        fill, ledge ? white : accent, 1.0f);
                }

                if (!annotations
                    || feature_screen.x < viewport.position.x - 20.0f
                    || feature_screen.x > viewport.position.x + viewport.size.x + 20.0f)
                    continue;
                const bool trainer_feature = feature.kind == sim::CourseFeatureKind::duck_press
                    || feature.kind == sim::CourseFeatureKind::ledge;
                const std::string label = granular_block
                    ? std::format("{}: {}", environment.granular_hazard_active()
                        ? "HAZARD" : "SAFE", environment.granular_hazard_active()
                            ? "FALLING BLOCK" : "SETTLED BLOCK")
                    : std::format("{}: {}", trainer_feature ? "TRAINER" : "HAZARD",
                        sim::course_feature_name(feature.kind));
                const float x = clamp(feature_screen.x - 68.0f,
                    viewport.position.x + 8.0f, viewport.position.x + viewport.size.x - 190.0f);
                const float y = viewport.position.y + 144.0f
                    + static_cast<float>(label_lane % 5) * 25.0f;
                const Rect callout{ { x, y }, { 182.0f, 22.0f } };
                const Vec2 tether{ clamp(feature_screen.x,
                    callout.position.x + 6.0f,
                    callout.position.x + callout.size.x - 6.0f), callout.position.y };
                canvas.line(feature_screen, tether, 1.25f, trainer_feature ? accent_dim : yellow);
                add_rounded_rect(canvas, callout, 4.0f, rgb(0x101820, 0.90f),
                    trainer_feature ? accent : yellow, 1.0f);
                add_text_fit(canvas, callout.position + Vec2{ 5.0f, 4.0f },
                    label, 0.72f, trainer_feature ? accent : yellow, 172.0f, 0.62f);
                ++label_lane;
            }

            for (const sim::MaterialParticle& material : environment.material_particles())
            {
                if (!material.active)
                    continue;
                const Vec2 center = world_to_screen(material.position, viewport, camera, scale);
                const Color color = material.kind == sim::MaterialKind::sand
                    ? rgb(0xd8bd70) : material.kind == sim::MaterialKind::rock
                        ? rgb(0x6c747d) : rgb(0x9a7854);
                canvas.circle(center, material.radius * scale, color, 14);
            }

            const sim::EquipmentTarget& target = environment.equipment_target();
            if (target.active)
            {
                const Vec2 center = world_to_screen(target.position, viewport, camera, scale);
                canvas.circle(center, target.radius * scale, rgb(0x183746), 24);
                canvas.circle(center, target.radius * scale * 0.62f, danger, 20);
                canvas.circle(center, target.radius * scale * 0.24f, white, 16);
            }
            for (const sim::EquipmentProjectile& projectile : environment.equipment_projectiles())
            {
                if (projectile.active)
                    canvas.circle(world_to_screen(projectile.position, viewport, camera, scale),
                        projectile.radius * scale, yellow, 12);
            }
            draw_equipment(environment, viewport, camera, scale, false);
        }
        [[nodiscard]] const SpeciesArtBundle* species_art_for(
            sim::CreatureSpecies species) const noexcept
        {
            switch (species)
            {
            case sim::CreatureSpecies::chicken: return &chicken_art;
            case sim::CreatureSpecies::dog: return &dog_art;
            case sim::CreatureSpecies::hexapod: return &hexapod_art;
            default: return nullptr;
            }
        }

        void draw_species_creature(const sim::Environment& environment, Rect viewport,
            float camera, float scale, bool show_nodes, bool show_bones)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            const SpeciesArtBundle* bundle = species_art_for(rig.presentation_species());
            const bool use_art = art_enabled_for_mode()
                && bundle != nullptr && bundle->loaded();
            const sim::CreatureSpecies species = rig.presentation_species();
            const art::SpeciesArtTopology topology = art::species_art_topology(species);
            const art::SpeciesArtProfile profile = art::species_art_profile(species);
            const bool mirrored = environment.facing_direction() < 0.0f;
            const bool transverse_mirror = art::presented_limb_transverse_mirror(
                environment.facing_direction());
            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            auto side = [&](const sim::MotorConstraint& motor)
            {
                const std::uint8_t mask = rig.support_branch_mask(motor);
                return mask == 0x1u ? -1 : mask == 0x2u ? 1 : 0;
            };

            std::vector<sim::MotorConstraint> motors{};
            motors.reserve(rig.active_motor_count + 4u);
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
                if (rig.motors[index].enabled)
                    motors.push_back(rig.motors[index]);
            for (const sim::CoupledMotorConstraint& coupled : rig.coupled_support_motors())
                if (coupled.motor.enabled)
                    motors.push_back(coupled.motor);

            if (!use_art)
            {
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    if (bone.a >= particles.size() || bone.b >= particles.size())
                        continue;
                    canvas.line(point(bone.a), point(bone.b),
                        std::max(1.0f, scale * 0.014f), rgb(0x9bd9e8, 0.56f));
                }
            }

            auto draw_limb_pass = [&](int pass)
            {
                for (const sim::MotorConstraint& motor : motors)
                {
                    if (motor.pivot >= particles.size() || motor.c >= particles.size()
                        || rig.support_branch_mask(motor) == 0u)
                        continue;
                    const int branch = side(motor);
                    const bool near = mirrored ? branch < 0 : branch > 0;
                    if ((near ? 2 : 0) != pass)
                        continue;
                    const bool has_distal_motor = std::ranges::any_of(motors,
                        [&](const sim::MotorConstraint& candidate)
                        {
                            return candidate.enabled && candidate.pivot == motor.c
                                && rig.support_branch_mask(candidate) != 0u;
                        });
                    const bool terminal = !has_distal_motor;
                    bool front_attachment = false;
                    if (motor.pivot < rig.nodes.size()
                        && topology.body_rear < rig.nodes.size()
                        && topology.body_front < rig.nodes.size())
                    {
                        const Vec2 body_axis = rig.nodes[topology.body_front]
                            - rig.nodes[topology.body_rear];
                        const float body_span_squared = dot(body_axis, body_axis);
                        if (body_span_squared > 1.0e-6f)
                            front_attachment = dot(
                                rig.nodes[motor.pivot]
                                    - rig.nodes[topology.body_rear],
                                body_axis) / body_span_squared >= 0.5f;
                    }
                    const Color depth_outline = near
                        ? (front_attachment ? rgb(0x173642, 0.98f)
                            : rgb(0x3d3048, 0.98f))
                        : (front_attachment ? rgb(0x3f8991, 0.88f)
                            : rgb(0x80698a, 0.88f));
                    const art::PixelArt* sprite = nullptr;
                    if (use_art)
                        sprite = has_distal_motor
                            ? &bundle->upper_leg : &bundle->lower_leg;
                    Vec2 beginning = point(motor.pivot);
                    Vec2 ending = point(motor.c);
                    const Vec2 direction = normalized(ending - beginning, { 1.0f, 0.0f });
                    beginning = beginning - direction * scale * profile.joint_overlap;
                    // A terminal sprite already overlaps the physical contact
                    // anchor. Extending its lower-leg art through that anchor
                    // produced the thin cyan/gray spikes beneath Hexapod and
                    // Chicken feet. Distal joints retain their authored seam.
                    if (!terminal)
                        ending = ending + direction * scale * profile.joint_overlap;
                    const float span = length(ending - beginning);
                    const float thickness = std::clamp(span * profile.limb_ratio,
                        scale * profile.limb_minimum, scale * profile.limb_maximum);
                    if (sprite != nullptr && sprite->loaded())
                    {
                        draw_authored_pixel_art(species,
                            has_distal_motor ? art::Module::upper_limb
                                : art::Module::lower_limb,
                            *sprite, beginning, ending, thickness,
                            near ? 0.98f : 0.86f,
                            transverse_mirror, false, true,
                            depth_outline, near ? 0.10f : 0.28f);
                    }
                    else
                        canvas.line(beginning, ending, thickness,
                            near ? rgb(0xcdd6d9) : rgb(0x73828b));

                    if (terminal && use_art && bundle->foot.loaded())
                    {
                        const float facing = environment.facing_direction();
                        const Vec2 contact = point(motor.c);
                        const art::OrientedArtTransform foot =
                            art::support_boot_transform(point(motor.pivot), contact,
                                scale * profile.foot_length,
                                std::max(thickness * 0.72f,
                                    scale * profile.foot_thickness),
                                facing, particles[motor.c].grounded);
                        draw_authored_pixel_art(species,
                            art::Module::terminal, bundle->foot,
                            foot.beginning, foot.ending, foot.thickness,
                            near ? 0.98f : 0.86f,
                            transverse_mirror, false, true,
                            depth_outline, near ? 0.12f : 0.32f);
                        canvas.circle(contact,
                            std::clamp(scale * (near ? 0.026f : 0.034f),
                                2.0f, 5.5f),
                            depth_outline, 16);
                    }
                }
            };

            draw_limb_pass(0);
            const std::size_t body_a = topology.body_rear;
            const std::size_t body_b = topology.body_front;
            Vec2 body_direction{ environment.facing_direction(), 0.0f };
            if (body_a < particles.size() && body_b < particles.size())
            {
                Vec2 beginning = point(body_a);
                Vec2 ending = point(body_b);
                body_direction = normalized(ending - beginning,
                    { environment.facing_direction(), 0.0f });
                beginning = beginning - body_direction * scale * profile.body_rear_overlap;
                ending = ending + body_direction * scale * profile.body_front_overlap;
                const float body_span = length(ending - beginning);
                const float thickness = std::clamp(body_span * profile.body_ratio,
                    scale * profile.body_minimum, scale * profile.body_maximum);
                if (use_art && bundle->body.loaded())
                    draw_authored_pixel_art(species,
                        art::Module::body, bundle->body, beginning, ending,
                        thickness, 0.98f, transverse_mirror, false, true);
                else
                    canvas.line(beginning, ending, thickness, rgb(0x8ba0aa));

                if (use_art && bundle->tail.loaded() && profile.tail_length > 0.0f)
                {
                    const Vec2 tail_anchor = point(body_a)
                        - body_direction * scale * (profile.body_rear_overlap * 0.35f)
                        + Vec2{ 0.0f, -scale * profile.tail_anchor_up };
                    Vec2 tail_ending = tail_anchor
                        - body_direction * scale * profile.tail_length;
                    if (topology.physical_tail
                        && topology.tail_tip < particles.size())
                    {
                        const Vec2 physical_tip = point(topology.tail_tip);
                        const Vec2 physical_axis = normalized(
                            physical_tip - tail_anchor, -1.0f * body_direction);
                        tail_ending = physical_tip
                            + physical_axis * scale * profile.body_rear_overlap * 0.20f;
                    }
                    draw_authored_pixel_art(species,
                        art::Module::tail, bundle->tail,
                        tail_anchor, tail_ending,
                        thickness * profile.tail_ratio, 0.90f,
                        art::tail_transverse_mirror(profile, transverse_mirror),
                        false, true);
                }
            }
            if (rig.head_node < particles.size()
                && topology.head_attachment < particles.size()
                && topology.head_tip < particles.size())
            {
                const float facing = environment.facing_direction();
                const Vec2 attachment = point(topology.head_attachment);
                const Vec2 physical_tip = point(topology.head_tip);
                const Vec2 head_direction = normalized(
                    physical_tip - attachment, { facing, 0.0f });
                const float head_length = std::clamp(
                    particles[rig.head_node].radius * scale * profile.head_scale,
                    scale * profile.head_minimum, scale * profile.head_maximum);
                const Vec2 beginning = attachment
                    - head_direction * scale * profile.head_anchor_forward
                    + Vec2{ 0.0f, -scale * profile.head_anchor_up };
                const Vec2 ending = beginning + head_direction * head_length;
                if (use_art && bundle->head.loaded())
                    draw_authored_pixel_art(species,
                        art::Module::head, bundle->head,
                        beginning, ending, head_length * profile.head_ratio, 0.98f,
                        transverse_mirror, false, true);
                else
                    canvas.circle(point(rig.head_node), std::max(3.0f,
                        particles[rig.head_node].radius * scale), rgb(0xcdd6d9), 18);
            }
            draw_limb_pass(2);
            if (show_bones || show_nodes)
            {
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    if (bone.a < particles.size() && bone.b < particles.size())
                        canvas.line(point(bone.a), point(bone.b),
                            std::max(1.5f, scale * 0.016f),
                            rgb(0x55d7e9, 0.72f));
                }
            }
            if (show_nodes)
                for (const sim::Particle& particle : particles)
                    canvas.circle(world_to_screen(particle.position, viewport, camera, scale),
                        std::max(2.0f, particle.radius * scale * 0.22f), accent, 12);
        }
        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false, bool show_bones = false)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            const float art_pixel_scale = art::presentation_pixel_scale(scale);
            if (particles.empty())
                return;
            if (rig.presentation_species() != sim::CreatureSpecies::human)
            {
                draw_species_creature(environment, viewport, camera, scale,
                    show_nodes, show_bones);
                return;
            }
            const bool mirrored_facing = environment.facing_direction() < 0.0f;
            const bool presentation_right_leg_near = mirrored_facing
                ? !right_leg_near : right_leg_near;
            auto point = [&](std::size_t index)
            {
                return world_to_screen(
                    particles[index].position, viewport, camera, scale);
            };
            auto branch_side = [](std::uint8_t mask) noexcept
            {
                return mask == 0x1u ? -1 : mask == 0x2u ? 1 : 0;
            };
            std::vector<int> presentation_side(particles.size(), 0);
            for (std::size_t index = 0; index < particles.size(); ++index)
                presentation_side[index] = branch_side(rig.node_support_mask(index));
            std::vector<std::size_t> manipulator_shoulders{};
            manipulator_shoulders.reserve(rig.active_motor_count);
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const sim::MotorConstraint& shoulder = rig.motors[index];
                if (!shoulder.enabled || rig.support_branch_mask(shoulder) != 0u
                    || shoulder.a != rig.torso_node
                    || shoulder.pivot >= particles.size()
                    || shoulder.c >= particles.size())
                    continue;
                const bool has_distal = std::ranges::any_of(rig.motors,
                    [&](const sim::MotorConstraint& other)
                    {
                        return other.enabled && rig.support_branch_mask(other) == 0u
                            && other.pivot == shoulder.c;
                    });
                if (has_distal)
                    manipulator_shoulders.push_back(index);
            }
            std::ranges::sort(manipulator_shoulders,
                [&](std::size_t left, std::size_t right)
                {
                    return rig.nodes[rig.motors[left].pivot].x
                        < rig.nodes[rig.motors[right].pivot].x;
                });
            for (std::size_t chain = 0;
                chain < manipulator_shoulders.size(); ++chain)
            {
                const int side = (chain & 1u) == 0u ? -1 : 1;
                const sim::MotorConstraint& shoulder =
                    rig.motors[manipulator_shoulders[chain]];
                presentation_side[shoulder.pivot] = side;
                presentation_side[shoulder.c] = side;
                for (const sim::MotorConstraint& distal : rig.motors)
                {
                    if (distal.enabled && rig.support_branch_mask(distal) == 0u
                        && distal.pivot == shoulder.c
                        && distal.c < presentation_side.size())
                        presentation_side[distal.c] = side;
                }
            }
            auto leg_side = [&](std::size_t index) noexcept
            {
                return index < presentation_side.size()
                    ? presentation_side[index] : 0;
            };
            // A physically shared two-hand object belongs between the rear
            // and front manipulator passes. Deriving the whole weapon layer
            // from only its primary mount hid the support hand beneath it.
            constexpr int held_equipment_layer = 2;
            auto draw_held_equipment_layer = [&](int pass)
            {
                if (pass == held_equipment_layer)
                    draw_equipment(environment, viewport, camera, scale, true);
            };
            float assembly_torso_span = art::scaled_pixels(48.0f, art_pixel_scale);
            float assembly_shoulder_span = 0.0f;
            if (rig.root_node < particles.size() && rig.torso_node < particles.size())
            {
                assembly_torso_span = std::max(
                    art::scaled_pixels(12.0f, art_pixel_scale),
                    length(point(rig.torso_node) - point(rig.root_node)));
                float minimum_shoulder = std::numeric_limits<float>::infinity();
                float maximum_shoulder = -std::numeric_limits<float>::infinity();
                const Vec2 assembly_axis = normalized(
                    point(rig.torso_node) - point(rig.root_node), { 0.0f, -1.0f });
                const Vec2 assembly_right{ -assembly_axis.y, assembly_axis.x };
                for (std::size_t motor_index = 0;
                    motor_index < rig.active_motor_count; ++motor_index)
                {
                    const sim::MotorConstraint& motor = rig.motors[motor_index];
                    if (!motor.enabled || rig.support_branch_mask(motor) != 0u
                        || motor.a != rig.torso_node || motor.pivot >= particles.size())
                        continue;
                    const float shoulder = dot(
                        point(motor.pivot) - point(rig.torso_node), assembly_right);
                    minimum_shoulder = std::min(minimum_shoulder, shoulder);
                    maximum_shoulder = std::max(maximum_shoulder, shoulder);
                }
                if (std::isfinite(minimum_shoulder)
                    && std::isfinite(maximum_shoulder)
                    && maximum_shoulder > minimum_shoulder)
                    assembly_shoulder_span = maximum_shoulder - minimum_shoulder;
            }
            const float assembled_art_scale = art::assembled_armor_scale(
                assembly_torso_span, assembly_shoulder_span, art_pixel_scale);
            auto draw_body_segments = [&](int pass)
            {
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    if (bone.a >= particles.size() || bone.b >= particles.size())
                        continue;
                    const int side_a = leg_side(bone.a);
                    const int side_b = leg_side(bone.b);
                    const int side = side_a != 0 ? side_a : side_b;
                    const bool near = side != 0 && ((side > 0) == presentation_right_leg_near);
                    const int layer = side == 0 ? 1 : near ? 2 : 0;
                    if (layer != pass)
                        continue;
                    const float radius_a = bone.a < rig.radii.size()
                        ? rig.radii[bone.a] : 0.15f;
                    const float radius_b = bone.b < rig.radii.size()
                        ? rig.radii[bone.b] : 0.15f;

                    if (!art_enabled_for_mode())
                    {
                        const Color color = side == 0 ? body
                            : near ? leg : rgb(0x5f493b);
                        const float radius = std::max(0.035f,
                            std::min(radius_a, radius_b) * 0.34f) * scale;
                        canvas.capsule(point(bone.a), point(bone.b), radius, color, 16);
                    }
                }
            };
            auto support_parent = [&](std::size_t support) noexcept
            {
                for (std::size_t motor_index = 0;
                    motor_index < rig.active_motor_count; ++motor_index)
                {
                    const sim::MotorConstraint& motor = rig.motors[motor_index];
                    if (motor.enabled && motor.c == support
                        && motor.pivot < particles.size())
                        return static_cast<std::size_t>(motor.pivot);
                }
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    if (bone.a == support && bone.b < particles.size())
                        return static_cast<std::size_t>(bone.b);
                    if (bone.b == support && bone.a < particles.size())
                        return static_cast<std::size_t>(bone.a);
                }
                return static_cast<std::size_t>(rig.root_node);
            };
            auto draw_nodes = [&](int pass)
            {
                for (std::size_t index = 0; index < particles.size(); ++index)
                {
                    const int side = leg_side(index);
                    const bool near = side != 0 && ((side > 0) == presentation_right_leg_near);
                    const int layer = side == 0 ? 1 : near ? 2 : 0;
                    if (layer != pass)
                        continue;
                    const float radius = (index < rig.radii.size()
                        ? rig.radii[index] : 0.15f) * scale;
                    Color color = index == rig.head_node ? body_light : body;
                    if (side != 0)
                        color = near ? leg : rgb(0x5f493b);
                    if (rig.is_support_seed(index))
                    {
                        const bool primary_support = index == rig.left_contact_node
                            || index == rig.right_contact_node;
                        if (primary_support)
                        {
                            const Vec2 center = point(index);
                            const std::size_t ankle_index = support_parent(index);
                            const std::size_t knee_index = support_parent(ankle_index);
                            const bool left = index == rig.left_contact_node;
                            const auto& extra = left
                                ? rig.additional_left_contact_nodes
                                : rig.additional_right_contact_nodes;
                            const Vec2 toe = extra.size() >= 2u
                                && extra[1] < particles.size()
                                ? point(extra[1]) : center;
                            if (art_enabled_for_mode())
                            {
                                if (optional_foot_art.loaded())
                                {
                                    const float plate_span = length(toe - center);
                                    const float lower_leg_span = ankle_index
                                            < particles.size()
                                            && knee_index < particles.size()
                                        ? length(point(ankle_index)
                                            - point(knee_index))
                                        : plate_span * 2.5f;
                                    const float authored_lower_leg_span = ankle_index
                                            < rig.nodes.size()
                                            && knee_index < rig.nodes.size()
                                        ? length(rig.nodes[ankle_index]
                                            - rig.nodes[knee_index]) * scale
                                        : lower_leg_span;
                                    const float authored_plate_span = extra.size() >= 2u
                                            && index < rig.nodes.size()
                                            && extra[1] < rig.nodes.size()
                                        ? length(rig.nodes[extra[1]]
                                            - rig.nodes[index]) * scale
                                        : plate_span;
                                    // Authored heel/toe and lower-leg anchors own
                                    // a stable boot silhouette. The live heel/toe
                                    // pair below owns only its physical center and
                                    // pitch, so toe roll cannot squash the art.
                                    const art::BootArtDimensions boot =
                                        art::articulated_boot_dimensions(
                                            authored_plate_span,
                                            authored_lower_leg_span, radius);
                                    const art::OrientedArtTransform transform =
                                        art::articulated_boot_transform(center, toe,
                                            boot.width, boot.height,
                                            environment.facing_direction());
                                    // Both boots occupy the exact same authored
                                    // heel->toe envelope. Depth color is mixed
                                    // only into supplied opaque pixels: no box,
                                    // wedge, or synthetic sole can replace the
                                    // irregular source silhouette.
                                    const Color boot_tint = near
                                        ? rgb(0x1d5260) : rgb(0x80698a);
                                    draw_authored_pixel_art(
                                        sim::CreatureSpecies::human,
                                        art::Module::terminal, optional_foot_art,
                                        transform.beginning, transform.ending,
                                        transform.thickness,
                                        art::human_limb_layer_opacity(
                                            near || side == 0),
                                        mirrored_facing, false, true,
                                        boot_tint, near ? 0.05f : 0.18f);
                                }
                            }
                            else
                            {
                                const float height = std::max(7.0f, radius * 0.55f);
                                const art::OrientedArtTransform transform =
                                    art::articulated_boot_transform(center, toe,
                                        radius * 1.63f, height * 2.0f,
                                        environment.facing_direction());
                                canvas.capsule(transform.beginning, transform.ending,
                                    height, color, 14);
                            }
                        }
                    }
                    else
                    {
                        if (!art_enabled_for_mode())
                        {
                            const float visual_radius = index == rig.head_node
                                ? std::clamp(radius, 9.0f, 18.0f)
                                : std::clamp(radius * 0.48f, 4.5f, 10.0f);
                            canvas.circle(point(index), visual_radius, color, 22);
                        }
                    }
                    if (show_nodes || debug_skeleton_overlay)
                    {
                        canvas.circle(point(index), 7.0f,
                            index == static_cast<std::size_t>(selected_node)
                                ? accent : white, 18);
                        add_text(canvas, point(index) + Vec2{ 10.0f, -8.0f },
                            std::to_string(index), 1.05f, white);
                    }
                }
            };

            auto draw_fitted_armor = [&](int pass)
            {
                if (!art_enabled_for_mode())
                    return;
                const auto is_articulated_foot_motor = [&](const
                    sim::MotorConstraint& candidate) noexcept
                {
                    // The Human ankle motor articulates ankle->ball. It owns no
                    // extra limb sprite: the knee motor's shin must end at the
                    // ankle, and draw_nodes() fits one boot across heel->toe.
                    return (!rig.additional_left_contact_nodes.empty()
                            && candidate.c
                                == rig.additional_left_contact_nodes.front())
                        || (!rig.additional_right_contact_nodes.empty()
                            && candidate.c
                                == rig.additional_right_contact_nodes.front());
                };
                for (std::size_t motor_index = 0;
                    motor_index < rig.active_motor_count; ++motor_index)
                {
                    if (rig.monopedal_gait() && motor_index >= 2u)
                        continue;

                    const sim::MotorConstraint& motor = rig.motors[motor_index];
                    if (!motor.enabled || motor.pivot >= particles.size()
                        || motor.c >= particles.size()
                        || is_articulated_foot_motor(motor))
                        continue;
                    const std::uint8_t support_mask = rig.support_branch_mask(motor);
                    const int side = support_mask != 0u
                        ? branch_side(support_mask) : leg_side(motor.c);
                    const bool near = side != 0 && ((side > 0) == presentation_right_leg_near);
                    const int layer = side == 0 ? 1 : near ? 2 : 0;
                    if (layer != pass)
                        continue;

                    bool has_distal_motor = false;
                    for (std::size_t other_index = 0;
                        other_index < rig.active_motor_count; ++other_index)
                    {
                        if (other_index == motor_index)
                            continue;
                        const sim::MotorConstraint& other = rig.motors[other_index];
                        if (other.enabled && other.pivot == motor.c
                            && !is_articulated_foot_motor(other)
                            && ((rig.support_branch_mask(other) != 0u)
                                == (support_mask != 0u)))
                        {
                            has_distal_motor = true;
                            break;
                        }
                    }
                    const art::PixelArt& sprite = rig.monopedal_gait()
                        ? (motor_index == 0u ? optional_thigh_art : optional_shin_art)
                        : support_mask != 0u
                        ? (has_distal_motor ? optional_thigh_art : optional_shin_art)
                        : (has_distal_motor ? optional_upper_arm_art : optional_forearm_art);
                    if (!sprite.loaded())
                        continue;

                    Vec2 beginning = point(motor.pivot);
                    Vec2 ending = point(motor.c);
                    const Vec2 terminal_joint = ending;
                    const Vec2 delta = ending - beginning;
                    const float span = length(delta);
                    if (span <= 1.0f)
                        continue;
                    const Vec2 axis = delta / span;
                    const bool support_limb = support_mask != 0u;
                    const float thickness_ratio = art::human_limb_thickness_ratio(
                        support_limb, has_distal_motor);
                    const float minimum_thickness = support_limb
                        ? 23.0f : has_distal_motor ? 12.0f : 11.0f;
                    const float maximum_thickness = support_limb
                        ? 70.0f : has_distal_motor ? 38.0f : 34.0f;
                    const float limb_assembly_scale = support_limb
                        ? assembled_art_scale : 1.0f;
                    const float limb_opacity = support_limb
                        ? art::human_limb_layer_opacity(near || side == 0)
                        : art::human_arm_layer_opacity(near || side == 0);
                    const float thickness = std::clamp(
                        span * thickness_ratio * limb_assembly_scale,
                        art::scaled_pixels(minimum_thickness, art_pixel_scale),
                        art::scaled_pixels(maximum_thickness, art_pixel_scale));
                    const float authored_joint_overlap =
                        art::fitted_joint_overlap(span, thickness, support_limb);
                    beginning = beginning - axis * authored_joint_overlap;
                    ending = ending + axis * authored_joint_overlap;
                    const bool transverse_mirror =
                        art::presented_limb_transverse_mirror(
                            environment.facing_direction());
                    const Color manipulator_outline = near
                        ? rgb(0x142630, 0.98f)
                        : rgb(0x2d6672, 0.96f);
                    // Arm depth is biased only inside supplied opaque pixels.
                    // No synthetic outline ribbon is added, and no endpoint is
                    // displaced to fake depth: the shoulder/elbow/wrist anchors
                    // remain the exact physical production pose.
                    draw_authored_pixel_art(sim::CreatureSpecies::human,
                        support_limb
                            ? (has_distal_motor ? art::Module::upper_limb
                                : art::Module::lower_limb)
                            : (has_distal_motor ? art::Module::upper_limb
                                : art::Module::lower_limb),
                        sprite, beginning, ending,
                        thickness, limb_opacity,
                        transverse_mirror, false, true,
                        support_limb
                            ? Color{ 0.0f, 0.0f, 0.0f, 0.0f }
                            : manipulator_outline,
                        support_limb ? 0.0f : near ? 0.06f : 0.18f);
                    if (!support_limb)
                    {
                        const float joint_radius = std::clamp(
                            thickness * (near ? 0.20f : 0.18f),
                            2.0f, 5.25f);
                        canvas.circle(terminal_joint,
                            joint_radius + (near ? 1.0f : 0.75f),
                            manipulator_outline, 16);
                        canvas.circle(terminal_joint, joint_radius,
                            near ? rgb(0xb7c9cc, 0.98f)
                                : rgb(0x68c4cf, 0.94f),
                            16);
                    }
                    if (support_mask == 0u && !has_distal_motor
                        && optional_hand_art.loaded())
                    {
                        const art::HandArtDimensions hand =
                            art::hand_art_dimensions(thickness,
                                optional_hand_art.width, optional_hand_art.height,
                                art_pixel_scale, span);
                        const Vec2 hand_beginning = terminal_joint
                            - axis * hand.wrist_overlap;
                        const Vec2 hand_ending = terminal_joint
                            + axis * (hand.length - hand.wrist_overlap);
                        draw_authored_pixel_art(
                            sim::CreatureSpecies::human,
                            art::Module::hand, optional_hand_art,
                            hand_beginning, hand_ending,
                            hand.thickness, limb_opacity,
                            transverse_mirror, false, true,
                            manipulator_outline, near ? 0.12f : 0.32f);
                        const Vec2 wrist_normal{ -axis.y, axis.x };
                        // Cuff and fingertip details terminate at the exact
                        // physical wrist/hand axis. A wider teal rear cuff and
                        // compact graphite near cuff remain visible even when
                        // a planted two-hand aim makes both forearms almost
                        // collinear; no endpoint is displaced to fake depth.
                        canvas.line(
                            terminal_joint
                                - wrist_normal * (hand.thickness * 0.30f),
                            terminal_joint
                                + wrist_normal * (hand.thickness * 0.30f),
                            std::max(2.25f, hand.thickness * 0.15f),
                            manipulator_outline);
                        const float endpoint_radius = std::clamp(
                            hand.thickness * (near ? 0.20f : 0.18f),
                            2.0f, 5.5f);
                        canvas.circle(hand_ending, endpoint_radius,
                            manipulator_outline, 16);
                        canvas.circle(hand_ending,
                            std::max(1.0f, endpoint_radius * 0.46f),
                            near ? rgb(0xb7c9cc, 0.98f)
                                : rgb(0x68c4cf, 0.94f),
                            14);
                    }
                }
            };
            draw_body_segments(0);
            draw_held_equipment_layer(0);
            draw_fitted_armor(0);
            draw_nodes(0);
            draw_body_segments(1);
            draw_held_equipment_layer(1);
            draw_fitted_armor(1);
            draw_nodes(1);

            if (art_enabled_for_mode()
                && rig.root_node < particles.size()
                && rig.torso_node < particles.size())
            {
                // Modular armor is the exclusive presentation. The topology is
                // used only to fit the authored side-view pieces; procedural body
                // shapes belong to the explicit skeleton debug view.
                const Vec2 root = point(rig.root_node);
                const Vec2 torso = point(rig.torso_node);
                const Vec2 body_axis = normalized(torso - root, { 0.0f, -1.0f });
                const Vec2 body_right{ -body_axis.y, body_axis.x };
                const float torso_length = std::max(
                    art::scaled_pixels(24.0f, art_pixel_scale), length(torso - root));

                float minimum_shoulder = std::numeric_limits<float>::infinity();
                float maximum_shoulder = -std::numeric_limits<float>::infinity();
                for (std::size_t motor_index = 0;
                    motor_index < rig.active_motor_count; ++motor_index)
                {
                    const sim::MotorConstraint& motor = rig.motors[motor_index];
                    if (!motor.enabled || rig.support_branch_mask(motor) != 0u
                        || motor.a != rig.torso_node
                        || motor.pivot >= particles.size())
                        continue;
                    const float shoulder = dot(point(motor.pivot) - torso, body_right);
                    minimum_shoulder = std::min(minimum_shoulder, shoulder);
                    maximum_shoulder = std::max(maximum_shoulder, shoulder);
                }
                const float authored_shoulder_span =
                    std::isfinite(minimum_shoulder) && std::isfinite(maximum_shoulder)
                    && maximum_shoulder > minimum_shoulder
                    ? maximum_shoulder - minimum_shoulder : 0.0f;
                const art::SkinEnvelopeDimensions envelope =
                    art::skin_envelope_dimensions(
                        torso_length, authored_shoulder_span, art_pixel_scale);

                const Vec2 chest_bottom = root + body_axis * (torso_length * 0.16f);
                const Vec2 chest_top = torso - body_axis * (torso_length * 0.08f);

                if (optional_torso_art.loaded())
                {
                    const Vec2 center = (chest_bottom + chest_top) * 0.5f;
                    const float height = std::clamp(
                        torso_length * 1.05f * assembled_art_scale,
                        art::scaled_pixels(60.0f, art_pixel_scale),
                        art::scaled_pixels(172.0f, art_pixel_scale));
                    const float source_width = height
                        * static_cast<float>(optional_torso_art.width)
                        / static_cast<float>(optional_torso_art.height);
                    const float width = std::clamp(
                        std::max(source_width, envelope.shoulder_width * 1.26f),
                        envelope.chest_radius * 2.15f,
                        torso_length * 1.45f);
                    const art::OrientedArtTransform transform =
                        art::oriented_box_transform(center, body_right, width, height);
                    draw_authored_pixel_art(
                        sim::CreatureSpecies::human,
                        art::Module::body, optional_torso_art,
                        transform.beginning, transform.ending,
                        transform.thickness, 0.96f, false, mirrored_facing);
                }

            }
            if (art_enabled_for_mode() && optional_helmet_art.loaded()
                && rig.head_node < particles.size())
            {
                const Vec2 head_node_center = point(rig.head_node);
                Vec2 head_axis{ 0.0f, -1.0f };
                const bool upright_creature_head = rig.avian_gait()
                    || rig.horizontal_multi_support_plan();
                if (!upright_creature_head && rig.torso_node < particles.size())
                    head_axis = normalized(
                        head_node_center - point(rig.torso_node), head_axis);
                else if (!upright_creature_head && rig.root_node < particles.size())
                    head_axis = normalized(
                        head_node_center - point(rig.root_node), head_axis);
                const Vec2 head_right{ -head_axis.y, head_axis.x };
                const art::HelmetArtDimensions helmet =
                    art::helmet_art_dimensions(
                        particles[rig.head_node].radius * scale,
                        assembled_art_scale, art_pixel_scale);
                const Vec2 center = head_node_center
                    - head_axis * helmet.downward_offset;
                const float height = helmet.height;
                const float width = height
                    * static_cast<float>(optional_helmet_art.width)
                    / static_cast<float>(optional_helmet_art.height);
                const art::OrientedArtTransform transform =
                    art::oriented_box_transform(center, head_right, width, height);
                draw_authored_pixel_art(
                    sim::CreatureSpecies::human,
                    art::Module::head, optional_helmet_art,
                    transform.beginning, transform.ending, transform.thickness,
                    0.92f, false, mirrored_facing);
            }
            draw_body_segments(2);
            draw_held_equipment_layer(2);
            draw_fitted_armor(2);
            draw_nodes(2);
            if (show_bones || debug_skeleton_overlay)
            {
                // Graph bones are diagnostic geometry. Draw the production
                // particle graph after the fitted armor, in the same rear to
                // front depth order as the production limbs. A uniform cyan
                // pass made two coincident heel/ball/toe triangles read as one
                // detached diamond even though their topology was correct.
                // This is the only Rig Lab bone overlay that includes the
                // runtime heel/ball/toe nodes and therefore cannot masquerade
                // an authored-rest rear foot as the current physical pose.
                const auto foot_chain = [&](bool left)
                {
                    const std::size_t heel = left
                        ? rig.left_contact_node : rig.right_contact_node;
                    const auto& extra = left
                        ? rig.additional_left_contact_nodes
                        : rig.additional_right_contact_nodes;
                    return std::array<std::size_t, 4>{
                        heel < particles.size() ? support_parent(heel)
                            : particles.size(),
                        heel,
                        !extra.empty() ? extra[0] : particles.size(),
                        extra.size() >= 2u ? extra[1] : particles.size()
                    };
                };
                const std::array left_foot = foot_chain(true);
                const std::array right_foot = foot_chain(false);
                const auto slot_in_chain = [](std::size_t node,
                    const std::array<std::size_t, 4>& chain) noexcept
                {
                    const auto match = std::ranges::find(chain, node);
                    return match == chain.end() ? -1
                        : static_cast<int>(match - chain.begin());
                };
                const auto foot_edge_kind = [&](std::size_t a, std::size_t b,
                    const std::array<std::size_t, 4>& chain) noexcept
                {
                    const int slot_a = slot_in_chain(a, chain);
                    const int slot_b = slot_in_chain(b, chain);
                    return art::human_foot_edge_kind(slot_a, slot_b);
                };
                for (int depth_pass = 0; depth_pass < 3; ++depth_pass)
                {
                    // Render ordinary graph edges first, the real triangle
                    // brace second, and the ordered sole path last. Every
                    // physical edge remains present, but the brace cannot turn
                    // two overlapping feet into a dominant cyan diamond.
                    for (int geometry_pass = 0; geometry_pass < 3;
                        ++geometry_pass)
                    {
                        for (const sim::DistanceConstraint& bone : rig.bones)
                        {
                            if (bone.a >= particles.size()
                                || bone.b >= particles.size())
                                continue;
                            const int side_a = leg_side(bone.a);
                            const int side_b = leg_side(bone.b);
                            const int side = side_a != 0 ? side_a : side_b;
                            const bool near = side != 0
                                && ((side > 0) == presentation_right_leg_near);
                            const int layer = side == 0 ? 1 : near ? 2 : 0;
                            if (layer != depth_pass)
                                continue;
                            const art::HumanFootEdgeKind left_kind = foot_edge_kind(
                                bone.a, bone.b, left_foot);
                            const art::HumanFootEdgeKind right_kind = foot_edge_kind(
                                bone.a, bone.b, right_foot);
                            const art::HumanFootEdgeKind foot_kind =
                                left_kind != art::HumanFootEdgeKind::ordinary
                                ? left_kind : right_kind;
                            const bool rearfoot_brace = foot_kind
                                == art::HumanFootEdgeKind::triangle_brace;
                            const bool rearfoot_path = foot_kind
                                == art::HumanFootEdgeKind::ordered_path;
                            const int intended_pass = rearfoot_path ? 2
                                : rearfoot_brace ? 1 : 0;
                            if (geometry_pass != intended_pass)
                                continue;
                            const art::HumanFootEdgeStyle foot_style =
                                art::human_foot_edge_style(foot_kind, near);
                            const float width = rearfoot_brace || rearfoot_path
                                ? foot_style.width
                                : side == 0 ? 2.1f : near ? 2.5f : 1.45f;
                            const Color color = rearfoot_brace
                                ? (near ? rgb(0x4ca4ad, foot_style.opacity)
                                    : rgb(0x5e5268, foot_style.opacity))
                                : side == 0
                                    ? rgb(0x65cfda, 0.68f)
                                    : near
                                        ? rgb(0x69e4ef,
                                            rearfoot_path
                                                ? foot_style.opacity : 0.86f)
                                        : rgb(0x80698a,
                                            rearfoot_path
                                                ? foot_style.opacity : 0.54f);
                            canvas.line(point(bone.a), point(bone.b), width, color);
                        }
                    }
                    // Explicit physical ankle/heel/ball/toe points let the eye
                    // follow each sole chain without changing a single anchor.
                    // Size progression encodes terminal order; color/weight
                    // alone encode depth, so there is no cosmetic separation.
                    for (const auto& chain : { left_foot, right_foot })
                    {
                        const int side = chain[1] < particles.size()
                            ? leg_side(chain[1]) : 0;
                        const bool near = side != 0
                            && ((side > 0) == presentation_right_leg_near);
                        const int layer = side == 0 ? 1 : near ? 2 : 0;
                        if (layer != depth_pass)
                            continue;
                        const Color point_color = near
                            ? rgb(0x9af4f7, 0.98f)
                            : rgb(0xa389ac, 0.84f);
                        constexpr std::array<float, 4> point_radii{
                            4.3f, 3.9f, 3.4f, 2.9f };
                        for (std::size_t slot = 0; slot < chain.size(); ++slot)
                        {
                            if (chain[slot] < particles.size())
                                canvas.circle(point(chain[slot]),
                                    point_radii[slot] * (near ? 1.0f : 0.84f),
                                    point_color, 16);
                        }
                    }
                }
            }

        }

        void draw_training_pip(Rect rect)
        {
            const bool fixed_eye_test = presentation::isolated(presentation_session);
            const bool course_eye_test = presentation::course_eye(presentation_session);
            const bool art_eye_test = presentation::art_eye(presentation_session);
            const bool walk_eye_test = presentation::walk_eye(presentation_session);
            const bool equipment_eye_test =
                presentation::equipment_eye(presentation_session);
            const bool rig_lab_snapshot = !fixed_eye_test && mode == Mode::rig_lab;
            const auto& training_status = trainer.autonomy_status();
            const bool worker_snapshot = !fixed_eye_test && !rig_lab_snapshot;
            const bool live_rollout_sample = worker_snapshot
                && trainer.training_preview_is_rollout_sample();
            const bool retained_terminal = worker_snapshot && !live_rollout_sample
                && training_status.training_preview_terminal;
            const bool walk_terminal = walk_eye_test && walk_eye_terminal.frozen;
            const bool rig_test_terminal_frozen = rig_lab_snapshot
                && rig_test_terminal.frozen;
            const bool expanded_footer = fixed_eye_test || retained_terminal
                || rig_test_terminal_frozen;
            const ui_layout::Box pip_layout{ rect.position.x, rect.position.y,
                rect.size.x, rect.size.y };
            const ui_layout::Box pip_title_layout =
                ui_layout::training_pip_title_box(pip_layout);
            const ui_layout::Box pip_state_layout =
                ui_layout::training_pip_state_box(pip_layout);
            const ui_layout::Box pip_world_layout =
                ui_layout::training_pip_world_box(pip_layout, expanded_footer);
            const ui_layout::Box pip_footer_layout =
                ui_layout::training_pip_footer_box(pip_layout, expanded_footer);
            const Rect inner{ { pip_world_layout.x, pip_world_layout.y },
                { pip_world_layout.width, pip_world_layout.height } };
            // Keep the PIP optically isolated from the large production world
            // subject behind it; even one-percent translucency made compact
            // species read as a clipped double image at the card boundary.
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019), accent_dim, 1.5f);
            add_text_fit(canvas, { pip_title_layout.x, pip_title_layout.y },
                art_eye_test ? "ART CHECK"
                    : walk_eye_test ? "WALK PROOF"
                    : equipment_eye_test ? "EQUIPMENT EYE"
                    : course_eye_test ? "COURSE EYE TEST"
                    : rig_lab_snapshot ? "RIG LAB PARITY"
                     : "TRAINER SNAPSHOT",
                1.22f, accent, pip_title_layout.width,
                ui_layout::persistent_text_minimum_scale);
            if (worker_snapshot && !trainer.has_training_preview())
            {
                add_text_fit(canvas, inner.position + Vec2{ 4.0f, 8.0f },
                    "AWAITING TRAINING SAMPLE", 1.25f, muted,
                    inner.size.x - 8.0f,
                    ui_layout::persistent_text_minimum_scale);
                return;
            }
            // This inset reads sampled worker state, never the main replay.
            const sim::Environment& environment = fixed_eye_test
                ? *course_eye_test_environment
                : rig_lab_snapshot ? rig_preview_environment()
                : trainer.training_preview();
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty() || rig.root_node >= particles.size())
            {
                add_text_fit(canvas, inner.position + Vec2{ 4.0f, 8.0f },
                    "TRAINING FRAME INCOMPLETE", 1.25f, danger,
                    inner.size.x - 8.0f,
                    ui_layout::persistent_text_minimum_scale);
                return;
            }

            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(environment.course_stage(), environment);
            const bool foot_only = !environment.non_foot_grounded();
            const bool intact = environment.body_integrity_valid();
            const Color state_color = walk_terminal || rig_test_terminal_frozen
                    || equipment_eye_test ? yellow
                : fixed_eye_test ? green
                : retained_terminal ? yellow : qualification.valid ? green
                : intact && foot_only ? yellow : danger;
            const std::string_view state_text = art_eye_test
                ? std::string_view{ "SIDE PROFILE" }
                : walk_terminal ? std::string_view{ "TERMINAL FROZEN" }
                : walk_eye_test ? retained_guidance_label(environment.guidance_mode())
                : equipment_eye_test ? std::string_view{ "ASSISTED / READY" }
                : course_eye_test ? std::string_view{ "RUNWAY CLEAR" }
                : rig_test_terminal_frozen
                    ? std::string_view{ "TERMINAL FROZEN" }
                : rig_lab_snapshot && art_preview_frozen
                    ? std::string_view{ "FROZEN PHYSICAL" }
                : rig_lab_snapshot && rig_test_physical_environment.has_value()
                    ? std::string_view{ "TEST / PHYSICAL" }
                : rig_lab_snapshot ? std::string_view{ "PRODUCTION PREVIEW" }
                : retained_terminal ? std::string_view{ "RECORDED OUTCOME" }
                : qualification.valid ? "STAGE VALID" : !intact ? "BROKEN RIG"
                : !foot_only ? "BODY CONTACT" : "STANCE INVALID";
            canvas.push_clip({ pip_state_layout.x, pip_state_layout.y },
                { pip_state_layout.x + pip_state_layout.width,
                    pip_state_layout.y + pip_state_layout.height });
            add_wrapped_text(canvas,
                { pip_state_layout.x, pip_state_layout.y }, state_text,
                ui_layout::persistent_text_minimum_scale, state_color,
                pip_state_layout.width, 2.0f);
            canvas.pop_clip();
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
            const float body_width = std::max(0.35f, body_max_x - body_min_x);
            const float body_height = std::max(0.75f, body_max_y - body_min_y);
            const bool compact_species = environment.blueprint()
                .presentation_species() != sim::CreatureSpecies::human;
            // Use the same physical renderer as the main view. Compact species
            // get a bounded inspection crop rather
            // than inheriting Human's 4.8 m height/forward-course framing,
            // which previously reduced Dog/Chicken/Hexapod to tiny icons.
            const float horizontal_margin = compact_species
                ? std::max(0.28f, body_width * 0.32f) : 0.28f;
            const float view_min_x = compact_species
                ? body_min_x - horizontal_margin
                : std::min(root_x - 1.55f, body_min_x - 0.28f);
            const float view_max_x = compact_species
                ? body_max_x + horizontal_margin
                : std::max(root_x + 3.35f, body_max_x + 0.42f);
            const float vertical_margin = compact_species
                ? std::max(0.12f, body_height * 0.14f) : 0.18f;
            const float view_min_y = std::min(body_min_y - vertical_margin,
                environment.ground_height_at(root_x) - vertical_margin);
            const float view_max_y = body_max_y + (compact_species
                ? vertical_margin : 0.32f);
            const float world_width = compact_species
                ? std::max(1.2f, view_max_x - view_min_x)
                : std::max(3.8f, view_max_x - view_min_x);
            const float world_height = compact_species
                ? std::max(1.1f, view_max_y - view_min_y)
                : std::max(view_camera::human_reference_height_m,
                    view_max_y - view_min_y);
            const float scale = view_camera::pip_pixels_per_meter(
                (inner.size.x - 12.0f) / world_width,
                (inner.size.y * 0.78f) / world_height);
            const float camera = (view_min_x + view_max_x) * 0.5f;

            canvas.push_clip(inner.position, inner.position + inner.size);
            draw_course_ground(environment, inner, camera, scale);
            const bool focused_pose_view = art_eye_test || equipment_eye_test;
            // Challenge tags and leader wires are diagnostic evidence. They
            // are deliberately absent from the default Course Eye so the
            // material terrain remains readable instead of becoming a label
            // wall. Walk Proof and the explicit Advanced page retain them.
            const bool show_pip_challenge_details = !rig_lab_snapshot
                && !focused_pose_view && !compact_species
                && (live_panel_page == LivePanelPage::advanced
                    || walk_eye_test);
            const bool show_pip_course_annotations =
                show_pip_challenge_details;
            if (show_pip_challenge_details)
                draw_terrain_challenges(environment, inner, camera, scale);
            if (show_pip_course_annotations)
                draw_course_reference(environment, inner, camera, scale,
                    show_pip_challenge_details);
            draw_course_features(environment, inner, camera, scale,
                show_pip_challenge_details);
            draw_creature(environment, inner, camera, scale);
            canvas.pop_clip();

            // Preserve the acceptance semantic "STRICT SIDE ELEVATION - NO PERSPECTIVE OR FORESHORTENING"
            // while presenting it as one concise, readable PIP label.
            const std::string pip_metrics = art_eye_test
                ? std::string("ORTHOGRAPHIC SIDE VIEW / NO PERSPECTIVE")
                : walk_terminal
                    ? std::format(
                        "FAILED: {}   R RETRY",
                        sim::trial_terminal_cause_name(
                            walk_eye_terminal.cause, walk_eye_terminal.reason))
                : walk_eye_test
                    ? std::format(
                        "RETAINED UPDATE {}  DIST {:.1f} M  STEPS {}  CROSSINGS {}  SCISSOR {:.3f} S",
                        walk_eye_test_proof.retained_update,
                        walk_eye_test_proof.displayed_distance,
                        walk_eye_test_proof.displayed_steps,
                        walk_eye_test_proof.displayed_crossings,
                        walk_eye_test_proof.displayed_max_scissor_seconds)
                : equipment_eye_test
                    ? std::format(
                        "PHYSICAL READY  PLANT {:.2f} S  SETTLE {:.2f} S  AIM ERR {:.3f} RAD  SHOTS {}",
                        environment.equipment_plant_seconds(),
                        environment.equipment_aim_settle_seconds(),
                        environment.equipment_aim_error(),
                        environment.shots_fired())
                : course_eye_test
                    ? std::string("CLEAR TO 8-12 M / 2 PHYSICAL GAIT CYCLES")
                : rig_test_terminal_frozen
                    ? std::format(
                        "FAILED: {}   R RETRY",
                        sim::trial_terminal_cause_name(
                            rig_test_terminal.cause, rig_test_terminal.reason))
                : rig_lab_snapshot
                    ? std::format(
                        "{}   ZOOM {:.2f}X",
                        rig_test_physical_environment.has_value()
                            ? physical_scrub_name() : std::string_view{ "LIVE" },
                        art_editor_zoom)
                : retained_terminal
                    ? std::format(
                        "TRAINING TRIAL {}: {}",
                        training_status.training_preview_trial_id,
                        sim::trial_terminal_cause_name(
                            training_status.training_preview_cause,
                            training_status.training_preview_reason))
                    : std::format("{} {:.1f}S / {} CYC",
                        live_rollout_sample ? "ROLLOUT" : "SAMPLE",
                        environment.elapsed_seconds(), environment.gait_cycles());
            const Rect pip_footer{
                { pip_footer_layout.x, pip_footer_layout.y },
                { pip_footer_layout.width, pip_footer_layout.height } };
            canvas.push_clip(pip_footer.position,
                pip_footer.position + pip_footer.size);
            if (expanded_footer)
                add_wrapped_text(canvas, pip_footer.position, pip_metrics,
                    ui_layout::persistent_text_minimum_scale,
                    state_color, pip_footer.size.x, 2.0f);
            else
                add_wrapped_text(canvas, pip_footer.position, pip_metrics,
                    ui_layout::persistent_text_minimum_scale,
                    state_color, pip_footer.size.x, 2.0f);
            canvas.pop_clip();
            add_rounded_rect(canvas, rect, 10.0f, ui_render::transparent_fill, accent_dim, 1.5f);
        }

        void draw_visual_session_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            canvas.push_clip(rect.position + Vec2{ 1.0f, 1.0f },
                rect.position + rect.size - Vec2{ 1.0f, 1.0f });
            const float usable = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            const bool walk = presentation::walk_eye(presentation_session);
            const bool art = presentation::art_eye(presentation_session);
            const bool equipment =
                presentation::equipment_eye(presentation_session);
            const sim::Environment& environment = *course_eye_test_environment;

            add_text_fit(canvas, cursor, "ISOLATED VISUAL ACCEPTANCE", 1.46f,
                white, usable, 1.02f);
            cursor.y += 42.0f;
            cursor.y += add_wrapped_text(canvas, cursor,
                art ? "AUTHORED ART / PHYSICAL ANCHORS"
                    : walk ? "RETAINED WALK / PHYSICAL REPLAY"
                    : equipment ? "PHYSICAL EQUIPMENT / PLANTED AIM"
                    : "AUTHORED COURSE / MATERIAL CELLS",
                ui_layout::persistent_text_minimum_scale, accent, usable,
                2.0f) + 7.0f;
            cursor.y += add_wrapped_text(canvas, cursor,
                "PERSISTENCE OFF  /  TRAINER DISCONNECTED",
                ui_layout::persistent_text_minimum_scale,
                green, usable, 2.0f) + 9.0f;

            const float half = (usable - 6.0f) * 0.5f;
            const bool stacked_authority =
                ui_layout::visual_session_stacks_authority_controls(usable);
            const Rect action_button{ cursor,
                { stacked_authority ? usable : half, 48.0f } };
            const Rect authority_button{
                cursor + Vec2{ stacked_authority ? 0.0f : half + 6.0f,
                    stacked_authority ? 56.0f : 0.0f },
                { stacked_authority ? usable : half, 48.0f } };
            const bool terminal_frozen = walk && walk_eye_terminal.frozen;
            if (button(action_button,
                    walk ? (terminal_frozen ? "TERMINAL FROZEN"
                        : run_paused ? "REPLAY PAUSED - RUN"
                        : "REPLAY RUNNING - PAUSE")
                        : equipment ? "READY AIM / FRAME FROZEN"
                        : "FIXED PHYSICAL FRAME",
                    input, walk && !run_paused && !terminal_frozen,
                    walk && !terminal_frozen))
            {
                run_paused = !run_paused;
                set_status(run_paused ? "PHYSICAL REPLAY PAUSED" : "PHYSICAL REPLAY RESUMED");
            }
            const bool raw = environment.raw_policy_audit();
            if (button(authority_button,
                    walk ? (raw ? "RAW / SWITCH ASSISTED + RESTART"
                        : "ASSISTED / SWITCH RAW + RESTART")
                        : equipment ? "ASSISTED TEACHER / NOT RAW"
                        : "NO TRAINER INPUT",
                    input, walk && raw, walk))
            {
                const sim::GuidanceMode guidance = raw
                    ? sim::GuidanceMode::assisted
                    : sim::GuidanceMode::raw_policy_audit;
                if (walk_eye_replay_start.has_value())
                {
                    retry_walk_eye_replay(guidance);
                    set_status(std::format("TRIAL {} EXPLICIT RESTART - {}",
                        walk_eye_trial_id, raw ? "ASSISTED REPLAY ENABLED"
                            : "RAW POLICY / OPTIONAL GUIDANCE OFF"));
                }
            }
            cursor.y += stacked_authority ? 118.0f : 62.0f;

            if (walk)
            {
                const bool can_retry = presentation::replay_retry_available(
                    presentation_session, walk_eye_terminal.frozen);
                if (button({ cursor, { usable, 48.0f } },
                        can_retry ? "RETRY RETAINED START"
                            : "RETRY AVAILABLE AFTER TERMINAL",
                        input, false, can_retry))
                {
                    retry_walk_eye_replay(environment.guidance_mode());
                    set_status("EXPLICIT RETRY - RETAINED START RESTORED");
                }
                cursor.y += 57.0f;
            }

            const float third = (usable - 12.0f) / 3.0f;
            if (button({ cursor, { third, 48.0f } }, "ZOOM OUT", input))
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(
                    live_zoom_factor, -1.0f);
                live_zoom_auto = false;
            }
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 48.0f } },
                    "AUTO VIEW", input, live_zoom_auto))
            {
                live_zoom_factor = 1.0f;
                live_zoom_auto = true;
            }
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 48.0f } }, "ZOOM IN", input))
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(
                    live_zoom_factor, 1.0f);
                live_zoom_auto = false;
            }
            cursor.y += 57.0f;
            add_text_fit(canvas, cursor,
                std::format("VIEW {:.0f} PX/M  /  ZOOM {:.2f}X",
                    live_pixels_per_meter, live_zoom_factor),
                1.00f, white, usable,
                ui_layout::persistent_text_minimum_scale);
            cursor.y += 32.0f;
            if (button({ cursor, { half, 48.0f } }, "METRIC / 10 M", input,
                    distance_units == ui_layout::DistanceUnits::metric))
                distance_units = ui_layout::DistanceUnits::metric;
            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 48.0f } },
                    "IMPERIAL / 50 FT", input,
                    distance_units == ui_layout::DistanceUnits::imperial))
                distance_units = ui_layout::DistanceUnits::imperial;
            cursor.y += 62.0f;

            add_text_fit(canvas, cursor, "PHYSICAL SNAPSHOT", 1.10f, accent,
                usable, ui_layout::persistent_text_minimum_scale);
            cursor.y += 30.0f;
            const auto proof_line = [&](std::string_view text,
                Color color = muted)
            {
                cursor.y += add_wrapped_text(canvas, cursor, text,
                    ui_layout::persistent_text_minimum_scale,
                    color, usable, 3.0f) + 5.0f;
            };
            proof_line(
                std::format("{}  /  {}  /  {}",
                    sim::course_stage_name(environment.course_stage()),
                    sim::creature_species_name(environment.blueprint().presentation_species()),
                    sim::terrain_region_name(
                        environment.terrain_region_at(camera_x))), white);
            proof_line(
                std::format("CONTACT L {}  /  R {}  /  WATER {:.2f} M",
                    sim::foot_contact_phase_name(environment.left_foot_phase()),
                    sim::foot_contact_phase_name(environment.right_foot_phase()),
                    environment.water_depth()));
            proof_line(
                std::format("EQUIP {} / {}  /  HITS {}",
                    sim::weapon_class_name(environment.weapon_class()),
                    sim::equipment_state_name(environment.equipment_state()),
                    environment.target_hits()));
            if (equipment)
            {
                proof_line(
                    std::format(
                        "PLANT {:.2f} S / SETTLE {:.2f} S / AIM ERR {:.3f} RAD / SHOTS {}",
                        environment.equipment_plant_seconds(),
                        environment.equipment_aim_settle_seconds(),
                        environment.equipment_aim_error(),
                        environment.shots_fired()), yellow);
            }
            if (terminal_frozen)
            {
                proof_line(
                    std::format("TRIAL {} {} / X {:.2f} M / CELL {} / {}",
                        walk_eye_trial_id,
                        sim::trial_terminal_cause_name(
                            walk_eye_terminal.cause, walk_eye_terminal.reason),
                        walk_eye_terminal.position.x,
                        sim::DeformableTerrain::global_cell_x(
                            walk_eye_terminal.position.x),
                        sim::terrain_region_name(
                            walk_eye_terminal.terrain)), yellow);
            }
            cursor.y += 7.0f;
            add_wrapped_text(canvas, cursor,
                "ISOLATED: NO TRAINING OR SAVES. VIEW CONTROLS ONLY.",
                ui_layout::persistent_text_minimum_scale,
                green, usable, 3.0f);

            canvas.pop_clip();
            add_rounded_rect(canvas, rect, 11.0f,
                ui_render::transparent_fill, border, 1.0f);
        }

        void restart_live_replay(bool hold_start)
        {
            trainer.reset_preview();
            run_paused = hold_start;
            camera_x = 0.0f;
            live_pixels_per_meter = view_camera::default_pixels_per_meter;
            live_zoom_factor = 1.0f;
            live_zoom_auto = true;
            live_projection_initialized = false;
            set_status(hold_start ? "START POSE HELD" : "NEW REPLAY RUNNING");
        }

        void draw_live_panel(Rect rect, const InputState& raw_input,
            bool show_embedded_session = false)
        {
            if (presentation::isolated(presentation_session))
            {
                draw_visual_session_panel(rect, raw_input);
                return;
            }
            const bool embedded_rig_trainer = mode == Mode::rig_lab;
            float& active_scroll_offset = embedded_rig_trainer
                ? rig_trainer_scroll_offset : trainer_scroll_offset;
            float& active_content_height = embedded_rig_trainer
                ? rig_trainer_content_height : trainer_content_height;
            LivePanelPage& active_panel_page = embedded_rig_trainer
                ? rig_trainer_panel_page : live_panel_page;
            bool& active_raw_telemetry_expanded = embedded_rig_trainer
                ? rig_raw_telemetry_expanded : live_raw_telemetry_expanded;
            active_scroll_offset = ui_layout::panel_scroll_offset(
                active_scroll_offset,
                contains(rect, raw_input.mouse) ? raw_input.wheel : 0.0f,
                active_content_height, rect.size.y);
            const InputState input = panel_input(rect, raw_input);
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            canvas.push_clip(rect.position + Vec2{ 1.0f, 1.0f },
                rect.position + rect.size - Vec2{ 1.0f, 1.0f });
            const float usable_width = rect.size.x - 36.0f;
            Vec2 cursor = rect.position
                + Vec2{ 18.0f, 16.0f
                    - (embedded_rig_trainer ? 0.0f : active_scroll_offset) };
            rl::AutonomyStatus autonomy = trainer.autonomy_status();
            if (course_eye_test_environment.has_value())
            {
                autonomy.stage = course_eye_test_environment->course_stage();
                autonomy.difficulty = course_eye_test_environment->course_difficulty();
                autonomy.mastery_streak = 0;
            }
            const rl::TrainingMetrics& metrics = trainer.metrics();
            const telemetry::LessonProgress progress = telemetry::lesson_progress(autonomy);
            const telemetry::StatusSummary human_status = telemetry::status_summary(
                autonomy, metrics, trainer.background_enabled(), trainer.has_best_policy(),
                trainer.controller_state_name());
            const Color human_color = telemetry_color(human_status.tone);
            const int progress_percent = static_cast<int>(std::lround(progress.overall * 100.0f));
            constexpr director::TaskGraph task_graph =
                director::default_training_graph();
            director_browser_index = std::min<std::uint16_t>(
                director_browser_index,
                static_cast<std::uint16_t>(task_graph.count - 1u));
            const director::TaskNode& browsed =
                task_graph.nodes[director_browser_index];
            const auto profile_name = [](director::Profile profile) noexcept
                -> std::string_view
            {
                switch (profile)
                {
                case director::Profile::autonomous_npc: return "AUTONOMOUS NPC";
                case director::Profile::player_guided: return "PLAYER GUIDED";
                case director::Profile::curriculum: return "CURRICULUM";
                }
                return "CURRICULUM";
            };
            const director::Profile requested_profile = autonomy.director_profile
                    == director::Profile::curriculum
                ? director::Profile::autonomous_npc
                : autonomy.director_profile == director::Profile::autonomous_npc
                    ? director::Profile::player_guided
                    : director::Profile::curriculum;
            const sim::GuidanceMode active_guidance =
                trainer.preview_guidance_mode();
            const bool raw_audit = active_guidance
                == sim::GuidanceMode::raw_policy_audit;
            const bool retained_raw = trainer.replay_uses_retained_controller()
                && trainer.replay_controller_guidance_mode()
                    == sim::GuidanceMode::raw_policy_audit;
            const std::string_view guidance_summary = !raw_audit
                ? "PREVIEW AUTHORITY   ASSISTED / TEACHER"
                : retained_raw
                    ? "PREVIEW AUTHORITY   RAW POLICY / JOINT OUTPUTS ONLY"
                    : trainer.replay_uses_retained_controller()
                        ? "PREVIEW AUTHORITY   RAW AUDIT / ASSISTED SOURCE"
                        : "PREVIEW AUTHORITY   RAW AUDIT / UNRETAINED";
            const bool show_primary_session = (mode == Mode::live
                    && active_panel_page == LivePanelPage::summary)
                || (embedded_rig_trainer && show_embedded_session);
            // Page navigation is a stable control surface. Scroll only the
            // selected page body so Mission/View/Details context never leaves
            // the rail or exposes a clipped half-line beneath the header.
            constexpr float control_layout_scroll = 0.0f;
            const ui_layout::TrainerControlGroupLayout control_groups =
                ui_layout::trainer_control_group_layout(
                    { rect.position.x, rect.position.y,
                        rect.size.x, rect.size.y },
                    control_layout_scroll, show_primary_session,
                    mode == Mode::live);

            add_text_fit(canvas, cursor,
                mode == Mode::live ? "AUTOPILOT"
                                   : "TRAINING EVIDENCE",
                ui_layout::workspace_heading_text_scale,
                white, usable_width, 1.08f);
            if (show_primary_session)
            {
            add_rounded_rect(canvas,
                { { control_groups.primary.x, control_groups.primary.y },
                    { control_groups.primary.width,
                        control_groups.primary.height } },
                10.0f, panel_alt, accent_dim, 1.0f);
            cursor = { control_groups.primary.x + 14.0f,
                control_groups.primary.y + 12.0f };
            add_text_fit(canvas, cursor,
                "SESSION",
                1.12f, accent, usable_width,
                ui_layout::trainer_primary_minimum_text_scale);
            cursor.y += 28.0f;
            if (button({ cursor, { usable_width,
                    ui_layout::trainer_primary_button_height } },
                trainer.background_enabled()
                    ? (embedded_rig_trainer
                        ? "PAUSE TRAINING" : "PAUSE AUTOPILOT")
                    : (embedded_rig_trainer
                        ? "START TRAINING" : "START AUTOPILOT"),
                input, trainer.background_enabled()))
            {
                trainer.set_background_enabled(!trainer.background_enabled());
                set_status(trainer.background_enabled() ? "BACKGROUND TRAINING RESUMED" : "BACKGROUND TRAINING PAUSED");
            }

            cursor.y += ui_layout::trainer_primary_button_height + 12.0f;
            if (mode == Mode::live)
            {
                constexpr float gap = 8.0f;
                const float width = (usable_width - gap) * 0.5f;
                if (button({ cursor, { width, ui_layout::trainer_primary_button_height } },
                    "RESTART", input, false))
                    restart_live_replay(false);
                if (button({ cursor + Vec2{ width + gap, 0.0f },
                        { width, ui_layout::trainer_primary_button_height } },
                    run_paused ? "PLAY" : "HOLD START", input, run_paused))
                {
                    if (run_paused)
                    {
                        run_paused = false;
                        set_status("REPLAY RUNNING");
                    }
                    else
                        restart_live_replay(true);
                }
                cursor.y += ui_layout::trainer_primary_button_height + 12.0f;
            }

            const auto session_gait_name = [](sim::GaitTask task) noexcept
                -> std::string_view
            {
                switch (task)
                {
                case sim::GaitTask::walk: return "WALK";
                case sim::GaitTask::speed_walk: return "SPEED WALK";
                case sim::GaitTask::walk_run_transition: return "WALK / RUN";
                case sim::GaitTask::run: return "RUN";
                }
                return "WALK";
            };
            const std::string lesson_name = autonomy.stage == sim::CourseStage::uneven
                ? std::format("3. {}", session_gait_name(autonomy.gait_task))
                : std::string{ sim::course_stage_name(autonomy.stage) };
            add_text_fit(canvas, cursor,
                std::format("TRAINING LEVEL {:.0f}%",
                    autonomy.difficulty * 100.0f),
                1.02f, muted, usable_width,
                ui_layout::trainer_primary_minimum_text_scale);
            cursor.y += 26.0f;
            add_text_fit(canvas, cursor, lesson_name, 1.68f,
                accent, usable_width,
                ui_layout::trainer_primary_minimum_text_scale);
            cursor.y += 36.0f;
            add_text_fit(canvas, cursor,
                std::format("LESSON COMPLETION {}%", progress_percent),
                1.14f, human_color, usable_width,
                ui_layout::trainer_primary_minimum_text_scale);
            cursor.y += 28.0f;
            const Rect progress_track{ cursor, { usable_width, 16.0f } };
            fill_rounded_rect(canvas, progress_track, 6.0f, rgb(0x101820));
            fill_rounded_rect(canvas,
                { progress_track.position,
                    { progress_track.size.x * progress.overall, progress_track.size.y } },
                6.0f, human_color);
            add_rounded_rect(canvas, progress_track, 6.0f, ui_render::transparent_fill, border, 1.0f);
            }

            const bool quick_live_surface = mode == Mode::live
                && show_primary_session
                && active_panel_page == LivePanelPage::summary;
            if (quick_live_surface)
            {
                const std::array<ui_layout::Box, 4> quick_actions =
                    ui_layout::trainer_quick_action_layout(
                        control_groups.page_tabs);
                if (button({ { quick_actions[0].x, quick_actions[0].y },
                        { quick_actions[0].width, quick_actions[0].height } },
                    "MISSION", input))
                {
                    active_panel_page = LivePanelPage::mission;
                    active_scroll_offset = 0.0f;
                }
                if (button({ { quick_actions[1].x, quick_actions[1].y },
                        { quick_actions[1].width, quick_actions[1].height } },
                    "VIEW", input))
                {
                    active_panel_page = LivePanelPage::view;
                    active_scroll_offset = 0.0f;
                }
                if (button({ { quick_actions[2].x, quick_actions[2].y },
                        { quick_actions[2].width, quick_actions[2].height } },
                    "DETAILS", input))
                {
                    active_panel_page = LivePanelPage::advanced;
                    active_scroll_offset = 0.0f;
                }
                if (button({ { quick_actions[3].x, quick_actions[3].y },
                        { quick_actions[3].width, quick_actions[3].height } },
                    live_art_enabled ? "ART ON" : "ART OFF", input,
                    live_art_enabled))
                {
                    live_art_enabled = !live_art_enabled;
                    set_status(live_art_enabled
                        ? "LIVE ART ENABLED" : "LIVE ART DISABLED");
                }
            }
            else
            {
                if (mode == Mode::live)
                {
                    const std::array<ui_layout::Box, 3> page_buttons =
                        ui_layout::trainer_live_page_button_layout(
                            control_groups.page_tabs);
                    constexpr std::array<std::string_view, 3> page_labels{
                        "MISSION", "VIEW", "DETAILS" };
                    constexpr std::array<LivePanelPage, 3> pages{
                        LivePanelPage::mission, LivePanelPage::view,
                        LivePanelPage::advanced };
                    for (std::size_t page_index = 0;
                        page_index < page_buttons.size(); ++page_index)
                    {
                        const ui_layout::Box tab = page_buttons[page_index];
                        const bool selected = page_index == 2u
                            ? active_panel_page == LivePanelPage::totals
                                || active_panel_page
                                    == LivePanelPage::advanced
                            : active_panel_page == pages[page_index];
                        if (button({ { tab.x, tab.y },
                                { tab.width, tab.height } },
                            page_labels[page_index], input, selected))
                        {
                            active_panel_page = pages[page_index];
                            active_scroll_offset = 0.0f;
                            active_raw_telemetry_expanded = false;
                        }
                    }
                }
                else
                {
                    const std::array<ui_layout::Box, 3> page_buttons =
                        ui_layout::trainer_page_button_layout(
                            control_groups.page_tabs);
                    constexpr std::array<std::string_view, 3> page_labels{
                        "STATUS", "MISSION", "DETAILS" };
                    constexpr std::array<LivePanelPage, 3> pages{
                        LivePanelPage::summary, LivePanelPage::mission,
                        LivePanelPage::advanced };
                    for (std::size_t page_index = 0;
                        page_index < page_buttons.size(); ++page_index)
                    {
                        const ui_layout::Box tab = page_buttons[page_index];
                        if (button({ { tab.x, tab.y },
                                { tab.width, tab.height } },
                            page_labels[page_index], input,
                            page_index == 2u
                                ? active_panel_page == LivePanelPage::view
                                    || active_panel_page == LivePanelPage::totals
                                    || active_panel_page
                                        == LivePanelPage::advanced
                                : active_panel_page == pages[page_index]))
                        {
                            active_panel_page = pages[page_index];
                            active_scroll_offset = 0.0f;
                            active_raw_telemetry_expanded = false;
                        }
                    }
                }
            }
            const ui_layout::Box page_body = quick_live_surface
                ? ui_layout::trainer_quick_page_body(control_groups.page_tabs)
                : control_groups.page_body;
            cursor = { page_body.x + 7.0f,
                page_body.y + 5.0f - active_scroll_offset };
            canvas.push_clip(
                { rect.position.x + 1.0f, page_body.y },
                rect.position + rect.size - Vec2{ 1.0f, 1.0f });

            if (active_panel_page == LivePanelPage::summary)
            {
                std::string evidence{};
                switch (autonomy.stage)
                {
                case sim::CourseStage::balance:
                {
                    const std::uint32_t valid_seeds = 6u
                        - std::min<std::uint32_t>(metrics.evaluation_invalid_runs, 6u);
                    evidence = std::format(
                        "UP {:.0f}/6S  SEEDS {}/6",
                        metrics.evaluation_longest_stance, valid_seeds);
                    break;
                }
                case sim::CourseStage::duck_press:
                    evidence = std::format(
                        "LOW {:.0f}S  UP {:.0f}  SAFE {:.0f}S",
                        metrics.evaluation_duck_seconds,
                        metrics.evaluation_duck_recoveries,
                        metrics.evaluation_survival);
                    break;
                case sim::CourseStage::shuttle:
                    evidence = std::format(
                        "{}  STEP{:.0f}  SAFE{:.0f}S",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events,
                        metrics.evaluation_survival);
                    break;
                case sim::CourseStage::uneven:
                    evidence = std::format(
                        "{:.0f}M  STEP{:.0f}  SAFE{:.0f}S  {}/{}",
                        std::max(0.0f, metrics.evaluation_distance),
                        metrics.evaluation_stride_events,
                        std::max(0.0f, metrics.evaluation_survival),
                        autonomy.mastery_streak,
                        rl::required_mastery_confirmations(autonomy.stage));
                    break;
                case sim::CourseStage::crouch_walk:
                    evidence = std::format(
                        "LOW{:.0f}S  STEP{:.0f}  CLEAR{:.0f}",
                        metrics.evaluation_duck_seconds,
                        metrics.evaluation_stride_events,
                        metrics.evaluation_obstacles_passed);
                    break;
                case sim::CourseStage::ramps:
                    evidence = std::format(
                        "JUMP{:.0f}  LAND{:.0f}  {}",
                        metrics.evaluation_powered_jumps,
                        metrics.evaluation_jump_landings,
                        format_distance(std::max(0.0f, metrics.evaluation_distance)));
                    break;
                case sim::CourseStage::hurdles:
                    evidence = std::format(
                        "CLEAR{:.0f}  LAND{:.0f}  {}",
                        metrics.evaluation_obstacles_passed,
                        metrics.evaluation_jump_landings,
                        format_distance(std::max(0.0f, metrics.evaluation_distance)));
                    break;
                case sim::CourseStage::duck_bars:
                    evidence = std::format(
                        "FLIP{:.0f}  TURN{:.1f}  JUMP{:.0f}",
                        metrics.evaluation_spin_landings,
                        metrics.evaluation_spin_turns,
                        metrics.evaluation_powered_jumps);
                    break;
                case sim::CourseStage::moving_hazards:
                    evidence = std::format(
                        "{}  STEP{:.0f}  CLEAR{:.0f}",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events,
                        metrics.evaluation_obstacles_passed);
                    break;
                case sim::CourseStage::climb_descent:
                    evidence = std::format(
                        "HAND{:.0f}  XFER{:.0f}  UP{:.0f}/DN{:.0f}",
                        metrics.evaluation_hand_contacts, metrics.evaluation_climb_transfers,
                        metrics.evaluation_climbs, metrics.evaluation_descents);
                    break;
                case sim::CourseStage::equipment_targets:
                    evidence = std::format(
                        "HIT{:.0f}/{:.0f}  HANDLE{:.0f}",
                        metrics.evaluation_target_hits, metrics.evaluation_shots,
                        metrics.evaluation_equipment_transitions);
                    break;
                case sim::CourseStage::combat_course:
                    evidence = std::format(
                        "{}  STEP{:.0f}  HIT{:.0f}",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events, metrics.evaluation_target_hits);
                    break;
                }

                const ui_layout::TrainerSummaryLayout summary_layout =
                    ui_layout::trainer_summary_layout(page_body);
                const std::string_view summary_headline = !summary_layout.wide
                        && human_status.headline == "RETRYING AFTER A FAILED TEST"
                    ? std::string_view{ "RETRYING FAILED TEST" }
                    : human_status.headline;
                const auto draw_summary_wrapped = [&](ui_layout::Box lane,
                    std::string_view text, float scale, Color color)
                {
                    canvas.push_clip({ lane.x, lane.y },
                        { lane.x + lane.width, lane.y + lane.height });
                    add_wrapped_text(canvas, { lane.x, lane.y }, text,
                        scale, color, lane.width, 3.0f);
                    canvas.pop_clip();
                };
                const std::string retained_status = trainer.has_best_policy()
                    ? std::string("CONTROLLER SAVED")
                    : std::string("NO SAVED CONTROLLER");
                if (quick_live_surface)
                {
                    const Rect quick_status{
                        { page_body.x, page_body.y },
                        { page_body.width, 154.0f } };
                    add_rounded_rect(canvas, quick_status, 10.0f,
                        panel_alt, border, 1.0f);
                    Vec2 quick_cursor = quick_status.position
                        + Vec2{ 14.0f, 14.0f };
                    add_text_fit(canvas, quick_cursor, summary_headline,
                        1.56f, human_color,
                        quick_status.size.x - 28.0f,
                        ui_layout::persistent_text_minimum_scale);
                    quick_cursor.y += 48.0f;
                    const auto concise_goal = [](sim::CourseStage stage)
                        noexcept -> std::string_view
                    {
                        switch (stage)
                        {
                        case sim::CourseStage::balance:
                            return "STAY UPRIGHT FOR SIX SECONDS.";
                        case sim::CourseStage::duck_press:
                            return "CROUCH, HOLD, THEN STAND.";
                        case sim::CourseStage::shuttle:
                            return "TURN CLEANLY AT EACH END.";
                        case sim::CourseStage::uneven:
                            return "CROSS THE COURSE WITH STABLE STEPS.";
                        case sim::CourseStage::crouch_walk:
                            return "CROUCH-WALK UNDER THE BAR.";
                        case sim::CourseStage::ramps:
                            return "CLIMB AND LAND THE RAMP.";
                        case sim::CourseStage::hurdles:
                            return "CLEAR EACH HURDLE AND LAND.";
                        case sim::CourseStage::duck_bars:
                            return "DUCK, TURN, AND RECOVER.";
                        case sim::CourseStage::moving_hazards:
                            return "PASS THE MOVING HAZARDS SAFELY.";
                        case sim::CourseStage::climb_descent:
                            return "CLIMB, TRANSFER, AND DESCEND.";
                        case sim::CourseStage::equipment_targets:
                            return "PLANT, AIM, THEN FIRE.";
                        case sim::CourseStage::combat_course:
                            return "TRAVERSE, AIM, AND HIT TARGETS.";
                        }
                        return "COMPLETE THE CURRENT MISSION.";
                    };
                    add_wrapped_text(canvas, quick_cursor,
                        std::format("NEXT: {}",
                            concise_goal(autonomy.stage)),
                        ui_layout::persistent_text_minimum_scale,
                        accent, quick_status.size.x - 28.0f, 4.0f);
                    cursor.y = quick_status.position.y
                        + quick_status.size.y;
                }
                else
                {
                add_rounded_rect(canvas,
                    { { summary_layout.card.x, summary_layout.card.y },
                        { summary_layout.card.width,
                            summary_layout.card.height } },
                    10.0f, panel_alt, border, 1.0f);
                add_text_fit(canvas,
                    { summary_layout.heading.x, summary_layout.heading.y },
                    "STATUS", 1.22f, accent,
                    summary_layout.heading.width,
                    ui_layout::persistent_text_minimum_scale);
                add_text_fit(canvas,
                    { summary_layout.headline.x, summary_layout.headline.y },
                    summary_headline, 1.50f, human_color,
                    summary_layout.headline.width,
                    summary_layout.wide ? 1.14f
                        : ui_layout::persistent_text_minimum_scale);
                if (summary_layout.wide)
                {
                    add_text_fit(canvas,
                        { summary_layout.evidence.x,
                            summary_layout.evidence.y },
                        evidence, 1.10f,
                        metrics.evaluation_valid ? green : yellow,
                        summary_layout.evidence.width,
                        ui_layout::persistent_text_minimum_scale);
                }
                else
                {
                    draw_summary_wrapped(summary_layout.evidence, evidence,
                        ui_layout::persistent_text_minimum_scale,
                        metrics.evaluation_valid ? green : yellow);
                }
                add_text_fit(canvas,
                    { summary_layout.retained_controller.x,
                        summary_layout.retained_controller.y },
                    retained_status,
                    1.06f, trainer.has_best_policy() ? green : yellow,
                    summary_layout.retained_controller.width,
                    ui_layout::persistent_text_minimum_scale);
                add_text(canvas,
                    { summary_layout.next_goal_label.x,
                        summary_layout.next_goal_label.y },
                    "NEXT GOAL", 1.04f, accent);
                draw_summary_wrapped(summary_layout.next_goal,
                    telemetry::stage_goal(autonomy.stage), 1.04f, white);
                cursor.y = summary_layout.card.y
                    + summary_layout.card.height;
                }
            }
            else if (active_panel_page == LivePanelPage::mission)
            {
                add_rounded_rect(canvas,
                    { { page_body.x,
                            page_body.y },
                        { page_body.width,
                            ui_layout::trainer_mission_card_height } },
                    10.0f, panel_alt, border, 1.0f);
                add_text_fit(canvas, cursor, "MISSION CONTROLS",
                    ui_layout::section_heading_text_scale, accent, usable_width,
                    ui_layout::trainer_primary_minimum_text_scale);
                cursor.y += 40.0f;
                add_text_fit(canvas, cursor, "GAIT", 1.25f, muted,
                    usable_width, ui_layout::persistent_text_minimum_scale);
                cursor.y += 30.0f;
                constexpr std::array gait_tasks{
                    sim::GaitTask::walk, sim::GaitTask::speed_walk,
                    sim::GaitTask::walk_run_transition, sim::GaitTask::run };
                constexpr std::array<std::uint16_t, 4> gait_task_indices{
                    1u, 7u, 8u, 9u };
                constexpr std::array<std::string_view, 4> gait_labels{
                    "WALK", "SPEED WALK", "WALK / RUN", "RUN" };
                for (std::size_t gait_index = 0u;
                    gait_index < gait_tasks.size(); ++gait_index)
                {
                    const std::uint16_t task_index =
                        gait_task_indices[gait_index];
                    const director::TaskNode& gait_node =
                        task_graph.nodes[task_index];
                    if (button({ cursor,
                            { usable_width,
                                ui_layout::trainer_gait_button_height } },
                        gait_labels[gait_index], input,
                        autonomy.director_stable_task_id
                            == gait_node.stable_id))
                    {
                        director_browser_index = task_index;
                        trainer.select_director_task(task_index);
                        set_status(std::format(
                            "DIRECTOR GAIT REQUEST - {}",
                            director::task_name(gait_node.task)));
                    }
                    cursor.y += ui_layout::trainer_gait_button_height
                        + ui_layout::trainer_gait_button_gap;
                }
                cursor.y += 8.0f;
                add_text_fit(canvas, cursor, "AUTHORITY", 1.25f, muted,
                    usable_width, ui_layout::persistent_text_minimum_scale);
                cursor.y += 30.0f;
                if (button({ cursor,
                        { usable_width,
                            ui_layout::trainer_secondary_button_height } },
                    raw_audit ? "RAW POLICY AUDIT" : "ASSISTED AUTHORITY",
                    input, raw_audit))
                {
                    const sim::GuidanceMode new_mode = raw_audit
                        ? sim::GuidanceMode::assisted
                        : sim::GuidanceMode::raw_policy_audit;
                    trainer.set_preview_guidance_mode(new_mode);
                    trainer.reset_preview();
                    set_status(new_mode == sim::GuidanceMode::raw_policy_audit
                        ? "RAW POLICY AUDIT ENABLED"
                        : "ASSISTED GUIDANCE ENABLED");
                }
                cursor.y += ui_layout::trainer_secondary_button_height
                    + 18.0f;
                add_text_fit(canvas, cursor, "TASK GRAPH", 1.25f, muted,
                    usable_width, ui_layout::persistent_text_minimum_scale);
                cursor.y += 30.0f;
                add_text_fit(canvas, cursor,
                    std::format("TASK {:02} OF {:02}",
                        director_browser_index + 1u, task_graph.count),
                    1.02f, muted, usable_width,
                    ui_layout::persistent_text_minimum_scale);
                cursor.y += 27.0f;
                add_text_fit(canvas, cursor,
                    std::format("{} / {}",
                        director::task_name(browsed.task),
                        director::challenge_name(browsed.challenge)),
                    1.32f, browsed.enabled ? white : yellow,
                    usable_width,
                    ui_layout::trainer_primary_minimum_text_scale);
                cursor.y += 38.0f;
                const float director_width = (usable_width - 8.0f) * 0.5f;
                if (button({ cursor,
                        { director_width,
                            ui_layout::trainer_task_button_height } },
                    "PREVIOUS", input))
                {
                    director_browser_index = director_browser_index == 0u
                        ? static_cast<std::uint16_t>(task_graph.count - 1u)
                        : static_cast<std::uint16_t>(
                            director_browser_index - 1u);
                }
                if (button({ cursor + Vec2{ director_width + 8.0f, 0.0f },
                        { director_width,
                            ui_layout::trainer_task_button_height } },
                    "NEXT", input))
                {
                    director_browser_index = static_cast<std::uint16_t>(
                        (director_browser_index + 1u) % task_graph.count);
                }
                cursor.y += ui_layout::trainer_task_button_height + 12.0f;
                if (button({ cursor,
                        { usable_width,
                            ui_layout::trainer_task_button_height } },
                    browsed.enabled ? "SELECT TASK" : "TASK LOCKED", input,
                    autonomy.director_stable_task_id == browsed.stable_id,
                    browsed.enabled))
                {
                    trainer.select_director_task(director_browser_index);
                    set_status(std::format(
                        "DIRECTOR REQUEST QUEUED - {} / {} - PREREQUISITES WILL BE VERIFIED",
                        director::task_name(browsed.task),
                        director::challenge_name(browsed.challenge)));
                }
                cursor.y += ui_layout::trainer_task_button_height + 12.0f;
                const auto compact_profile_name = [](director::Profile profile)
                    noexcept -> std::string_view
                {
                    switch (profile)
                    {
                    case director::Profile::autonomous_npc:
                        return "NPC PROFILE";
                    case director::Profile::player_guided:
                        return "PLAYER PROFILE";
                    case director::Profile::curriculum:
                        return "CURRICULUM PROFILE";
                    }
                    return "CURRICULUM PROFILE";
                };
                if (button({ cursor,
                        { usable_width,
                            ui_layout::trainer_task_button_height } },
                    compact_profile_name(autonomy.director_profile),
                    input, true))
                {
                    trainer.set_director_profile(requested_profile);
                    set_status(std::format(
                        "DIRECTOR PROFILE REQUEST QUEUED - {} - ASYNC VALIDATION WILL REPORT RESULT",
                        profile_name(requested_profile)));
                }
                cursor.y += ui_layout::trainer_task_button_height + 16.0f;
                add_text_fit(canvas, cursor,
                    std::format("MASTERY {:.0f}%   FAILURES {}",
                        std::clamp(autonomy.director_task_mastery,
                            0.0f, 1.0f) * 100.0f,
                        autonomy.director_task_failures),
                    1.00f, browsed.enabled ? muted : yellow,
                    usable_width,
                    ui_layout::persistent_text_minimum_scale);
                cursor.y = std::max(cursor.y,
                    page_body.y
                        + ui_layout::trainer_mission_card_height);
            }
            else if (active_panel_page == LivePanelPage::view)
            {
                const float view_card_height =
                    ui_layout::trainer_view_card_height_for_width(
                        usable_width)
                    + (embedded_rig_trainer
                        ? ui_layout::trainer_secondary_button_height + 10.0f
                        : 0.0f);
                add_rounded_rect(canvas,
                    { { page_body.x,
                            page_body.y },
                        { page_body.width,
                            view_card_height } },
                    10.0f, panel_alt, border, 1.0f);
                if (embedded_rig_trainer)
                {
                    if (button({ cursor,
                            { usable_width,
                                ui_layout::trainer_secondary_button_height } },
                        "BACK TO DETAILS", input))
                    {
                        active_panel_page = LivePanelPage::advanced;
                        active_scroll_offset = 0.0f;
                    }
                    cursor.y += ui_layout::trainer_secondary_button_height
                        + 10.0f;
                }
                add_text_fit(canvas, cursor, "VIEW CONTROLS",
                    ui_layout::section_heading_text_scale, accent,
                    usable_width,
                    ui_layout::trainer_primary_minimum_text_scale);
                cursor.y += 42.0f;
                add_text_fit(canvas, cursor, "TRAINING CPU BUDGET", 1.25f,
                    muted, usable_width,
                    ui_layout::persistent_text_minimum_scale);
                cursor.y += 32.0f;
                const auto choice_rect = [](ui_layout::Box box) noexcept
                {
                    return Rect{ { box.x, box.y },
                        { box.width, box.height } };
                };
                const std::array<ui_layout::Box, 3> rate_buttons =
                    ui_layout::trainer_three_choice_layout(
                        { cursor.x, cursor.y, usable_width, 0.0f });
                if (button(choice_rect(rate_buttons[0]),
                    "ECO", input, trainer.updates_per_cycle() == 1))
                    trainer.set_updates_per_cycle(1);
                if (button(choice_rect(rate_buttons[1]),
                    "BALANCED", input, trainer.updates_per_cycle() == 2))
                    trainer.set_updates_per_cycle(2);
                if (button(choice_rect(rate_buttons[2]),
                    "MAXIMUM", input, trainer.updates_per_cycle() == 4))
                    trainer.set_updates_per_cycle(4);
                cursor.y += ui_layout::trainer_three_choice_height(
                    usable_width) + 18.0f;
                add_text_fit(canvas, cursor, "CAMERA", 1.25f,
                    muted, usable_width,
                    ui_layout::persistent_text_minimum_scale);
                cursor.y += 32.0f;
                const bool editor_zoom_controls = mode == Mode::rig_lab;
                const std::array<ui_layout::Box, 3> camera_buttons =
                    ui_layout::trainer_three_choice_layout(
                        { cursor.x, cursor.y, usable_width, 0.0f });
                if (button(choice_rect(camera_buttons[0]),
                    "ZOOM OUT", input))
                {
                    if (editor_zoom_controls)
                        art_editor_zoom = ui_layout::rig_lab_zoom_step(
                            art_editor_zoom, -1.0f);
                    else
                    {
                        live_zoom_factor = view_camera::apply_wheel_zoom(
                            live_zoom_factor, -1.0f);
                        live_zoom_auto = false;
                    }
                }
                const std::string fit_zoom_label = editor_zoom_controls
                    ? std::format("FIT {:.2f}X", art_editor_zoom)
                    : live_zoom_auto ? std::string("AUTO VIEW")
                        : std::format("VIEW {:.2f}X", live_zoom_factor);
                if (button(choice_rect(camera_buttons[1]),
                    fit_zoom_label, input,
                    editor_zoom_controls
                        ? std::abs(art_editor_zoom
                            - ui_layout::rig_lab_default_zoom) < 1.0e-5f
                        : live_zoom_auto))
                {
                    if (editor_zoom_controls)
                        art_editor_zoom = ui_layout::rig_lab_default_zoom;
                    else
                    {
                        live_zoom_factor = 1.0f;
                        live_zoom_auto = true;
                    }
                }
                if (button(choice_rect(camera_buttons[2]),
                    "ZOOM IN", input))
                {
                    if (editor_zoom_controls)
                        art_editor_zoom = ui_layout::rig_lab_zoom_step(
                            art_editor_zoom, 1.0f);
                    else
                    {
                        live_zoom_factor = view_camera::apply_wheel_zoom(
                            live_zoom_factor, 1.0f);
                        live_zoom_auto = false;
                    }
                }
                cursor.y += ui_layout::trainer_three_choice_height(
                    usable_width) + 18.0f;
                add_text_fit(canvas, cursor, "UNITS", 1.25f,
                    muted, usable_width,
                    ui_layout::persistent_text_minimum_scale);
                cursor.y += 32.0f;
                const float half = std::max(0.0f,
                    (usable_width - ui_layout::trainer_page_tab_gap) * 0.5f);
                if (button({ cursor, { half,
                        ui_layout::trainer_secondary_button_height } },
                    "METRIC", input,
                    distance_units == ui_layout::DistanceUnits::metric))
                    distance_units = ui_layout::DistanceUnits::metric;
                if (button({ cursor + Vec2{
                            half + ui_layout::trainer_page_tab_gap, 0.0f },
                        { half,
                            ui_layout::trainer_secondary_button_height } },
                    "IMPERIAL", input,
                    distance_units == ui_layout::DistanceUnits::imperial))
                    distance_units = ui_layout::DistanceUnits::imperial;
                cursor.y = std::max(cursor.y
                        + ui_layout::trainer_secondary_button_height,
                    page_body.y
                        + view_card_height);
            }
            else if (active_panel_page == LivePanelPage::totals)
            {
                add_rounded_rect(canvas,
                    { { page_body.x,
                            page_body.y },
                        { page_body.width,
                            ui_layout::trainer_totals_card_height } },
                    8.0f, panel_alt, border, 1.0f);
                add_text_fit(canvas, cursor, "TRAINING HISTORY",
                    ui_layout::section_heading_text_scale, accent, usable_width,
                    ui_layout::persistent_text_minimum_scale);
                cursor.y += 42.0f;
                if (button({ cursor,
                        { usable_width,
                            ui_layout::trainer_secondary_button_height } },
                    "BACK TO DETAILS", input))
                {
                    active_panel_page = LivePanelPage::advanced;
                    active_raw_telemetry_expanded = false;
                    active_scroll_offset = 0.0f;
                }
                cursor.y += ui_layout::trainer_secondary_button_height
                    + 18.0f;
                const auto totals_line = [&](std::string_view text,
                    Color color = white)
                {
                    cursor.y += add_wrapped_text(canvas, cursor, text,
                        ui_layout::persistent_text_minimum_scale,
                        color, usable_width, 4.0f) + 7.0f;
                };
                totals_line(std::format(
                    "TOTAL RIG UPDATES {}", metrics.total_updates));
                totals_line(telemetry::total_updates_help(), muted);
                const auto totals_heading = [&](std::string_view text)
                {
                    cursor.y += 6.0f;
                    add_text_fit(canvas, cursor, text, 1.10f, accent,
                        usable_width,
                        ui_layout::persistent_text_minimum_scale);
                    cursor.y += 30.0f;
                };
                totals_heading(ui_layout::training_totals_scope_headings[0]);
                totals_line(
                    std::format("WALL TIME {}   OPTIMIZER UPDATES {}",
                        format_duration(rig_lifetime_seconds),
                        ui_layout::lifetime_delta(
                            metrics.total_updates, rig_start_total_updates)));
                totals_line(
                    std::format("COMPLETED AGENT RUNS {}   PASSED STAGE CHECKS {}   FAILED STAGE CHECKS {}",
                        ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                        ui_layout::lifetime_delta(metrics.total_valid_episodes,
                            rig_start_valid_episodes),
                        ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                            rig_start_invalid_episodes)));
                totals_line(
                    std::format("AGENT-EQUIVALENT DISTANCE {}   SUPPORT GAIT CYCLES {}   FALLS {}",
                        format_distance(static_cast<float>(std::max(0.0,
                            metrics.total_distance - rig_start_distance))),
                        ui_layout::lifetime_delta(metrics.total_alternating_steps,
                            rig_start_steps),
                        ui_layout::lifetime_delta(
                            metrics.total_falls, rig_start_falls)));
                totals_line(
                    std::format("COLLISIONS {}   FEATURES CLEARED {}   BEST LESSON {}",
                        ui_layout::lifetime_delta(metrics.total_collisions,
                            rig_start_collisions),
                        ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                            rig_start_obstacles),
                        static_cast<unsigned>(
                            sim::course_stage_curriculum_index(
                                static_cast<sim::CourseStage>(
                                    rig_best_stage))) + 1u));

                totals_heading(ui_layout::training_totals_scope_headings[1]);
                totals_line(
                    std::format("WALL TIME {}   AGENT-SIM TIME {}",
                        format_duration(session_runtime_seconds),
                        format_duration(static_cast<float>(
                            session_totals.agent_sim_seconds))));
                totals_line(
                    std::format("COMPLETED AGENT RUNS {}   PASSED {}   FAILED {}",
                        session_totals.completed_agent_runs,
                        session_totals.passed_stage_checks,
                        session_totals.failed_stage_checks));
                totals_line(
                    std::format("AGENT-EQUIVALENT DISTANCE {}   SUPPORT GAIT CYCLES {}   FALLS {}",
                        format_distance(static_cast<float>(
                            session_totals.agent_distance)),
                        session_totals.support_gait_cycles,
                        session_totals.falls));

                totals_heading(ui_layout::training_totals_scope_headings[2]);
                totals_line(
                    std::format("TOTAL OPTIMIZER UPDATES {}   COMPLETED AGENT RUNS {}   PASSED {}   FAILED {}",
                        metrics.total_updates, metrics.total_episodes,
                        metrics.total_valid_episodes,
                        metrics.total_invalid_episodes));
                totals_line(
                    std::format("AGENT-EQUIVALENT DISTANCE {}   SUPPORT GAIT CYCLES {}   FALLS {}",
                        format_distance(static_cast<float>(metrics.total_distance)),
                        metrics.total_alternating_steps,
                        metrics.total_falls));
                cursor.y = std::max(cursor.y,
                    page_body.y
                        + ui_layout::trainer_totals_card_height);
            }
            else
            {
                if (!active_raw_telemetry_expanded)
                {
                    const sim::Environment& debug_environment = trainer.preview();
                    const rl::GuidanceAuthorityReport authority =
                        trainer.preview_authority_report();
                    float debug_root_x = 0.0f;
                    if (!debug_environment.particles().empty())
                        debug_root_x = debug_environment.particles()[
                            debug_environment.blueprint().root_node].position.x;
                    const auto raw_number = [](float value)
                    {
                        return std::isfinite(value)
                            ? std::format("{:+.4f}", value)
                            : std::string("NOT AVAILABLE");
                    };
                    const auto summary_card = [&](std::string_view heading,
                        std::string_view value, Color value_color = white)
                    {
                        const Rect card{ cursor,
                            { usable_width,
                                ui_layout::trainer_detail_card_height } };
                        add_rounded_rect(canvas, card, 10.0f, panel_alt,
                            border, 1.0f);
                        add_text_fit(canvas, cursor + Vec2{ 14.0f, 12.0f },
                            heading, ui_layout::section_heading_text_scale,
                            accent,
                            usable_width - 28.0f,
                            ui_layout::persistent_text_minimum_scale);
                        canvas.push_clip(
                            card.position + Vec2{ 14.0f,
                                ui_layout::trainer_detail_value_top },
                            card.position + card.size - Vec2{ 14.0f,
                                ui_layout::trainer_detail_value_bottom });
                        add_wrapped_text(canvas,
                            cursor + Vec2{ 14.0f,
                                ui_layout::trainer_detail_value_top }, value,
                            ui_layout::persistent_text_minimum_scale,
                            value_color, usable_width - 28.0f, 3.0f);
                        canvas.pop_clip();
                        cursor.y += ui_layout::trainer_detail_card_height
                            + ui_layout::trainer_detail_card_spacing;
                    };
                    if (embedded_rig_trainer)
                    {
                        if (button({ cursor,
                                { usable_width,
                                    ui_layout::trainer_secondary_button_height } },
                            "VIEW CONTROLS", input))
                        {
                            active_panel_page = LivePanelPage::view;
                            active_scroll_offset = 0.0f;
                        }
                        cursor.y += ui_layout::trainer_secondary_button_height
                            + 10.0f;
                    }
                    summary_card("PHYSICS",
                        std::format("MODE {}\nCONTACT {:.2f}",
                            sim::guidance_mode_name(authority.mode),
                            authority.physical_contact),
                        authority.optional_guidance_active() ? yellow : green);
                    summary_card("CONTACT",
                        std::format("{}\nWATER {:.2f} M\nSLIP {:.3f} M/S",
                            sim::terrain_region_name(
                                debug_environment.terrain_region_at(debug_root_x)),
                            debug_environment.water_depth(),
                            debug_environment.stance_slip_speed()), green);
                    summary_card("POLICY",
                        std::format("UPDATE {}\nRUNS {} / TESTS {}",
                            metrics.update, autonomy.stage_fresh_episodes,
                            autonomy.stage_fresh_evaluations),
                        metrics.evaluation_valid ? green : yellow);
                    summary_card("OPTIMIZER",
                        std::format("LOSS {}\nRATE {:.7f}",
                            raw_number(metrics.policy_loss), metrics.learning_rate),
                        muted);
                    summary_card("WEAPON",
                        std::format("{}\nAIM {:+.1f} DEG\nMISSES {}",
                            sim::equipment_state_name(
                                debug_environment.equipment_state()),
                            debug_environment.equipment_aim_angle()
                                * 180.0f / pi,
                            debug_environment.shots_missed()),
                        debug_environment.equipment_engagement_ready()
                            ? green : muted);
                    if (button({ cursor,
                            { usable_width,
                                ui_layout::trainer_secondary_button_height } },
                        "TRAINING HISTORY", input))
                    {
                        active_panel_page = LivePanelPage::totals;
                        active_scroll_offset = 0.0f;
                    }
                    cursor.y += ui_layout::trainer_secondary_button_height
                        + 10.0f;
                    if (button({ cursor,
                            { usable_width,
                                ui_layout::trainer_secondary_button_height } },
                        "SHOW RAW TELEMETRY", input))
                    {
                        active_raw_telemetry_expanded = true;
                        active_scroll_offset = 0.0f;
                    }
                    cursor.y += ui_layout::trainer_secondary_button_height;
                }
                else
                {
                add_rounded_rect(canvas,
                    { { page_body.x,
                            page_body.y },
                        { page_body.width,
                            ui_layout::trainer_advanced_card_height } },
                    8.0f, panel_alt, border, 1.0f);
                add_text_fit(canvas, cursor,
                    "RAW TELEMETRY", ui_layout::section_heading_text_scale,
                    accent, usable_width,
                    ui_layout::persistent_text_minimum_scale);
                cursor.y += 36.0f;
                if (button({ cursor,
                        { usable_width,
                            ui_layout::trainer_secondary_button_height } },
                    "HIDE RAW TELEMETRY", input, true))
                {
                    active_raw_telemetry_expanded = false;
                    active_scroll_offset = 0.0f;
                }
                cursor.y += ui_layout::trainer_secondary_button_height + 16.0f;
                const auto advanced_line = [&](std::string_view text,
                    Color color = muted)
                {
                    cursor.y += add_wrapped_text(canvas, cursor, text,
                        ui_layout::persistent_text_minimum_scale,
                        color, usable_width, 4.0f) + 7.0f;
                };
                const auto advanced_heading = [&](std::string_view text)
                {
                    cursor.y += 7.0f;
                    add_text_fit(canvas, cursor, text, 1.10f, accent,
                        usable_width,
                        ui_layout::persistent_text_minimum_scale);
                    cursor.y += 31.0f;
                };
                advanced_heading("LATEST EVALUATION");
                advanced_line(human_status.headline, human_color);
                advanced_line(human_status.explanation, white);
                const Color test_color = telemetry_color(
                    telemetry::latest_test_tone(metrics));
                advanced_line(telemetry::latest_test_title(metrics),
                    test_color);
                advanced_line(
                    telemetry::latest_test_explanation(
                        metrics, autonomy.stage),
                    metrics.evaluation_valid ? green : yellow);
                advanced_line(telemetry::sample_budget_message(progress),
                    progress.sample_budget_complete ? green : muted);
                advanced_heading("PHYSICS AUTHORITY");
                // Detailed preview authority belongs with the raw diagnostic
                // path. The primary mode button still exposes and controls it
                // without making Summary read like a debug console.
                advanced_line(guidance_summary,
                    raw_audit && retained_raw ? green : yellow);
                advanced_line(
                    std::format("{}   {}   {}",
                        format_work_counter("UPDATES",
                            autonomy.stage_fresh_updates,
                            autonomy.stage_required_updates),
                        format_work_counter("RUNS",
                            autonomy.stage_fresh_episodes,
                            autonomy.stage_required_episodes),
                        format_work_counter("TESTS",
                            autonomy.stage_fresh_evaluations,
                            autonomy.stage_required_evaluations)));
                auto raw_number = [](float value)
                {
                    return std::isfinite(value)
                        ? std::format("{:+.4f}", value)
                        : std::string("NOT AVAILABLE");
                };
                const std::string quality = metrics.evaluation_quality_key == 0u
                    ? std::string("NOT AVAILABLE")
                    : std::format("{:016X}", metrics.evaluation_quality_key);
                const sim::Environment& debug_environment = trainer.preview();
                const rl::GuidanceAuthorityReport authority = trainer.preview_authority_report();
                float debug_root_x = 0.0f;
                if (!debug_environment.particles().empty())
                    debug_root_x = debug_environment.particles()[
                        debug_environment.blueprint().root_node].position.x;
                const sim::CourseFeature* nearest_feature = nullptr;
                float nearest_distance = std::numeric_limits<float>::max();
                for (const sim::CourseFeature& feature : debug_environment.course_features())
                {
                    const float distance = feature.center.x - debug_root_x;
                    if (distance >= 0.0f && distance < nearest_distance)
                    {
                        nearest_distance = distance;
                        nearest_feature = &feature;
                    }
                }
                const bool optional_guidance_active =
                    authority.optional_guidance_active();
                advanced_line(
                    std::format("MODE {}   TEACHER {:.2f}   CONTACT PHYSICS {:.2f}",
                        sim::guidance_mode_name(authority.mode),
                        authority.lesson_teacher,
                        authority.physical_contact),
                    optional_guidance_active ? yellow : green);
                const bool static_course = !debug_environment.course_motion_enabled();
                const std::string authority_path = !optional_guidance_active
                    ? std::format(
                        "ROOT DRIVE NONE / BODY PROJECTORS OFF / COURSE MOTION {}",
                        static_course ? "OFF" : "ON")
                    : std::format(
                        "POSE/CONTACT ASSIST ACTIVE / COURSE MOTION {}",
                        static_course ? "OFF" : "ON");
                advanced_line(authority_path,
                    !optional_guidance_active && static_course ? green : yellow);
                advanced_line(
                    std::format("REFLEX S/B {:.2f}/{:.2f}   SAFETY S/B {:.2f}/{:.2f}",
                        authority.topology_support, authority.topology_body,
                        authority.safety_support, authority.safety_body));
                advanced_line(
                    std::format(
                        "POSTURE {:.2f}   SWING {:.2f}   LOCAL JOINT CLUSTER {:.2f}",
                        authority.posture_guide, authority.swing_clearance,
                        authority.mandatory_joint_cluster));
                const std::uint32_t global_cell_x =
                    sim::DeformableTerrain::global_cell_x(debug_root_x);
                const float debug_root_y = debug_environment.particles().empty()
                    ? 0.0f : debug_environment.particles()[
                        debug_environment.blueprint().root_node].position.y;
                const std::uint32_t global_cell_y =
                    sim::DeformableTerrain::global_cell_y(debug_root_y);
                advanced_line(
                    std::format("STANCE SLIP {:.3f} M/S   STATIC WORLD CELL {},{}   GAIT CYCLES {}",
                        debug_environment.stance_slip_speed(),
                        global_cell_x, global_cell_y,
                        debug_environment.gait_cycles()), green);
                advanced_line(
                    std::format("WEAPON {}   STOPPED {}   AIM CMD/ACT {:+.1f}/{:+.1f} DEG   SETTLE {:.2f} S   MISSES {}",
                        sim::equipment_state_name(debug_environment.equipment_state()),
                        debug_environment.equipment_stopped() ? "YES" : "NO",
                        debug_environment.equipment_commanded_aim_angle() * 180.0f / pi,
                        debug_environment.equipment_aim_angle() * 180.0f / pi,
                        debug_environment.equipment_aim_settle_seconds(),
                        debug_environment.shots_missed()),
                    debug_environment.equipment_engagement_ready()
                        ? green : muted);
                advanced_heading("POLICY + OPTIMIZER");
                const bool raw_evaluation =
                    rl::foundational_walk_uses_raw_evaluation(
                        autonomy.stage_fresh_updates, autonomy.stage,
                        trainer.blueprint());
                const std::size_t raw_rollouts =
                    rl::foundational_walk_raw_rollout_count(
                        autonomy.stage_fresh_updates, autonomy.stage,
                        trainer.blueprint(), trainer.environment_count());
                advanced_line(
                    std::format("ACTIVE {} EVALUATION {}   RETAINED BEST {}   RAW ROLLOUTS {}/{}",
                        raw_evaluation ? "RAW POLICY" : "ASSISTED",
                        raw_number(metrics.evaluation_score),
                        raw_number(metrics.best_evaluation_score),
                        raw_rollouts, trainer.environment_count()));
                advanced_line(
                    std::format("BEST SAVED AT LOCAL UPDATE {}   QUALITY KEY {}",
                        metrics.best_update, quality));
                advanced_line(
                    std::format("REJECTION MASK 0x{:08X}   {}",
                        metrics.evaluation_rejection_mask,
                        rl::primary_motion_rejection_name(
                            metrics.evaluation_rejection_mask)),
                    metrics.evaluation_valid ? green : yellow);
                advanced_line(
                    std::format("FAILURES {}", rl::motion_rejection_summary(
                        metrics.evaluation_rejection_mask)),
                    metrics.evaluation_valid ? green : yellow);
                advanced_line(
                    std::format("INVALID DETAIL {}   GROUND {}   WATER {:.2f} M",
                        sim::invalid_motion_name(metrics.evaluation_invalid_reason),
                        sim::terrain_region_name(debug_environment.terrain_region_at(debug_root_x)),
                        debug_environment.water_depth()),
                    metrics.evaluation_invalid_reason == sim::InvalidMotion::none
                        ? muted : danger);
                advanced_line(
                    nearest_feature == nullptr
                        ? std::format("TERRAIN LESSON   SUPPORT SPAN {:.2f}X   SCISSOR MAX {:.3f} S",
                            debug_environment.primary_support_span_ratio(),
                            debug_environment.maximum_lower_leg_scissor_seconds())
                        : std::format("NEAREST {} {:.1f} M   SPAN {:.2f}X   SCISSOR {:.3f} S",
                            sim::course_feature_name(nearest_feature->kind), nearest_distance,
                            debug_environment.primary_support_span_ratio(),
                            debug_environment.maximum_lower_leg_scissor_seconds()),
                    nearest_feature == nullptr ? muted : yellow);
                advanced_line(
                    std::format("BACKWARD BRACE PREVIEW {:.3f} S   TEST {:.3f} S",
                        debug_environment.maximum_backward_brace_seconds(),
                        metrics.evaluation_max_backward_brace_seconds),
                    metrics.evaluation_max_backward_brace_seconds
                            > sim::sustained_backward_brace_limit_seconds
                        ? yellow : muted);
                advanced_line(
                    std::format("POLICY LOSS {}   VALUE LOSS {}",
                        raw_number(metrics.policy_loss),
                        raw_number(metrics.value_loss)));
                advanced_line(
                    std::format("ENTROPY {}   LEARNING RATE {:.7f}",
                        raw_number(metrics.entropy), metrics.learning_rate));
                advanced_line(
                    std::format("MEAN REWARD {}   MEAN SPEED {}",
                        raw_number(metrics.mean_reward),
                        format_speed(metrics.mean_speed)));
                advanced_line(
                    std::format("LOCAL UPDATE {}   ENVIRONMENT STEPS {}",
                        metrics.update, metrics.environment_steps));
                advanced_line(
                    std::format("OPTIMIZER STEP {}   EXPLORATION {:.4f}",
                        trainer.optimizer_step(), trainer.exploration()));
                advanced_line(
                    std::format("WORKERS {}   SIMULATIONS {}   {:.2f} UPDATES/SECOND",
                        autonomy.rollout_threads, autonomy.environment_count,
                        autonomy.updates_per_second));
                advanced_line(
                    std::format("PIPELINE {}   CONTROLLER {}",
                        autonomy.pipeline_stage,
                        trainer.controller_state_name()));
                advanced_line(
                    std::format("CONTROLLER TUNING {}   ACCEPTED {}   REJECTED {}   ROLLBACKS {}",
                        autonomy.rig_generation, autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes,
                        autonomy.rollback_count));
                cursor.y = std::max(cursor.y,
                    page_body.y
                        + ui_layout::trainer_advanced_card_height);
                }
            }
            active_content_height = std::max(rect.size.y,
                cursor.y + active_scroll_offset - rect.position.y + 72.0f);
            active_scroll_offset = ui_layout::clamp_scroll_offset(
                active_scroll_offset, active_content_height, rect.size.y);
            canvas.pop_clip();
            canvas.pop_clip();
            draw_panel_scrollbar(rect, active_content_height,
                active_scroll_offset);
            add_rounded_rect(canvas, rect, 11.0f, ui_render::transparent_fill, border, 1.0f);
        }

        void draw_live_world(Rect viewport, float dt, const InputState& input,
            float shared_rig_camera = std::numeric_limits<float>::quiet_NaN(),
            float shared_rig_scale = std::numeric_limits<float>::quiet_NaN())
        {
            const bool fixed_eye_test = presentation::isolated(presentation_session);
            const bool course_eye_test = presentation::course_eye(presentation_session);
            const bool art_eye_test = presentation::art_eye(presentation_session);
            const bool walk_eye_test = presentation::walk_eye(presentation_session);
            const bool equipment_eye_test =
                presentation::equipment_eye(presentation_session);
            const bool rig_lab_snapshot = !fixed_eye_test && mode == Mode::rig_lab;
            if (presentation::replay_may_advance(presentation_session,
                    run_paused, walk_eye_terminal.frozen)
                && course_eye_test_environment.has_value()
                && walk_eye_replay_start.has_value())
            {
                constexpr double fixed_step = 1.0 / 60.0;
                walk_eye_accumulator_seconds += std::clamp(
                    static_cast<double>(dt), 0.0, 0.10);
                while (walk_eye_accumulator_seconds >= fixed_step)
                {
                    sim::Environment& replay = *course_eye_test_environment;
                    const auto raw_action = walk_eye_test_policy.deterministic_action(
                        rl::foundational_policy_observation(replay,
                            sim::CourseStage::uneven, replay.gait_task(),
                            replay.guidance_mode()));
                    const auto action = rl::effective_policy_action(replay,
                        raw_action, sim::CourseStage::uneven, 0.0f,
                        replay.guidance_mode());
                    const sim::StepResult result = replay.step(
                        action, static_cast<float>(fixed_step));
                    walk_eye_accumulator_seconds -= fixed_step;
                    if (result.terminated)
                    {
                        walk_eye_terminal.frozen = true;
                        walk_eye_terminal.cause = result.terminal_cause;
                        walk_eye_terminal.reason = result.invalid_reason;
                        const auto& terminal_particles = replay.particles();
                        const std::size_t terminal_root = replay.blueprint().root_node;
                        if (terminal_root < terminal_particles.size())
                        {
                            walk_eye_terminal.position =
                                terminal_particles[terminal_root].position;
                            walk_eye_terminal.terrain = replay.terrain_region_at(
                                walk_eye_terminal.position.x);
                        }
                        walk_eye_terminal.water_depth = replay.water_depth();
                        run_paused = true;
                        walk_eye_accumulator_seconds = 0.0;
                        set_status(std::format(
                            "TRIAL {} TERMINAL FROZEN - {} AT X {:.2f} M / CELL {} - EXPLICIT RETRY REQUIRED",
                            walk_eye_trial_id,
                            sim::trial_terminal_cause_name(
                                walk_eye_terminal.cause, walk_eye_terminal.reason),
                            walk_eye_terminal.position.x,
                            sim::DeformableTerrain::global_cell_x(
                                walk_eye_terminal.position.x)));
                        break;
                    }
                }
            }
            const bool live_preview_advances = !rig_lab_snapshot && !run_paused
                && presentation::permits_training_controls(presentation_session);
            if (live_preview_advances)
                trainer.step_preview(dt);
            const sim::Environment& environment = fixed_eye_test
                ? *course_eye_test_environment
                : rig_lab_snapshot ? rig_preview_environment()
                : trainer.preview();
            const bool rig_lab_physical_test = rig_lab_snapshot
                && rig_test_physical_environment.has_value();
            const auto& particles = environment.particles();
            if (!rig_lab_snapshot && contains(viewport, input.mouse)
                && std::abs(input.wheel) >= 0.01f)
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(
                    live_zoom_factor, input.wheel);
                live_zoom_auto = false;
            }

            float rig_height = 2.4f;
            float rig_width = 1.0f;
            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            if (!particles.empty())
            {
                float minimum_y = std::numeric_limits<float>::infinity();
                float maximum_y = -std::numeric_limits<float>::infinity();
                for (const sim::Particle& particle : particles)
                {
                    minimum_x = std::min(minimum_x,
                        particle.position.x - particle.radius);
                    maximum_x = std::max(maximum_x,
                        particle.position.x + particle.radius);
                    minimum_y = std::min(minimum_y, particle.position.y - particle.radius);
                    maximum_y = std::max(maximum_y, particle.position.y + particle.radius);
                }
                if (std::isfinite(minimum_x) && std::isfinite(maximum_x))
                    rig_width = std::max(0.35f, maximum_x - minimum_x);
                if (std::isfinite(minimum_y) && std::isfinite(maximum_y))
                    rig_height = std::max(0.75f, maximum_y - minimum_y);
            }
            const bool shared_rig_projection = rig_lab_snapshot
                && std::isfinite(shared_rig_camera)
                && std::isfinite(shared_rig_scale)
                && shared_rig_scale > 0.0f;
            const bool fitted_pose_inspection = (art_eye_test
                || equipment_eye_test)
                && std::isfinite(minimum_x) && std::isfinite(maximum_x);
            float inspection_camera_x = fitted_pose_inspection
                ? (minimum_x + maximum_x) * 0.5f : camera_x;
            float target_pixels_per_meter = shared_rig_projection
                ? shared_rig_scale
                : course_eye_test
                    ? view_camera::course_pixels_per_meter(
                    viewport.size.x, course_eye_focus_span_meters,
                    live_zoom_factor)
                : fitted_pose_inspection
                ? view_camera::inspection_pixels_per_meter(
                    viewport.size.x, viewport.size.y, rig_width, rig_height,
                    live_zoom_factor)
                : view_camera::fitted_pixels_per_meter(
                    viewport.size.y,
                    std::max(rig_height, live_default_reference_height_m),
                    live_zoom_factor);
            if (fitted_pose_inspection)
            {
                // Isolated art/equipment proof keeps the complete production
                // silhouette below the persistent telemetry and PIP cards.
                // Particle bounds drive the camera, while this conservative
                // envelope allowance reserves room for opaque art extending
                // beyond those physical anchors (heads, tails, hands, feet,
                // and held equipment). Zoom may reduce this fit, never hide it.
                const ui_layout::Box inspection_world{
                    viewport.position.x, viewport.position.y,
                    viewport.size.x, viewport.size.y };
                const ui_layout::Box inspection_pip =
                    ui_layout::training_pip_box(inspection_world);
                const ui_layout::Box inspection_telemetry =
                    ui_layout::primary_telemetry_display_box(
                        inspection_world, true);
                const float overlay_bottom = std::max(
                    inspection_pip.y + inspection_pip.height,
                    inspection_telemetry.y + inspection_telemetry.height);
                const float ground_y = viewport.position.y
                    + viewport.size.y * view_camera::live_ground_fraction;
                // Start from the full physical envelope; the opaque probe below
                // then accounts for every species' actual helmet, hands, feet,
                // tail, and equipment without needlessly miniaturizing it.
                constexpr float opaque_envelope_allowance = 1.00f;
                const float safe_pose_height = std::max(1.0f,
                    ground_y - overlay_bottom - 20.0f);
                const float safe_pixels_per_meter = safe_pose_height
                    / (std::max(rig_height, 0.75f)
                        * opaque_envelope_allowance);
                target_pixels_per_meter = std::min(target_pixels_per_meter,
                    std::max(view_camera::minimum_pixels_per_meter,
                        safe_pixels_per_meter));

                // Physical particles are deliberately smaller than several
                // production art envelopes (notably Human arms/head and the
                // long-backed quadrupeds). Measure the exact rendered subject
                // once, then fit that opaque envelope into the unobstructed
                // world band. This is presentation-only: no particle, contact,
                // gait, or authored art anchor is moved.
                render::Canvas subject_probe{};
                std::swap(canvas, subject_probe);
                bool& inspection_art_enabled = art_toggle_for_mode();
                const bool saved_inspection_art = inspection_art_enabled;
                inspection_art_enabled = true;
                draw_creature(environment, viewport, inspection_camera_x,
                    target_pixels_per_meter, false, false);
                inspection_art_enabled = saved_inspection_art;
                std::swap(canvas, subject_probe);
                float subject_min_x = std::numeric_limits<float>::infinity();
                float subject_max_x = -std::numeric_limits<float>::infinity();
                float subject_min_y = std::numeric_limits<float>::infinity();
                float subject_max_y = -std::numeric_limits<float>::infinity();
                for (const render::Vertex& vertex : subject_probe.vertices())
                {
                    if (!(vertex.color.a > 0.001f)
                        || !std::isfinite(vertex.position.x)
                        || !std::isfinite(vertex.position.y))
                        continue;
                    subject_min_x = std::min(subject_min_x, vertex.position.x);
                    subject_max_x = std::max(subject_max_x, vertex.position.x);
                    subject_min_y = std::min(subject_min_y, vertex.position.y);
                    subject_max_y = std::max(subject_max_y, vertex.position.y);
                }
                if (std::isfinite(subject_min_x)
                    && std::isfinite(subject_max_x)
                    && std::isfinite(subject_min_y)
                    && std::isfinite(subject_max_y))
                {
                    const ui_layout::Box inspection_bottom =
                        ui_layout::bottom_telemetry_box(inspection_world);
                    const float safe_top = overlay_bottom + 20.0f;
                    const float safe_bottom = inspection_bottom.y - 20.0f;
                    const float safe_left = viewport.position.x + 24.0f;
                    const float safe_right = viewport.position.x
                        + viewport.size.x - 24.0f;
                    float exact_fit = std::min(1.0f,
                        (safe_right - safe_left)
                            / std::max(1.0f, subject_max_x - subject_min_x));
                    if (subject_min_y < safe_top)
                    {
                        exact_fit = std::min(exact_fit,
                            (ground_y - safe_top)
                                / std::max(1.0f, ground_y - subject_min_y));
                    }
                    if (subject_max_y > safe_bottom)
                    {
                        exact_fit = std::min(exact_fit,
                            (safe_bottom - ground_y)
                                / std::max(1.0f, subject_max_y - ground_y));
                    }
                    const float subject_center_x =
                        (subject_min_x + subject_max_x) * 0.5f;
                    const float viewport_center_x = viewport.position.x
                        + viewport.size.x * 0.5f;
                    inspection_camera_x += (subject_center_x - viewport_center_x)
                        / std::max(target_pixels_per_meter,
                            view_camera::minimum_pixels_per_meter);
                    target_pixels_per_meter = std::max(
                        view_camera::minimum_pixels_per_meter,
                        target_pixels_per_meter * std::max(0.05f, exact_fit));
                }
            }
            if (!fixed_eye_test && environment.shuttle_enabled() && live_zoom_auto)
            {
                const float arena_span = sim::shuttle_right_boundary
                    - sim::shuttle_left_boundary + 3.0f;
                target_pixels_per_meter = std::min(target_pixels_per_meter,
                    viewport.size.x / arena_span);
            }
            float render_pixels_per_meter = shared_rig_scale;
            float render_camera_x = shared_rig_camera;
            if (shared_rig_projection)
            {
                // Rig Lab's Live pane borrows the editor projection for this
                // draw only. It must not overwrite the independent Live camera,
                // zoom smoothing state, or manual-follow history; otherwise a
                // Live -> Rig Lab zoom -> Live round trip appears to reset or
                // lurch even though the user's Live zoom factor was preserved.
                render_pixels_per_meter = shared_rig_scale;
                render_camera_x = shared_rig_camera;
            }
            else
            {
                live_pixels_per_meter = fitted_pose_inspection
                        || !live_projection_initialized
                    ? target_pixels_per_meter
                    : view_camera::smooth_zoom(
                        live_pixels_per_meter, target_pixels_per_meter, dt);
                live_projection_initialized = true;
                if (fitted_pose_inspection)
                    camera_x = inspection_camera_x;
                else if (!particles.empty() && presentation::tracks_subject(
                        presentation_session))
                {
                    const std::size_t root = environment.blueprint().root_node;
                    if (root < particles.size())
                    {
                        const float target_camera = environment.shuttle_enabled()
                            && live_zoom_auto
                            ? 0.5f * (sim::shuttle_left_boundary
                                + sim::shuttle_right_boundary)
                            : particles[root].position.x
                                + view_camera::lookahead_meters(
                                    viewport.size.x, live_pixels_per_meter)
                                    * environment.facing_direction();
                        camera_x = view_camera::smooth_camera(
                            camera_x, target_camera, live_pixels_per_meter, dt);
                    }
                }
                render_pixels_per_meter = live_pixels_per_meter;
                render_camera_x = camera_x;
            }

            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            canvas.push_clip(viewport.position + Vec2{ 1.0f, 1.0f },
                viewport.position + viewport.size - Vec2{ 1.0f, 1.0f });
            draw_course_ground(environment, viewport,
                render_camera_x, render_pixels_per_meter);
            const bool focused_pose_view = art_eye_test || equipment_eye_test;
            const bool show_challenge_details = !rig_lab_snapshot
                && !focused_pose_view
                && (live_panel_page == LivePanelPage::advanced
                    || walk_eye_test);
            const bool show_course_annotations = show_challenge_details;
            if (show_challenge_details)
                draw_terrain_challenges(environment, viewport,
                    render_camera_x, render_pixels_per_meter);
            if (show_course_annotations)
                draw_course_reference(environment, viewport,
                    render_camera_x, render_pixels_per_meter,
                    show_challenge_details);
            draw_course_features(environment, viewport,
                render_camera_x, render_pixels_per_meter,
                show_challenge_details);
            draw_creature(environment, viewport,
                render_camera_x, render_pixels_per_meter);

            const ui_layout::Box world_box{
                viewport.position.x, viewport.position.y,
                viewport.size.x, viewport.size.y };
            const bool walk_terminal = walk_eye_test && walk_eye_terminal.frozen;
            const bool preview_terminal = trainer.preview_terminal();
            const bool expanded_telemetry = fixed_eye_test || walk_terminal
                || (rig_lab_snapshot && rig_test_terminal.frozen)
                || (!rig_lab_snapshot && preview_terminal);
            const ui_layout::Box telemetry_box =
                ui_layout::primary_telemetry_display_box(
                    world_box, expanded_telemetry);
            const ui_layout::Box pip_box = ui_layout::training_pip_box(world_box);
            const ui_layout::Box bottom_box = ui_layout::bottom_telemetry_box(world_box);
            const Rect telemetry{ { telemetry_box.x, telemetry_box.y },
                { telemetry_box.width, telemetry_box.height } };
            const Rect bottom{ { bottom_box.x, bottom_box.y },
                { bottom_box.width, bottom_box.height } };
            // View explicitly owns the live overlay. Details, History, and Raw
            // disclosures keep their information in the rail so the same
            // telemetry is not duplicated over the production world.
            const bool details_overlay_page =
                live_panel_page == LivePanelPage::view;
            const bool show_primary_telemetry = rig_lab_snapshot
                || fixed_eye_test || expanded_telemetry
                || details_overlay_page;
            if (show_primary_telemetry)
            {
            add_rounded_rect(canvas, telemetry, 9.0f,
                rgb(0x07111b, 0.95f), border, 1.0f);
            const sim::CourseStage displayed_stage = environment.course_stage();
            const float displayed_difficulty = environment.course_difficulty();
            const float text_width = telemetry.size.x - 24.0f;
            Vec2 line = telemetry.position + Vec2{ 12.0f, 11.0f };
            const std::string displayed_lesson = displayed_stage == sim::CourseStage::uneven
                ? std::format("3. {}", sim::gait_task_name(environment.gait_task()))
                : std::string{ sim::course_stage_name(displayed_stage) };
            add_text_fit(canvas, line,
                std::format("{}  /  {:.0f}%", displayed_lesson,
                    displayed_difficulty * 100.0f),
                1.52f, white, text_width, 1.08f);
            line.y += 34.0f;
            float displayed_world_x = 0.0f;
            float displayed_world_y = 0.0f;
            if (!environment.particles().empty())
            {
                const sim::Particle& root = environment.particles()[
                    environment.blueprint().root_node];
                displayed_world_x = root.position.x;
                displayed_world_y = root.position.y;
            }
            if (!expanded_telemetry)
            {
                add_text_fit(canvas, line,
                    std::format("SPEED {}   DIST {}",
                        format_speed(environment.forward_speed()),
                        format_distance(environment.distance_travelled())),
                    1.12f, environment.valid_motion() ? green : danger,
                    text_width, ui_layout::persistent_text_minimum_scale);
                line.y += 31.0f;
                const std::string concise_state = !environment.valid_motion()
                    ? std::format("MOTION {}",
                        sim::invalid_motion_name(environment.invalid_reason()))
                    : rig_lab_physical_test
                    ? std::format("PHYSICAL TEST {}", physical_scrub_name())
                    : rig_lab_snapshot ? std::string("RIG PREVIEW")
                    : trainer.background_enabled()
                    ? std::string("TRAINING ACTIVE")
                    : std::string("TRAINING PAUSED");
                add_text_fit(canvas, line, concise_state,
                    1.10f, environment.valid_motion() ? accent : danger,
                    text_width, ui_layout::persistent_text_minimum_scale);
            }
            else
            {
            add_text_fit(canvas, line,
                std::format("SPEED {}   DIST {}",
                    format_speed(environment.forward_speed()),
                    format_distance(environment.distance_travelled())),
                1.00f, environment.valid_motion() ? green : danger, text_width,
                ui_layout::persistent_text_minimum_scale);
            line.y += 25.0f;
            add_text_fit(canvas, line,
                std::format("WORLD X {:.2f} M   CELL {},{}",
                    displayed_world_x,
                    sim::DeformableTerrain::global_cell_x(displayed_world_x),
                    sim::DeformableTerrain::global_cell_y(displayed_world_y)),
                1.00f, environment.valid_motion() ? green : danger, text_width,
                ui_layout::persistent_text_minimum_scale);
            line.y += 25.0f;
            add_text_fit(canvas, line,
                std::format("SUPPORT CYCLES {}   LEG CROSSINGS {}",
                    environment.gait_cycles(), environment.limb_crossings()),
                1.00f, environment.recovering() ? yellow : muted,
                text_width, ui_layout::persistent_text_minimum_scale);
            line.y += 25.0f;
            add_text_fit(canvas, line,
                std::format("HEEL STRIKES {}   TOE LIFTS {}   FEATURES {}",
                    environment.heel_strikes(), environment.toe_offs(),
                    environment.obstacles_passed()),
                1.00f, environment.recovering() ? yellow : muted,
                text_width, ui_layout::persistent_text_minimum_scale);
            line.y += 25.0f;
            add_text_fit(canvas, line,
                std::format("LEFT FOOT {}   RIGHT FOOT {}",
                    sim::foot_contact_phase_name(environment.left_foot_phase()),
                    sim::foot_contact_phase_name(environment.right_foot_phase())),
                1.00f, muted, text_width,
                ui_layout::persistent_text_minimum_scale);
            line.y += 25.0f;
            const std::string expanded_status = rig_test_terminal.frozen
                    ? std::format(
                        "TEST {} TERMINAL FROZEN   {}   EXPLICIT RETRY OR R REQUIRED",
                        rig_test_terminal.physical_test_generation,
                        sim::trial_terminal_cause_name(
                            rig_test_terminal.cause, rig_test_terminal.reason))
                    : rig_lab_physical_test
                    ? std::format("TEST / PHYSICAL {} MOTOR SCRUB   {}",
                        physical_scrub_name(), physical_scrub_authority())
                    : walk_terminal
                    ? std::format(
                        "TRIAL {} TERMINAL FROZEN   {}   EXPLICIT RETRY REQUIRED",
                        walk_eye_trial_id,
                        sim::trial_terminal_cause_name(
                            walk_eye_terminal.cause, walk_eye_terminal.reason))
                    : walk_eye_test
                    ? std::format(
                        "MOTION VALID   COLD UPDATES {}   AUTHORITY {:.3f}   SEEDS {}/6",
                        walk_eye_test_proof.updates,
                        walk_eye_test_proof.teacher_authority,
                        6u - walk_eye_test_proof.retained_invalid_runs)
                    : equipment_eye_test
                    ? std::format(
                        "PHYSICAL READY AIM FROZEN   ASSISTED TEACHER   PLANT {:.2f} S   SETTLE {:.2f} S",
                        environment.equipment_plant_seconds(),
                        environment.equipment_aim_settle_seconds())
                    : fixed_eye_test
                    ? std::format(
                        "MOTION {}   TRAINER DISCONNECTED   PHYSICAL SNAPSHOT",
                        sim::invalid_motion_name(environment.invalid_reason()))
                    : std::format(
                        "MOTION {}   RIG UPDATES {}   POLICY AGE {}   PRIOR LINEAGE {}",
                        sim::invalid_motion_name(environment.invalid_reason()),
                        trainer.metrics().total_updates, trainer.metrics().update,
                        telemetry::prior_policy_lineage_updates(trainer.metrics()));
            line.y += add_wrapped_text(canvas, line, expanded_status,
                ui_layout::persistent_text_minimum_scale,
                walk_terminal || rig_test_terminal.frozen ? yellow
                    : environment.valid_motion() ? accent : danger,
                text_width, 2.0f);
            line.y += 6.0f;
            const std::uint64_t preview_restarts = trainer.preview_reset_count();
            const sim::InvalidMotion preview_reason = trainer.preview_last_reset_reason();
            const std::string expanded_detail = rig_test_terminal.frozen
                    ? std::format(
                        "X {:.2f} M / CELL {},{} / {} / WATER {:.2f} M - POSE AND GENERATION HELD",
                        rig_test_terminal.position.x,
                        sim::DeformableTerrain::global_cell_x(
                            rig_test_terminal.position.x),
                        sim::DeformableTerrain::global_cell_y(
                            rig_test_terminal.position.y),
                        sim::terrain_region_name(rig_test_terminal.terrain),
                        rig_test_terminal.water_depth)
                    : rig_lab_physical_test
                    ? std::format(
                        "PHYSICAL SNAPSHOT   L {} / R {}   HEEL STRIKES {}   TOE LIFTS {}",
                        sim::foot_contact_phase_name(environment.left_foot_phase()),
                        sim::foot_contact_phase_name(environment.right_foot_phase()),
                        environment.heel_strikes(), environment.toe_offs())
                    : walk_terminal
                    ? std::format(
                        "X {:.2f} M / CELL {},{} / {} / WATER {:.2f} M - RETRY BUTTON OR R",
                        walk_eye_terminal.position.x,
                        sim::DeformableTerrain::global_cell_x(
                            walk_eye_terminal.position.x),
                        sim::DeformableTerrain::global_cell_y(
                            walk_eye_terminal.position.y),
                        sim::terrain_region_name(walk_eye_terminal.terrain),
                        walk_eye_terminal.water_depth)
                    : walk_eye_test
                    ? std::format(
                        "RETAINED MEAN {:.1f} M / {:.1f} STEPS   REPLAY SEED 0x{:X}",
                        walk_eye_test_proof.retained_distance,
                        walk_eye_test_proof.retained_stride_events,
                        walk_eye_test_proof.selected_seed)
                    : equipment_eye_test
                    ? std::format(
                        "TARGET {}   AIM ERR {:.3f} RAD   AIM RATE {:.3f} RAD/S   SHOTS {}",
                        environment.equipment_target_visible()
                            ? "VISIBLE" : "OCCLUDED",
                        environment.equipment_aim_error(),
                        environment.equipment_aim_rate(),
                        environment.shots_fired())
                    : fixed_eye_test
                    ? std::string("ISOLATED VIEW - NO AUTOSAVE LOAD, TRAINING, OR PREVIEW RESET")
                    : preview_terminal
                    ? std::format("TRIAL {} ENDED: {} AT X {:.2f} M / CELL {} / {} / WATER {:.2f} M - {}",
                        trainer.preview_trial_id(), sim::trial_terminal_cause_name(
                            trainer.preview_terminal_cause(), preview_reason),
                        trainer.preview_terminal_position().x,
                        sim::DeformableTerrain::global_cell_x(
                            trainer.preview_terminal_position().x),
                        sim::terrain_region_name(trainer.preview_terminal_terrain()),
                        trainer.preview_terminal_water_depth(),
                        "R FOR NEW REPLAY")
                    : preview_restarts == 0u
                    ? std::format("TRIAL {} ACTIVE - REAL STATIC COURSE",
                        trainer.preview_trial_id())
                    : std::format("TRIAL {} ACTIVE   PRIOR TERMINALS {}",
                        trainer.preview_trial_id(), preview_restarts);
            add_wrapped_text(canvas, line, expanded_detail,
                ui_layout::persistent_text_minimum_scale,
                rig_lab_physical_test ? muted
                    : walk_terminal ? yellow
                    : walk_eye_test || (!preview_terminal
                        && preview_restarts == 0u) ? muted : yellow,
                text_width, 2.0f);
            }
            }

            draw_training_pip({ { pip_box.x, pip_box.y },
                { pip_box.width, pip_box.height } });
            add_rounded_rect(canvas, bottom, 8.0f,
                rgb(0x07111b, 0.96f), border, 1.0f);
            const std::string run_status = art_eye_test ? "FIXED SIDE PROFILE"
                : rig_test_terminal.frozen
                    ? "TEST TERMINAL FROZEN / RETRY OR R REQUIRED"
                : rig_lab_physical_test && art_preview_frozen
                    ? "TEST / ASSISTED PHYSICAL SCRUB FROZEN"
                : rig_lab_physical_test
                    ? "TEST / ASSISTED PHYSICAL SCRUB"
                : walk_terminal ? "TERMINAL FROZEN / RETRY REQUIRED"
                : walk_eye_test
                    ? std::string{ retained_guidance_label(
                        environment.guidance_mode()) }
                : equipment_eye_test ? "ASSISTED / PLANTED READY AIM FROZEN"
                : course_eye_test ? "FIXED START FRAME"
                : !rig_lab_snapshot && preview_terminal
                    ? "REPLAY ENDED / RESTART OR HOLD START"
                : !rig_lab_snapshot && run_paused ? "REPLAY / START POSE HELD"
                : environment.shuttle_enabled()
                    ? std::format("SHUTTLE {} {}",
                        sim::shuttle_phase_name(environment.shuttle_phase()),
                        environment.facing_direction() > 0.0f ? "RIGHT" : "LEFT")
                : rig_lab_snapshot ? "RIG LAB"
                : std::format("REPLAY / {} UPDATE {}   TRAINER {}",
                    trainer.replay_uses_retained_controller() ? "RETAINED" : "CANDIDATE",
                    trainer.replay_controller_update(),
                    trainer.background_enabled() ? "RUNNING" : "PAUSED");
            const std::string footer_summary = fixed_eye_test
                ? std::format("{}   {}", run_status,
                    sim::terrain_region_name(environment.terrain_region_at(
                        render_camera_x)))
                : std::format("{}   {}   {}",
                    run_status,
                    sim::terrain_region_name(environment.terrain_region_at(
                        render_camera_x)),
                    sim::weapon_class_name(environment.weapon_class()));
            add_text_fit(canvas, bottom.position + Vec2{ 11.0f, 13.0f },
                footer_summary,
                1.00f, equipment_eye_test ? yellow
                    : fixed_eye_test || trainer.replay_uses_retained_controller() ? green : yellow,
                bottom.size.x - 22.0f,
                ui_layout::persistent_text_minimum_scale);
            canvas.pop_clip();
            add_rounded_rect(canvas, viewport, 11.0f, ui_render::transparent_fill, border, 1.0f);
        }

        void draw_joint_lab(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x0b1721, 0.98f), accent_dim, 1.0f);
            const auto names = motor_names();
            add_text_fit(canvas, rect.position + Vec2{ 14.0f, 10.0f },
                std::format("JOINT TEST - {}", names[static_cast<std::size_t>(selected_motor)]),
                1.50f, white, rect.size.x - 28.0f, 1.25f);
            const ui_layout::RigLabTestLayout layout = ui_layout::rig_lab_test_layout({
                rect.position.x, rect.position.y, rect.size.x, rect.size.y });
            const float group_width = layout.selection_row.width * 0.25f;
            Vec2 row{ layout.selection_row.x, layout.selection_row.y };
            if (button({ row, { group_width - 4.0f, layout.selection_row.height } }, "SELECTED", input,
                joint_test_group == JointTestGroup::selected))
                joint_test_group = JointTestGroup::selected;
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, layout.selection_row.height } }, "LEGS 1-4", input,
                joint_test_group == JointTestGroup::pair_a))
                joint_test_group = JointTestGroup::pair_a;
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, layout.selection_row.height } }, "ARMS 5-8", input,
                joint_test_group == JointTestGroup::pair_b))
                joint_test_group = JointTestGroup::pair_b;
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, layout.selection_row.height } }, "ALL", input,
                joint_test_group == JointTestGroup::all))
                joint_test_group = JointTestGroup::all;

            row = { layout.range_row.x, layout.range_row.y };
            if (button({ row, { group_width - 4.0f, layout.range_row.height } }, "MIN", input,
                    false, !rig_test_terminal.frozen))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = -1.0f;
                start_physical_rig_test(sim::CourseStage::balance);
                set_status("TEST / PHYSICAL MANUAL MOTOR SCRUB - DIRECT JOINT INPUT / ASSISTED STABILITY");
            }
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, layout.range_row.height } }, "REST", input,
                    false, !rig_test_terminal.frozen))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = 0.0f;
                start_physical_rig_test(sim::CourseStage::balance);
                set_status("TEST / PHYSICAL MANUAL MOTOR SCRUB - DIRECT JOINT INPUT / ASSISTED STABILITY");
            }
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, layout.range_row.height } }, "MAX", input,
                    false, !rig_test_terminal.frozen))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = 1.0f;
                start_physical_rig_test(sim::CourseStage::balance);
                set_status("TEST / PHYSICAL MANUAL MOTOR SCRUB - DIRECT JOINT INPUT / ASSISTED STABILITY");
            }
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, layout.range_row.height } },
                joint_auto_sweep ? "STOP" : "SWEEP", input, joint_auto_sweep,
                !rig_test_terminal.frozen))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = !joint_auto_sweep;
                start_physical_rig_test(sim::CourseStage::balance);
                set_status(joint_auto_sweep
                    ? "TEST / PHYSICAL MOTOR SWEEP - DIRECT JOINT INPUT / ASSISTED STABILITY"
                    : "TEST / PHYSICAL MANUAL MOTOR SCRUB - DIRECT JOINT INPUT / ASSISTED STABILITY");
            }

            row = { layout.pattern_row.x, layout.pattern_row.y };
            if (button({ row, { group_width - 4.0f, layout.pattern_row.height } }, "CROUCH", input,
                rig_test_pattern == sim::RigTestPattern::crouch,
                !rig_test_terminal.frozen))
            {
                joint_auto_sweep = false;
                rig_test_pattern = sim::RigTestPattern::crouch;
                start_physical_rig_test(sim::CourseStage::duck_press);
                set_status("TEST / PHYSICAL CROUCH MOTOR SCRUB - ASSISTED TEACHER / STATIC COURSE");
            }
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, layout.pattern_row.height } }, "GAIT", input,
                rig_test_pattern == sim::RigTestPattern::gait,
                !rig_test_terminal.frozen))
            {
                joint_auto_sweep = false;
                rig_test_pattern = sim::RigTestPattern::gait;
                start_physical_rig_test(sim::CourseStage::uneven);
                set_status("TEST / PHYSICAL GAIT MOTOR SCRUB - ASSISTED TEACHER / STATIC COURSE");
            }
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, layout.pattern_row.height } }, "FIRM", input,
                !rig_test_loose_ground))
                rig_test_loose_ground = false;
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, layout.pattern_row.height } }, "LOOSE", input,
                rig_test_loose_ground))
                rig_test_loose_ground = true;

            const float friction = sim::foot_friction_retention(0.45f,
                rig_test_loose_ground ? 0.25f : 1.0f,
                rig_test_loose_ground ? 0.75f : 0.0f, false, false);
            add_text_fit(canvas, { layout.status_row.x, layout.status_row.y },
                rig_test_terminal.frozen
                    ? std::format("TERMINAL {} / TEST {} FROZEN",
                        sim::trial_terminal_cause_name(rig_test_terminal.cause,
                            rig_test_terminal.reason),
                        rig_test_terminal.physical_test_generation)
                    : std::format("TRACTION {:.3f} / {}",
                        friction, rig_test_loose_ground ? "LOOSE" : "FIRM"),
                1.25f, rig_test_terminal.frozen || rig_test_loose_ground
                    ? yellow : green,
                layout.status_row.width,
                1.12f);
            const Rect terminal_control{
                { layout.manual_slider.x, layout.manual_slider.y },
                { layout.manual_slider.width, layout.manual_slider.height } };
            if (rig_test_terminal.frozen)
            {
                if (button(terminal_control, "RETRY TERMINAL TEST", input))
                {
                    start_physical_rig_test(rig_test_physical_stage);
                    set_status("EXPLICIT RIG LAB RETRY - PHYSICAL TEST RESTARTED");
                }
            }
            else
            {
                joint_test_input = slider(terminal_control,
                    "MANUAL INPUT", joint_test_input, -1.0f, 1.0f, input);
            }
        }

        struct RigPreviewProjection
        {
            const sim::Environment* environment{};
            float camera{};
            float scale{ view_camera::default_pixels_per_meter };
        };
        std::optional<RigPreviewProjection> rig_frame_projection{};

        [[nodiscard]] const sim::Environment& rig_preview_environment() const
        {
            if (rig_test_terminal.frozen
                && rig_test_physical_environment.has_value())
                return *rig_test_physical_environment;
            if (art_preview_frozen && frozen_art_environment.has_value())
                return *frozen_art_environment;
            if (rig_test_physical_environment.has_value())
                return *rig_test_physical_environment;
            return trainer.preview();
        }

        [[nodiscard]] RigPreviewProjection physical_rig_preview_projection(
            Rect viewport, float relative_zoom) const
        {
            const sim::Environment& environment = rig_preview_environment();
            const auto& particles = environment.particles();
            float camera = 0.0f;
            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            float minimum_y = std::numeric_limits<float>::infinity();
            float maximum_y = -std::numeric_limits<float>::infinity();
            if (!particles.empty())
            {
                for (const sim::Particle& particle : particles)
                {
                    minimum_x = std::min(minimum_x,
                        particle.position.x - particle.radius);
                    maximum_x = std::max(maximum_x,
                        particle.position.x + particle.radius);
                    minimum_y = std::min(minimum_y,
                        particle.position.y - particle.radius);
                    maximum_y = std::max(maximum_y,
                        particle.position.y + particle.radius);
                }
                if (std::isfinite(minimum_x) && std::isfinite(maximum_x))
                    camera = (minimum_x + maximum_x) * 0.5f;
            }
            const float rig_width = std::isfinite(minimum_x)
                    && std::isfinite(maximum_x)
                ? std::max(view_camera::inspection_minimum_span_meters,
                    maximum_x - minimum_x) : 1.0f;
            const float rig_height = std::isfinite(minimum_y)
                    && std::isfinite(maximum_y)
                ? std::max(view_camera::inspection_minimum_span_meters,
                    maximum_y - minimum_y) : 2.4f;
            const float scale = view_camera::inspection_pixels_per_meter(
                viewport.size.x, viewport.size.y, rig_width, rig_height,
                relative_zoom);
            return { &environment, camera, scale };
        }

        [[nodiscard]] RigPreviewProjection rig_preview_projection(
            Rect viewport) const
        {
            if (rig_frame_projection.has_value())
                return *rig_frame_projection;
            return physical_rig_preview_projection(viewport,
                art_editor_zoom);
        }

        [[nodiscard]] RigPreviewProjection fit_rig_preview_projection(
            Rect viewport)
        {
            // First establish the canonical physical fit at relative zoom 1.
            // Render the actual authored production creature into an isolated
            // canvas to measure only opaque triangles emitted from those live
            // physical anchors. The probe never advances or mutates the rig.
            RigPreviewProjection projection =
                physical_rig_preview_projection(viewport, 1.0f);
            render::Canvas probe_canvas{};
            std::swap(canvas, probe_canvas);
            const bool saved_rig_lab_art = rig_lab_art_enabled;
            rig_lab_art_enabled = true;
            draw_creature(*projection.environment, viewport,
                projection.camera, projection.scale, false, false);
            rig_lab_art_enabled = saved_rig_lab_art;
            std::swap(canvas, probe_canvas);

            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            float minimum_y = std::numeric_limits<float>::infinity();
            float maximum_y = -std::numeric_limits<float>::infinity();
            for (const render::Vertex& vertex : probe_canvas.vertices())
            {
                if (!(vertex.color.a > 0.001f)
                    || !std::isfinite(vertex.position.x)
                    || !std::isfinite(vertex.position.y))
                    continue;
                minimum_x = std::min(minimum_x, vertex.position.x);
                maximum_x = std::max(maximum_x, vertex.position.x);
                minimum_y = std::min(minimum_y, vertex.position.y);
                maximum_y = std::max(maximum_y, vertex.position.y);
            }
            if (std::isfinite(minimum_x) && std::isfinite(maximum_x)
                && std::isfinite(minimum_y) && std::isfinite(maximum_y)
                && maximum_x > minimum_x && maximum_y > minimum_y)
            {
                const float ground_y = viewport.position.y
                    + viewport.size.y * view_camera::live_ground_fraction;
                const float available_above_ground = ground_y
                    - (viewport.position.y
                        + view_camera::inspection_art_top_margin_pixels);
                const float available_below_ground = viewport.position.y
                    + viewport.size.y
                    - view_camera::inspection_art_bottom_margin_pixels
                    - ground_y;
                const float fit =
                    view_camera::inspection_envelope_scale_multiplier(
                        viewport.size.x, available_above_ground,
                        available_below_ground, maximum_x - minimum_x,
                        std::max(0.0f, ground_y - minimum_y),
                        std::max(0.0f, maximum_y - ground_y));
                const float envelope_center = (minimum_x + maximum_x) * 0.5f;
                const float viewport_center = viewport.position.x
                    + viewport.size.x * 0.5f;
                projection.camera +=
                    view_camera::inspection_camera_delta_meters(
                        envelope_center, viewport_center, projection.scale);
                projection.scale *= fit;
            }
            const float relative_zoom = view_camera::clamp_zoom_factor(
                art_editor_zoom);
            projection.scale = std::clamp(projection.scale * relative_zoom,
                view_camera::minimum_pixels_per_meter,
                view_camera::inspection_maximum_pixels_per_meter);
            return projection;
        }

        void draw_art_editor_world(Rect viewport, bool annotations = true,
            bool physical_bones = false)
        {
            const RigPreviewProjection projection =
                rig_preview_projection(viewport);
            const sim::Environment& environment = *projection.environment;
            if (art_editor_high_contrast)
                canvas.quad(viewport.position, viewport.position + viewport.size,
                    rgb(0x03070a));
            draw_course_ground(environment, viewport,
                projection.camera, projection.scale);
            // Rig Lab keeps the canonical terrain/features but hides world
            // placards and distance tags. At editor zoom those tethers covered
            // heel/ball/toe handles and appeared to be detached debug labels.
            // Advanced and diagnostic Live/PIP views retain the fully tethered
            // course annotations; default Live stays presentation-clean.
            draw_course_features(environment, viewport,
                projection.camera, projection.scale, false);
            draw_creature(environment, viewport,
                projection.camera, projection.scale,
                debug_skeleton_overlay, physical_bones);
            if (!annotations)
                return;
            const ui_layout::Box preview_box{
                viewport.position.x, viewport.position.y,
                viewport.size.x, viewport.size.y };
            const ui_layout::Box title_box =
                ui_layout::rig_preview_title_box(preview_box);
            add_text_fit(canvas, { title_box.x, title_box.y },
                rig_test_physical_environment.has_value()
                    ? "TEST VIEW" : "ART VIEW",
                1.00f, accent, title_box.width,
                ui_layout::persistent_text_minimum_scale);
            if (rig_test_physical_environment.has_value())
            {
                const ui_layout::RigPreviewToolbarLayout toolbar =
                    ui_layout::rig_preview_toolbar_layout(preview_box);
                add_text_fit(canvas,
                    { viewport.position.x + 18.0f,
                        toolbar.zoom_out.y + toolbar.zoom_out.height + 8.0f },
                    rig_test_terminal.frozen
                        ? std::format(
                            "TEST {} TERMINAL {} / X {:.2f} M / CELL {} / EXACT POSE HELD",
                            rig_test_terminal.physical_test_generation,
                            sim::trial_terminal_cause_name(
                                rig_test_terminal.cause,
                                rig_test_terminal.reason),
                            rig_test_terminal.position.x,
                            sim::DeformableTerrain::global_cell_x(
                                rig_test_terminal.position.x))
                        : std::format(
                        "TEST / {} / {} / L {} / R {}",
                        physical_scrub_name(), physical_scrub_authority(),
                        sim::foot_contact_phase_name(
                            environment.left_foot_phase()),
                        sim::foot_contact_phase_name(
                            environment.right_foot_phase())),
                    1.00f, yellow, viewport.size.x - 36.0f,
                    ui_layout::persistent_text_minimum_scale);
            }
            add_text_fit(canvas,
                viewport.position + Vec2{ 18.0f, viewport.size.y - 31.0f },
                std::format("{} / WATER {} / SLIP {:.3f} M/S / ZOOM {:.2f}X",
                    sim::water_traversal_phase_name(environment.water_traversal_phase()),
                    environment.water_depth(), environment.stance_slip_speed(),
                    art_editor_zoom),
                1.00f, muted, viewport.size.x - 36.0f,
                ui_layout::persistent_text_minimum_scale);
        }

        void draw_blueprint(Rect viewport, const InputState& input)
        {
            // Presets, Structure, Motors, and Test use the same physical
            // production snapshot as the Art page. Editing pages place their
            // selectable handles on that physical graph; authored coordinates
            // remain the saved state but are never drawn as a second live pose.
            // Pixels, contacts, terrain, held equipment, and runtime-derived
            // foot plates all come from one Environment and renderer.
            const bool inspect_physical_topology =
                rig_panel_page == RigPanelPage::structure
                || rig_panel_page == RigPanelPage::motors
                || rig_panel_page == RigPanelPage::test;
            draw_art_editor_world(viewport, false, inspect_physical_topology);
            const RigPreviewProjection projection =
                rig_preview_projection(viewport);
            const sim::Environment& environment = *projection.environment;
            if (rig_test_physical_environment.has_value())
            {
                const ui_layout::RigPreviewToolbarLayout toolbar =
                    ui_layout::rig_preview_toolbar_layout({
                        viewport.position.x, viewport.position.y,
                        viewport.size.x, viewport.size.y });
                add_text_fit(canvas,
                    { viewport.position.x + 18.0f,
                        toolbar.zoom_out.y + toolbar.zoom_out.height + 8.0f },
                    std::format(
                        "TEST / {} / {} / L {} / R {}",
                        physical_scrub_name(), physical_scrub_authority(),
                        sim::foot_contact_phase_name(
                            environment.left_foot_phase()),
                        sim::foot_contact_phase_name(
                            environment.right_foot_phase())),
                    1.00f, yellow, viewport.size.x - 36.0f,
                    ui_layout::persistent_text_minimum_scale);
            }
            const auto& particles = environment.particles();
            const sim::CreatureBlueprint& physical_rig = environment.blueprint();
            const bool physical_edit_overlay =
                rig_panel_page == RigPanelPage::structure
                || rig_panel_page == RigPanelPage::motors;
            const std::size_t authored_root_index = blueprint.root_node;
            const Vec2 authored_root = authored_root_index < blueprint.nodes.size()
                ? blueprint.nodes[authored_root_index] : Vec2{};
            const Vec2 physical_root = authored_root_index < particles.size()
                ? particles[authored_root_index].position : authored_root;
            const float facing = environment.facing_direction() < 0.0f
                ? -1.0f : 1.0f;
            const float scale = projection.scale;
            auto authored_to_physical = [&](Vec2 authored)
            {
                Vec2 offset = authored - authored_root;
                offset.x *= facing;
                return physical_root + offset;
            };
            auto project = [&](Vec2 authored)
            {
                return world_to_screen(authored_to_physical(authored),
                    viewport, projection.camera, scale);
            };
            auto unproject = [&](Vec2 screen_position)
            {
                const float ground_y = viewport.position.y + viewport.size.y
                    * view_camera::live_ground_fraction;
                const Vec2 physical{
                    projection.camera + (screen_position.x
                        - (viewport.position.x + viewport.size.x * 0.5f)) / scale,
                    (ground_y - screen_position.y) / scale
                };
                Vec2 offset = physical - physical_root;
                offset.x *= facing;
                return authored_root + offset;
            };

            auto physical_screen = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position,
                    viewport, projection.camera, scale);
            };
            // Editing handles must sit on the exact production pose the user
            // sees.  Projecting blueprint rest nodes here created dangling
            // circles and long false "bones" beneath articulated Human feet.
            // The authored coordinates remain the saved edit state, but the
            // visible/selectable anchors are the matching physical particles.
            auto screen = [&](std::size_t index)
            {
                return index < particles.size() ? physical_screen(index)
                    : project(blueprint.nodes[index]);
            };
            const bool physical_right_leg_near = facing < 0.0f
                ? !right_leg_near : right_leg_near;
            const auto physical_branch_side = [&](std::size_t index) noexcept
            {
                const std::uint8_t mask = index < physical_rig.nodes.size()
                    ? physical_rig.node_support_mask(index) : 0u;
                return mask == 0x1u ? -1 : mask == 0x2u ? 1 : 0;
            };
            const auto physical_branch_near = [&](std::size_t index) noexcept
            {
                const int side = physical_branch_side(index);
                return side != 0
                    && ((side > 0) == physical_right_leg_near);
            };
            if (physical_edit_overlay && selected_bone >= 0
                && static_cast<std::size_t>(selected_bone)
                    < blueprint.bones.size())
            {
                // draw_creature() already rendered every canonical physical
                // bone in depth order. Only the selected edit affordance is
                // repeated here; a second uniform graph pass would collapse
                // the rear and near sole triangles back into one cyan diamond.
                const sim::DistanceConstraint& bone = blueprint.bones[
                    static_cast<std::size_t>(selected_bone)];
                if (bone.a < blueprint.nodes.size()
                    && bone.b < blueprint.nodes.size())
                {
                    canvas.line(screen(bone.a), screen(bone.b),
                        6.0f, with_alpha(accent, 0.88f));
                }
            }
            const bool structure_labels = rig_panel_page == RigPanelPage::structure;
            std::vector<ui_layout::Box> occupied_annotations{};
            occupied_annotations.reserve(blueprint.nodes.size() * 2u + 8u);
            if (physical_edit_overlay)
            {
                for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
                {
                    const float physical_radius = (index < blueprint.radii.size()
                        ? blueprint.radii[index] : 0.15f) * scale;
                    const float radius = std::clamp(physical_radius, 4.0f, 8.0f);
                    const Vec2 center = screen(index);
                    occupied_annotations.push_back({ center.x - radius - 3.0f,
                        center.y - radius - 3.0f, radius * 2.0f + 6.0f,
                        radius * 2.0f + 6.0f });
                }
            }
            auto place_blueprint_label = [&](Vec2 anchor, std::string_view label,
                float text_scale, Color color, bool prefer_left = false,
                int row_offset = 0)
            {
                const Vec2 measured = font::measure_text(label, font_size(text_scale));
                const float row = static_cast<float>(row_offset)
                    * (measured.y + 3.0f);
                const std::array<Vec2, 6> offsets = prefer_left
                    ? std::array<Vec2, 6>{ Vec2{ -measured.x - 10.0f, -measured.y - 5.0f - row },
                        Vec2{ -measured.x - 10.0f, 7.0f + row },
                        Vec2{ 10.0f, -measured.y - 5.0f - row },
                        Vec2{ 10.0f, 7.0f + row },
                        Vec2{ -measured.x * 0.5f, -measured.y - 18.0f - row },
                        Vec2{ -measured.x * 0.5f, 18.0f + row } }
                    : std::array<Vec2, 6>{ Vec2{ 10.0f, -measured.y - 5.0f - row },
                        Vec2{ 10.0f, 7.0f + row },
                        Vec2{ -measured.x - 10.0f, -measured.y - 5.0f - row },
                        Vec2{ -measured.x - 10.0f, 7.0f + row },
                        Vec2{ -measured.x * 0.5f, -measured.y - 18.0f - row },
                        Vec2{ -measured.x * 0.5f, 18.0f + row } };
                for (const Vec2 offset : offsets)
                {
                    const Vec2 position = anchor + offset;
                    const ui_layout::Box bounds{ position.x - 2.0f,
                        position.y - 2.0f, measured.x + 4.0f,
                        measured.y + 4.0f };
                    const bool contained = bounds.x >= viewport.position.x + 4.0f
                        && bounds.y >= viewport.position.y + 4.0f
                        && bounds.x + bounds.width
                            <= viewport.position.x + viewport.size.x - 4.0f
                        && bounds.y + bounds.height
                            <= viewport.position.y + viewport.size.y - 4.0f;
                    if (!contained || std::ranges::any_of(occupied_annotations,
                        [&](const ui_layout::Box& occupied)
                        {
                            return ui_layout::overlaps(bounds, occupied);
                        }))
                        continue;
                    add_text(canvas, position, label, text_scale, color);
                    occupied_annotations.push_back(bounds);
                    return true;
                }
                return false;
            };
            if (physical_edit_overlay)
            {
                for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
                {
                    const float physical_radius = (index < blueprint.radii.size()
                        ? blueprint.radii[index] : 0.15f) * scale;
                    const float radius = std::clamp(physical_radius, 4.0f, 8.0f);
                    Color color = index == blueprint.head_node ? body_light : body;
                    if (blueprint.is_support_seed(index))
                        color = physical_branch_near(index)
                            ? rgb(0x69e4ef) : rgb(0x80698a);
                    canvas.circle(screen(index), radius, with_alpha(color, 0.28f), 24);
                    canvas.circle(screen(index), index == static_cast<std::size_t>(selected_node)
                        ? 5.0f : 3.0f,
                        index == static_cast<std::size_t>(selected_node)
                            ? accent : blueprint.is_support_seed(index)
                                ? (physical_branch_near(index)
                                    ? rgb(0x9af4f7) : rgb(0xa389ac))
                                : white,
                        18);
                    if (index == static_cast<std::size_t>(selected_node))
                        static_cast<void>(place_blueprint_label(screen(index),
                            std::to_string(index),
                            ui_layout::minimum_readable_text_scale, white));
                }
            }
            if (structure_labels)
            {
                constexpr std::size_t missing_contact =
                    std::numeric_limits<std::size_t>::max();
                const auto contact_triplet = [&](bool left)
                {
                    const auto& additional = left
                        ? physical_rig.additional_left_contact_nodes
                        : physical_rig.additional_right_contact_nodes;
                    return std::array<std::size_t, 3>{
                        left ? physical_rig.left_contact_node
                            : physical_rig.right_contact_node,
                        !additional.empty() ? additional[0] : missing_contact,
                        additional.size() >= 2u ? additional[1] : missing_contact
                    };
                };
                const auto draw_live_contact_triplet = [&](bool left)
                {
                    const std::array indices = contact_triplet(left);
                    const std::array<std::string_view, 3> labels = left
                        ? std::array<std::string_view, 3>{
                            "L LIVE HEEL", "L LIVE BALL", "L LIVE TOE" }
                        : std::array<std::string_view, 3>{
                            "R LIVE HEEL", "R LIVE BALL", "R LIVE TOE" };
                    const bool near = indices[0] < particles.size()
                        && physical_branch_near(indices[0]);
                    const Color chain_tone = near
                        ? rgb(0x69e4ef) : rgb(0xa389ac);
                    float minimum_x = std::numeric_limits<float>::infinity();
                    float maximum_x = -std::numeric_limits<float>::infinity();
                    float minimum_y = std::numeric_limits<float>::infinity();
                    for (const std::size_t index : indices)
                    {
                        if (index >= particles.size())
                            continue;
                        const Vec2 anchor = physical_screen(index);
                        minimum_x = std::min(minimum_x, anchor.x);
                        maximum_x = std::max(maximum_x, anchor.x);
                        minimum_y = std::min(minimum_y, anchor.y);
                    }
                    if (!std::isfinite(minimum_x) || !std::isfinite(minimum_y))
                        return;
                    constexpr float card_width = 148.0f;
                    constexpr float card_height = 24.0f;
                    const float label_x = left
                        ? clamp(minimum_x - card_width - 24.0f,
                            viewport.position.x + 8.0f,
                            viewport.position.x + viewport.size.x
                                - card_width * 2.0f - 36.0f)
                        : clamp(maximum_x + 24.0f,
                            viewport.position.x + card_width + 36.0f,
                            viewport.position.x + viewport.size.x
                                - card_width - 8.0f);
                    const float label_y = clamp(minimum_y - 68.0f,
                        viewport.position.y + 72.0f,
                        viewport.position.y + viewport.size.y - 72.0f);
                    for (std::size_t slot = 0u; slot < indices.size(); ++slot)
                    {
                        if (indices[slot] >= particles.size())
                            continue;
                        const Vec2 anchor = physical_screen(indices[slot]);
                        const Rect card{ { label_x,
                                label_y + static_cast<float>(slot) * 27.0f },
                            { card_width, card_height } };
                        const Vec2 tether = left
                            ? Vec2{ card.position.x + card.size.x,
                                card.position.y + card.size.y * 0.5f }
                            : Vec2{ card.position.x,
                                card.position.y + card.size.y * 0.5f };
                        canvas.line(anchor, tether,
                            near ? 1.35f : 1.05f,
                            with_alpha(chain_tone, near ? 0.90f : 0.66f));
                        canvas.circle(anchor, near ? 3.7f : 3.0f,
                            chain_tone, 14);
                        add_rounded_rect(canvas, card, 3.0f,
                            rgb(0x0a1822, 0.92f),
                            with_alpha(chain_tone, 0.74f), 1.0f);
                        add_text_fit(canvas, card.position + Vec2{ 5.0f, 4.0f },
                            labels[slot], ui_layout::minimum_readable_text_scale,
                            chain_tone, card.size.x - 10.0f,
                            ui_layout::minimum_readable_text_scale);
                    }
                };
                // Match the production renderer: rear evidence first, then
                // the near chain and its tethers. Crossed feet therefore stay
                // traceable by both depth color and deterministic draw order.
                if (physical_right_leg_near)
                {
                    draw_live_contact_triplet(true);
                    draw_live_contact_triplet(false);
                }
                else
                {
                    draw_live_contact_triplet(false);
                    draw_live_contact_triplet(true);
                }
            }
            add_text_fit(canvas,
                viewport.position + Vec2{ 18.0f, viewport.size.y - 31.0f },
                std::format("PHYSICAL PREVIEW / ZOOM {:.2f}X",
                    art_editor_zoom),
                1.00f, muted, viewport.size.x - 36.0f,
                ui_layout::persistent_text_minimum_scale);
            const sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(selected_motor)];
            if (rig_panel_page == RigPanelPage::motors
                && motor.enabled && motor.a < blueprint.nodes.size() && motor.pivot < blueprint.nodes.size()
                && motor.c < blueprint.nodes.size()
                && motor.a < particles.size() && motor.pivot < particles.size()
                && motor.c < particles.size())
            {
                // Motor limits are authored values, but their preview rays and
                // pivot are rooted in the current physical parent segment.
                const Vec2 pivot_world = particles[motor.pivot].position;
                const Vec2 reference = normalized(
                    particles[motor.a].position - pivot_world,
                    { 0.0f, 1.0f });
                const float arm_length = std::max(0.25f,
                    std::min(length(particles[motor.a].position - pivot_world),
                        length(particles[motor.c].position - pivot_world)) * 0.72f);
                const auto project_motor_world = [&](Vec2 world_position)
                {
                    return world_to_screen(world_position, viewport,
                        projection.camera, scale);
                };
                std::vector<Vec2> arc{};
                for (int segment = 0; segment <= 32; ++segment)
                {
                    const float t = static_cast<float>(segment) / 32.0f;
                    const float angle = lerp(motor.minimum_angle, motor.maximum_angle, t);
                    arc.push_back(project_motor_world(
                        pivot_world + rotate(reference, angle) * arm_length));
                }
                canvas.polyline(arc, 4.0f, accent);
                const Vec2 pivot_screen = screen(motor.pivot);
                auto ray = [&](float angle, Color color, float width)
                {
                    canvas.line(pivot_screen,
                        project_motor_world(pivot_world
                            + rotate(reference, angle) * arm_length),
                        width, color);
                };
                ray(motor.minimum_angle, danger, 2.5f);
                ray(motor.maximum_angle, danger, 2.5f);
                ray(motor.neutral_angle, white, 3.0f);
                ray(sim::motor_target_angle(motor, joint_test_input), yellow, 4.0f);
                static_cast<void>(place_blueprint_label(screen(motor.a),
                    rig_panel_page == RigPanelPage::test ? "A" : "A PARENT",
                    ui_layout::minimum_readable_text_scale, accent, true));
                static_cast<void>(place_blueprint_label(pivot_screen,
                    rig_panel_page == RigPanelPage::test ? "P" : "PIVOT",
                    ui_layout::minimum_readable_text_scale, white));
                static_cast<void>(place_blueprint_label(screen(motor.c),
                    rig_panel_page == RigPanelPage::test ? "C" : "C DRIVEN",
                    ui_layout::minimum_readable_text_scale, yellow));            }

            if (rig_panel_page == RigPanelPage::test
                && editor_weapon_class != sim::WeaponClass::none)
            {
                // The production Environment already drew the physical hand,
                // grip-anchored weapon, target, projectiles, and impacts above.
                // Test mode adds telemetry only; it must not manufacture a
                // second horizontal editor gun at a different anchor.
                add_text_fit(canvas,
                    { viewport.position.x + viewport.size.x - 410.0f,
                      viewport.position.y + 68.0f },
                    std::format("{} / {}   SHOTS {}   HITS {}",
                        sim::weapon_class_name(environment.weapon_class()),
                        sim::equipment_state_name(environment.equipment_state()),
                        environment.shots_fired(), environment.target_hits()),
                    0.92f, accent, 392.0f,
                    ui_layout::minimum_readable_text_scale);
            }
            const bool over_joint_lab = false;
            if (physical_edit_overlay && input.left_pressed && input.alt
                && contains(viewport, input.mouse) && !over_joint_lab)
            {
                auto segment_distance = [](Vec2 point, Vec2 a, Vec2 b) noexcept
                {
                    const Vec2 segment = b - a;
                    const float denominator = dot(segment, segment);
                    const float t = denominator > 1.0e-6f
                        ? clamp(dot(point - a, segment) / denominator, 0.0f, 1.0f)
                        : 0.0f;
                    return length(point - (a + segment * t));
                };
                selected_bone = -1;
                float best_distance = 16.0f;
                for (std::size_t index = 0; index < blueprint.bones.size(); ++index)
                {
                    const sim::DistanceConstraint& bone = blueprint.bones[index];
                    if (bone.a >= blueprint.nodes.size() || bone.b >= blueprint.nodes.size())
                        continue;
                    const float distance = segment_distance(
                        input.mouse, screen(bone.a), screen(bone.b));
                    if (distance < best_distance)
                    {
                        best_distance = distance;
                        selected_bone = static_cast<int>(index);
                    }
                }
                selected_node = -1;
                dragging_node = false;
            }
            if (physical_edit_overlay && input.left_pressed && !input.alt
                && contains(viewport, input.mouse) && !over_joint_lab)
            {
                int hit = -1;
                float best = 20.0f;
                for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
                {
                    const float distance = length(screen(index) - input.mouse);
                    if (distance < best)
                    {
                        best = distance;
                        hit = static_cast<int>(index);
                    }
                }
                if (input.shift && hit < 0 && blueprint.nodes.size() < 128)
                {
                    blueprint.nodes.push_back(unproject(input.mouse));
                    blueprint.radii.push_back(0.16f);
                    selected_node = static_cast<int>(blueprint.nodes.size() - 1);
                    rig_preset = RigPreset::custom;
                    set_status("NODE ADDED - CTRL CLICK ANOTHER NODE TO CONNECT");
                }
                else if (input.control && selected_node >= 0 && hit >= 0 && selected_node != hit)
                {
                    const auto a = static_cast<std::uint16_t>(selected_node);
                    const auto b = static_cast<std::uint16_t>(hit);
                    const bool exists = std::ranges::any_of(blueprint.bones, [&](const sim::DistanceConstraint& bone)
                    {
                        return (bone.a == a && bone.b == b) || (bone.a == b && bone.b == a);
                    });
                    if (!exists)
                    {
                        blueprint.bones.push_back({ a, b, length(blueprint.nodes[b] - blueprint.nodes[a]), 1.0f });
                        apply_small_rig_change("BONE CONNECTED");
                    }
                }
                else
                {
                    selected_node = hit;
                    dragging_node = hit >= 0;
                }
            }
            if (physical_edit_overlay && dragging_node && input.left_down
                && !over_joint_lab && selected_node >= 0
                && static_cast<std::size_t>(selected_node) < blueprint.nodes.size())
            {
                blueprint.nodes[static_cast<std::size_t>(selected_node)] = unproject(input.mouse);
                queue_rig_change("NODE MOVED");
            }
            if (dragging_node && input.left_released)
            {
                dragging_node = false;
            }
        }

        bool delete_selected_node()
        {
            if (selected_node < 0 || static_cast<std::size_t>(selected_node) >= blueprint.nodes.size())
            {
                set_status("SELECT A NODE FIRST");
                return false;
            }
            if (blueprint.nodes.size() <= 3)
            {
                set_status("A TRAINABLE RIG NEEDS AT LEAST THREE NODES");
                return false;
            }
            const auto removed = static_cast<std::uint16_t>(selected_node);
            const bool semantic = removed == blueprint.root_node
                || removed == blueprint.torso_node || removed == blueprint.head_node
                || blueprint.is_support_seed(removed);
            const bool motor_endpoint = std::ranges::any_of(blueprint.motors,
                [removed](const sim::MotorConstraint& motor)
                {
                    return motor.enabled && (motor.a == removed
                        || motor.pivot == removed || motor.c == removed);
                });
            if (rig_preset != RigPreset::custom || semantic || motor_endpoint)
            {
                set_status("REQUIRED PRESET / SEMANTIC / MOTOR NODE CANNOT BE DELETED");
                return false;
            }
            blueprint.nodes.erase(blueprint.nodes.begin() + selected_node);
            blueprint.radii.erase(blueprint.radii.begin() + selected_node);
            std::erase_if(blueprint.bones, [removed](const sim::DistanceConstraint& bone)
            {
                return bone.a == removed || bone.b == removed;
            });
            for (sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a > removed) --bone.a;
                if (bone.b > removed) --bone.b;
            }
            const auto last = static_cast<std::uint16_t>(blueprint.nodes.size() - 1);
            auto remap = [removed, last](std::uint16_t& index)
            {
                if (index == removed) index = 0;
                else if (index > removed) --index;
                index = std::min(index, last);
            };
            remap(blueprint.root_node); remap(blueprint.torso_node); remap(blueprint.head_node);
            remap(blueprint.left_contact_node); remap(blueprint.right_contact_node);
            auto remap_supports = [removed](std::vector<std::uint16_t>& nodes)
            {
                std::erase(nodes, removed);
                for (std::uint16_t& node : nodes)
                {
                    if (node > removed)
                        --node;
                }
            };
            remap_supports(blueprint.additional_left_contact_nodes);
            remap_supports(blueprint.additional_right_contact_nodes);
            for (sim::MotorConstraint& item : blueprint.motors)
            {
                const bool affected = item.a == removed || item.pivot == removed || item.c == removed;
                remap(item.a); remap(item.pivot); remap(item.c);
                if (affected || item.a == item.pivot || item.pivot == item.c || item.a == item.c)
                    item.enabled = false;
            }
            selected_node = -1;
            selected_bone = -1;
            apply_small_rig_change("NODE DELETED; AFFECTED MOTORS DISABLED");
            return true;
        }

        void draw_rig_panel(Rect rect, const InputState& raw_input,
            bool persistent_commands)
        {
            const float inspector_header_height =
                ui_layout::rig_inspector_header_height_for_width(rect.size.x);
            const Rect body_rect{
                rect.position + Vec2{ 0.0f, inspector_header_height },
                { rect.size.x,
                    std::max(0.0f, rect.size.y - inspector_header_height) }
            };
            rig_panel_scroll_offset = ui_layout::panel_scroll_offset(
                rig_panel_scroll_offset,
                contains(body_rect, raw_input.mouse) ? raw_input.wheel : 0.0f,
                rig_panel_content_height, body_rect.size.y);
            const InputState input = panel_input(rect, raw_input);
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const float usable = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            add_text_fit(canvas, cursor, "RIG INSPECTOR",
                ui_layout::workspace_heading_text_scale,
                white, usable, ui_layout::persistent_text_minimum_scale);
            const std::array<ui_layout::Box, 5> page_tabs =
                ui_layout::rig_inspector_page_button_layout({
                    rect.position.x, rect.position.y,
                    rect.size.x, rect.size.y });
            auto page_button = [&](std::size_t slot,
                std::string_view label, RigPanelPage page)
            {
                const ui_layout::Box tab = page_tabs[slot];
                if (button({ { tab.x, tab.y }, { tab.width, tab.height } },
                    label, input, rig_panel_page == page))
                {
                    rig_panel_page = page;
                    rig_panel_scroll_offset = 0.0f;
                }
            };
            page_button(0, "RIGS", RigPanelPage::presets);
            page_button(1, "BONES", RigPanelPage::structure);
            page_button(2, "MOTORS", RigPanelPage::motors);
            page_button(3, "ART", RigPanelPage::art);
            page_button(4, "TEST", RigPanelPage::test);
            canvas.line({ rect.position.x + 12.0f,
                    body_rect.position.y - 1.0f },
                { rect.position.x + rect.size.x - 12.0f,
                    body_rect.position.y - 1.0f }, 1.0f, border);
            canvas.push_clip(body_rect.position + Vec2{ 1.0f, 1.0f },
                body_rect.position + body_rect.size - Vec2{ 1.0f, 1.0f });
            cursor = body_rect.position
                + Vec2{ 18.0f, 18.0f - rig_panel_scroll_offset };

            const auto inspector_page_header = [&](std::string_view title,
                std::string_view subtitle)
            {
                const Rect card{ cursor, { usable, 104.0f } };
                add_rounded_rect(canvas, card, 10.0f, panel_alt, border, 1.0f);
                add_text_fit(canvas, cursor + Vec2{ 14.0f, 12.0f }, title,
                    ui_layout::section_heading_text_scale, accent,
                    usable - 28.0f,
                    ui_layout::persistent_text_minimum_scale);
                canvas.push_clip(card.position + Vec2{ 14.0f, 48.0f },
                    card.position + card.size - Vec2{ 14.0f, 8.0f });
                add_wrapped_text(canvas, cursor + Vec2{ 14.0f, 48.0f },
                    subtitle, ui_layout::persistent_text_minimum_scale,
                    muted, usable - 28.0f, 2.0f);
                canvas.pop_clip();
                cursor.y += 120.0f;
            };

            if (rig_panel_page == RigPanelPage::presets)
            {
                inspector_page_header("RIG LIBRARY", "SPECIES, POLICY, AND SAVED RIGS");
                const float card_bottom = cursor.y + 760.0f;
                add_rounded_rect(canvas, { cursor, { usable, 760.0f } },
                    10.0f, panel_alt, border, 1.0f);
                cursor += Vec2{ 14.0f, 16.0f };
                const float page_usable = usable - 28.0f;
                const float half = (page_usable - 6.0f) * 0.5f;
                auto preset = [&](int row, int column, std::string_view label,
                    RigPreset value)
                {
                    const Vec2 position = cursor + Vec2{
                        static_cast<float>(column) * (half + 6.0f),
                        static_cast<float>(row) * 58.0f };
                    if (button({ position, { half, 50.0f } }, label, input,
                        rig_preset == value))
                        use_preset(value);
                };
                preset(0, 0, "HUMAN", RigPreset::humanoid);
                preset(0, 1, "CHICKEN", RigPreset::chicken);
                preset(1, 0, "DOG", RigPreset::crawler4);
                preset(1, 1, "HEXAPOD", RigPreset::hexapod);
                cursor.y += 126.0f;
                const bool morphology_mode = autonomy.optimization_mode
                    == rl::RigOptimizationMode::morphology_evolve;
                if (button({ cursor, { page_usable, 50.0f } },
                    "CONTROL OPTIMIZE", input,
                    !morphology_mode))
                {
                    trainer.set_rig_optimization_mode(
                        rl::RigOptimizationMode::control_optimize);
                    set_status("CONTROL OPTIMIZE SELECTED - ANATOMY LOCKED");
                }
                cursor.y += 58.0f;
                if (button({ cursor, { page_usable, 50.0f } },
                    "MORPHOLOGY EVOLVE", input, morphology_mode))
                {
                    trainer.set_rig_optimization_mode(
                        rl::RigOptimizationMode::morphology_evolve);
                    set_status("MORPHOLOGY EVOLVE SELECTED - HELD-OUT CHANGES ONLY");
                }
                cursor.y += 64.0f;

                if (button({ cursor, { half, 50.0f } }, "SAVE RIG", input)
                    || input.save_pressed)
                {
                    const std::filesystem::path path = active_rig_path();
                    std::string error{};
                    set_status(blueprint.save(path, error)
                        ? std::format("{} SAVED", path.filename().string())
                        : error);
                }
                if (button({ cursor + Vec2{ half + 6.0f, 0.0f },
                    { half, 50.0f } }, "LOAD RIG", input) || input.load_pressed)
                {
                    const std::filesystem::path path = active_rig_path();
                    std::string error{};
                    std::optional<sim::CreatureBlueprint> candidate{};
                    if (const auto species = preset_species(rig_preset))
                    {
                        candidate = sim::CreatureBlueprint::load_for_species(
                            path, *species, error);
                    }
                    else
                    {
                        sim::CreatureBlueprint loaded =
                            sim::CreatureBlueprint::load(path, error);
                        if (error.empty())
                            candidate = std::move(loaded);
                    }
                    if (candidate)
                    {
                        set_status(commit_canonical_rig(
                                std::move(*candidate), false)
                            ? std::format(
                                "{} LOADED SAFELY - FRESH BALANCE LESSON",
                                path.filename().string())
                            : std::format(
                                "{} NOT LOADED - CANONICAL PREVIEW REJECTED",
                                path.filename().string()));
                    }
                    else
                    {
                        set_status(std::format("{} NOT LOADED - ACTIVE RIG UNCHANGED: {}",
                            path.filename().string(), error));
                    }
                }
                cursor.y += 64.0f;
                if (button({ cursor, { page_usable, 52.0f } },
                    "COPY TRAINING RIG", input))
                {
                    if (install_canonical_rig_preview(trainer.blueprint()))
                    {
                        rig_preset = preset_for_species(
                            blueprint.presentation_species());
                        select_autosave_rig_file(blueprint);
                        set_status(std::format(
                            "TRAINING {} COPIED TO EDITOR; SAVE TARGET {}",
                            preset_name(), active_rig_path().filename().string()));
                    }
                    else
                        set_status(
                            "TRAINING RIG COPY REJECTED - ACTIVE RIG UNCHANGED");
                }
                cursor.y += 66.0f;
                if (button({ cursor, { page_usable, 50.0f } }, "RESTORE CHAMPION",
                    input, trainer.has_best_policy(), trainer.has_best_policy()))
                {
                    set_status(trainer.restore_best_policy()
                        ? "RETAINED CONTROLLER RESTORE QUEUED"
                        : "NO RETAINED CONTROLLER AVAILABLE");
                }
                cursor.y += 58.0f;
                if (button({ cursor, { page_usable, 50.0f } },
                    "FRESH CONTROLLER", input))
                {
                    trainer.reset_policy(0x721300u
                        + autonomy.rig_generation * 0x9E3779B97F4A7C15ULL);
                    set_status("FRESH CONTROLLER QUEUED FOR UNCHANGED ANATOMY");
                }
                cursor.y += 64.0f;
                const float visual_width = persistent_commands
                    ? (page_usable - 6.0f) * 0.5f
                    : (page_usable - 12.0f) / 3.0f;
                if (button({ cursor, { visual_width, 50.0f } },
                    right_leg_near ? "NEAR: RIGHT" : "NEAR: LEFT",
                    input, right_leg_near))
                    right_leg_near = !right_leg_near;
                if (!persistent_commands
                    && button({ cursor + Vec2{ visual_width + 6.0f, 0.0f },
                        { visual_width, 50.0f } },
                        rig_lab_art_enabled ? "ART ON" : "ART OFF",
                        input, rig_lab_art_enabled))
                    rig_lab_art_enabled = !rig_lab_art_enabled;
                const float bones_x = persistent_commands
                    ? visual_width + 6.0f
                    : (visual_width + 6.0f) * 2.0f;
                if (button({ cursor + Vec2{ bones_x, 0.0f },
                    { visual_width, 50.0f } },
                    debug_skeleton_overlay ? "BONES ON" : "BONES OFF",
                    input, debug_skeleton_overlay))
                    debug_skeleton_overlay = !debug_skeleton_overlay;
                cursor.y += 66.0f;
                add_text_fit(canvas, cursor,
                    std::format("{} {}",
                        rl::rig_optimization_mode_name(autonomy.optimization_mode),
                        autonomy.rig_generation),
                    1.25f, accent, page_usable, 1.12f);
                cursor.y += 34.0f;
                add_text_fit(canvas, cursor,
                    std::format("ACCEPTED {} / REJECTED {}",
                        autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes),
                    1.25f, muted, page_usable, 1.12f);
                cursor.y += 34.0f;
                add_text_fit(canvas, cursor,
                    std::format("ROLLBACKS {}", autonomy.rollback_count),
                    1.25f, muted, page_usable, 1.12f);
                cursor.y = std::max(cursor.y, card_bottom);
            }
            else if (rig_panel_page == RigPanelPage::structure)
            {
                inspector_page_header("STRUCTURE", "DIRECT ANATOMY AND CONTACT ROLES");
                const float card_bottom = cursor.y + 690.0f;
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 10.0f, 0.0f },
                        { usable + 20.0f, 690.0f } },
                    10.0f, panel_alt, border, 1.0f);
                cursor.y += 16.0f;
                add_text_fit(canvas, cursor,
                    std::format("LIVE APPLY: {}",
                        trainer.live_morphology_preview_active()
                            ? "PREVIEW" : "COMMITTED"),
                    1.25f, trainer.live_morphology_preview_active()
                        ? yellow : green, usable, 1.12f);
                cursor.y += 34.0f;
                add_text_fit(canvas, cursor,
                    std::format("SIGNATURE {:016X}", blueprint.signature()),
                    1.25f, muted, usable, 1.12f);
                cursor.y += 36.0f;
                add_text_fit(canvas, cursor,
                    std::format("SELECTED NODE: {}", selected_node),
                    1.25f, white, usable, 1.12f);
                cursor.y += 38.0f;
                if (button({ cursor, { usable, 50.0f } }, "DELETE NODE", input,
                    false, selected_node >= 0))
                    delete_selected_node();
                cursor.y += 64.0f;
                if (selected_node >= 0
                    && static_cast<std::size_t>(selected_node) < blueprint.radii.size())
                {
                    float& radius = blueprint.radii[static_cast<std::size_t>(selected_node)];
                    const std::size_t node_index = static_cast<std::size_t>(selected_node);
                    const std::size_t connected = static_cast<std::size_t>(std::count_if(
                        blueprint.bones.begin(), blueprint.bones.end(),
                        [node_index](const sim::DistanceConstraint& bone) {
                            return bone.a == node_index || bone.b == node_index;
                        }));
                    add_text_fit(canvas, cursor,
                        std::format("AUTHORED ({:.3f}, {:.3f}) M   LINKS {}   SUPPORT {}",
                            blueprint.nodes[node_index].x, blueprint.nodes[node_index].y,
                            connected, blueprint.node_support_mask(node_index) != 0u ? "YES" : "NO"),
                        1.25f, muted, usable, 1.12f);
                    cursor.y += 38.0f;
                    const float updated = slider({ cursor, { usable, 48.0f } },
                        "NODE SIZE", radius, 0.08f, 0.60f, input);
                    if (updated != radius)
                    {
                        radius = updated;
                        queue_rig_change("NODE SIZE UPDATED");
                    }
                    cursor.y += 64.0f;
                    add_text(canvas, cursor, "SEMANTIC ROLE", 1.25f, accent);
                    cursor.y += 34.0f;
                    const float role = (usable - 8.0f) * 0.5f;
                    auto set_role = [&](int slot, std::string_view label,
                        std::uint16_t& target)
                    {
                        const float role_x = static_cast<float>(slot % 2)
                            * (role + 8.0f);
                        const float role_y = static_cast<float>(slot / 2) * 58.0f;
                        if (button({ cursor + Vec2{ role_x, role_y },
                            { role, 50.0f } }, label, input,
                            target == selected_node))
                        {
                            target = static_cast<std::uint16_t>(selected_node);
                            apply_small_rig_change("NODE ROLE UPDATED");
                        }
                    };
                    set_role(0, "ROOT", blueprint.root_node);
                    set_role(1, "TORSO", blueprint.torso_node);
                    set_role(2, "HEAD", blueprint.head_node);
                    set_role(3, "PHASE A", blueprint.left_contact_node);
                    set_role(4, "PHASE B", blueprint.right_contact_node);
                    cursor.y += 180.0f;
                }
                add_text_fit(canvas, cursor,
                    std::format("SELECTED BONE: {}", selected_bone),
                    1.25f, white, usable, 1.12f);
                cursor.y += 42.0f;
                if (selected_bone >= 0
                    && static_cast<std::size_t>(selected_bone) < blueprint.bones.size())
                {
                    sim::DistanceConstraint& selected = blueprint.bones[
                        static_cast<std::size_t>(selected_bone)];
                    const float stiffness = slider({ cursor, { usable, 48.0f } },
                        "BONE STIFFNESS", selected.stiffness, 0.20f, 1.0f, input);
                    if (stiffness != selected.stiffness)
                    {
                        selected.stiffness = stiffness;
                        queue_rig_change("BONE STIFFNESS UPDATED");
                    }
                    cursor.y += 64.0f;
                    add_text_fit(canvas, cursor,
                        std::format("ENDPOINTS {}-{}   LENGTH {:.3f} M   STIFFNESS {:.2f}",
                            selected.a, selected.b,
                            runner::length(blueprint.nodes[selected.b] - blueprint.nodes[selected.a]),
                            selected.stiffness),
                        1.25f, muted, usable, 1.12f);
                    cursor.y += 42.0f;
                    if (button({ cursor, { usable, 50.0f } }, "DELETE SELECTED BONE", input))
                    {
                        sim::CreatureBlueprint candidate = blueprint;
                        candidate.bones.erase(candidate.bones.begin() + selected_bone);
                        if (candidate.valid() && blueprint_connected(candidate))
                        {
                            blueprint = std::move(candidate);
                            selected_bone = -1;
                            apply_small_rig_change("BONE DELETED");
                        }
                        else
                            set_status("BONE DELETE REJECTED - RIG WOULD DISCONNECT");
                    }
                }
                cursor.y = std::max(cursor.y, card_bottom);
            }
            else if (rig_panel_page == RigPanelPage::motors)
            {
                inspector_page_header("MOTORS", "JOINT RANGE, POWER, AND LIVE RESPONSE");
                const float card_bottom = cursor.y + 910.0f;
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 10.0f, 0.0f },
                        { usable + 20.0f, 910.0f } },
                    10.0f, panel_alt, border, 1.0f);
                cursor.y += 16.0f;
                const float motor_width = (usable - 8.0f) * 0.5f;
                for (int index = 0; index < static_cast<int>(sim::anatomy_action_count); ++index)
                {
                    const int column = index % 2;
                    const int row = index / 2;
                    const bool available = static_cast<std::size_t>(index)
                        < blueprint.active_motor_count;
                    if (button({ cursor + Vec2{
                            static_cast<float>(column) * (motor_width + 8.0f),
                            static_cast<float>(row) * 58.0f },
                        { motor_width, 50.0f } }, std::format("MOTOR {}", index + 1),
                        input, selected_motor == index, available))
                    {
                        selected_motor = index;
                        joint_test_group = JointTestGroup::selected;
                    }
                }
                const int motor_rows = (static_cast<int>(
                    sim::anatomy_action_count) + 1) / 2;
                cursor.y += static_cast<float>(motor_rows) * 58.0f + 8.0f;
                const auto names = motor_names();
                add_text_fit(canvas, cursor,
                    names[static_cast<std::size_t>(selected_motor)],
                    1.28f, white, usable,
                    ui_layout::trainer_primary_minimum_text_scale);
                cursor.y += 42.0f;
                sim::MotorConstraint& motor = blueprint.motors[
                    static_cast<std::size_t>(selected_motor)];
                const float third = (usable - 12.0f) / 3.0f;
                auto endpoint = [&](int slot, std::string_view label,
                    std::uint16_t& value)
                {
                    if (!button({ cursor + Vec2{ static_cast<float>(slot) * (third + 6.0f), 0.0f },
                        { third, 50.0f } }, label, input, false, selected_node >= 0))
                        return;
                    value = static_cast<std::uint16_t>(selected_node);
                    const bool connected = motor.a != motor.pivot
                        && motor.pivot != motor.c && motor.a != motor.c
                        && has_direct_bone(motor.a, motor.pivot)
                        && has_direct_bone(motor.pivot, motor.c);
                    motor.enabled = connected;
                    if (connected)
                    {
                        blueprint.calibrate_motor(
                            static_cast<std::size_t>(selected_motor), 30.0f,
                            30.0f, motor.strength);
                        apply_small_rig_change("MOTOR ENDPOINT UPDATED");
                    }
                    else
                        set_status("MOTOR NEEDS REAL A-PIVOT AND PIVOT-C BONES");
                };
                endpoint(0, "SET PARENT", motor.a);
                endpoint(1, "SET PIVOT", motor.pivot);
                endpoint(2, "SET DRIVEN", motor.c);
                cursor.y += 64.0f;
                const bool connected = motor.a < blueprint.nodes.size()
                    && motor.pivot < blueprint.nodes.size()
                    && motor.c < blueprint.nodes.size()
                    && motor.a != motor.pivot && motor.pivot != motor.c
                    && motor.a != motor.c
                    && has_direct_bone(motor.a, motor.pivot)
                    && has_direct_bone(motor.pivot, motor.c);
                add_text_fit(canvas, cursor,
                    std::format("PARENT {} / PIVOT {} / DRIVEN {}",
                        motor.a, motor.pivot, motor.c),
                    1.25f, connected ? green : yellow, usable, 1.12f);
                cursor.y += 36.0f;
                add_text_fit(canvas, cursor,
                    std::format("STATE: {}",
                        connected ? (motor.enabled ? "READY" : "DISABLED")
                            : "NOT CONNECTED"),
                    1.25f, connected ? green : yellow, usable, 1.12f);
                cursor.y += 42.0f;
                const sim::MotorDiagnostic diagnostic =
                    trainer.preview().motor_diagnostic(
                        static_cast<std::size_t>(selected_motor));
                if (diagnostic.available)
                {
                    constexpr float radians_to_degrees = 180.0f / pi;
                    add_text_fit(canvas, cursor, std::format(
                        "LIVE {:+.1f} DEG / REST {:+.1f} DEG",
                        diagnostic.current_angle * radians_to_degrees,
                        diagnostic.authored_neutral * radians_to_degrees),
                        1.25f, trainer.live_morphology_preview_active()
                            ? green : accent, usable, 1.12f);
                    cursor.y += 36.0f;
                    add_text_fit(canvas, cursor, std::format(
                        "TARGET {:+.1f} DEG / ACTION {:+.2f}",
                        diagnostic.target_angle * radians_to_degrees,
                        diagnostic.applied_action),
                        1.25f, muted, usable, 1.12f);
                    cursor.y += 36.0f;
                    add_text_fit(canvas, cursor, std::format(
                        "VEL {:+.1f} DEG/S / SLOT {}",
                        diagnostic.angular_velocity * radians_to_degrees,
                        diagnostic.action_slot + 1u),
                        1.25f, muted, usable, 1.12f);
                    cursor.y += 36.0f;
                    add_text_fit(canvas, cursor, std::format(
                        "BRANCH: {}",
                        diagnostic.support_branch ? "SUPPORT" : "MANIPULATOR"),
                        1.25f, muted, usable, 1.12f);
                    cursor.y += 38.0f;
                }
                float negative = (motor.neutral_angle - motor.minimum_angle)
                    * 180.0f / pi;
                float positive = (motor.maximum_angle - motor.neutral_angle)
                    * 180.0f / pi;
                const float updated_negative = slider({ cursor, { usable, 48.0f } },
                    "NEGATIVE RANGE", negative, 2.0f, 120.0f, input, " DEG");
                if (updated_negative != negative)
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, positive, motor.strength);
                    queue_rig_change("NEGATIVE RANGE UPDATED");
                }
                cursor.y += 64.0f;
                const float updated_positive = slider({ cursor, { usable, 48.0f } },
                    "POSITIVE RANGE", positive, 2.0f, 120.0f, input, " DEG");
                if (updated_positive != positive)
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, updated_positive, motor.strength);
                    queue_rig_change("POSITIVE RANGE UPDATED");
                }
                cursor.y += 64.0f;
                const float power = slider({ cursor, { usable, 48.0f } },
                    "MOTOR POWER", motor.strength, 0.0f, 0.20f, input);
                if (power != motor.strength)
                {
                    motor.strength = power;
                    queue_rig_change("MOTOR POWER UPDATED");
                }
                cursor.y += 66.0f;
                if (button({ cursor, { third, 50.0f } }, "SET REST", input,
                    false, connected))
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, updated_positive, motor.strength);
                    apply_small_rig_change("REST POSE RECALIBRATED");
                }
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 50.0f } },
                    "SAFE RANGE", input, false, connected))
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        18.0f, 20.0f, 0.050f);
                    apply_small_rig_change("SAFE MOTOR DEFAULT APPLIED");
                }
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 50.0f } }, motor.enabled ? "DISABLE" : "ENABLE",
                    input, motor.enabled, connected))
                {
                    motor.enabled = !motor.enabled;
                    apply_small_rig_change(motor.enabled
                        ? "MOTOR ENABLED" : "MOTOR DISABLED");
                }
                cursor.y = std::max(cursor.y, card_bottom);
            }
            else if (rig_panel_page == RigPanelPage::art)
            {
                inspector_page_header("ART", "MODULE FIT, LAYERS, AND PREVIEW");
                const float card_bottom = cursor.y + 1180.0f;
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 10.0f, 0.0f },
                        { usable + 20.0f, 1180.0f } },
                    10.0f, panel_alt, border, 1.0f);
                cursor.y += 16.0f;
                // Art remains an independent Rig Lab presentation choice. Keep
                // it directly beside the authoring controls in both compact
                // and dual-pane workspaces so the canonical preview can be
                // compared with the skeleton without opening Setup.
                if (button({ cursor, { usable, 52.0f } },
                    rig_lab_art_enabled ? "RIG ART ON" : "RIG ART OFF",
                    input, rig_lab_art_enabled))
                    rig_lab_art_enabled = !rig_lab_art_enabled;
                cursor.y += 60.0f;
                const ui_layout::RigArtActionLayout art_actions =
                    ui_layout::rig_art_action_layout({ cursor.x, cursor.y,
                        usable, 0.0f });
                if (button({ { art_actions.skeleton.x, art_actions.skeleton.y },
                        { art_actions.skeleton.width,
                            art_actions.skeleton.height } },
                    debug_skeleton_overlay ? "HIDE SKELETON" : "SHOW SKELETON",
                    input,
                    debug_skeleton_overlay))
                    debug_skeleton_overlay = !debug_skeleton_overlay;
                if (button({ { art_actions.freeze.x, art_actions.freeze.y },
                        { art_actions.freeze.width, art_actions.freeze.height } },
                    art_preview_frozen ? "RESUME" : "FREEZE",
                    input, art_preview_frozen))
                {
                    art_preview_frozen = !art_preview_frozen;
                    if (art_preview_frozen)
                        frozen_art_environment = rig_preview_environment();
                    else
                        frozen_art_environment.reset();
                }
                if (button({ { art_actions.backdrop.x, art_actions.backdrop.y },
                        { art_actions.backdrop.width,
                            art_actions.backdrop.height } },
                    "BACKDROP", input,
                    art_editor_high_contrast))
                    art_editor_high_contrast = !art_editor_high_contrast;
                cursor.y += 120.0f;

                constexpr std::array modules{
                    art::Module::body, art::Module::head,
                    art::Module::tail, art::Module::equipment,
                    art::Module::upper_limb, art::Module::lower_limb,
                    art::Module::terminal, art::Module::hand
                };
                constexpr std::array<std::string_view, 8> module_labels{
                    "BODY", "HEAD", "TAIL", "EQUIPMENT",
                    "UPPER LIMB", "LOWER LIMB", "FOOT", "HAND" };
                const float module_button_width = (usable - 8.0f) * 0.5f;
                for (std::size_t index = 0u; index < modules.size(); ++index)
                {
                    const float x = static_cast<float>(index % 2u)
                        * (module_button_width + 8.0f);
                    const float y = static_cast<float>(index / 2u) * 58.0f;
                    if (button({ cursor + Vec2{ x, y },
                            { module_button_width, 50.0f } },
                        module_labels[index], input,
                        selected_art_module == modules[index]))
                        selected_art_module = modules[index];
                }
                cursor.y += 238.0f;
                art::Layout& layout = active_art_layout();
                art::ModuleAdjustment& module = layout.at(selected_art_module);
                add_text_fit(canvas, cursor,
                    std::format("{} / {}",
                        sim::creature_species_name(layout.species),
                        art::module_name(selected_art_module)),
                    1.25f, green, usable, 1.12f);
                cursor.y += 34.0f;
                add_text_fit(canvas, cursor,
                    std::format("PIVOT {:.2f} / DEPTH {:+d}", module.pivot,
                        static_cast<int>(module.layer)),
                    1.25f, muted, usable, 1.12f);
                cursor.y += 38.0f;
                auto update = [&](float& target, float value)
                {
                    if (std::abs(target - value) <= 1.0e-5f)
                        return;
                    target = value;
                    art_edit_pending = true;
                };
                float value = slider({ cursor, { usable, 48.0f } },
                    "ANCHOR ALONG", module.anchor_along,
                    -0.30f, 0.30f, input);
                update(module.anchor_along, value);
                cursor.y += 62.0f;
                value = slider({ cursor, { usable, 48.0f } },
                    "ANCHOR NORMAL", module.anchor_normal,
                    -0.30f, 0.30f, input);
                update(module.anchor_normal, value);
                cursor.y += 62.0f;
                value = slider({ cursor, { usable, 48.0f } },
                    "PIVOT", module.pivot, 0.0f, 1.0f, input);
                update(module.pivot, value);
                cursor.y += 62.0f;
                value = slider({ cursor, { usable, 48.0f } },
                    "LENGTH", module.length_scale, 0.65f, 1.35f, input);
                update(module.length_scale, value);
                cursor.y += 62.0f;
                value = slider({ cursor, { usable, 48.0f } },
                    "THICKNESS", module.thickness_scale, 0.65f, 1.35f, input);
                update(module.thickness_scale, value);
                cursor.y += 62.0f;
                const float rotation = angle_slider(
                    { cursor, { usable, 48.0f } }, "ROTATION", module.rotation,
                    -35.0f, 35.0f, input);
                update(module.rotation, rotation);
                cursor.y += 62.0f;
                const float half_action = (usable - 8.0f) * 0.5f;
                auto commit_now = [&](std::string_view message)
                {
                    active_art_history().commit(layout);
                    set_status(std::string{ message });
                };
                if (button({ cursor, { half_action, 48.0f } },
                    module.flip_vertical ? "MIRROR VERTICAL ON" : "MIRROR VERTICAL", input,
                    module.flip_vertical))
                {
                    module.flip_vertical = !module.flip_vertical;
                    commit_now("ART VERTICAL FLIP UPDATED");
                }
                if (button({ cursor + Vec2{ half_action + 8.0f, 0.0f },
                    { half_action, 48.0f } }, module.flip_horizontal
                        ? "MIRROR HORIZONTAL ON" : "MIRROR HORIZONTAL", input,
                    module.flip_horizontal))
                {
                    module.flip_horizontal = !module.flip_horizontal;
                    commit_now("ART HORIZONTAL FLIP UPDATED");
                }
                cursor.y += 58.0f;
                const float third_action = (usable - 16.0f) / 3.0f;
                if (button({ cursor, { third_action, 48.0f } }, "LAYER -", input, false,
                    module.layer > -2))
                {
                    --module.layer;
                    commit_now("ART DEPTH UPDATED");
                }
                if (button({ cursor + Vec2{ third_action + 8.0f, 0.0f },
                    { third_action, 48.0f } }, "LAYER +", input, false,
                    module.layer < 2))
                {
                    ++module.layer;
                    commit_now("ART DEPTH UPDATED");
                }
                if (button({ cursor + Vec2{ (third_action + 8.0f) * 2.0f, 0.0f },
                    { third_action, 48.0f } }, "RESET MODULE", input))
                {
                    module = {};
                    commit_now("SELECTED ART MODULE RESET");
                }
                cursor.y += 64.0f;
                const float third = (usable - 12.0f) / 3.0f;
                if (button({ cursor, { third, 48.0f } }, "UNDO", input,
                    false, active_art_history().can_undo()))
                    static_cast<void>(active_art_history().undo(layout));
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f },
                    { third, 48.0f } }, "REDO", input,
                    false, active_art_history().can_redo()))
                    static_cast<void>(active_art_history().redo(layout));
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 48.0f } }, right_leg_near
                        ? "NEAR: RIGHT" : "NEAR: LEFT", input,
                    right_leg_near))
                    right_leg_near = !right_leg_near;
                cursor.y += 64.0f;
                const float half = (usable - 6.0f) * 0.5f;
                if (button({ cursor, { half, 48.0f } }, "SAVE LAYOUT", input))
                {
                    std::error_code directory_error{};
                    std::filesystem::create_directories(
                        art_layout_directory, directory_error);
                    std::string error{};
                    set_status(!directory_error
                            && art::save_layout(active_art_layout_path(), layout, error)
                        ? "SPECIES ART LAYOUT SAVED"
                        : "ART SAVE FAILED - " + error);
                }
                if (button({ cursor + Vec2{ half + 6.0f, 0.0f },
                    { half, 48.0f } }, "LOAD LAYOUT", input))
                {
                    art::Layout loaded{};
                    std::string error{};
                    if (art::load_layout(active_art_layout_path(),
                        layout.species, loaded, error))
                    {
                        layout = loaded;
                        active_art_history().reset(layout);
                        set_status("SPECIES ART LAYOUT LOADED");
                    }
                    else
                        set_status("ART LOAD FAILED - " + error);
                }
                cursor.y += 64.0f;
                add_text_fit(canvas, cursor,
                    std::format("SLIP {:.3f} M / WATER {}",
                        trainer.preview().stance_slip_distance(),
                        sim::water_traversal_phase_name(
                            trainer.preview().water_traversal_phase())),
                    1.25f, green, usable, 1.12f);
                cursor.y = std::max(cursor.y, card_bottom);
            }
            else
            {
                inspector_page_header("TEST", "PHYSICAL JOINT, TRACTION, AND AIMING");
                const float card_bottom = cursor.y + 730.0f;
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 10.0f, 0.0f },
                        { usable + 20.0f, 730.0f } },
                    10.0f, panel_alt, border, 1.0f);
                cursor.y += 16.0f;
                const Rect test_card{ cursor,
                    { usable, ui_layout::rig_lab_test_card_height } };
                draw_joint_lab(test_card, input);
                cursor.y += 354.0f;
                add_text_fit(canvas, cursor, "EQUIPMENT TEST AUTHORING",
                    1.50f, accent, usable, 1.25f);
                cursor.y += 42.0f;
                const float weapon_width = (usable - 8.0f) * 0.5f;
                const std::array weapon_choices{ sim::WeaponClass::none,
                    sim::WeaponClass::sidearm, sim::WeaponClass::carbine,
                    sim::WeaponClass::launcher };
                for (std::size_t index = 0; index < weapon_choices.size(); ++index)
                {
                    const sim::WeaponClass weapon = weapon_choices[index];
                    constexpr std::array<std::string_view, 4> weapon_labels{
                        "NONE", "PISTOL", "RIFLE", "LAUNCH" };
                    if (button({ cursor + Vec2{
                            static_cast<float>(index % 2u)
                                * (weapon_width + 8.0f),
                            static_cast<float>(index / 2u) * 58.0f },
                            { weapon_width, 50.0f } },
                        weapon_labels[index], input,
                        editor_weapon_class == weapon,
                        !rig_test_terminal.frozen))
                    {
                        editor_weapon_class = weapon;
                        trainer.configure_preview_equipment(
                            editor_weapon_class, editor_target_distance);
                        if (editor_weapon_class == sim::WeaponClass::none)
                        {
                            if (rig_test_physical_environment.has_value())
                                rig_test_physical_environment->clear_equipment();
                            set_status("PHYSICAL TEST EQUIPMENT CLEARED");
                        }
                        else
                        {
                            start_physical_rig_test(
                                sim::CourseStage::equipment_targets);
                            set_status(
                                "TEST / PHYSICAL AIM MOTOR SCRUB - ASSISTED TEACHER / STATIC COURSE");
                        }
                    }
                }
                cursor.y += 126.0f;
                const float previous_target_distance = editor_target_distance;
                const InputState target_input = rig_test_terminal.frozen
                    ? InputState{} : input;
                editor_target_distance = slider({ cursor, { usable, 48.0f } },
                    "TARGET DISTANCE", editor_target_distance, 3.0f, 24.0f,
                    target_input, " M");
                if (std::abs(editor_target_distance - previous_target_distance) > 1.0e-4f)
                {
                    trainer.configure_preview_equipment(
                        editor_weapon_class, editor_target_distance);
                    if (rig_test_physical_environment.has_value()
                        && editor_weapon_class != sim::WeaponClass::none)
                        rig_test_physical_environment->configure_equipment(
                            editor_weapon_class, editor_target_distance);
                }
                cursor.y += 64.0f;
                add_text_fit(canvas, cursor,
                    "TESTS DO NOT ALTER SAVED POLICY",
                    1.25f, muted, usable, 1.12f);
                cursor.y += 38.0f;
                cursor.y = std::max(cursor.y, card_bottom);
            }
            rig_panel_content_height = std::max(body_rect.size.y,
                cursor.y + rig_panel_scroll_offset
                    - body_rect.position.y + 48.0f);
            rig_panel_scroll_offset = ui_layout::clamp_scroll_offset(
                rig_panel_scroll_offset, rig_panel_content_height,
                body_rect.size.y);
            canvas.pop_clip();
            draw_panel_scrollbar(body_rect, rig_panel_content_height,
                rig_panel_scroll_offset);
            add_rounded_rect(canvas, rect, 11.0f, ui_render::transparent_fill, border, 1.0f);
        }

        void process_shortcuts(const InputState& input)
        {
            if (input.escape_pressed)
                quit = true;
            if (input.units_pressed)
                distance_units = distance_units == ui_layout::DistanceUnits::metric
                    ? ui_layout::DistanceUnits::imperial : ui_layout::DistanceUnits::metric;
            if (input.art_pressed)
                art_toggle_for_mode() = !art_enabled_for_mode();
            if (presentation::isolated(presentation_session))
            {
                if (input.space_pressed && presentation::permits_replay_controls(
                        presentation_session) && !walk_eye_terminal.frozen)
                    run_paused = !run_paused;
                if (input.reset_pressed)
                {
                    live_zoom_factor = 1.0f;
                    live_zoom_auto = true;
                    if (presentation::walk_eye(presentation_session)
                        && walk_eye_replay_start.has_value()
                        && course_eye_test_environment.has_value())
                    {
                        const sim::GuidanceMode guidance =
                            course_eye_test_environment->guidance_mode();
                        retry_walk_eye_replay(guidance);
                        set_status("EXPLICIT RETRY - RETAINED START RESTORED");
                    }
                }
                return;
            }
            if (input.tab_pressed)
                mode = mode == Mode::live ? Mode::rig_lab : Mode::live;
            if (input.key_1_pressed) trainer.set_updates_per_cycle(1);
            if (input.key_2_pressed) trainer.set_updates_per_cycle(2);
            if (input.key_3_pressed) trainer.set_updates_per_cycle(4);
            if (input.totals_pressed)
            {
                LivePanelPage& active_page = mode == Mode::rig_lab
                    ? rig_trainer_panel_page : live_panel_page;
                if (mode == Mode::rig_lab)
                {
                    active_page = active_page == LivePanelPage::summary
                        ? LivePanelPage::mission
                        : active_page == LivePanelPage::mission
                            ? LivePanelPage::advanced
                            : LivePanelPage::summary;
                    rig_trainer_scroll_offset = 0.0f;
                }
                else
                {
                    switch (active_page)
                    {
                    case LivePanelPage::summary:
                        active_page = LivePanelPage::mission;
                        break;
                    case LivePanelPage::mission:
                        active_page = LivePanelPage::view;
                        break;
                    case LivePanelPage::view:
                        active_page = LivePanelPage::totals;
                        break;
                    case LivePanelPage::totals:
                        active_page = LivePanelPage::advanced;
                        break;
                    case LivePanelPage::advanced:
                        active_page = LivePanelPage::summary;
                        break;
                    }
                    trainer_scroll_offset = 0.0f;
                }
            }
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
            if (input.reset_pressed)
            {
                if (mode == Mode::live)
                    restart_live_replay(false);
                else
                {
                    trainer.reset_preview();
                    camera_x = 0.0f;
                    live_pixels_per_meter = view_camera::default_pixels_per_meter;
                    live_zoom_factor = 1.0f;
                    live_zoom_auto = true;
                    live_projection_initialized = false;
                }
                if (mode == Mode::rig_lab)
                {
                    art_editor_zoom = ui_layout::rig_lab_default_zoom;
                    rig_trainer_scroll_offset = 0.0f;
                    rig_panel_scroll_offset = 0.0f;
                    if (rig_test_physical_environment.has_value())
                        start_physical_rig_test(rig_test_physical_stage);
                }
            }
            if (input.delete_pressed && mode == Mode::rig_lab)
                delete_selected_node();
        }

        void frame(const InputState& input, float dt, int width, int height)
        {
            trainer.synchronize();
            canvas.clear();
            rig_frame_projection.reset();
            canvas.reserve(120000);
            canvas.quad({ 0.0f, 0.0f }, { static_cast<float>(width), static_cast<float>(height) },
                rgb(0x080a0d));
            status_time = std::max(0.0f, status_time - dt);
            session_runtime_seconds += std::max(0.0f, dt);
            const rl::TrainingMetrics& current_metrics = trainer.metrics();
            const rl::AutonomyStatus& current_autonomy = trainer.autonomy_status();
            const std::uint64_t current_signature = trainer.rig_signature();
            const ui_layout::TrainingTotals current_totals =
                training_totals_from(current_metrics, current_autonomy);
            const auto session_sample = std::find_if(
                session_rig_samples.begin(), session_rig_samples.end(),
                [current_signature](const SessionRigSample& sample)
                {
                    return sample.signature == current_signature;
                });
            if (session_sample == session_rig_samples.end())
            {
                session_rig_samples.push_back({ current_signature, current_totals });
            }
            else
            {
                ui_layout::accumulate_training_totals(session_totals,
                    ui_layout::training_totals_delta(
                        current_totals, session_sample->latest));
                session_sample->latest = current_totals;
            }
            if (tracked_rig_signature == 0u || tracked_rig_signature != current_signature)
            {
                tracked_rig_signature = current_signature;
                rig_lifetime_seconds = 0.0f;
                rig_start_total_updates = current_metrics.total_updates;
                rig_start_episodes = current_metrics.total_episodes;
                rig_start_valid_episodes = current_metrics.total_valid_episodes;
                rig_start_invalid_episodes = current_metrics.total_invalid_episodes;
                rig_start_steps = current_metrics.total_alternating_steps;
                rig_start_falls = current_metrics.total_falls;
                rig_start_collisions = current_metrics.total_collisions;
                rig_start_obstacles = current_metrics.total_obstacles_passed;
                rig_start_distance = current_metrics.total_distance;
                rig_best_stage = static_cast<std::uint8_t>(current_autonomy.stage);
            }
            else
            {
                rig_lifetime_seconds += std::max(0.0f, dt);
                rig_best_stage = std::max(rig_best_stage,
                    static_cast<std::uint8_t>(current_autonomy.stage));
            }
            if (joint_auto_sweep)
            {
                joint_test_phase += dt;
                joint_test_input = std::sin(joint_test_phase * 1.55f);
            }
            process_shortcuts(input);
            draw_top_bar(input, width);

            const ui_layout::Box layout_content = ui_layout::content_box(
                static_cast<float>(width), static_cast<float>(height));
            if (!ui_layout::supported_window(static_cast<float>(width), static_cast<float>(height)))
            {
                add_text(canvas, { 24.0f, 100.0f },
                    "WINDOW TOO SMALL - MINIMUM WINDOW 1280 X 799", 2.0f, danger);
                return;
            }

            if (mode == Mode::live)
            {
                const ui_layout::Box layout_world = ui_layout::live_world_box(layout_content);
                const ui_layout::Box layout_side = ui_layout::live_panel_box(layout_content);
                const Rect world{ { layout_world.x, layout_world.y },
                    { layout_world.width, layout_world.height } };
                const Rect side{ { layout_side.x, layout_side.y },
                    { layout_side.width, layout_side.height } };
                draw_live_world(world, dt, input);
                draw_live_panel(side, input);
            }
            else
            {
                ui_layout::RigLabWorkspaceLayout layout =
                    ui_layout::rig_lab_workspace_layout(
                        layout_content, rig_lab_workspace_page);
                if (layout.compact)
                {
                    const Rect workspace_bar{
                        { layout.workspace_bar.x, layout.workspace_bar.y },
                        { layout.workspace_bar.width,
                            layout.workspace_bar.height }
                    };
                    draw_rig_lab_workspace_bar(workspace_bar, input);
                    // The selector applies to this frame. Recompute after the
                    // click so the newly selected controls replace the old page
                    // immediately instead of visually lagging one frame.
                    layout = ui_layout::rig_lab_workspace_layout(
                        layout_content, rig_lab_workspace_page);
                }
                const ui_layout::Box& layout_live = layout.live;
                const ui_layout::Box& layout_trainer = layout.trainer;
                const ui_layout::Box& layout_side = layout.inspector;
                const ui_layout::Box& layout_world = layout.world;
                const Rect side{ { layout_side.x, layout_side.y },
                    { layout_side.width, layout_side.height } };
                const Rect trainer_panel{ { layout_trainer.x, layout_trainer.y },
                    { layout_trainer.width, layout_trainer.height } };
                const Rect world{ { layout_world.x, layout_world.y },
                    { layout_world.width, layout_world.height } };
                const bool show_dual_setup = !layout.compact
                    && layout_trainer.width > 0.0f;
                const ui_layout::Box dual_setup_box = show_dual_setup
                    ? ui_layout::rig_lab_dual_setup_button_layout(layout_trainer)
                    : ui_layout::Box{};
                const Rect dual_setup_button{
                    { dual_setup_box.x, dual_setup_box.y },
                    { dual_setup_box.width, dual_setup_box.height } };
                const bool dual_setup_clicked = show_dual_setup
                    && input.left_pressed
                    && contains(dual_setup_button, input.mouse);
                if (dual_setup_clicked)
                    rig_lab_controls_expanded = !rig_lab_controls_expanded;
                InputState panel_input = input;
                if (dual_setup_clicked)
                {
                    panel_input.left_pressed = false;
                    panel_input.left_down = false;
                }
                const ui_layout::RigPreviewToolbarLayout preview_toolbar =
                    ui_layout::rig_preview_toolbar_layout({
                        world.position.x, world.position.y,
                        world.size.x, world.size.y });
                const auto toolbar_rect = [](ui_layout::Box box) noexcept
                {
                    return Rect{ { box.x, box.y },
                        { box.width, box.height } };
                };
                const auto toolbar_clicked = [&](ui_layout::Box box) noexcept
                {
                    return input.left_pressed
                        && contains(toolbar_rect(box), input.mouse);
                };
                const bool zoom_out_clicked = toolbar_clicked(
                    preview_toolbar.zoom_out);
                const bool fit_clicked = toolbar_clicked(preview_toolbar.fit);
                const bool zoom_in_clicked = toolbar_clicked(
                    preview_toolbar.zoom_in);
                if (zoom_out_clicked)
                    art_editor_zoom = ui_layout::rig_lab_zoom_step(
                        art_editor_zoom, -1.0f);
                if (fit_clicked)
                    art_editor_zoom = ui_layout::rig_lab_default_zoom;
                if (zoom_in_clicked)
                    art_editor_zoom = ui_layout::rig_lab_zoom_step(
                        art_editor_zoom, 1.0f);
                InputState world_input = input;
                if (zoom_out_clicked || fit_clicked || zoom_in_clicked)
                {
                    world_input.left_pressed = false;
                    world_input.left_down = false;
                }
                // Advance the canonical physical Test Environment before
                // either pane renders it. On ultrawide layouts this keeps the
                // live pane, PIP, and editor on the exact same fixed-step pose.
                step_physical_rig_test(dt);
                if (!rig_test_physical_environment.has_value())
                    trainer.step_preview(dt);
                const bool show_rig_live =
                    layout_live.width > 0.0f;
                if (std::abs(input.wheel) >= 0.01f
                    && (contains(world, input.mouse)
                        || (show_rig_live && contains(Rect{
                                { layout_live.x, layout_live.y },
                                { layout_live.width, layout_live.height } },
                            input.mouse))))
                {
                    art_editor_zoom = ui_layout::rig_lab_zoom_step(
                        art_editor_zoom, input.wheel);
                }
                // Panel input can replace or discard the canonical preview
                // (preset/load/copy/PLAY) and can change its shared zoom. Apply
                // those mutations before caching a pointer into that preview;
                // otherwise PLAY can leave this frame with a dangling frozen-
                // environment pointer and ultrawide live/editor panes can draw
                // different species or scales from the same input event.
                if (rig_lab_controls_expanded)
                {
                    const Rect drawer = layout_trainer.width > 0.0f
                        ? trainer_panel : side;
                    draw_rig_lab_control_drawer(drawer, panel_input);
                    if (!layout.compact && layout_side.width > 0.0f)
                        draw_rig_panel(side, panel_input, false);
                }
                else
                {
                    if (layout_trainer.width > 0.0f)
                        draw_live_panel(trainer_panel, panel_input, !layout.compact);
                    if (layout_side.width > 0.0f)
                        draw_rig_panel(side, panel_input, layout.compact);
                }
                if (show_dual_setup)
                {
                    InputState display_input = input;
                    display_input.left_pressed = false;
                    static_cast<void>(button(dual_setup_button,
                        rig_lab_controls_expanded ? "CLOSE" : "SETUP",
                        display_input, rig_lab_controls_expanded));
                }

                // One exact production-envelope projection owns the complete
                // frame. Live, Art, Structure, Motors, and Test then reuse the
                // same physical snapshot/camera/zoom instead of independently
                // fitting different visible geometry.
                rig_frame_projection = fit_rig_preview_projection(world);
                const RigPreviewProjection shared_projection =
                    *rig_frame_projection;
                if (show_rig_live)
                {
                    const Rect live{ { layout_live.x, layout_live.y },
                        { layout_live.width, layout_live.height } };
                    draw_live_world(live, dt, input,
                        shared_projection.camera, shared_projection.scale);
                }
                add_rounded_rect(canvas, world, 11.0f, rgb(0x0a131d), border, 1.0f);
                canvas.push_clip(world.position + Vec2{ 1.0f, 1.0f },
                    world.position + world.size - Vec2{ 1.0f, 1.0f });
                if (rig_panel_page == RigPanelPage::art)
                    draw_art_editor_world(world);
                else
                    draw_blueprint(world, world_input);
                canvas.pop_clip();
                const std::string overlay_legend =
                    rig_panel_page == RigPanelPage::art
                        ? std::format("ART EDITOR / RIG ART {}",
                            rig_lab_art_enabled ? "ON" : "OFF")
                    : rig_panel_page == RigPanelPage::structure
                        ? "BONE EDITOR"
                    : rig_panel_page == RigPanelPage::motors
                        ? "MOTOR EDITOR"
                    : rig_panel_page == RigPanelPage::test
                        ? "TEST VIEW"
                        : "RIG PREVIEW";
                const ui_layout::Box title_box =
                    ui_layout::rig_preview_title_box(layout_world);
                add_text_fit(canvas, { title_box.x, title_box.y },
                    overlay_legend,
                    1.00f, muted, title_box.width,
                    ui_layout::persistent_text_minimum_scale);
                static_cast<void>(button(toolbar_rect(preview_toolbar.zoom_out),
                    "ZOOM OUT", input));
                static_cast<void>(button(toolbar_rect(preview_toolbar.fit),
                    "FIT", input,
                    std::abs(art_editor_zoom
                        - ui_layout::rig_lab_default_zoom) < 1.0e-5f));
                static_cast<void>(button(toolbar_rect(preview_toolbar.zoom_in),
                    "ZOOM IN", input));
                add_rounded_rect(canvas, world, 11.0f, ui_render::transparent_fill, border, 1.0f);
            }

            if (input.left_released && rig_edit_pending)
            {
                const std::string reason = rig_edit_reason;
                rig_edit_pending = false;
                rig_edit_reason.clear();
                apply_small_rig_change(reason);
            }
            if (input.left_released && art_edit_pending)
            {
                art_edit_pending = false;
                active_art_history().commit(active_art_layout());
                set_status("ART TRANSFORM COMMITTED TO SHARED LIVE RENDERER");
            }

            if (status_time > 0.0f)
            {
                const ui_layout::Box footer = ui_layout::status_footer_box(
                    static_cast<float>(width), static_cast<float>(height));
                const Rect toast{ { footer.x, footer.y },
                    { footer.width, footer.height } };
                add_rounded_rect(canvas, toast, 8.0f, rgb(0x10202b, 0.97f), accent, 1.0f);
                const Rect text_area{ toast.position + Vec2{ 14.0f, 7.0f },
                    toast.size - Vec2{ 28.0f, 14.0f } };
                canvas.push_clip(text_area.position,
                    text_area.position + text_area.size);
                add_wrapped_text(canvas, text_area.position, status,
                    ui_layout::persistent_text_minimum_scale,
                    white, text_area.size.x, 2.0f);
                canvas.pop_clip();
            }
        }
    };

    Application::Application()
        : impl_(new Impl{})
    {
    }

    Application::~Application()
    {
        delete impl_;
    }

    bool Application::initialize(const std::filesystem::path& asset_directory,
        std::string& error, presentation::StartupMode startup_mode,
        const std::filesystem::path& persistence_directory)
    {
        impl_->persistence_directory = persistence_directory;
        impl_->presentation_session = presentation::Session::interactive;
        impl_->art_layout_directory = asset_directory / "optional" / "layouts";
        for (std::size_t index = 0u; index < impl_->art_layouts.size(); ++index)
            impl_->art_layout_history[index].reset(impl_->art_layouts[index]);
        std::string artwork_error{};
        if (!art::load_p3_pixel_art(asset_directory / "chicken.ppm",
                impl_->original_runner_art, artwork_error))
        {
            impl_->original_runner_art = {};
            impl_->status = "ARTWORK WARNING - " + artwork_error;
            impl_->status_time = 9.0f;
        }

        auto load_optional = [&](std::string_view name, art::PixelArt& destination)
        {
            std::string optional_error{};
            const std::filesystem::path path = asset_directory / "optional"
                / "runner_armor_concepts" / "runtime" / std::string(name);
            if (!art::load_p3_pixel_art(path, destination, optional_error))
                destination = {};
        };
        load_optional("foot_side.ppm", impl_->optional_foot_art);
        load_optional("helmet_side.ppm", impl_->optional_helmet_art);
        load_optional("torso_side.ppm", impl_->optional_torso_art);
        load_optional("upper_arm_side.ppm", impl_->optional_upper_arm_art);
        load_optional("forearm_side.ppm", impl_->optional_forearm_art);
        load_optional("hand_side.ppm", impl_->optional_hand_art);
        load_optional("thigh_side.ppm", impl_->optional_thigh_art);
        load_optional("shin_side.ppm", impl_->optional_shin_art);
        load_optional("weapon_side.ppm", impl_->optional_weapon_art);
        auto load_species = [&](std::string_view species, Impl::SpeciesArtBundle& bundle)
        {
            auto load_part = [&](std::string_view part, art::PixelArt& destination)
            {
                std::string optional_error{};
                const std::filesystem::path path = asset_directory / "optional"
                    / "species_runtime" / std::format("{}_{}_side.ppm", species, part);
                if (!art::load_p3_pixel_art(path, destination, optional_error))
                    destination = {};
            };
            load_part("head", bundle.head);
            load_part("body", bundle.body);
            load_part("tail", bundle.tail);
            load_part("upper_leg", bundle.upper_leg);
            load_part("lower_leg", bundle.lower_leg);
            load_part("foot", bundle.foot);
        };
        load_species("chicken", impl_->chicken_art);
        load_species("dog", impl_->dog_art);
        load_species("hexapod", impl_->hexapod_art);
        impl_->live_art_enabled = impl_->optional_foot_art.loaded()
            || impl_->optional_helmet_art.loaded()
            || impl_->optional_torso_art.loaded()
            || impl_->optional_upper_arm_art.loaded()
            || impl_->optional_forearm_art.loaded()
            || impl_->optional_hand_art.loaded()
            || impl_->optional_thigh_art.loaded()
            || impl_->optional_shin_art.loaded()
            || impl_->optional_weapon_art.loaded()
            || impl_->chicken_art.loaded()
            || impl_->dog_art.loaded()
            || impl_->hexapod_art.loaded();
        // Live is the presented product view; Rig Lab opens on the unobscured
        // physical graph.  Their toggles remain independent across tab changes.
        impl_->rig_lab_art_enabled = false;
        constexpr std::array layout_species{
            sim::CreatureSpecies::human, sim::CreatureSpecies::chicken,
            sim::CreatureSpecies::dog, sim::CreatureSpecies::hexapod
        };
        constexpr std::array layout_stems{
            std::string_view{ "human" }, std::string_view{ "chicken" },
            std::string_view{ "dog" }, std::string_view{ "hexapod" }
        };
        for (std::size_t index = 0u; index < layout_species.size(); ++index)
        {
            art::Layout loaded{};
            std::string layout_error{};
            const std::filesystem::path path = impl_->art_layout_directory
                / std::format("{}.artlayout", layout_stems[index]);
            if (std::filesystem::exists(path)
                && art::load_layout(path, layout_species[index], loaded, layout_error))
                impl_->art_layouts[index] = loaded;
            impl_->art_layout_history[index].reset(impl_->art_layouts[index]);
        }

        impl_->rig_preset = Impl::preset_for_species(
            impl_->blueprint.presentation_species());
        std::string message{};
        if (startup_mode == presentation::StartupMode::interactive)
        {
            if (!persistence_directory.empty())
            {
                std::error_code filesystem_error{};
                std::filesystem::create_directories(
                    persistence_directory, filesystem_error);
                if (filesystem_error)
                {
                    error = "Could not create Epoch2DWalkEngine persistence directory: "
                        + filesystem_error.message();
                    return false;
                }
            }
            impl_->select_autosave_rig_file(impl_->blueprint);
            const bool resume_queued = impl_->trainer.load_autosave(message, true);
            impl_->trainer.synchronize();
            impl_->blueprint = impl_->trainer.blueprint();
            if (!resume_queued)
            {
                message = std::format("{} - AUTOPILOT RUNNING (ECO)",
                    impl_->startup_rig.source_note);
                impl_->trainer.set_background_enabled(true);
            }
            impl_->rig_preset = Impl::preset_for_species(
                impl_->blueprint.presentation_species());
            impl_->select_autosave_rig_file(impl_->blueprint);
        }
        else
        {
            impl_->trainer.set_background_enabled(false);
            impl_->trainer.synchronize();
            message = "ISOLATED VISUAL PROOF - NO AUTOSAVE OR TRAINING STATE";
        }
        if (impl_->original_runner_art.loaded())
        {
            impl_->status = message;
            impl_->status_time = 6.0f;
        }
        error.clear();
        return true;
    }

    void Application::prepare_course_eye_test()
    {
        impl_->trainer.set_background_enabled(false);
        impl_->trainer.synchronize();
        impl_->presentation_session = presentation::Session::course_eye;
        impl_->course_eye_test_environment.emplace(
            sim::CreatureBlueprint::humanoid(), 728314u);
        impl_->course_eye_test_environment->set_course(
            sim::CourseStage::uneven, 0.65f);
        impl_->course_eye_test_environment->set_course_motion_enabled(false);
        impl_->run_paused = true;
        const auto& particles = impl_->course_eye_test_environment->particles();
        const std::size_t root =
            impl_->course_eye_test_environment->blueprint().root_node;
        const float subject_x = root < particles.size()
            ? particles[root].position.x : 0.0f;
        impl_->camera_x = view_camera::course_eye_camera_center(
            subject_x,
            impl_->course_eye_test_environment->facing_direction(),
            course_eye_focus_span_meters);
        impl_->live_pixels_per_meter = 20.0f;
        impl_->live_zoom_factor = 1.0f;
        impl_->live_zoom_auto = true;
        impl_->live_projection_initialized = false;
        impl_->status = "PACKAGED COURSE EYE TEST - PRODUCTION TERRAIN, FIXED START FRAME";
        impl_->status_time = 30.0f;
    }

    bool Application::prepare_art_diagnostic_rig(std::size_t index)
    {
        sim::CreatureBlueprint rig{};
        std::string_view name{};
        switch (index)
        {
        case 0u: rig = sim::CreatureBlueprint::humanoid(); name = "HUMAN"; break;
        case 1u: rig = sim::CreatureBlueprint::chicken(); name = "CHICKEN"; break;
        case 2u: rig = sim::CreatureBlueprint::crawler4(); name = "DOG"; break;
        case 3u: rig = sim::CreatureBlueprint::hexapod(); name = "HEXAPOD"; break;
        default: return false;
        }

        impl_->trainer.set_background_enabled(false);
        impl_->trainer.synchronize();
        impl_->presentation_session = presentation::Session::art_eye;
        impl_->course_eye_test_environment.emplace(rig, 729349u + index * 4099u);
        impl_->course_eye_test_environment->set_course(
            sim::CourseStage::balance, 0.10f);
        if (index == 0u)
        {
            impl_->course_eye_test_environment->configure_equipment(
                sim::WeaponClass::carbine, 8.0f);
            impl_->course_eye_test_environment->set_equipment_directive(
                sim::EquipmentDirective::safe_carry_walk);
            const std::array<float, sim::action_count> neutral{};
            for (std::size_t frame = 0; frame < 72u; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    *impl_->course_eye_test_environment, neutral,
                    sim::CourseStage::balance, 1.0f,
                    sim::GuidanceMode::assisted);
                static_cast<void>(impl_->course_eye_test_environment->step(action));
            }
        }
        impl_->course_eye_test_environment->set_course_motion_enabled(false);
        impl_->run_paused = true;
        const auto& particles = impl_->course_eye_test_environment->particles();
        const std::size_t root = impl_->course_eye_test_environment->blueprint().root_node;
        impl_->camera_x = root < particles.size()
            ? particles[root].position.x + 0.25f : 0.25f;
        // draw_live_world snaps this isolated inspection surface to the exact
        // particle width/height fit on its first frame. No species is resized
        // in simulation; only the side-view inspection camera changes.
        impl_->live_pixels_per_meter = view_camera::default_pixels_per_meter;
        impl_->live_zoom_factor = 1.0f;
        impl_->live_zoom_auto = true;
        impl_->live_projection_initialized = false;
        impl_->debug_skeleton_overlay = false;
        impl_->status = std::format(
            "PACKAGED ORTHOGRAPHIC ART TEST - {} SIDE ELEVATION", name);
        impl_->status_time = 30.0f;
        return true;
    }

    void Application::prepare_art_eye_test()
    {
        static_cast<void>(prepare_art_diagnostic_rig(0u));
        impl_->status = "PACKAGED ORTHOGRAPHIC ART TEST - EXACT SIDE ELEVATION";
    }

    void Application::prepare_art_fallen_eye_test()
    {
        static_cast<void>(prepare_art_diagnostic_rig(0u));
        impl_->course_eye_test_environment->set_diagnostic_rigid_rotation(
            -pi * 0.5f);
        impl_->status =
            "PACKAGED ORTHOGRAPHIC ART TEST - HORIZONTAL ROTATION EVIDENCE";
    }

    bool Application::prepare_equipment_eye_test(std::string& error)
    {
        impl_->trainer.set_background_enabled(false);
        impl_->trainer.synchronize();
        impl_->presentation_session = presentation::Session::equipment_eye;
        impl_->course_eye_test_environment.emplace(
            sim::CreatureBlueprint::humanoid(), 0x751100u);
        sim::Environment& environment = *impl_->course_eye_test_environment;
        environment.set_course(sim::CourseStage::equipment_targets, 0.30f);
        environment.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        environment.set_equipment_directive(
            sim::EquipmentDirective::fire_and_correct);
        environment.set_guidance_mode(sim::GuidanceMode::assisted);
        environment.set_course_motion_enabled(false);

        constexpr float fixed_step = 1.0f / 60.0f;
        constexpr std::size_t maximum_frames = 900u;
        const std::array<float, sim::action_count> no_policy{};
        bool captured{};
        for (std::size_t frame = 0u; frame < maximum_frames; ++frame)
        {
            const sim::WeaponProfile profile =
                sim::weapon_profile(environment.weapon_class());
            captured = presentation::equipment_eye_frame_ready(
                environment.equipment_state() == sim::EquipmentState::ready
                    && environment.equipment_engagement_ready(),
                environment.valid_motion(), environment.course_motion_enabled(),
                environment.equipment_target_visible(),
                environment.equipment_plant_seconds(),
                environment.equipment_aim_settle_seconds(),
                environment.equipment_aim_error(), profile.aim_tolerance,
                environment.equipment_aim_rate(), environment.shots_fired());
            if (captured)
                break;

            const auto action = rl::effective_policy_action(
                environment, no_policy, sim::CourseStage::equipment_targets,
                1.0f, sim::GuidanceMode::assisted);
            const sim::StepResult result = environment.step(action, fixed_step);
            if (result.terminated || !environment.valid_motion())
                break;
        }

        if (!captured)
        {
            error = std::format(
                "deterministic physical equipment frame never reached a no-shot "
                "planted READY state: state={} valid={} stopped={} plant={:.3f}s "
                "settle={:.3f}s aim_error={:.4f} aim_rate={:.4f} "
                "target_visible={} shots={} reason={}",
                sim::equipment_state_name(environment.equipment_state()),
                environment.valid_motion(), environment.equipment_stopped(),
                environment.equipment_plant_seconds(),
                environment.equipment_aim_settle_seconds(),
                environment.equipment_aim_error(),
                environment.equipment_aim_rate(),
                environment.equipment_target_visible(),
                environment.shots_fired(),
                sim::invalid_motion_name(environment.invalid_reason()));
            impl_->course_eye_test_environment.reset();
            impl_->presentation_session = presentation::Session::interactive;
            return false;
        }

        impl_->run_paused = true;
        const auto& particles = environment.particles();
        const std::size_t root = environment.blueprint().root_node;
        impl_->camera_x = root < particles.size()
            ? particles[root].position.x + 2.5f : 2.5f;
        impl_->live_pixels_per_meter = 112.0f;
        impl_->live_zoom_factor = 1.45f;
        impl_->live_zoom_auto = false;
        impl_->live_projection_initialized = true;
        impl_->debug_skeleton_overlay = false;
        impl_->status = std::format(
            "PACKAGED PHYSICAL EQUIPMENT EYE TEST - ASSISTED TEACHER / "
            "{} / PLANT {:.2f} S / SETTLE {:.2f} S / NO SHOT",
            sim::equipment_state_name(environment.equipment_state()),
            environment.equipment_plant_seconds(),
            environment.equipment_aim_settle_seconds());
        impl_->status_time = 30.0f;
        error.clear();
        return true;
    }

    bool Application::prepare_walk_eye_test(std::string& error)
    {
        impl_->trainer.set_background_enabled(false);
        impl_->trainer.synchronize();
        impl_->presentation_session = presentation::Session::interactive;
        impl_->course_eye_test_environment.reset();
        impl_->walk_eye_test_proof = diagnostics::run_walk_eye_test_proof();
        const diagnostics::WalkEyeTestProof& proof = impl_->walk_eye_test_proof;
        if (!proof.passed)
        {
            error = std::format(
                "walk eye test failed: updates={} retained={} authority={:.3f} "
                "mean={:.3f}m/{:.2f} steps invalid={}/6 displayed={:.3f}m/{} steps; "
                "latest eval valid={} distance={:.3f}m steps={:.2f} survival={:.2f}s "
                "quality={:016X} rejection={} invalid={}/6 reason={}",
                proof.updates, proof.retained_update, proof.teacher_authority,
                proof.retained_distance, proof.retained_stride_events,
                proof.retained_invalid_runs, proof.displayed_distance,
                proof.displayed_steps, proof.evaluation_valid,
                proof.evaluation_distance, proof.evaluation_stride_events,
                proof.evaluation_survival, proof.evaluation_quality_key,
                proof.evaluation_rejection_mask, proof.evaluation_invalid_runs,
                sim::invalid_motion_name(proof.evaluation_invalid_reason));
            return false;
        }

        impl_->walk_eye_test_policy.parameters() = proof.retained_policy_parameters;
        impl_->walk_eye_replay_start.emplace(proof.environment);
        impl_->course_eye_test_environment.emplace(*impl_->walk_eye_replay_start);
        impl_->presentation_session = presentation::Session::walk_eye;
        impl_->walk_eye_terminal = {};
        impl_->walk_eye_trial_id = 1u;
        impl_->walk_eye_accumulator_seconds = 0.0;
        impl_->run_paused = false;
        const auto& particles = impl_->course_eye_test_environment->particles();
        const std::size_t root = impl_->course_eye_test_environment->blueprint().root_node;
        impl_->camera_x = root < particles.size()
            ? particles[root].position.x + 2.5f : proof.displayed_distance;
        impl_->live_pixels_per_meter = 105.0f;
        impl_->live_zoom_factor = 1.0f;
        impl_->live_zoom_auto = true;
        impl_->live_projection_initialized = true;
        impl_->debug_skeleton_overlay = false;
        impl_->status = std::format(
            "PACKAGED RETAINED WALK PROOF - FRESH TRAINING, {}",
            retained_guidance_label(
                impl_->course_eye_test_environment->guidance_mode()));
        impl_->status_time = 30.0f;
        error.clear();
        return true;
    }

    bool Application::prepare_rig_lab_page(
        RigLabStartupPage page, std::size_t species_index)
    {
        Impl::RigPreset preset{};
        switch (species_index)
        {
        case 0u: preset = Impl::RigPreset::humanoid; break;
        case 1u: preset = Impl::RigPreset::chicken; break;
        case 2u: preset = Impl::RigPreset::crawler4; break;
        case 3u: preset = Impl::RigPreset::hexapod; break;
        default: return false;
        }
        switch (page)
        {
        case RigLabStartupPage::presets:
            impl_->rig_panel_page = Impl::RigPanelPage::presets;
            break;
        case RigLabStartupPage::structure:
            impl_->rig_panel_page = Impl::RigPanelPage::structure;
            break;
        case RigLabStartupPage::motors:
            impl_->rig_panel_page = Impl::RigPanelPage::motors;
            break;
        case RigLabStartupPage::art:
            impl_->rig_panel_page = Impl::RigPanelPage::art;
            break;
        case RigLabStartupPage::test:
            impl_->rig_panel_page = Impl::RigPanelPage::test;
            break;
        }
        impl_->presentation_session = presentation::Session::interactive;
        impl_->mode = Impl::Mode::rig_lab;
        impl_->rig_lab_workspace_page =
            ui_layout::RigLabWorkspacePage::inspector;
        impl_->rig_lab_controls_expanded = false;
        impl_->rig_trainer_panel_page = Impl::LivePanelPage::summary;
        impl_->rig_raw_telemetry_expanded = false;
        impl_->rig_trainer_scroll_offset = 0.0f;
        impl_->rig_panel_scroll_offset = 0.0f;
        impl_->use_preset(preset);
        // use_preset() installs the canonical rig in the live physical preview
        // immediately and queues the same worker commit as an interactive
        // selection. Consume any already-published snapshot before the first
        // frame; the morphology-preview hold prevents an older Human publish
        // from replacing this requested species while its command is pending.
        impl_->trainer.synchronize();
        impl_->art_preview_frozen = false;
        impl_->frozen_art_environment.reset();
        if (page == RigLabStartupPage::test)
        {
            // A direct Test-page launch is deterministic visual evidence, not
            // a continuation of whichever equipment happened to be active in
            // persisted training. Start with unobstructed arms and a real
            // physical gait scrub; equipment buttons can then replace it with
            // the canonical planted aiming environment.
            impl_->editor_weapon_class = sim::WeaponClass::none;
            impl_->trainer.configure_preview_equipment(
                sim::WeaponClass::none, impl_->editor_target_distance);
            impl_->trainer.synchronize();
            impl_->rig_test_pattern = sim::RigTestPattern::gait;
            impl_->start_physical_rig_test(sim::CourseStage::uneven);
            impl_->status = std::format(
                "RIG LAB DIRECT VIEW - {} / TEST / PHYSICAL GAIT MOTOR SCRUB",
                impl_->preset_name());
        }
        else
        {
            impl_->rig_test_physical_environment.reset();
            impl_->rig_test_physical_accumulator_seconds = 0.0;
            impl_->status = std::format(
                "RIG LAB DIRECT VIEW - {} / SHARED LIVE SNAPSHOT",
                impl_->preset_name());
        }
        impl_->status_time = 12.0f;
        return true;
    }

    void Application::frame(const InputState& input, float dt, int width, int height)
    {
        const ui_layout::PresentationSurface presentation =
            ui_layout::presentation_surface(width, height);
        InputState mapped_input = input;
        if (presentation.scale_x > 0.0f)
        {
            mapped_input.mouse.x /= presentation.scale_x;
            mapped_input.mouse_delta.x /= presentation.scale_x;
        }
        if (presentation.scale_y > 0.0f)
        {
            mapped_input.mouse.y /= presentation.scale_y;
            mapped_input.mouse_delta.y /= presentation.scale_y;
        }
        impl_->frame(mapped_input, dt,
            presentation.canvas_width, presentation.canvas_height);
        impl_->presentation_scaled = presentation.density_scaled;
        impl_->presented_vertices.clear();
        if (presentation.density_scaled)
        {
            const std::span<const render::Vertex> source =
                impl_->canvas.vertices();
            impl_->presented_vertices.reserve(source.size());
            for (const render::Vertex& vertex : source)
            {
                render::Vertex presented = vertex;
                presented.position.x *= presentation.scale_x;
                presented.position.y *= presentation.scale_y;
                impl_->presented_vertices.push_back(presented);
            }
        }
    }

    std::span<const render::Vertex> Application::vertices() const noexcept
    {
        if (impl_->presentation_scaled)
            return impl_->presented_vertices;
        return impl_->canvas.vertices();
    }

    std::uint64_t Application::rig_preview_signature() const noexcept
    {
        return impl_->rig_preview_environment().blueprint().signature();
    }

    RigPreviewContactEvidence
        Application::rig_preview_contact_evidence() const noexcept
    {
        const sim::Environment& environment = impl_->rig_preview_environment();
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const auto& particles = environment.particles();
        const auto grounded = [&](std::size_t index) noexcept
        {
            return index < particles.size() && particles[index].grounded;
        };
        const auto foot = [&](bool left) noexcept
        {
            const auto& additional = left
                ? rig.additional_left_contact_nodes
                : rig.additional_right_contact_nodes;
            return std::array<bool, 3>{
                grounded(left ? rig.left_contact_node
                    : rig.right_contact_node),
                !additional.empty() && grounded(additional[0]),
                additional.size() >= 2u && grounded(additional[1])
            };
        };
        const std::array left = foot(true);
        const std::array right = foot(false);
        std::uint16_t left_contacts{};
        std::uint16_t right_contacts{};
        for (std::size_t index = 0u; index < particles.size(); ++index)
        {
            if (!particles[index].grounded)
                continue;
            if (rig.is_left_support_seed(index))
                ++left_contacts;
            if (rig.is_right_support_seed(index))
                ++right_contacts;
        }
        return {
            left[0], left[1], left[2],
            right[0], right[1], right[2],
            environment.valid_motion(),
            impl_->rig_test_physical_generation,
            left_contacts, right_contacts
        };
    }

    RigPreviewTerminalEvidence
        Application::rig_preview_terminal_evidence() const noexcept
    {
        RigPreviewTerminalEvidence evidence = impl_->rig_test_terminal;
        evidence.physical_test_generation = impl_->rig_test_physical_generation;
        if (impl_->rig_test_physical_environment.has_value())
            evidence.pose_signature = Impl::physical_pose_signature(
                *impl_->rig_test_physical_environment);
        return evidence;
    }

    LiveCameraEvidence Application::live_camera_evidence() const noexcept
    {
        return { impl_->camera_x, impl_->live_pixels_per_meter,
            impl_->live_zoom_factor, impl_->live_zoom_auto,
            impl_->live_projection_initialized };
    }

    bool Application::prepare_forced_rig_test_terminal() noexcept
    {
        if (!impl_->rig_test_physical_environment.has_value()
            || impl_->rig_test_terminal.frozen)
            return false;
        impl_->rig_test_physical_environment->set_diagnostic_rigid_rotation(pi);
        impl_->rig_test_physical_accumulator_seconds = 0.0;
        return true;
    }

    bool Application::wants_quit() const noexcept
    {
        return impl_->quit;
    }
}
