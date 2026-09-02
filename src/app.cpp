#include "app.hpp"
#include "art_authoring.hpp"
#include "autonomy.hpp"
#include "pixel_art.hpp"
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
            return {
                .logical_height = font::default_logical_height
                    * (style_scale > 0.0f ? style_scale : 1.0f),
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
            bool mirror_horizontal = false, bool fit_opaque_horizontal = false)
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
            const auto presentation_color = [](Color color) noexcept
            {
                const auto quantize = [](float channel) noexcept
                {
                    return std::round(std::clamp(channel, 0.0f, 1.0f) * 7.0f)
                        / 7.0f;
                };
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
            float maximum_width, float minimum_scale = ui_layout::minimum_readable_text_scale) noexcept
        {
            float scale = requested_scale;
            while (scale > minimum_scale
                && font::measure_text(text, font_size(scale)).x > maximum_width)
                scale -= 0.05f;
            return std::max(scale, minimum_scale);
        }

        void add_text_fit(render::Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float maximum_width, float minimum_scale = ui_layout::minimum_readable_text_scale)
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
        enum class LivePanelPage : std::uint8_t { summary, totals, advanced };
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
        sim::CreatureBlueprint blueprint{ startup_rig.blueprint };
        rl::AutonomousTrainer trainer{ blueprint, 64 };
        std::optional<sim::Environment> course_eye_test_environment{};
        bool art_eye_test{};
        bool walk_eye_test{};
        diagnostics::WalkEyeTestProof walk_eye_test_proof{};
        rl::PolicyNetwork walk_eye_test_policy{ 0x7300u };
        std::optional<sim::Environment> walk_eye_replay_start{};
        double walk_eye_accumulator_seconds{};
        Mode mode{ Mode::live };
        RigPreset rig_preset{ RigPreset::humanoid };
        JointTestGroup joint_test_group{ JointTestGroup::selected };
        RigPanelPage rig_panel_page{ RigPanelPage::presets };
        LivePanelPage live_panel_page{ LivePanelPage::summary };
        int selected_node{ -1 };
        int selected_bone{ -1 };
        int selected_motor{};
        std::uint16_t director_browser_index{};
        bool dragging_node{};
        bool joint_auto_sweep{};
        bool right_leg_near{ true };
        bool rig_test_loose_ground{};
        sim::RigTestPattern rig_test_pattern{ sim::RigTestPattern::manual };
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
        float art_editor_zoom{ 1.0f };
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
        bool optional_art_enabled{ false };
        bool debug_skeleton_overlay{};
        std::string status{ "AUTOPILOT STARTING" };
        float status_time{ 4.0f };
        bool quit{};
        std::filesystem::path custom_rig_path{ "custom.rig" };
        std::filesystem::path autosave_policy_path{
            sim::creature_species_paths(sim::CreatureSpecies::human).autosave_checkpoint };
        std::filesystem::path autosave_rig_path{
            sim::creature_species_paths(sim::CreatureSpecies::human).evolved_rig };
        std::filesystem::path autosave_state_path{
            sim::creature_species_paths(sim::CreatureSpecies::human).autonomy_state };

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
            autosave_policy_path = paths.autosave_checkpoint;
            autosave_rig_path = paths.evolved_rig;
            autosave_state_path = paths.autonomy_state;
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
            if (distance_units == ui_layout::DistanceUnits::metric)
                return meters >= 1000.0f ? std::format("{:.2f} KM", meters / 1000.0f)
                    : std::format("{:.1f} M", meters);
            const float feet = meters * 3.2808399f;
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
            blueprint = std::move(candidate);
            return trainer.preview_blueprint(blueprint);
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
            switch (rig_preset)
            {
            case RigPreset::quadruped:
                return { "REAR PHASE A HIP", "REAR PHASE A KNEE",
                    "REAR PHASE B HIP", "REAR PHASE B KNEE",
                    "FRONT PHASE B HIP", "FRONT PHASE B KNEE",
                    "FRONT PHASE A HIP", "FRONT PHASE A KNEE" };
            case RigPreset::crawler4:
                return { "REAR PHASE A HIP", "REAR PHASE A KNEE",
                    "REAR PHASE B HIP", "REAR PHASE B KNEE",
                    "FRONT PHASE B HIP", "FRONT PHASE B KNEE",
                    "FRONT PHASE A HIP", "FRONT PHASE A KNEE" };
            case RigPreset::hexapod:
                return { "REAR LEFT HIP", "REAR LEFT KNEE",
                    "REAR RIGHT HIP", "REAR RIGHT KNEE",
                    "FRONT LEFT HIP", "FRONT LEFT KNEE",
                    "FRONT RIGHT HIP", "FRONT RIGHT KNEE" };
            case RigPreset::monoped:
                return { "HIP", "KNEE", "LEFT FOOT", "RIGHT FOOT",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::humanoid:
                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE",
                    "LEFT SHOULDER", "LEFT ELBOW", "RIGHT SHOULDER", "RIGHT ELBOW" };
            case RigPreset::scaffold:
            case RigPreset::biped:
            case RigPreset::chicken:
                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::custom:
                return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4",
                    "MOTOR 5", "MOTOR 6", "MOTOR 7", "MOTOR 8" };
            }
            return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4",
                "MOTOR 5", "MOTOR 6", "MOTOR 7", "MOTOR 8" };
        }

        void set_status(std::string text)
        {
            status = std::move(text);
            status_time = 4.0f;
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

            float scale = 1.28f;
            Vec2 measured = font::measure_text(label, font_size(scale));
            while (measured.x > rect.size.x - 14.0f
                && scale > ui_layout::minimum_readable_text_scale)
            {
                scale -= 0.06f;
                measured = font::measure_text(label, font_size(scale));
            }
            add_text(canvas,
                { rect.position.x + (rect.size.x - measured.x) * 0.5f,
                  rect.position.y + (rect.size.y - measured.y) * 0.5f },
                label, scale, enabled ? white : muted);
            return enabled && hovered && input.left_pressed;
        }

        float slider(Rect rect, std::string_view label, float value, float minimum, float maximum,
            const InputState& input, std::string_view suffix = {})
        {
            add_text(canvas, rect.position, label, 1.35f, muted);
            Rect track{ { rect.position.x, rect.position.y + 23.0f }, { rect.size.x, 10.0f } };
            add_rounded_rect(canvas, track, 5.0f, rgb(0x101820), border, 1.0f);
            float fraction = (value - minimum) / std::max(0.0001f, maximum - minimum);
            fraction = clamp(fraction, 0.0f, 1.0f);
            add_rounded_rect(canvas, { track.position, { track.size.x * fraction, track.size.y } }, 5.0f, accent);
            canvas.circle({ track.position.x + track.size.x * fraction, track.position.y + 5.0f }, 8.0f, white, 18);
            if (input.left_down && contains({ track.position - Vec2{ 8.0f, 10.0f },
                track.size + Vec2{ 16.0f, 20.0f } }, input.mouse))
            {
                fraction = clamp((input.mouse.x - track.position.x) / track.size.x, 0.0f, 1.0f);
                value = lerp(minimum, maximum, fraction);
            }
            add_text(canvas, { rect.position.x + rect.size.x - 100.0f, rect.position.y },
                std::format("{:.2f}{}", value, suffix), 1.25f, white);
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

            rig_preset = preset;
            blueprint = std::move(candidate);
            selected_node = -1;
            selected_bone = -1;
            selected_motor = 0;
            dragging_node = false;
            select_autosave_rig_file(blueprint);
            trainer.set_blueprint(blueprint, false);
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
            rig_preset = preset_for_species(species);
            select_autosave_rig_file(blueprint);
            static_cast<void>(trainer.preview_blueprint(blueprint));
            trainer.set_blueprint(blueprint, true);
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
            case JointTestGroup::pair_b: return index >= 4;
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
            add_text(canvas, { 18.0f, 12.0f }, "RUNNER v" RUNNER_VERSION, 1.68f, white);
            add_text(canvas, { 19.0f, 46.0f },
                "AUTONOMOUS PHYSICS LOCOMOTION LAB", 0.86f, muted);

            const float tab_width = width >= 1500 ? 164.0f : 148.0f;
            const float start_x = static_cast<float>(width) - tab_width * 2.0f - 16.0f;
            if (start_x > 620.0f)
            {
                add_text_fit(canvas, { 330.0f, 20.0f },
                    "TAB VIEW  SPACE TRAIN  1/2/3 SPEED  T DATA PAGE  U UNITS  A ART  R RESET",
                    0.82f, muted, start_x - 352.0f, 0.78f);
            }
            if (button({ { start_x, 17.0f }, { tab_width - 6.0f, 42.0f } },
                "LIVE AUTOPILOT", input, mode == Mode::live))
                mode = Mode::live;
            if (button({ { start_x + tab_width, 17.0f }, { tab_width - 6.0f, 42.0f } },
                "RIG LAB", input, mode == Mode::rig_lab))
                mode = Mode::rig_lab;
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
                canvas.triangle(surface_a, surface_b, bottom_b, rgb(0x4d392c));
                canvas.triangle(surface_a, bottom_b, bottom_a, rgb(0x4d392c));

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
                    const float lift = 38.0f
                        + static_cast<float>(challenge.sequence & 1u) * 24.0f;
                    const float label_x = clamp(anchor.x - label_width * 0.5f,
                        viewport.position.x + 6.0f,
                        viewport.position.x + viewport.size.x - label_width - 6.0f);
                    const float label_y = clamp(anchor.y - lift - label_height,
                        viewport.position.y + 8.0f,
                        viewport.position.y + viewport.size.y - label_height - 8.0f);
                    const Rect sign{ { label_x, label_y },
                        { label_width, label_height } };
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
            float camera, float scale)
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
                    const Rect sign{ top + Vec2{ -58.0f, -26.0f },
                        { 116.0f, 26.0f } };
                    add_rounded_rect(canvas, sign, 5.0f,
                        rgb(0x102431, 0.97f), accent, 1.0f);
                    add_text_fit(canvas, sign.position + Vec2{ 6.0f, 5.0f },
                        index == 0u ? "TURN A" : "TURN B",
                        0.82f, white, sign.size.x - 12.0f, 0.72f);
                }
                return;
            }
            const float marker_spacing = ui_layout::course_reference_marker_spacing_m(distance_units);
            const int first_marker = static_cast<int>(std::floor(left / marker_spacing));
            const int last_marker = static_cast<int>(std::ceil(right / marker_spacing));
            for (int index = first_marker; index <= last_marker; ++index)
            {
                if (index < 0)
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
            bool fit_opaque_horizontal = false)
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
                adjusted.flip_horizontal, fit_opaque_horizontal);
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
            const Vec2 direction{ std::cos(environment.equipment_aim_angle()),
                std::sin(environment.equipment_aim_angle()) };
            const float alpha = loose ? 0.86f
                : environment.equipment_state() == sim::EquipmentState::ready
                    ? 0.98f : 0.90f;
            if (optional_art_enabled && optional_weapon_art.loaded())
            {
                const bool carried = environment.equipment_state()
                    == sim::EquipmentState::safe_carry
                    || environment.equipment_state() == sim::EquipmentState::low_ready;
                // Safe carry remains on the real hand/forearm axis, but the
                // stock extends back along that link instead of hiding the
                // complete weapon below the hand and inside the support leg.
                const float rear = carried ? 0.46f : 0.12f;
                const float front = carried ? 0.34f : 0.68f;
                const Vec2 beginning = world_to_screen(
                    mount - direction * rear, viewport, camera, scale);
                const Vec2 ending = world_to_screen(
                    mount + direction * front, viewport, camera, scale);
                const float art_pixel_scale = art::presentation_pixel_scale(scale);
                const float thickness = std::clamp(scale * 0.24f,
                    art::scaled_pixels(16.0f, art_pixel_scale),
                    art::scaled_pixels(28.0f, art_pixel_scale));
                draw_authored_pixel_art(
                    environment.blueprint().presentation_species(),
                    art::Module::equipment, optional_weapon_art,
                    beginning, ending, thickness, alpha,
                    environment.facing_direction() < 0.0f, false, true);
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
            float camera, float scale)
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

                if (feature_screen.x < viewport.position.x - 20.0f
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
            float camera, float scale, bool show_nodes)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            const SpeciesArtBundle* bundle = species_art_for(rig.presentation_species());
            const bool use_art = optional_art_enabled && bundle != nullptr && bundle->loaded();
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

            if (!use_art || show_nodes)
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
                    const art::PixelArt* sprite = nullptr;
                    if (use_art)
                        sprite = has_distal_motor
                            ? &bundle->upper_leg : &bundle->lower_leg;
                    Vec2 beginning = point(motor.pivot);
                    Vec2 ending = point(motor.c);
                    const Vec2 direction = normalized(ending - beginning, { 1.0f, 0.0f });
                    beginning = beginning - direction * scale * profile.joint_overlap;
                    ending = ending + direction * scale * profile.joint_overlap;
                    const float span = length(ending - beginning);
                    const float thickness = std::clamp(span * profile.limb_ratio,
                        scale * profile.limb_minimum, scale * profile.limb_maximum);
                    if (sprite != nullptr && sprite->loaded())
                        draw_authored_pixel_art(species,
                            has_distal_motor ? art::Module::upper_limb
                                : art::Module::lower_limb,
                            *sprite, beginning, ending, thickness,
                            near ? 0.98f : 0.46f,
                            transverse_mirror, false, true);
                    else
                        canvas.line(beginning, ending, thickness,
                            near ? rgb(0xcdd6d9) : rgb(0x73828b));

                    const bool terminal = !std::ranges::any_of(motors,
                        [&](const sim::MotorConstraint& candidate)
                        {
                            return candidate.enabled && candidate.pivot == motor.c
                                && rig.support_branch_mask(candidate) != 0u;
                        });
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
                            near ? 0.98f : 0.46f, transverse_mirror, false, true);
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
            if (show_nodes)
                for (const sim::Particle& particle : particles)
                    canvas.circle(world_to_screen(particle.position, viewport, camera, scale),
                        std::max(2.0f, particle.radius * scale * 0.22f), accent, 12);
        }
        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            const float art_pixel_scale = art::presentation_pixel_scale(scale);
            if (particles.empty())
                return;
            if (rig.presentation_species() != sim::CreatureSpecies::human)
            {
                draw_species_creature(environment, viewport, camera, scale, show_nodes);
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

                    if (!optional_art_enabled)
                    {
                        const Color color = side == 0 ? body
                            : near ? leg : rgb(0x5f493b);
                        const float radius = std::max(0.035f,
                            std::min(radius_a, radius_b) * 0.34f) * scale;
                        canvas.capsule(point(bone.a), point(bone.b), radius, color, 16);
                    }
                    else
                    {
                        // Authoritative graph bones stay visible beneath authored
                        // armor; generated body capsules and joint caps stay absent.
                        const Color bone_color = debug_skeleton_overlay
                            ? rgb(0x55d7e9, near || side == 0 ? 0.92f : 0.32f)
                            : rgb(0xb8c7cf, near || side == 0 ? 0.72f : 0.16f);
                        canvas.line(point(bone.a), point(bone.b),
                            debug_skeleton_overlay ? 2.0f : 1.35f, bone_color);
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
                            const Vec2 proximal = point(support_parent(index));
                            const bool left = index == rig.left_contact_node;
                            const auto& extra = left
                                ? rig.additional_left_contact_nodes
                                : rig.additional_right_contact_nodes;
                            const Vec2 toe = extra.size() >= 2u
                                && extra[1] < particles.size()
                                ? point(extra[1]) : center;
                            if (optional_art_enabled)
                            {
                                if (optional_foot_art.loaded())
                                {
                                    const float support_span = length(center - proximal);
                                    const float plate_span = length(toe - center);
                                    // Heel/ball/toe geometry owns boot size. Screen
                                    // zoom and the torso assembly scale must never
                                    // inflate the foot beyond its physical chain.
                                    const art::BootArtDimensions boot =
                                        art::articulated_boot_dimensions(
                                            plate_span, support_span, radius);
                                    const art::OrientedArtTransform transform =
                                        art::articulated_boot_transform(center, toe,
                                            boot.width, boot.height,
                                            environment.facing_direction());
                                    draw_authored_pixel_art(
                                        sim::CreatureSpecies::human,
                                        art::Module::terminal, optional_foot_art,
                                        transform.beginning, transform.ending,
                                        transform.thickness,
                                        art::human_limb_layer_opacity(
                                            near || side == 0),
                                        mirrored_facing, false, true);
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
                        if (!optional_art_enabled)
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
                if (!optional_art_enabled)
                    return;
                for (std::size_t motor_index = 0;
                    motor_index < rig.active_motor_count; ++motor_index)
                {
                    if (rig.monopedal_gait() && motor_index >= 2u)
                        continue;

                    const sim::MotorConstraint& motor = rig.motors[motor_index];
                    if (!motor.enabled || motor.pivot >= particles.size()
                        || motor.c >= particles.size())
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
                        ? 23.0f : has_distal_motor ? 9.0f : 8.0f;
                    const float maximum_thickness = support_limb
                        ? 70.0f : has_distal_motor ? 30.0f : 26.0f;
                    const float limb_assembly_scale = support_limb
                        ? assembled_art_scale : 1.0f;
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
                    draw_authored_pixel_art(sim::CreatureSpecies::human,
                        support_limb
                            ? (has_distal_motor ? art::Module::upper_limb
                                : art::Module::lower_limb)
                            : (has_distal_motor ? art::Module::upper_limb
                                : art::Module::lower_limb),
                        sprite, beginning, ending,
                        thickness, art::human_limb_layer_opacity(
                            near || side == 0),
                        transverse_mirror, false, true);
                    if (support_mask == 0u && !has_distal_motor
                        && optional_hand_art.loaded())
                    {
                        const art::HandArtDimensions hand =
                            art::hand_art_dimensions(thickness,
                                optional_hand_art.width, optional_hand_art.height,
                                art_pixel_scale, span);
                        draw_authored_pixel_art(
                            sim::CreatureSpecies::human,
                            art::Module::hand, optional_hand_art,
                            terminal_joint - axis * hand.wrist_overlap,
                            terminal_joint + axis * (hand.length
                                - hand.wrist_overlap),
                            hand.thickness,
                            art::human_limb_layer_opacity(
                                near || side == 0),
                            transverse_mirror, false, true);
                    }
                }
            };
            draw_body_segments(0);
            draw_fitted_armor(0);
            draw_nodes(0);
            draw_body_segments(1);
            draw_fitted_armor(1);
            draw_nodes(1);

            if (optional_art_enabled
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
            if (optional_art_enabled && optional_helmet_art.loaded()
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
            draw_fitted_armor(2);
            draw_nodes(2);


        }

        void draw_training_pip(Rect rect)
        {
            const bool fixed_eye_test = course_eye_test_environment.has_value();
            const bool course_eye_test = fixed_eye_test && !art_eye_test && !walk_eye_test;
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019, 0.99f), accent_dim, 1.5f);
            add_text(canvas, rect.position + Vec2{ 12.0f, 9.0f },
                art_eye_test ? "ART CHECK"
                    : walk_eye_test ? "WALK PROOF"
                    : course_eye_test ? "COURSE EYE TEST"
                    : "LIVE TRAINING",
                0.88f, accent);
            if (!fixed_eye_test && !trainer.has_training_preview())
            {
                add_text_fit(canvas, rect.position + Vec2{ 12.0f, 42.0f },
                    "WAITING FOR FIRST INTACT TRAINING FRAME", 0.90f, muted,
                    rect.size.x - 24.0f);
                return;
            }

            const sim::Environment& environment = fixed_eye_test
                ? *course_eye_test_environment : trainer.training_preview();
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty() || rig.root_node >= particles.size())
            {
                add_text_fit(canvas, rect.position + Vec2{ 12.0f, 42.0f },
                    "TRAINING FRAME HAS NO COMPLETE RIG", 0.90f, danger,
                    rect.size.x - 24.0f);
                return;
            }

            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(environment.course_stage(), environment);
            const bool foot_only = !environment.non_foot_grounded();
            const bool intact = environment.body_integrity_valid();
            const rl::AutonomyStatus& preview_status = trainer.autonomy_status();
            const bool retained_terminal = !fixed_eye_test
                && preview_status.training_preview_terminal;
            const Color state_color = fixed_eye_test ? green
                : retained_terminal ? yellow : qualification.valid ? green
                : intact && foot_only ? yellow : danger;
            const std::string_view state_text = art_eye_test ? std::string_view{ "SIDE PROFILE" }
                : walk_eye_test ? (environment.raw_policy_audit()
                    ? std::string_view{ "RAW POLICY" }
                    : std::string_view{ "TEACHER 0 / ASSISTED" })
                : course_eye_test ? std::string_view{ "RUNWAY CLEAR" }
                : retained_terminal ? std::string_view{ "TRIAL FROZEN" }
                : qualification.valid ? "STAGE VALID" : !intact ? "BROKEN RIG"
                : !foot_only ? "BODY CONTACT"
                : rl::primary_motion_rejection_name(qualification.rejection_mask);
            add_text_fit(canvas, rect.position + Vec2{ rect.size.x - 132.0f, 9.0f },
                state_text, 0.76f, state_color, 120.0f, 0.68f);

            const Rect inner{ rect.position + Vec2{ 8.0f, 34.0f },
                { rect.size.x - 16.0f, rect.size.y - 64.0f } };
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
            const float view_min_x = std::min(root_x - 1.55f, body_min_x - 0.28f);
            const float view_max_x = std::max(root_x + 3.35f, body_max_x + 0.42f);
            const float view_min_y = std::min(body_min_y - 0.18f,
                environment.ground_height_at(root_x) - 0.18f);
            const float view_max_y = body_max_y + 0.32f;
            const float world_width = std::max(3.8f, view_max_x - view_min_x);
            const float world_height = std::max(view_camera::human_reference_height_m, view_max_y - view_min_y);
            const float scale = view_camera::pip_pixels_per_meter(
                (inner.size.x - 12.0f) / world_width,
                (inner.size.y * 0.78f) / world_height);
            const float camera = (view_min_x + view_max_x) * 0.5f;

            canvas.push_clip(inner.position, inner.position + inner.size);
            draw_course_ground(environment, inner, camera, scale);
            draw_terrain_challenges(environment, inner, camera, scale);
            draw_course_reference(environment, inner, camera, scale);
            draw_course_features(environment, inner, camera, scale);
            draw_creature(environment, inner, camera, scale);
            draw_equipment(environment, inner, camera, scale, true);
            canvas.pop_clip();

            const std::string pip_metrics = art_eye_test
                ? std::string("STRICT SIDE ELEVATION - NO PERSPECTIVE OR FORESHORTENING")
                : walk_eye_test
                    ? std::format(
                        "RETAINED UPDATE {}  DIST {:.1f} M  STEPS {}  CROSSINGS {}  SCISSOR {:.3f} S",
                        walk_eye_test_proof.retained_update,
                        walk_eye_test_proof.displayed_distance,
                        walk_eye_test_proof.displayed_steps,
                        walk_eye_test_proof.displayed_crossings,
                        walk_eye_test_proof.displayed_max_scissor_seconds)
                : course_eye_test
                    ? std::string("NO FALLING OBJECTS BEFORE 8-12 M + 2 REAL GAIT CYCLES")
                : retained_terminal
                    ? std::format(
                        "TRIAL {} FROZEN: {}  X {:.2f} M  CELL {}  {}  WATER {:.2f} M",
                        preview_status.training_preview_trial_id,
                        sim::trial_terminal_cause_name(
                            preview_status.training_preview_cause, preview_status.training_preview_reason),
                        preview_status.training_preview_position.x,
                        sim::DeformableTerrain::global_cell_x(
                            preview_status.training_preview_position.x),
                        sim::terrain_region_name(preview_status.training_preview_terrain),
                        preview_status.training_preview_water_depth)
                    : std::format(
                    "RIG UPDATES {}  POLICY AGE {}  PRIOR LINEAGE {}  DIST {:.1f} M  STEPS {}",
                    trainer.metrics().total_updates, trainer.metrics().update,
                    telemetry::prior_policy_lineage_updates(trainer.metrics()),
                    environment.distance_travelled(), environment.gait_cycles());
            add_text_fit(canvas, rect.position + Vec2{ 12.0f, rect.size.y - 23.0f },
                pip_metrics, 0.70f, state_color, rect.size.x - 24.0f, 0.64f);
            add_rounded_rect(canvas, rect, 10.0f, ui_render::transparent_fill, accent_dim, 1.5f);
        }

        void draw_live_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            canvas.push_clip(rect.position + Vec2{ 1.0f, 1.0f },
                rect.position + rect.size - Vec2{ 1.0f, 1.0f });
            const float usable_width = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
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

            add_text_fit(canvas, cursor, "AUTONOMOUS RIG TRAINER", 1.54f,
                white, usable_width, 1.08f);
            cursor.y += 42.0f;
            const float trainer_button_gap = 8.0f;
            const float trainer_button_width =
                (usable_width - trainer_button_gap) * 0.5f;
            if (button({ cursor, { trainer_button_width, 48.0f } },
                trainer.background_enabled() ? "AUTOPILOT ON - PAUSE" : "AUTOPILOT PAUSED - RUN",
                input, trainer.background_enabled()))
            {
                trainer.set_background_enabled(!trainer.background_enabled());
                set_status(trainer.background_enabled() ? "BACKGROUND TRAINING RESUMED" : "BACKGROUND TRAINING PAUSED");
            }
            const Rect guidance_button{
                cursor + Vec2{ trainer_button_width + trainer_button_gap, 0.0f },
                { trainer_button_width, 48.0f }
            };
            const sim::GuidanceMode active_guidance = walk_eye_test
                && course_eye_test_environment.has_value()
                ? course_eye_test_environment->guidance_mode()
                : trainer.preview_guidance_mode();
            const bool raw_audit = active_guidance
                == sim::GuidanceMode::raw_policy_audit;
            if (button(guidance_button, raw_audit ? "RAW AUDIT" : "ASSISTED",
                input, raw_audit))
            {
                const sim::GuidanceMode new_mode = raw_audit
                    ? sim::GuidanceMode::assisted
                    : sim::GuidanceMode::raw_policy_audit;
                if (walk_eye_test && course_eye_test_environment.has_value()
                    && walk_eye_replay_start.has_value())
                {
                    walk_eye_replay_start->set_guidance_mode(new_mode);
                    *course_eye_test_environment = *walk_eye_replay_start;
                    walk_eye_accumulator_seconds = 0.0;
                }
                else
                {
                    trainer.set_preview_guidance_mode(new_mode);
                    trainer.reset_preview();
                }
                set_status(new_mode == sim::GuidanceMode::raw_policy_audit
                    ? "RAW POLICY AUDIT - OPTIONAL GUIDANCE DISABLED"
                    : "ASSISTED GUIDANCE ENABLED");
            }

            cursor.y += 64.0f;

            add_text(canvas, cursor, "CURRENT LESSON", 1.05f, muted);
            cursor.y += 23.0f;
            const std::string lesson_name = autonomy.stage == sim::CourseStage::uneven
                ? std::format("3. {}", sim::gait_task_name(autonomy.gait_task))
                : std::string{ sim::course_stage_name(autonomy.stage) };
            add_text_fit(canvas, cursor, lesson_name, 2.05f,
                accent, usable_width, 1.30f);
            cursor.y += 38.0f;
            const float gait_button_width = (usable_width - 12.0f) * 0.25f;
            constexpr std::array gait_tasks{
                sim::GaitTask::walk, sim::GaitTask::speed_walk,
                sim::GaitTask::walk_run_transition, sim::GaitTask::run };
            constexpr std::array<std::string_view, 4> gait_labels{
                "WALK", "SPEED WALK", "TRANSITION", "RUN" };
            for (std::size_t gait_index = 0; gait_index < gait_tasks.size(); ++gait_index)
            {
                if (button({ cursor + Vec2{
                        static_cast<float>(gait_index) * (gait_button_width + 4.0f), 0.0f },
                        { gait_button_width, 32.0f } }, gait_labels[gait_index], input,
                    autonomy.stage == sim::CourseStage::uneven
                        && autonomy.gait_task == gait_tasks[gait_index]))
                {
                    trainer.set_gait_task(gait_tasks[gait_index]);
                    set_status(std::format("GAIT TASK SELECTED - {}",
                        sim::gait_task_name(gait_tasks[gait_index])));
                }
            }
            cursor.y += 39.0f;
            constexpr director::TaskGraph task_graph =
                director::default_training_graph();
            director_browser_index = std::min<std::uint16_t>(
                director_browser_index,
                static_cast<std::uint16_t>(task_graph.count - 1u));
            const float director_width = (usable_width - 12.0f) / 3.0f;
            if (button({ cursor, { director_width, 29.0f } }, "< TASK", input))
                director_browser_index = director_browser_index == 0u
                    ? static_cast<std::uint16_t>(task_graph.count - 1u)
                    : static_cast<std::uint16_t>(director_browser_index - 1u);
            const director::TaskNode& browsed = task_graph.nodes[director_browser_index];
            if (button({ cursor + Vec2{ director_width + 6.0f, 0.0f },
                    { director_width, 29.0f } },
                browsed.enabled ? "SELECT TASK" : "NOT IMPLEMENTED", input,
                autonomy.director_stable_task_id == browsed.stable_id,
                browsed.enabled))
            {
                trainer.select_director_task(director_browser_index);
                set_status(std::format("DIRECTOR REQUEST - {}",
                    director::task_name(browsed.task)));
            }
            if (button({ cursor + Vec2{ (director_width + 6.0f) * 2.0f, 0.0f },
                    { director_width, 29.0f } }, "TASK >", input))
                director_browser_index = static_cast<std::uint16_t>(
                    (director_browser_index + 1u) % task_graph.count);
            cursor.y += 35.0f;
            add_text_fit(canvas, cursor,
                std::format("DIRECTOR {:02}/{:02}  {} / {}{}",
                    director_browser_index + 1u, task_graph.count,
                    director::task_name(browsed.task),
                    director::challenge_name(browsed.challenge),
                    browsed.enabled ? "" : "  LOCKED"),
                0.65f, browsed.enabled ? muted : yellow, usable_width, 0.50f);
            cursor.y += 18.0f;
            add_text_fit(canvas, cursor,
                std::format("TRAINING LEVEL {:.0f}%", autonomy.difficulty * 100.0f),
                0.98f, white, usable_width, 0.80f);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor,
                std::format("LESSON COMPLETION {}%", progress_percent),
                1.02f, human_color, usable_width);
            cursor.y += 23.0f;
            const Rect progress_track{ cursor, { usable_width, 12.0f } };
            fill_rounded_rect(canvas, progress_track, 6.0f, rgb(0x101820));
            fill_rounded_rect(canvas,
                { progress_track.position,
                    { progress_track.size.x * progress.overall, progress_track.size.y } },
                6.0f, human_color);
            add_rounded_rect(canvas, progress_track, 6.0f, ui_render::transparent_fill, border, 1.0f);
            cursor.y += 21.0f;
            const int training_work_percent = static_cast<int>(std::lround(
                progress.training_work * 100.0f));
            const std::string final_checks = telemetry::final_check_label(
                human_status.state, progress.sample_budget_complete,
                training_work_percent, autonomy.stage_fresh_evaluations,
                autonomy.mastery_streak,
                rl::required_mastery_confirmations(autonomy.stage));
            add_text_fit(canvas, cursor, final_checks,
                0.78f, human_color, usable_width, 0.64f);
            cursor.y += 20.0f;
            add_text_fit(canvas, cursor,
                std::format("{}   {}   {}",
                    format_work_counter("UPDATES",
                        autonomy.stage_fresh_updates,
                        autonomy.stage_required_updates),
                    format_work_counter("RUNS",
                        autonomy.stage_fresh_episodes,
                        autonomy.stage_required_episodes),
                    format_work_counter("TESTS",
                        autonomy.stage_fresh_evaluations,
                        autonomy.stage_required_evaluations)),
                0.74f, muted, usable_width, 0.60f);
            cursor.y += 28.0f;

            const float third = (usable_width - 12.0f) / 3.0f;
            if (button({ cursor, { third, 40.0f } }, "NORMAL", input, trainer.updates_per_cycle() == 1))
                trainer.set_updates_per_cycle(1);
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 40.0f } },
                "FASTER", input, trainer.updates_per_cycle() == 2))
                trainer.set_updates_per_cycle(2);
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 40.0f } },
                "MAX CPU", input, trainer.updates_per_cycle() == 4))
                trainer.set_updates_per_cycle(4);
            cursor.y += 48.0f;
            if (button({ cursor, { third, 38.0f } }, "ZOOM OUT", input))
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(live_zoom_factor, -1.0f);
                live_zoom_auto = false;
            }
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 38.0f } },
                "AUTO VIEW", input, live_zoom_auto))
            {
                live_zoom_factor = 1.0f;
                live_zoom_auto = true;
            }
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 38.0f } },
                "ZOOM IN", input))
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(live_zoom_factor, 1.0f);
                live_zoom_auto = false;
            }
            cursor.y += 46.0f;
            const float half = (usable_width - 6.0f) * 0.5f;
            if (button({ cursor, { half, 36.0f } }, "METRIC / 10 M", input,
                distance_units == ui_layout::DistanceUnits::metric))
                distance_units = ui_layout::DistanceUnits::metric;
            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 36.0f } },
                "IMPERIAL / 50 FT", input,
                distance_units == ui_layout::DistanceUnits::imperial))
                distance_units = ui_layout::DistanceUnits::imperial;
            cursor.y += 53.0f;

            const float page_third = (usable_width - 12.0f) / 3.0f;
            if (button({ cursor, { page_third, 38.0f } }, "SUMMARY", input,
                live_panel_page == LivePanelPage::summary))
                live_panel_page = LivePanelPage::summary;
            if (button({ cursor + Vec2{ page_third + 6.0f, 0.0f },
                    { page_third, 38.0f } }, "TOTALS", input,
                live_panel_page == LivePanelPage::totals))
                live_panel_page = LivePanelPage::totals;
            if (button({ cursor + Vec2{ (page_third + 6.0f) * 2.0f, 0.0f },
                    { page_third, 38.0f } }, "ADVANCED", input,
                live_panel_page == LivePanelPage::advanced))
                live_panel_page = LivePanelPage::advanced;
            cursor.y += 47.0f;

            if (live_panel_page == LivePanelPage::summary)
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 365.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text_fit(canvas, cursor, "WHAT THE TRAINER IS TELLING YOU",
                    1.12f, accent, usable_width, 0.92f);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, human_status.headline,
                    1.32f, human_color, usable_width, 1.0f);
                cursor.y += 27.0f;
                cursor.y += add_wrapped_text(canvas, cursor, human_status.explanation,
                    0.78f, white, usable_width, 3.0f);
                cursor.y += 8.0f;
                add_text_fit(canvas, cursor,
                    std::format("TOTAL RIG UPDATES {}   LESSON COMPLETION {}%",
                        metrics.total_updates, progress_percent),
                    1.10f, white, usable_width, 0.86f);
                cursor.y += 25.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::total_updates_help(), 0.68f, muted,
                    usable_width, 2.0f);
                cursor.y += 8.0f;
                const Color test_color = telemetry_color(
                    telemetry::latest_test_tone(metrics));
                add_text_fit(canvas, cursor, telemetry::latest_test_title(metrics),
                    1.02f, test_color, usable_width, 0.82f);
                cursor.y += 23.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::latest_test_explanation(metrics, autonomy.stage),
                    0.76f, metrics.evaluation_valid ? green : yellow,
                    usable_width, 2.0f);
                cursor.y += 7.0f;

                std::string evidence{};
                switch (autonomy.stage)
                {
                case sim::CourseStage::balance:
                {
                    const std::uint32_t valid_seeds = 6u
                        - std::min<std::uint32_t>(metrics.evaluation_invalid_runs, 6u);
                    evidence = std::format(
                        "CURRENT EVIDENCE: UPRIGHT {:.1f} / 6.0 S   VALID TEST SEEDS {} / 6",
                        metrics.evaluation_longest_stance, valid_seeds);
                    break;
                }
                case sim::CourseStage::duck_press:
                    evidence = std::format(
                        "CURRENT EVIDENCE: CROUCH {:.1f} S   RECOVERIES {:.0f}   SURVIVAL {:.1f} S",
                        metrics.evaluation_duck_seconds,
                        metrics.evaluation_duck_recoveries,
                        metrics.evaluation_survival);
                    break;
                case sim::CourseStage::shuttle:
                    evidence = std::format(
                        "CURRENT EVIDENCE: DISTANCE {}   STRIDE EVENTS {:.0f}   SURVIVAL {:.1f} S",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events,
                        metrics.evaluation_survival);
                    break;
                case sim::CourseStage::uneven:
                    evidence = std::format(
                        "CURRENT: DIST {:.1f}/{:.0f} M   STEPS {:.0f}/{:.0f}   SPEED {:.2f}/{:.2f} M/S   SURV {:.1f}/18 S   COLL {:.0f}/1   CONFIRM {}/{}",
                        std::max(0.0f, metrics.evaluation_distance),
                        rl::gait_task_mastery_distance(autonomy.gait_task),
                        metrics.evaluation_stride_events,
                        rl::gait_task_mastery_stride_events(autonomy.gait_task),
                        std::max(0.0f, metrics.evaluation_speed),
                        rl::gait_task_mastery_speed(autonomy.gait_task),
                        std::max(0.0f, metrics.evaluation_survival),
                        std::max(0.0f, metrics.evaluation_collisions),
                        autonomy.mastery_streak,
                        rl::required_mastery_confirmations(autonomy.stage));
                    break;
                case sim::CourseStage::crouch_walk:
                    evidence = std::format(
                        "CURRENT EVIDENCE: LOW {:.1f} S   STRIDES {:.0f}   OBSTACLES {:.0f}",
                        metrics.evaluation_duck_seconds,
                        metrics.evaluation_stride_events,
                        metrics.evaluation_obstacles_passed);
                    break;
                case sim::CourseStage::ramps:
                    evidence = std::format(
                        "CURRENT EVIDENCE: POWERED JUMPS {:.0f}   SAFE LANDINGS {:.0f}   DISTANCE {}",
                        metrics.evaluation_powered_jumps,
                        metrics.evaluation_jump_landings,
                        format_distance(std::max(0.0f, metrics.evaluation_distance)));
                    break;
                case sim::CourseStage::hurdles:
                    evidence = std::format(
                        "CURRENT EVIDENCE: FEATURES {:.0f}   LANDINGS {:.0f}   DISTANCE {}",
                        metrics.evaluation_obstacles_passed,
                        metrics.evaluation_jump_landings,
                        format_distance(std::max(0.0f, metrics.evaluation_distance)));
                    break;
                case sim::CourseStage::duck_bars:
                    evidence = std::format(
                        "CURRENT EVIDENCE: FLIP LANDINGS {:.0f}   MAX TURNS {:.2f}   JUMPS {:.0f}",
                        metrics.evaluation_spin_landings,
                        metrics.evaluation_spin_turns,
                        metrics.evaluation_powered_jumps);
                    break;
                case sim::CourseStage::moving_hazards:
                    evidence = std::format(
                        "CURRENT EVIDENCE: DISTANCE {}   STRIDES {:.0f}   FEATURES CLEARED {:.0f}",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events,
                        metrics.evaluation_obstacles_passed);
                    break;
                case sim::CourseStage::climb_descent:
                    evidence = std::format(
                        "CURRENT EVIDENCE: HANDS {:.0f}   TRANSFERS {:.0f}   CLIMBS {:.0f}   DESCENTS {:.0f}",
                        metrics.evaluation_hand_contacts, metrics.evaluation_climb_transfers,
                        metrics.evaluation_climbs, metrics.evaluation_descents);
                    break;
                case sim::CourseStage::equipment_targets:
                    evidence = std::format(
                        "CURRENT EVIDENCE: HITS {:.0f} / SHOTS {:.0f}   HANDLING TRANSITIONS {:.0f}",
                        metrics.evaluation_target_hits, metrics.evaluation_shots,
                        metrics.evaluation_equipment_transitions);
                    break;
                case sim::CourseStage::combat_course:
                    evidence = std::format(
                        "CURRENT EVIDENCE: DISTANCE {}   STRIDES {:.0f}   TARGET HITS {:.0f}",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events, metrics.evaluation_target_hits);
                    break;
                }
                add_text_fit(canvas, cursor, evidence, 0.76f,
                    metrics.evaluation_valid ? green : muted, usable_width, 0.60f);
                cursor.y += 23.0f;
                add_text_fit(canvas, cursor,
                    trainer.has_best_policy()
                        ? std::format("RETAINED BEST CONTROLLER: SAVED AT UPDATE {}",
                            metrics.best_update)
                        : std::string("RETAINED BEST CONTROLLER: NONE YET - STILL SEARCHING"),
                    0.76f, trainer.has_best_policy() ? green : yellow,
                    usable_width, 0.62f);
                cursor.y += 24.0f;
                add_text(canvas, cursor, "NEXT GOAL", 0.86f, accent);
                cursor.y += 20.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::stage_goal(autonomy.stage), 0.73f, white,
                    usable_width, 2.0f);
                cursor.y += 5.0f;
                add_wrapped_text(canvas, cursor,
                    telemetry::sample_budget_message(progress), 0.68f,
                    progress.sample_budget_complete ? green : muted,
                    usable_width, 2.0f);
            }
            else if (live_panel_page == LivePanelPage::totals)
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 365.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, ui_layout::training_totals_scope_headings[0], 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("WALL TIME {}   OPTIMIZER UPDATES {}",
                        format_duration(rig_lifetime_seconds),
                        ui_layout::lifetime_delta(
                            metrics.total_updates, rig_start_total_updates)),
                    0.76f, white, usable_width, 0.64f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("COMPLETED AGENT RUNS {}   PASSED STAGE CHECKS {}   FAILED STAGE CHECKS {}",
                        ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                        ui_layout::lifetime_delta(metrics.total_valid_episodes,
                            rig_start_valid_episodes),
                        ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                            rig_start_invalid_episodes)),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("AGENT-EQUIVALENT DISTANCE {}   SUPPORT GAIT CYCLES {}   FALLS {}",
                        format_distance(static_cast<float>(std::max(0.0,
                            metrics.total_distance - rig_start_distance))),
                        ui_layout::lifetime_delta(metrics.total_alternating_steps,
                            rig_start_steps),
                        ui_layout::lifetime_delta(metrics.total_falls, rig_start_falls)),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("COLLISIONS {}   FEATURES CLEARED {}   BEST LESSON {}",
                        ui_layout::lifetime_delta(metrics.total_collisions,
                            rig_start_collisions),
                        ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                            rig_start_obstacles),
                        static_cast<unsigned>(
                            sim::course_stage_curriculum_index(
                                static_cast<sim::CourseStage>(rig_best_stage))) + 1u),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 28.0f;

                add_text(canvas, cursor, ui_layout::training_totals_scope_headings[1], 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("WALL TIME {}   AGENT-SIM TIME {}",
                        format_duration(session_runtime_seconds),
                        format_duration(static_cast<float>(
                            session_totals.agent_sim_seconds))),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("COMPLETED AGENT RUNS {}   PASSED {}   FAILED {}",
                        session_totals.completed_agent_runs,
                        session_totals.passed_stage_checks,
                        session_totals.failed_stage_checks),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("AGENT-EQUIVALENT DISTANCE {}   SUPPORT GAIT CYCLES {}   FALLS {}",
                        format_distance(static_cast<float>(
                            session_totals.agent_distance)),
                        session_totals.support_gait_cycles,
                        session_totals.falls),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 28.0f;

                add_text(canvas, cursor, ui_layout::training_totals_scope_headings[2], 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("TOTAL OPTIMIZER UPDATES {}   COMPLETED AGENT RUNS {}   PASSED {}   FAILED {}",
                        metrics.total_updates, metrics.total_episodes,
                        metrics.total_valid_episodes,
                        metrics.total_invalid_episodes),
                    0.76f, white, usable_width, 0.62f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("AGENT-EQUIVALENT DISTANCE {}   SUPPORT GAIT CYCLES {}   FALLS {}",
                        format_distance(static_cast<float>(metrics.total_distance)),
                        metrics.total_alternating_steps, metrics.total_falls),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 26.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::attempts_help(), 0.66f, muted,
                    usable_width, 2.0f);
                cursor.y += 4.0f;
                add_wrapped_text(canvas, cursor,
                    telemetry::reset_help(), 0.66f, muted,
                    usable_width, 2.0f);
            }
            else
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 470.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, "ADVANCED DIAGNOSTICS", 1.08f, accent);
                cursor.y += 27.0f;
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
                add_text_fit(canvas, cursor,
                    std::format("MODE {}   TEACHER {:.2f}   CONTACT PHYSICS {:.2f}",
                        sim::guidance_mode_name(authority.mode),
                        authority.lesson_teacher, authority.physical_contact),
                    0.70f, authority.optional_guidance_active() ? yellow : green,
                    usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("REFLEX S/B {:.2f}/{:.2f}   SAFETY S/B {:.2f}/{:.2f}",
                        authority.topology_support, authority.topology_body,
                        authority.safety_support, authority.safety_body),
                    0.68f, muted, usable_width, 0.54f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format(
                        "POSTURE {:.2f}   SWING {:.2f}   LOCAL JOINT CLUSTER {:.2f}",
                        authority.posture_guide, authority.swing_clearance,
                        authority.mandatory_joint_cluster),
                    0.68f, muted, usable_width, 0.54f);
                cursor.y += 21.0f;
                const std::uint32_t global_cell_x =
                    sim::DeformableTerrain::global_cell_x(debug_root_x);
                const float debug_root_y = debug_environment.particles().empty()
                    ? 0.0f : debug_environment.particles()[
                        debug_environment.blueprint().root_node].position.y;
                const std::uint32_t global_cell_y =
                    sim::DeformableTerrain::global_cell_y(debug_root_y);
                add_text_fit(canvas, cursor,
                    std::format("STANCE SLIP {:.3f} M/S   STATIC WORLD CELL {},{}   GAIT CYCLES {}",
                        debug_environment.stance_slip_speed(),
                        global_cell_x, global_cell_y,
                        debug_environment.gait_cycles()),
                    0.68f, green, usable_width, 0.54f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("WEAPON {}   STOPPED {}   AIM CMD/ACT {:+.1f}/{:+.1f} DEG   SETTLE {:.2f} S   MISSES {}",
                        sim::equipment_state_name(debug_environment.equipment_state()),
                        debug_environment.equipment_stopped() ? "YES" : "NO",
                        debug_environment.equipment_commanded_aim_angle() * 180.0f / pi,
                        debug_environment.equipment_aim_angle() * 180.0f / pi,
                        debug_environment.equipment_aim_settle_seconds(),
                        debug_environment.shots_missed()),
                    0.68f, debug_environment.equipment_engagement_ready()
                        ? green : muted, usable_width, 0.54f);
                cursor.y += 21.0f;
                const bool raw_evaluation =
                    rl::foundational_walk_uses_raw_evaluation(
                        autonomy.stage_fresh_updates, autonomy.stage,
                        trainer.blueprint());
                const std::size_t raw_rollouts =
                    rl::foundational_walk_raw_rollout_count(
                        autonomy.stage_fresh_updates, autonomy.stage,
                        trainer.blueprint(), trainer.environment_count());
                add_text_fit(canvas, cursor,
                    std::format("ACTIVE {} EVALUATION {}   RETAINED BEST {}   RAW ROLLOUTS {}/{}",
                        raw_evaluation ? "RAW POLICY" : "ASSISTED",
                        raw_number(metrics.evaluation_score),
                        raw_number(metrics.best_evaluation_score),
                        raw_rollouts, trainer.environment_count()),
                    0.72f, muted, usable_width, 0.58f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("BEST SAVED AT LOCAL UPDATE {}   QUALITY KEY {}",
                        metrics.best_update, quality),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("REJECTION MASK 0x{:08X}   {}",
                        metrics.evaluation_rejection_mask,
                        rl::primary_motion_rejection_name(
                            metrics.evaluation_rejection_mask)),
                    0.70f, metrics.evaluation_valid ? green : yellow,
                    usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("FAILURES {}", rl::motion_rejection_summary(
                        metrics.evaluation_rejection_mask)),
                    0.68f, metrics.evaluation_valid ? green : yellow,
                    usable_width, 0.54f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("INVALID DETAIL {}   GROUND {}   WATER {:.2f} M",
                        sim::invalid_motion_name(metrics.evaluation_invalid_reason),
                        sim::terrain_region_name(debug_environment.terrain_region_at(debug_root_x)),
                        debug_environment.water_depth()),
                    0.68f, metrics.evaluation_invalid_reason == sim::InvalidMotion::none
                        ? muted : danger, usable_width, 0.54f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    nearest_feature == nullptr
                        ? std::format("TERRAIN LESSON   SUPPORT SPAN {:.2f}X   SCISSOR MAX {:.3f} S",
                            debug_environment.primary_support_span_ratio(),
                            debug_environment.maximum_lower_leg_scissor_seconds())
                        : std::format("NEAREST {} {:.1f} M   SPAN {:.2f}X   SCISSOR {:.3f} S",
                            sim::course_feature_name(nearest_feature->kind), nearest_distance,
                            debug_environment.primary_support_span_ratio(),
                            debug_environment.maximum_lower_leg_scissor_seconds()),
                    0.68f, nearest_feature == nullptr ? muted : yellow,
                    usable_width, 0.54f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("BACKWARD BRACE PREVIEW {:.3f} S   TEST {:.3f} S",
                        debug_environment.maximum_backward_brace_seconds(),
                        metrics.evaluation_max_backward_brace_seconds),
                    0.68f, metrics.evaluation_max_backward_brace_seconds
                            > sim::sustained_backward_brace_limit_seconds
                        ? yellow : muted, usable_width, 0.54f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("POLICY LOSS {}   VALUE LOSS {}",
                        raw_number(metrics.policy_loss), raw_number(metrics.value_loss)),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("ENTROPY {}   LEARNING RATE {:.7f}",
                        raw_number(metrics.entropy), metrics.learning_rate),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("MEAN REWARD {}   MEAN SPEED {}",
                        raw_number(metrics.mean_reward),
                        format_speed(metrics.mean_speed)),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("LOCAL UPDATE {}   ENVIRONMENT STEPS {}",
                        metrics.update, metrics.environment_steps),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("OPTIMIZER STEP {}   EXPLORATION {:.4f}",
                        trainer.optimizer_step(), trainer.exploration()),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("WORKERS {}   SIMULATIONS {}   {:.2f} UPDATES/SECOND",
                        autonomy.rollout_threads, autonomy.environment_count,
                        autonomy.updates_per_second),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("PIPELINE {}   CONTROLLER {}",
                        autonomy.pipeline_stage, trainer.controller_state_name()),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("CONTROLLER TUNING {}   ACCEPTED {}   REJECTED {}   ROLLBACKS {}",
                        autonomy.rig_generation, autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes, autonomy.rollback_count),
                    0.68f, muted, usable_width, 0.54f);
                cursor.y += 25.0f;
                add_wrapped_text(canvas, cursor,
                    "These raw values are for debugging. A negative score or loss does not mean the trainer can never learn.",
                    0.68f, accent, usable_width, 2.0f);
            }
            canvas.pop_clip();
            add_rounded_rect(canvas, rect, 11.0f, ui_render::transparent_fill, border, 1.0f);
        }

        void draw_live_world(Rect viewport, float dt, const InputState& input)
        {
            if (!run_paused && walk_eye_test
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
                        replay.observation());
                    const auto action = rl::effective_policy_action(replay,
                        raw_action, sim::CourseStage::uneven, 0.0f,
                        replay.guidance_mode());
                    const sim::GuidanceMode replay_guidance = replay.guidance_mode();
                    if (replay.step(action, static_cast<float>(fixed_step)).terminated)
                    {
                        replay = *walk_eye_replay_start;
                        replay.set_guidance_mode(replay_guidance);
                        const auto& reset_particles = replay.particles();
                        const std::size_t reset_root = replay.blueprint().root_node;
                        if (reset_root < reset_particles.size())
                        {
                            camera_x = reset_particles[reset_root].position.x
                                + view_camera::lookahead_meters(
                                    viewport.size.x, live_pixels_per_meter)
                                    * replay.facing_direction();
                        }
                    }
                    walk_eye_accumulator_seconds -= fixed_step;
                }
            }
            else if (!run_paused && !course_eye_test_environment.has_value())
                trainer.step_preview(dt);
            const sim::Environment& environment = course_eye_test_environment.has_value()
                ? *course_eye_test_environment : trainer.preview();
            const auto& particles = environment.particles();
            if (contains(viewport, input.mouse) && std::abs(input.wheel) >= 0.01f)
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(
                    live_zoom_factor, input.wheel);
                live_zoom_auto = false;
            }

            float rig_height = 2.4f;
            if (!particles.empty())
            {
                float minimum_y = std::numeric_limits<float>::infinity();
                float maximum_y = -std::numeric_limits<float>::infinity();
                for (const sim::Particle& particle : particles)
                {
                    minimum_y = std::min(minimum_y, particle.position.y - particle.radius);
                    maximum_y = std::max(maximum_y, particle.position.y + particle.radius);
                }
                if (std::isfinite(minimum_y) && std::isfinite(maximum_y))
                    rig_height = std::max(0.75f, maximum_y - minimum_y);
            }
            if (!course_eye_test_environment.has_value())
            {
                float target_pixels_per_meter = view_camera::fitted_pixels_per_meter(
                    viewport.size.y,
                    std::max(rig_height, view_camera::human_reference_height_m),
                    live_zoom_factor);
                if (environment.shuttle_enabled() && live_zoom_auto)
                {
                    const float arena_span = sim::shuttle_right_boundary
                        - sim::shuttle_left_boundary + 3.0f;
                    target_pixels_per_meter = std::min(target_pixels_per_meter,
                        viewport.size.x / arena_span);
                }
                live_pixels_per_meter = view_camera::smooth_zoom(
                    live_pixels_per_meter, target_pixels_per_meter, dt);
            }
            if (!particles.empty() && view_camera::tracks_live_subject(
                    course_eye_test_environment.has_value(), walk_eye_test))
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

            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            canvas.push_clip(viewport.position + Vec2{ 1.0f, 1.0f },
                viewport.position + viewport.size - Vec2{ 1.0f, 1.0f });
            draw_course_ground(environment, viewport, camera_x, live_pixels_per_meter);
            draw_terrain_challenges(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_reference(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_features(environment, viewport, camera_x, live_pixels_per_meter);
            draw_creature(environment, viewport, camera_x, live_pixels_per_meter);
            draw_equipment(environment, viewport, camera_x, live_pixels_per_meter, true);

            const ui_layout::Box world_box{
                viewport.position.x, viewport.position.y,
                viewport.size.x, viewport.size.y };
            const ui_layout::Box telemetry_box = ui_layout::primary_telemetry_box(world_box);
            const ui_layout::Box pip_box = ui_layout::training_pip_box(world_box);
            const ui_layout::Box bottom_box = ui_layout::bottom_telemetry_box(world_box);
            const Rect telemetry{ { telemetry_box.x, telemetry_box.y },
                { telemetry_box.width, telemetry_box.height } };
            const Rect bottom{ { bottom_box.x, bottom_box.y },
                { bottom_box.width, bottom_box.height } };
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
                1.42f, white, text_width, 1.00f);
            line.y += 31.0f;
            float displayed_world_x = 0.0f;
            float displayed_world_y = 0.0f;
            if (!environment.particles().empty())
            {
                const sim::Particle& root = environment.particles()[
                    environment.blueprint().root_node];
                displayed_world_x = root.position.x;
                displayed_world_y = root.position.y;
            }
            add_text_fit(canvas, line,
                std::format("SPEED {}   DIST {}   WORLD X {:.2f} M   CELL {},{}",
                    format_speed(environment.forward_speed()),
                    format_distance(environment.distance_travelled()),
                    displayed_world_x,
                    sim::DeformableTerrain::global_cell_x(displayed_world_x),
                    sim::DeformableTerrain::global_cell_y(displayed_world_y)),
                0.92f, environment.valid_motion() ? green : danger, text_width);
            line.y += 24.0f;
            add_text_fit(canvas, line,
                std::format("SUPPORT GAIT CYCLES {}   LEG CROSSINGS {}   HEEL STRIKES {}   TOE LIFTS {}",
                    environment.gait_cycles(), environment.limb_crossings(),
                    environment.heel_strikes(), environment.toe_offs()),
                0.84f, environment.recovering() ? yellow : muted, text_width);
            line.y += 23.0f;
            add_text_fit(canvas, line,
                std::format("LEFT FOOT {}   RIGHT FOOT {}   FEATURES CLEARED {}",
                    sim::foot_contact_phase_name(environment.left_foot_phase()),
                    sim::foot_contact_phase_name(environment.right_foot_phase()),
                    environment.obstacles_passed()),
                0.82f, muted, text_width);
            line.y += 23.0f;
            add_text_fit(canvas, line,
                walk_eye_test
                    ? std::format(
                        "MOTION VALID   COLD UPDATES {}   AUTHORITY {:.3f}   SEEDS {}/6",
                        walk_eye_test_proof.updates,
                        walk_eye_test_proof.teacher_authority,
                        6u - walk_eye_test_proof.retained_invalid_runs)
                    : std::format(
                        "MOTION {}   RIG UPDATES {}   POLICY AGE {}   PRIOR LINEAGE {}",
                        sim::invalid_motion_name(environment.invalid_reason()),
                        trainer.metrics().total_updates, trainer.metrics().update,
                        telemetry::prior_policy_lineage_updates(trainer.metrics())),
                0.80f, environment.valid_motion() ? accent : danger, text_width);
            line.y += 21.0f;
            const std::uint64_t preview_restarts = trainer.preview_reset_count();
            const sim::InvalidMotion preview_reason = trainer.preview_last_reset_reason();
            const bool preview_terminal = trainer.preview_terminal();
            add_text_fit(canvas, line,
                walk_eye_test
                    ? std::format(
                        "RETAINED MEAN {:.1f} M / {:.1f} STEPS   REPLAY SEED 0x{:X}",
                        walk_eye_test_proof.retained_distance,
                        walk_eye_test_proof.retained_stride_events,
                        walk_eye_test_proof.selected_seed)
                    : preview_terminal
                    ? std::format("TRIAL {} FROZEN: {} AT X {:.2f} M / CELL {} / {} / WATER {:.2f} M - R RETRY",
                        trainer.preview_trial_id(), sim::trial_terminal_cause_name(
                            trainer.preview_terminal_cause(), preview_reason),
                        trainer.preview_terminal_position().x,
                        sim::DeformableTerrain::global_cell_x(
                            trainer.preview_terminal_position().x),
                        sim::terrain_region_name(trainer.preview_terminal_terrain()),
                        trainer.preview_terminal_water_depth())
                    : preview_restarts == 0u
                    ? std::format("TRIAL {} ACTIVE - REAL STATIC COURSE",
                        trainer.preview_trial_id())
                    : std::format("TRIAL {} ACTIVE   PRIOR TERMINALS {}",
                        trainer.preview_trial_id(), preview_restarts),
                0.72f, walk_eye_test || (!preview_terminal
                    && preview_restarts == 0u) ? muted : yellow, text_width);

            draw_training_pip({ { pip_box.x, pip_box.y },
                { pip_box.width, pip_box.height } });
            add_rounded_rect(canvas, bottom, 8.0f,
                rgb(0x07111b, 0.96f), border, 1.0f);
            const std::string run_status = art_eye_test ? "FIXED SIDE PROFILE"
                : walk_eye_test ? (environment.raw_policy_audit()
                    ? "RAW POLICY REPLAY - OPTIONAL GUIDANCE OFF"
                    : "TEACHER-0 / ASSISTED REPLAY")
                : course_eye_test_environment.has_value() ? "FIXED START FRAME"
                : environment.shuttle_enabled()
                    ? std::format("SHUTTLE {} {}",
                        sim::shuttle_phase_name(environment.shuttle_phase()),
                        environment.facing_direction() > 0.0f ? "RIGHT" : "LEFT")
                : trainer.background_enabled() ? "TRAINING" : "PAUSED";
            add_text_fit(canvas, bottom.position + Vec2{ 11.0f, 10.0f },
                std::format("{}   v{}   GROUND {}   WATER {:.2f} M   EQUIP {} / {}   HITS {}   {}",
                    art_eye_test ? "PACKAGED ORTHOGRAPHIC ART TEST"
                        : walk_eye_test ? "PACKAGED RETAINED WALK EYE TEST"
                        : course_eye_test_environment.has_value()
                            ? "PACKAGED COURSE EYE TEST"
                            : trainer.has_best_policy()
                            ? "RETAINED CHAMPION PREVIEW"
                            : "CURRENT EXPLORATORY POLICY",
                    RUNNER_VERSION,
                    sim::terrain_region_name(environment.terrain_region_at(camera_x)),
                    environment.water_depth(),
                    sim::weapon_class_name(environment.weapon_class()),
                    sim::equipment_state_name(environment.equipment_state()),
                    environment.target_hits(),
                    run_status),
                0.86f, course_eye_test_environment.has_value() || trainer.has_best_policy()
                    ? green : yellow,
                bottom.size.x - 22.0f, 0.76f);
            canvas.pop_clip();
            add_rounded_rect(canvas, viewport, 11.0f, ui_render::transparent_fill, border, 1.0f);
        }

        void draw_joint_lab(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x0b1721, 0.98f), accent_dim, 1.0f);
            const auto names = motor_names();
            add_text(canvas, rect.position + Vec2{ 14.0f, 10.0f },
                std::format("JOINT TEST - {}", names[static_cast<std::size_t>(selected_motor)]), 1.45f, white);
            const ui_layout::RigLabTestLayout layout = ui_layout::rig_lab_test_layout({
                rect.position.x, rect.position.y, rect.size.x, rect.size.y });
            const float group_width = layout.selection_row.width * 0.25f;
            Vec2 row{ layout.selection_row.x, layout.selection_row.y };
            if (button({ row, { group_width - 4.0f, 31.0f } }, "SELECTED", input,
                joint_test_group == JointTestGroup::selected))
                joint_test_group = JointTestGroup::selected;
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "LEGS 1-4", input,
                joint_test_group == JointTestGroup::pair_a))
                joint_test_group = JointTestGroup::pair_a;
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "ARMS 5-8", input,
                joint_test_group == JointTestGroup::pair_b))
                joint_test_group = JointTestGroup::pair_b;
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "ALL", input,
                joint_test_group == JointTestGroup::all))
                joint_test_group = JointTestGroup::all;

            row = { layout.range_row.x, layout.range_row.y };
            if (button({ row, { group_width - 4.0f, 31.0f } }, "MIN", input))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = -1.0f;
            }
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "REST", input))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = 0.0f;
            }
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "MAX", input))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = 1.0f;
            }
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, 31.0f } },
                joint_auto_sweep ? "STOP" : "SWEEP", input, joint_auto_sweep))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = !joint_auto_sweep;
            }

            row = { layout.pattern_row.x, layout.pattern_row.y };
            if (button({ row, { group_width - 4.0f, 31.0f } }, "CROUCH", input,
                rig_test_pattern == sim::RigTestPattern::crouch))
            {
                joint_auto_sweep = false;
                rig_test_pattern = sim::RigTestPattern::crouch;
            }
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "GAIT CYCLE", input,
                rig_test_pattern == sim::RigTestPattern::gait))
            {
                joint_auto_sweep = false;
                rig_test_pattern = sim::RigTestPattern::gait;
            }
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "FIRM GROUND", input,
                !rig_test_loose_ground))
                rig_test_loose_ground = false;
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "LOOSE GROUND", input,
                rig_test_loose_ground))
                rig_test_loose_ground = true;

            const float friction = sim::foot_friction_retention(0.45f,
                rig_test_loose_ground ? 0.25f : 1.0f,
                rig_test_loose_ground ? 0.75f : 0.0f, false, false);
            add_text(canvas, { layout.status_row.x, layout.status_row.y },
                std::format("TRACTION TEST RETENTION {:.3f}  {}",
                    friction, rig_test_loose_ground ? "LOOSE" : "FIRM"),
                0.92f, rig_test_loose_ground ? yellow : green);
            joint_test_input = slider({ { layout.manual_slider.x, layout.manual_slider.y },
                { layout.manual_slider.width, layout.manual_slider.height } },
                "MANUAL INPUT", joint_test_input, -1.0f, 1.0f, input);
        }

        void draw_art_editor_world(Rect viewport)
        {
            const sim::Environment& environment = art_preview_frozen
                    && frozen_art_environment.has_value()
                ? *frozen_art_environment : trainer.preview();
            if (art_editor_high_contrast)
                canvas.quad(viewport.position, viewport.position + viewport.size,
                    rgb(0x03070a));
            const auto& particles = environment.particles();
            float camera = 0.0f;
            float minimum_y = std::numeric_limits<float>::infinity();
            float maximum_y = -std::numeric_limits<float>::infinity();
            if (!particles.empty())
            {
                const std::size_t root = environment.blueprint().root_node;
                if (root < particles.size())
                    camera = particles[root].position.x;
                for (const sim::Particle& particle : particles)
                {
                    minimum_y = std::min(minimum_y,
                        particle.position.y - particle.radius);
                    maximum_y = std::max(maximum_y,
                        particle.position.y + particle.radius);
                }
            }
            const float rig_height = std::isfinite(minimum_y)
                    && std::isfinite(maximum_y)
                ? std::max(0.75f, maximum_y - minimum_y) : 2.4f;
            const float scale = art_editor_zoom * std::clamp(view_camera::fitted_pixels_per_meter(
                viewport.size.y, std::max(rig_height, 1.0f), 1.20f),
                28.0f, 150.0f);
            draw_course_ground(environment, viewport, camera, scale);
            draw_terrain_challenges(environment, viewport, camera, scale);
            draw_course_reference(environment, viewport, camera, scale);
            draw_course_features(environment, viewport, camera, scale);
            draw_creature(environment, viewport, camera, scale,
                debug_skeleton_overlay);
            draw_equipment(environment, viewport, camera, scale, true);
            add_text_fit(canvas, viewport.position + Vec2{ 18.0f, 16.0f },
                art_preview_frozen
                    ? "PRODUCTION ART VIEW   FROZEN PHYSICAL POSE / LIVE TRAINING CONTINUES"
                    : "PRODUCTION ART VIEW   SAME POSE / SCALE / CONTACTS AS LIVE PREVIEW",
                0.80f, accent, viewport.size.x - 36.0f, 0.68f);
            add_text_fit(canvas,
                viewport.position + Vec2{ 18.0f, viewport.size.y - 31.0f },
                std::format("{}   WATER {}   SLIP {:.3f} M/S   TRACTION {:.2f}/{:.2f}",
                    sim::water_traversal_phase_name(environment.water_traversal_phase()),
                    environment.water_depth(), environment.stance_slip_speed(),
                    environment.used_traction(), environment.available_traction()),
                0.74f, muted, viewport.size.x - 36.0f, 0.62f);
        }

        void draw_blueprint(Rect viewport, const InputState& input)
        {
            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            float minimum_y = std::numeric_limits<float>::infinity();
            float maximum_y = -std::numeric_limits<float>::infinity();
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const float radius = index < blueprint.radii.size()
                    ? blueprint.radii[index] : 0.15f;
                minimum_x = std::min(minimum_x, blueprint.nodes[index].x - radius);
                maximum_x = std::max(maximum_x, blueprint.nodes[index].x + radius);
                minimum_y = std::min(minimum_y, blueprint.nodes[index].y - radius);
                maximum_y = std::max(maximum_y, blueprint.nodes[index].y + radius);
            }
            if (!std::isfinite(minimum_x) || !std::isfinite(maximum_x)
                || !std::isfinite(minimum_y) || !std::isfinite(maximum_y))
            {
                minimum_x = -1.0f;
                maximum_x = 1.0f;
                minimum_y = 0.0f;
                maximum_y = 2.0f;
            }
            const ui_layout::Box layout_viewport{ viewport.position.x,
                viewport.position.y, viewport.size.x, viewport.size.y };
            const ui_layout::BlueprintFit fit = ui_layout::fit_blueprint(
                layout_viewport, minimum_x, maximum_x, minimum_y, maximum_y);
            const float scale = fit.pixels_per_meter;
            auto project = [&](Vec2 world)
            {
                return Vec2{
                    ui_layout::blueprint_screen_x(fit, layout_viewport, world.x),
                    ui_layout::blueprint_screen_y(fit, world.y)
                };
            };
            auto unproject = [&](Vec2 screen_position)
            {
                return Vec2{
                    fit.camera_x + (screen_position.x
                        - (viewport.position.x + viewport.size.x * 0.5f)) / scale,
                    fit.world_center_y
                        + (fit.content_center_y - screen_position.y) / scale
                };
            };
            const float ground_y = project({ 0.0f, 0.0f }).y;
            if (ground_y < viewport.position.y + viewport.size.y)
                canvas.quad({ viewport.position.x, std::max(ground_y, viewport.position.y) },
                    viewport.position + viewport.size, rgb(0x111820));
            canvas.line({ viewport.position.x, ground_y },
                { viewport.position.x + viewport.size.x, ground_y },
                3.0f, rgb(0x475762));

            auto screen = [&](std::size_t index)
            {
                return project(blueprint.nodes[index]);
            };
            std::vector<Vec2> preview = blueprint.nodes;
            for (int motor_index = 0; motor_index < static_cast<int>(sim::anatomy_action_count); ++motor_index)
            {
                if (!test_motor_active(motor_index))
                    continue;
                const sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(motor_index)];
                if (!motor.enabled || motor.a >= preview.size() || motor.pivot >= preview.size() || motor.c >= preview.size())
                    continue;
                const Vec2 pivot = preview[motor.pivot];
                const float current = signed_angle(preview[motor.a] - pivot, preview[motor.c] - pivot);
                const float delta = wrap_angle(sim::motor_target_angle(
                    motor, test_input_for_motor(static_cast<std::size_t>(motor_index))) - current);
                std::vector<std::uint16_t> stack{ motor.c };
                std::vector<bool> visited(preview.size(), false);
                visited[motor.pivot] = true;
                visited[motor.c] = true;
                while (!stack.empty())
                {
                    const std::uint16_t node = stack.back();
                    stack.pop_back();
                    for (const sim::DistanceConstraint& bone : blueprint.bones)
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
                for (std::size_t index = 0; index < preview.size(); ++index)
                {
                    if (visited[index] && index != motor.pivot)
                        preview[index] = pivot + rotate(preview[index] - pivot, delta);
                }
            }
            auto preview_screen = [&](std::size_t index)
            {
                return project(preview[index]);
            };
            for (const sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a < preview.size() && bone.b < preview.size())
                    canvas.line(preview_screen(bone.a), preview_screen(bone.b),
                        1.5f, with_alpha(accent, 0.08f));
            }
            for (std::size_t bone_index = 0; bone_index < blueprint.bones.size(); ++bone_index)
            {
                const sim::DistanceConstraint& bone = blueprint.bones[bone_index];
                if (bone.a < blueprint.nodes.size() && bone.b < blueprint.nodes.size())
                    canvas.line(screen(bone.a), screen(bone.b),
                        bone_index == static_cast<std::size_t>(selected_bone) ? 10.0f : 7.0f,
                        bone_index == static_cast<std::size_t>(selected_bone) ? accent : rgb(0x835927));
            }
            const bool structure_labels = rig_panel_page == RigPanelPage::structure;
            std::vector<ui_layout::Box> occupied_annotations{};
            occupied_annotations.reserve(blueprint.nodes.size() * 2u + 8u);
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const float physical_radius = (index < blueprint.radii.size()
                    ? blueprint.radii[index] : 0.15f) * scale;
                const float radius = std::clamp(physical_radius, 7.0f, 18.0f);
                const Vec2 center = screen(index);
                occupied_annotations.push_back({ center.x - radius - 3.0f,
                    center.y - radius - 3.0f, radius * 2.0f + 6.0f,
                    radius * 2.0f + 6.0f });
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
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const float physical_radius = (index < blueprint.radii.size()
                    ? blueprint.radii[index] : 0.15f) * scale;
                const float radius = std::clamp(physical_radius, 7.0f, 18.0f);
                Color color = index == blueprint.head_node ? body_light : body;
                if (blueprint.is_support_seed(index))
                    color = leg;
                canvas.circle(screen(index), radius, color, 24);
                canvas.circle(screen(index), index == static_cast<std::size_t>(selected_node)
                    ? 5.0f : 3.0f,
                    index == static_cast<std::size_t>(selected_node) ? accent : white, 18);
                if (structure_labels || index == static_cast<std::size_t>(selected_node))
                    static_cast<void>(place_blueprint_label(screen(index),
                        std::to_string(index), 0.68f, white));

                std::string_view foot_label{};
                int contact_row = 0;
                bool left_contact = false;
                if (index == blueprint.left_contact_node)
                {
                    foot_label = "L HEEL";
                    left_contact = true;
                }
                else if (index == blueprint.right_contact_node)
                    foot_label = "R HEEL";
                else if (blueprint.additional_left_contact_nodes.size() >= 1u
                    && index == blueprint.additional_left_contact_nodes[0])
                {
                    foot_label = "L BALL";
                    contact_row = 1;
                    left_contact = true;
                }
                else if (blueprint.additional_left_contact_nodes.size() >= 2u
                    && index == blueprint.additional_left_contact_nodes[1])
                {
                    foot_label = "L TOE";
                    contact_row = 2;
                    left_contact = true;
                }
                else if (blueprint.additional_right_contact_nodes.size() >= 1u
                    && index == blueprint.additional_right_contact_nodes[0])
                {
                    foot_label = "R BALL";
                    contact_row = 1;
                }
                else if (blueprint.additional_right_contact_nodes.size() >= 2u
                    && index == blueprint.additional_right_contact_nodes[1])
                {
                    foot_label = "R TOE";
                    contact_row = 2;
                }
                if (!foot_label.empty())
                    static_cast<void>(place_blueprint_label(screen(index), foot_label,
                        0.62f, yellow, left_contact, contact_row));
            }
            const sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(selected_motor)];
            if (motor.enabled && motor.a < blueprint.nodes.size() && motor.pivot < blueprint.nodes.size()
                && motor.c < blueprint.nodes.size())
            {
                const Vec2 pivot_world = blueprint.nodes[motor.pivot];
                const Vec2 reference = normalized(blueprint.nodes[motor.a] - pivot_world, { 0.0f, 1.0f });
                const float arm_length = std::max(0.25f,
                    std::min(length(blueprint.nodes[motor.a] - pivot_world),
                        length(blueprint.nodes[motor.c] - pivot_world)) * 0.72f);
                std::vector<Vec2> arc{};
                for (int segment = 0; segment <= 32; ++segment)
                {
                    const float t = static_cast<float>(segment) / 32.0f;
                    const float angle = lerp(motor.minimum_angle, motor.maximum_angle, t);
                    arc.push_back(project(pivot_world + rotate(reference, angle) * arm_length));
                }
                canvas.polyline(arc, 4.0f, accent);
                const Vec2 pivot_screen = screen(motor.pivot);
                auto ray = [&](float angle, Color color, float width)
                {
                    canvas.line(pivot_screen,
                        project(pivot_world + rotate(reference, angle) * arm_length), width, color);
                };
                ray(motor.minimum_angle, danger, 2.5f);
                ray(motor.maximum_angle, danger, 2.5f);
                ray(motor.neutral_angle, white, 3.0f);
                ray(sim::motor_target_angle(motor, joint_test_input), yellow, 4.0f);
                static_cast<void>(place_blueprint_label(screen(motor.a),
                    rig_panel_page == RigPanelPage::test ? "A" : "A PARENT",
                    0.72f, accent, true));
                static_cast<void>(place_blueprint_label(pivot_screen,
                    rig_panel_page == RigPanelPage::test ? "P" : "PIVOT",
                    0.72f, white));
                static_cast<void>(place_blueprint_label(screen(motor.c),
                    rig_panel_page == RigPanelPage::test ? "C" : "C DRIVEN",
                    0.72f, yellow));            }

            if (rig_panel_page == RigPanelPage::test
                && editor_weapon_class != sim::WeaponClass::none)
            {
                std::size_t hand = preview.size();
                float best_reach = -std::numeric_limits<float>::infinity();
                for (std::size_t motor_index = 0;
                    motor_index < blueprint.active_motor_count; ++motor_index)
                {
                    const sim::MotorConstraint& candidate = blueprint.motors[motor_index];
                    if (!candidate.enabled || blueprint.support_branch_mask(candidate) != 0u
                        || candidate.c >= preview.size())
                        continue;
                    bool has_distal_manipulator{};
                    for (std::size_t other_index = 0;
                        other_index < blueprint.active_motor_count; ++other_index)
                    {
                        const sim::MotorConstraint& other = blueprint.motors[other_index];
                        if (other.enabled && blueprint.support_branch_mask(other) == 0u
                            && other.pivot == candidate.c)
                        {
                            has_distal_manipulator = true;
                            break;
                        }
                    }
                    if (has_distal_manipulator)
                        continue;
                    const float reach = blueprint.torso_node < preview.size()
                        ? preview[candidate.c].x - preview[blueprint.torso_node].x
                        : preview[candidate.c].x;
                    if (reach > best_reach)
                    {
                        hand = candidate.c;
                        best_reach = reach;
                    }
                }
                if (hand < preview.size())
                {
                    const Vec2 anchor = preview_screen(hand);
                    const float weapon_length = std::clamp(scale
                        * (editor_weapon_class == sim::WeaponClass::sidearm ? 0.58f
                            : editor_weapon_class == sim::WeaponClass::carbine ? 0.92f
                            : 1.10f), 54.0f, 126.0f);
                    const float weapon_thickness = std::clamp(
                        weapon_length * 0.34f, 24.0f, 42.0f);
                    if (optional_art_enabled && optional_weapon_art.loaded())
                        draw_oriented_pixel_art(canvas, optional_weapon_art,
                            anchor - Vec2{ weapon_length * 0.12f, 0.0f },
                            anchor + Vec2{ weapon_length * 0.88f, 0.0f },
                            weapon_thickness, 0.98f, false, false, true);
                    else
                        canvas.line(anchor, anchor + Vec2{ weapon_length, 0.0f },
                            6.0f, accent);
                    const float target_x = std::clamp(
                        anchor.x + editor_target_distance * scale,
                        viewport.position.x + 46.0f,
                        viewport.position.x + viewport.size.x - 46.0f);
                    const Vec2 target{ target_x, anchor.y };
                    canvas.line(anchor + Vec2{ weapon_length * 0.70f, 0.0f },
                        target, 1.5f, with_alpha(accent, 0.34f));
                    canvas.circle(target, 18.0f, rgb(0x183746), 20);
                    canvas.circle(target, 10.0f, danger, 18);
                    canvas.circle(target, 4.0f, white, 14);
                    add_text_fit(canvas,
                        { viewport.position.x + viewport.size.x - 360.0f,
                          viewport.position.y + 44.0f },
                        std::format("{} FIXED-STEP PREVIEW   SHOTS {}   HITS {}",
                            sim::weapon_class_name(editor_weapon_class),
                            trainer.preview().shots_fired(),
                            trainer.preview().target_hits()),
                        0.72f, accent, 342.0f, 0.60f);
                }
            }
            const bool over_joint_lab = false;
            if (input.left_pressed && input.alt
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
            if (input.left_pressed && !input.alt
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
            if (dragging_node && input.left_down && !over_joint_lab && selected_node >= 0
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

        void draw_rig_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            canvas.push_clip(rect.position + Vec2{ 1.0f, 1.0f },
                rect.position + rect.size - Vec2{ 1.0f, 1.0f });
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const float usable = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            add_text_fit(canvas, cursor, "RIG LAB", 1.70f, white, usable, 1.10f);
            cursor.y += 38.0f;
            const float tab = (usable - 24.0f) * 0.20f;
            auto page_button = [&](int slot, std::string_view label, RigPanelPage page)
            {
                if (button({ cursor + Vec2{ static_cast<float>(slot) * (tab + 6.0f), 0.0f },
                    { tab, 35.0f } }, label, input, rig_panel_page == page))
                    rig_panel_page = page;
            };
            page_button(0, "PRESETS", RigPanelPage::presets);
            page_button(1, "STRUCTURE", RigPanelPage::structure);
            page_button(2, "MOTORS", RigPanelPage::motors);
            page_button(3, "ART", RigPanelPage::art);
            page_button(4, "TEST", RigPanelPage::test);
            cursor.y += 48.0f;

            if (rig_panel_page == RigPanelPage::presets)
            {
                add_text(canvas, cursor, "CANONICAL SIDE-VIEW RIGS", 1.02f, accent);
                cursor.y += 25.0f;
                const float half = (usable - 6.0f) * 0.5f;
                auto preset = [&](int row, int column, std::string_view label,
                    RigPreset value)
                {
                    const Vec2 position = cursor + Vec2{
                        static_cast<float>(column) * (half + 6.0f),
                        static_cast<float>(row) * 41.0f };
                    if (button({ position, { half, 35.0f } }, label, input,
                        rig_preset == value))
                        use_preset(value);
                };
                preset(0, 0, "HUMAN", RigPreset::humanoid);
                preset(0, 1, "CHICKEN", RigPreset::chicken);
                preset(1, 0, "DOG", RigPreset::crawler4);
                preset(1, 1, "HEXAPOD", RigPreset::hexapod);
                cursor.y += 94.0f;
                add_wrapped_text(canvas, cursor,
                    "Four unique production rigs are exposed. Legacy Biped, Quadruped, and Monoped files remain load-compatible; custom and evolved rigs remain supported.",
                    0.73f, muted, usable);
                cursor.y += 55.0f;
                const bool morphology_mode = autonomy.optimization_mode
                    == rl::RigOptimizationMode::morphology_evolve;
                if (button({ cursor, { half, 35.0f } }, "CONTROL OPTIMIZE", input,
                    !morphology_mode))
                {
                    trainer.set_rig_optimization_mode(
                        rl::RigOptimizationMode::control_optimize);
                    set_status("CONTROL OPTIMIZE SELECTED - ANATOMY LOCKED");
                }
                if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 35.0f } },
                    "MORPHOLOGY EVOLVE", input, morphology_mode))
                {
                    trainer.set_rig_optimization_mode(
                        rl::RigOptimizationMode::morphology_evolve);
                    set_status("MORPHOLOGY EVOLVE SELECTED - HELD-OUT CHANGES ONLY");
                }
                cursor.y += 48.0f;

                const float third = (usable - 12.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "SAVE RIG", input)
                    || input.save_pressed)
                {
                    const std::filesystem::path path = active_rig_path();
                    std::string error{};
                    set_status(blueprint.save(path, error)
                        ? std::format("{} SAVED", path.filename().string())
                        : error);
                }
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f },
                    { third, 35.0f } }, "LOAD RIG", input) || input.load_pressed)
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
                        blueprint = std::move(*candidate);
                        rig_preset = preset_for_species(
                            blueprint.presentation_species());
                        select_autosave_rig_file(blueprint);
                        trainer.set_blueprint(blueprint, false);
                        set_status(std::format("{} LOADED SAFELY - FRESH BALANCE LESSON",
                            path.filename().string()));
                    }
                    else
                    {
                        set_status(std::format("{} NOT LOADED - ACTIVE RIG UNCHANGED: {}",
                            path.filename().string(), error));
                    }
                }
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 35.0f } }, "COPY TRAINING RIG", input))
                {
                    blueprint = trainer.blueprint();
                    rig_preset = preset_for_species(blueprint.presentation_species());
                    select_autosave_rig_file(blueprint);
                    set_status(std::format("TRAINING {} COPIED TO EDITOR; SAVE TARGET {}",
                        preset_name(), active_rig_path().filename().string()));
                }
                cursor.y += 48.0f;
                if (button({ cursor, { half, 35.0f } }, "RESTORE RETAINED CONTROLLER",
                    input, trainer.has_best_policy(), trainer.has_best_policy()))
                {
                    set_status(trainer.restore_best_policy()
                        ? "RETAINED CONTROLLER RESTORE QUEUED"
                        : "NO RETAINED CONTROLLER AVAILABLE");
                }
                if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 35.0f } },
                    "START FRESH CONTROLLER", input))
                {
                    trainer.reset_policy(0x721300u
                        + autonomy.rig_generation * 0x9E3779B97F4A7C15ULL);
                    set_status("FRESH CONTROLLER QUEUED FOR UNCHANGED ANATOMY");
                }
                cursor.y += 48.0f;
                const float visual_third = (usable - 12.0f) / 3.0f;
                if (button({ cursor, { visual_third, 35.0f } },
                    right_leg_near ? "NEAR LEG: RIGHT" : "NEAR LEG: LEFT",
                    input, right_leg_near))
                    right_leg_near = !right_leg_near;
                if (button({ cursor + Vec2{ visual_third + 6.0f, 0.0f },
                    { visual_third, 35.0f } },
                    optional_art_enabled ? "ART: ON" : "ART: OFF",
                    input, optional_art_enabled))
                    optional_art_enabled = !optional_art_enabled;
                if (button({ cursor + Vec2{ (visual_third + 6.0f) * 2.0f, 0.0f },
                    { visual_third, 35.0f } },
                    debug_skeleton_overlay ? "SKELETON: ON" : "SKELETON: OFF",
                    input, debug_skeleton_overlay))
                    debug_skeleton_overlay = !debug_skeleton_overlay;
                cursor.y += 50.0f;
                add_text_fit(canvas, cursor,
                    std::format("{} {}   ACCEPTED {}   REJECTED {}   ROLLBACKS {}",
                        rl::rig_optimization_mode_name(autonomy.optimization_mode),
                        autonomy.rig_generation, autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes, autonomy.rollback_count),
                    0.74f, accent, usable, 0.56f);
            }
            else if (rig_panel_page == RigPanelPage::structure)
            {
                add_text(canvas, cursor, "MANUAL STRUCTURE EDITING", 1.02f, accent);
                cursor.y += 25.0f;
                add_wrapped_text(canvas, cursor,
                    "Edit anatomy directly here, or select Morphology Evolve on Presets for bounded held-out changes. Shift adds a node, Ctrl connects it, Alt selects a bone.",
                    0.72f, muted, usable, 2.0f);
                cursor.y += 80.0f;
                add_text_fit(canvas, cursor,
                    std::format("LIVE APPLY: {}   SIGNATURE {:016X}",
                        trainer.live_morphology_preview_active() ? "PREVIEW" : "COMMITTED",
                        blueprint.signature()),
                    0.70f, trainer.live_morphology_preview_active() ? yellow : green, usable);
                add_text_fit(canvas, cursor,
                    std::format("SELECTED NODE: {}", selected_node),
                    1.02f, white, usable - 120.0f);
                if (button({ cursor + Vec2{ usable - 110.0f, -5.0f },
                    { 110.0f, 32.0f } }, "DELETE NODE", input,
                    false, selected_node >= 0))
                    delete_selected_node();
                cursor.y += 36.0f;
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
                        0.68f, muted, usable);
                    cursor.y += 22.0f;
                    const float updated = slider({ cursor, { usable, 38.0f } },
                        "NODE SIZE", radius, 0.08f, 0.60f, input);
                    if (updated != radius)
                    {
                        radius = updated;
                        queue_rig_change("NODE SIZE UPDATED");
                    }
                    cursor.y += 52.0f;
                    add_text(canvas, cursor, "SEMANTIC ROLE", 0.92f, muted);
                    cursor.y += 22.0f;
                    const float role = (usable - 16.0f) * 0.20f;
                    auto set_role = [&](int slot, std::string_view label,
                        std::uint16_t& target)
                    {
                        if (button({ cursor + Vec2{ static_cast<float>(slot) * (role + 4.0f), 0.0f },
                            { role, 31.0f } }, label, input,
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
                    cursor.y += 45.0f;
                }
                add_text_fit(canvas, cursor,
                    std::format("SELECTED BONE: {}", selected_bone),
                    1.00f, white, usable - 120.0f);
                cursor.y += 34.0f;
                if (selected_bone >= 0
                    && static_cast<std::size_t>(selected_bone) < blueprint.bones.size())
                {
                    sim::DistanceConstraint& selected = blueprint.bones[
                        static_cast<std::size_t>(selected_bone)];
                    const float stiffness = slider({ cursor, { usable, 38.0f } },
                        "BONE STIFFNESS", selected.stiffness, 0.20f, 1.0f, input);
                    add_text_fit(canvas, cursor,
                        std::format("ENDPOINTS {}-{}   LENGTH {:.3f} M   STIFFNESS {:.2f}",
                            selected.a, selected.b,
                            runner::length(blueprint.nodes[selected.b] - blueprint.nodes[selected.a]),
                            selected.stiffness),
                        0.68f, muted, usable);
                    if (stiffness != selected.stiffness)
                    cursor.y += 22.0f;
                    {
                        selected.stiffness = stiffness;
                        queue_rig_change("BONE STIFFNESS UPDATED");
                    }
                    cursor.y += 52.0f;
                    if (button({ cursor, { usable, 34.0f } }, "DELETE SELECTED BONE", input))
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
            }
            else if (rig_panel_page == RigPanelPage::motors)
            {
                add_text(canvas, cursor, "MOTOR CHAINS", 1.02f, accent);
                cursor.y += 25.0f;
                const float quarter = (usable - 18.0f) * 0.25f;
                for (int index = 0; index < static_cast<int>(sim::anatomy_action_count); ++index)
                {
                    const int column = index % 4;
                    const int row = index / 4;
                    const bool available = static_cast<std::size_t>(index)
                        < blueprint.active_motor_count;
                    if (button({ cursor + Vec2{
                            static_cast<float>(column) * (quarter + 6.0f),
                            static_cast<float>(row) * 40.0f },
                        { quarter, 34.0f } }, std::format("MOTOR {}", index + 1),
                        input, selected_motor == index, available))
                    {
                        selected_motor = index;
                        joint_test_group = JointTestGroup::selected;
                    }
                }
                cursor.y += 88.0f;
                const auto names = motor_names();
                add_text_fit(canvas, cursor,
                    names[static_cast<std::size_t>(selected_motor)],
                    1.28f, white, usable, 0.90f);
                cursor.y += 31.0f;
                sim::MotorConstraint& motor = blueprint.motors[
                    static_cast<std::size_t>(selected_motor)];
                const float third = (usable - 12.0f) / 3.0f;
                auto endpoint = [&](int slot, std::string_view label,
                    std::uint16_t& value)
                {
                    if (!button({ cursor + Vec2{ static_cast<float>(slot) * (third + 6.0f), 0.0f },
                        { third, 34.0f } }, label, input, false, selected_node >= 0))
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
                cursor.y += 45.0f;
                const bool connected = motor.a < blueprint.nodes.size()
                    && motor.pivot < blueprint.nodes.size()
                    && motor.c < blueprint.nodes.size()
                    && motor.a != motor.pivot && motor.pivot != motor.c
                    && motor.a != motor.c
                    && has_direct_bone(motor.a, motor.pivot)
                    && has_direct_bone(motor.pivot, motor.c);
                add_text_fit(canvas, cursor,
                    std::format("PARENT {}   PIVOT {}   DRIVEN {}   {}",
                        motor.a, motor.pivot, motor.c,
                        connected ? (motor.enabled ? "READY" : "DISABLED")
                            : "NOT CONNECTED"),
                    0.78f, connected ? green : yellow, usable, 0.62f);
                cursor.y += 29.0f;
                const sim::MotorDiagnostic diagnostic =
                    trainer.preview().motor_diagnostic(
                        static_cast<std::size_t>(selected_motor));
                if (diagnostic.available)
                {
                    constexpr float radians_to_degrees = 180.0f / pi;
                    add_text_fit(canvas, cursor, std::format(
                        "LIVE {:+.1f} DEG  REST {:+.1f} DEG  TARGET {:+.1f} DEG",
                        diagnostic.current_angle * radians_to_degrees,
                        diagnostic.authored_neutral * radians_to_degrees,
                        diagnostic.target_angle * radians_to_degrees),
                        0.72f, trainer.live_morphology_preview_active()
                            ? green : accent, usable, 0.56f);
                    cursor.y += 22.0f;
                    add_text_fit(canvas, cursor, std::format(
                        "ACTION {:+.2f}  VEL {:+.1f} DEG/S  SLOT {}  {}",
                        diagnostic.applied_action,
                        diagnostic.angular_velocity * radians_to_degrees,
                        diagnostic.action_slot + 1u,
                        diagnostic.support_branch ? "SUPPORT" : "MANIPULATOR"),
                        0.70f, muted, usable, 0.54f);
                    cursor.y += 25.0f;
                }
                float negative = (motor.neutral_angle - motor.minimum_angle)
                    * 180.0f / pi;
                float positive = (motor.maximum_angle - motor.neutral_angle)
                    * 180.0f / pi;
                const float updated_negative = slider({ cursor, { usable, 38.0f } },
                    "NEGATIVE RANGE", negative, 2.0f, 120.0f, input, " DEG");
                if (updated_negative != negative)
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, positive, motor.strength);
                    queue_rig_change("NEGATIVE RANGE UPDATED");
                }
                cursor.y += 50.0f;
                const float updated_positive = slider({ cursor, { usable, 38.0f } },
                    "POSITIVE RANGE", positive, 2.0f, 120.0f, input, " DEG");
                if (updated_positive != positive)
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, updated_positive, motor.strength);
                    queue_rig_change("POSITIVE RANGE UPDATED");
                }
                cursor.y += 50.0f;
                const float power = slider({ cursor, { usable, 38.0f } },
                    "MOTOR POWER", motor.strength, 0.0f, 0.20f, input);
                if (power != motor.strength)
                {
                    motor.strength = power;
                    queue_rig_change("MOTOR POWER UPDATED");
                }
                cursor.y += 53.0f;
                if (button({ cursor, { third, 34.0f } }, "SET REST", input,
                    false, connected))
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, updated_positive, motor.strength);
                    apply_small_rig_change("REST POSE RECALIBRATED");
                }
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 34.0f } },
                    "SAFE RANGE", input, false, connected))
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        18.0f, 20.0f, 0.050f);
                    apply_small_rig_change("SAFE MOTOR DEFAULT APPLIED");
                }
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 34.0f } }, motor.enabled ? "DISABLE" : "ENABLE",
                    input, motor.enabled, connected))
                {
                    motor.enabled = !motor.enabled;
                    apply_small_rig_change(motor.enabled
                        ? "MOTOR ENABLED" : "MOTOR DISABLED");
                }
            }
            else if (rig_panel_page == RigPanelPage::art)
            {
                add_text(canvas, cursor, "LIVE MODULE ART AUTHORING", 1.02f, accent);
                cursor.y += 27.0f;
                const float quarter = (usable - 18.0f) * 0.25f;
                if (button({ cursor, { quarter, 33.0f } },
                    optional_art_enabled ? "ART: ON" : "ART: OFF", input,
                    optional_art_enabled))
                    optional_art_enabled = !optional_art_enabled;
                if (button({ cursor + Vec2{ quarter + 6.0f, 0.0f },
                    { quarter, 33.0f } },
                    debug_skeleton_overlay ? "RIG: ON" : "RIG: OFF", input,
                    debug_skeleton_overlay))
                    debug_skeleton_overlay = !debug_skeleton_overlay;
                if (button({ cursor + Vec2{ (quarter + 6.0f) * 2.0f, 0.0f },
                    { quarter, 33.0f } }, art_preview_frozen ? "PLAY" : "FREEZE",
                    input, art_preview_frozen))
                {
                    art_preview_frozen = !art_preview_frozen;
                    if (art_preview_frozen)
                        frozen_art_environment = trainer.preview();
                    else
                        frozen_art_environment.reset();
                }
                if (button({ cursor + Vec2{ (quarter + 6.0f) * 3.0f, 0.0f },
                    { quarter, 33.0f } }, art_editor_high_contrast
                        ? "BG: BLACK" : "BG: WORLD", input,
                    art_editor_high_contrast))
                    art_editor_high_contrast = !art_editor_high_contrast;
                cursor.y += 43.0f;

                constexpr std::array modules{
                    art::Module::body, art::Module::head,
                    art::Module::tail, art::Module::equipment,
                    art::Module::upper_limb, art::Module::lower_limb,
                    art::Module::terminal, art::Module::hand
                };
                for (std::size_t index = 0u; index < modules.size(); ++index)
                {
                    const float x = static_cast<float>(index % 4u)
                        * (quarter + 6.0f);
                    const float y = static_cast<float>(index / 4u) * 35.0f;
                    if (button({ cursor + Vec2{ x, y }, { quarter, 29.0f } },
                        art::module_name(modules[index]), input,
                        selected_art_module == modules[index]))
                        selected_art_module = modules[index];
                }
                cursor.y += 77.0f;
                art::Layout& layout = active_art_layout();
                art::ModuleAdjustment& module = layout.at(selected_art_module);
                add_text_fit(canvas, cursor,
                    std::format("{} / {}   PIVOT {:.2f}   DEPTH {:+d}",
                        sim::creature_species_name(layout.species),
                        art::module_name(selected_art_module), module.pivot,
                        static_cast<int>(module.layer)),
                    0.72f, green, usable, 0.58f);
                cursor.y += 25.0f;
                auto update = [&](float& target, float value)
                {
                    if (std::abs(target - value) <= 1.0e-5f)
                        return;
                    target = value;
                    art_edit_pending = true;
                };
                float value = slider({ cursor, { usable, 34.0f } },
                    "ANCHOR ALONG", module.anchor_along,
                    -0.30f, 0.30f, input);
                update(module.anchor_along, value);
                cursor.y += 43.0f;
                value = slider({ cursor, { usable, 34.0f } },
                    "ANCHOR NORMAL", module.anchor_normal,
                    -0.30f, 0.30f, input);
                update(module.anchor_normal, value);
                cursor.y += 43.0f;
                value = slider({ cursor, { usable, 34.0f } },
                    "PIVOT", module.pivot, 0.0f, 1.0f, input);
                update(module.pivot, value);
                cursor.y += 43.0f;
                value = slider({ cursor, { usable, 34.0f } },
                    "LENGTH", module.length_scale, 0.65f, 1.35f, input);
                update(module.length_scale, value);
                cursor.y += 43.0f;
                value = slider({ cursor, { usable, 34.0f } },
                    "THICKNESS", module.thickness_scale, 0.65f, 1.35f, input);
                update(module.thickness_scale, value);
                cursor.y += 43.0f;
                const float rotation = angle_slider(
                    { cursor, { usable, 34.0f } }, "ROTATION", module.rotation,
                    -35.0f, 35.0f, input);
                update(module.rotation, rotation);
                cursor.y += 43.0f;
                const float prior_zoom = art_editor_zoom;
                art_editor_zoom = slider({ cursor, { usable, 34.0f } },
                    "EDITOR ZOOM", art_editor_zoom, 0.65f, 1.80f, input);
                if (std::abs(prior_zoom - art_editor_zoom) > 1.0e-5f)
                    frozen_art_environment = art_preview_frozen
                        ? std::optional<sim::Environment>{ trainer.preview() }
                        : std::nullopt;
                cursor.y += 45.0f;
                const float fifth = (usable - 24.0f) * 0.20f;
                auto commit_now = [&](std::string_view message)
                {
                    active_art_history().commit(layout);
                    set_status(std::string{ message });
                };
                if (button({ cursor, { fifth, 31.0f } },
                    module.flip_vertical ? "FLIP V: ON" : "FLIP V", input,
                    module.flip_vertical))
                {
                    module.flip_vertical = !module.flip_vertical;
                    commit_now("ART VERTICAL FLIP UPDATED");
                }
                if (button({ cursor + Vec2{ fifth + 6.0f, 0.0f },
                    { fifth, 31.0f } }, module.flip_horizontal
                        ? "FLIP H: ON" : "FLIP H", input,
                    module.flip_horizontal))
                {
                    module.flip_horizontal = !module.flip_horizontal;
                    commit_now("ART HORIZONTAL FLIP UPDATED");
                }
                if (button({ cursor + Vec2{ (fifth + 6.0f) * 2.0f, 0.0f },
                    { fifth, 31.0f } }, "DEPTH -", input, false,
                    module.layer > -2))
                {
                    --module.layer;
                    commit_now("ART DEPTH UPDATED");
                }
                if (button({ cursor + Vec2{ (fifth + 6.0f) * 3.0f, 0.0f },
                    { fifth, 31.0f } }, "DEPTH +", input, false,
                    module.layer < 2))
                {
                    ++module.layer;
                    commit_now("ART DEPTH UPDATED");
                }
                if (button({ cursor + Vec2{ (fifth + 6.0f) * 4.0f, 0.0f },
                    { fifth, 31.0f } }, "RESET", input))
                {
                    module = {};
                    commit_now("SELECTED ART MODULE RESET");
                }
                cursor.y += 40.0f;
                const float third = (usable - 12.0f) / 3.0f;
                if (button({ cursor, { third, 31.0f } }, "UNDO", input,
                    false, active_art_history().can_undo()))
                    static_cast<void>(active_art_history().undo(layout));
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f },
                    { third, 31.0f } }, "REDO", input,
                    false, active_art_history().can_redo()))
                    static_cast<void>(active_art_history().redo(layout));
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 31.0f } }, right_leg_near
                        ? "NEAR: RIGHT" : "NEAR: LEFT", input,
                    right_leg_near))
                    right_leg_near = !right_leg_near;
                cursor.y += 40.0f;
                const float half = (usable - 6.0f) * 0.5f;
                if (button({ cursor, { half, 31.0f } }, "SAVE LAYOUT", input))
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
                    { half, 31.0f } }, "LOAD LAYOUT", input))
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
                cursor.y += 40.0f;
                add_text_fit(canvas, cursor,
                    std::format("CONTACT SLIP {:.3f} M   WATER {}   LIVE STATS RETAINED",
                        trainer.preview().stance_slip_distance(),
                        sim::water_traversal_phase_name(
                            trainer.preview().water_traversal_phase())),
                    0.75f, green, usable, 0.62f);
            }
            else
            {
                add_text(canvas, cursor, "JOINT AND TRACTION TESTS", 1.02f, accent);
                cursor.y += 28.0f;
                const Rect test_card{ cursor, { usable, 225.0f } };
                draw_joint_lab(test_card, input);
                cursor.y += 240.0f;
                add_text(canvas, cursor, "EQUIPMENT TEST AUTHORING", 0.92f, accent);
                cursor.y += 25.0f;
                const float weapon_width = (usable - 18.0f) * 0.25f;
                const std::array weapon_choices{ sim::WeaponClass::none,
                    sim::WeaponClass::sidearm, sim::WeaponClass::carbine,
                    sim::WeaponClass::launcher };
                for (std::size_t index = 0; index < weapon_choices.size(); ++index)
                {
                    const sim::WeaponClass weapon = weapon_choices[index];
                    if (button({ cursor + Vec2{ static_cast<float>(index)
                            * (weapon_width + 6.0f), 0.0f },
                            { weapon_width, 32.0f } },
                        sim::weapon_class_name(weapon), input,
                        editor_weapon_class == weapon))
                    {
                        editor_weapon_class = weapon;
                        trainer.configure_preview_equipment(
                            editor_weapon_class, editor_target_distance);
                    }
                }
                cursor.y += 43.0f;
                const float previous_target_distance = editor_target_distance;
                editor_target_distance = slider({ cursor, { usable, 36.0f } },
                    "TARGET DISTANCE", editor_target_distance, 3.0f, 24.0f,
                    input, " M");
                if (std::abs(editor_target_distance - previous_target_distance) > 1.0e-4f)
                    trainer.configure_preview_equipment(
                        editor_weapon_class, editor_target_distance);
                cursor.y += 49.0f;
                add_wrapped_text(canvas, cursor,
                    "GAIT, handling class, and target distance are side-view authoring checks. Test controls never change the saved training policy.",
                    0.72f, muted, usable, 2.0f);
            }
            canvas.pop_clip();
            add_rounded_rect(canvas, rect, 11.0f, ui_render::transparent_fill, border, 1.0f);
        }

        void process_shortcuts(const InputState& input)
        {
            if (input.tab_pressed)
                mode = mode == Mode::live ? Mode::rig_lab : Mode::live;
            if (input.key_1_pressed) trainer.set_updates_per_cycle(1);
            if (input.key_2_pressed) trainer.set_updates_per_cycle(2);
            if (input.key_3_pressed) trainer.set_updates_per_cycle(4);
            if (input.totals_pressed)
            {
                switch (live_panel_page)
                {
                case LivePanelPage::summary:
                    live_panel_page = LivePanelPage::totals;
                    break;
                case LivePanelPage::totals:
                    live_panel_page = LivePanelPage::advanced;
                    break;
                case LivePanelPage::advanced:
                    live_panel_page = LivePanelPage::summary;
                    break;
                }
            }
            if (input.units_pressed)
                distance_units = distance_units == ui_layout::DistanceUnits::metric
                    ? ui_layout::DistanceUnits::imperial : ui_layout::DistanceUnits::metric;
            if (input.art_pressed)
                optional_art_enabled = !optional_art_enabled;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
            if (input.reset_pressed)
            {
                trainer.reset_preview();
                camera_x = 0.0f;
                live_pixels_per_meter = view_camera::default_pixels_per_meter;
                live_zoom_factor = 1.0f;
                live_zoom_auto = true;
            }
            if (input.delete_pressed && mode == Mode::rig_lab)
                delete_selected_node();
        }

        void frame(const InputState& input, float dt, int width, int height)
        {
            trainer.synchronize();
            canvas.clear();
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
                    "WINDOW TOO SMALL - MINIMUM WINDOW 1280 X 820", 2.0f, danger);
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
                const ui_layout::Box layout_live =
                    ui_layout::rig_lab_live_box(layout_content);
                const ui_layout::Box layout_trainer =
                    ui_layout::rig_lab_trainer_panel_box(layout_content);
                const ui_layout::Box layout_side =
                    ui_layout::rig_lab_panel_box(layout_content);
                const ui_layout::Box layout_world =
                    ui_layout::rig_lab_world_box(layout_content);
                const Rect side{ { layout_side.x, layout_side.y },
                    { layout_side.width, layout_side.height } };
                const Rect trainer_panel{ { layout_trainer.x, layout_trainer.y },
                    { layout_trainer.width, layout_trainer.height } };
                const Rect world{ { layout_world.x, layout_world.y },
                    { layout_world.width, layout_world.height } };
                if (ui_layout::rig_lab_shows_live(layout_content))
                {
                    const Rect live{ { layout_live.x, layout_live.y },
                        { layout_live.width, layout_live.height } };
                    draw_live_world(live, dt, input);
                }
                else
                {
                    trainer.step_preview(dt);
                }
                draw_live_panel(trainer_panel, input);
                draw_rig_panel(side, input);
                add_rounded_rect(canvas, world, 11.0f, rgb(0x0a131d), border, 1.0f);
                canvas.push_clip(world.position + Vec2{ 1.0f, 1.0f },
                    world.position + world.size - Vec2{ 1.0f, 1.0f });
                if (rig_panel_page == RigPanelPage::art)
                    draw_art_editor_world(world);
                else
                    draw_blueprint(world, input);
                canvas.pop_clip();
                if (rig_panel_page != RigPanelPage::art)
                    add_text_fit(canvas, world.position + Vec2{ 18.0f, 16.0f },
                        "SIDE VIEW   DRAG NODE   SHIFT ADD   CTRL CONNECT   ALT SELECT BONE",
                        0.80f, muted, world.size.x - 36.0f, 0.68f);
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
                const float scale = 1.30f;
                const Vec2 measured = font::measure_text(status, scale);
                const Rect toast{ { 20.0f, static_cast<float>(height) - 58.0f },
                    { std::min(measured.x + 30.0f, static_cast<float>(width) - 40.0f), 38.0f } };
                add_rounded_rect(canvas, toast, 8.0f, rgb(0x10202b, 0.97f), accent, 1.0f);
                add_text(canvas, toast.position + Vec2{ 14.0f, 10.0f }, status, 1.30f, white);
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
        std::string& error)
    {
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
        impl_->optional_art_enabled = impl_->optional_foot_art.loaded()
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

        impl_->trainer.set_autosave_paths(impl_->autosave_policy_path,
            impl_->autosave_rig_path, impl_->autosave_state_path);
        std::string message{};
        const bool resume_queued = impl_->trainer.load_autosave(message);
        impl_->trainer.synchronize();
        impl_->blueprint = impl_->trainer.blueprint();
        if (!resume_queued)
            message = std::format("{} - AUTOPILOT READY",
                impl_->startup_rig.source_note);
        impl_->rig_preset = Impl::preset_for_species(
            impl_->blueprint.presentation_species());
        impl_->select_autosave_rig_file(impl_->blueprint);
        impl_->trainer.set_background_enabled(true);
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
        impl_->art_eye_test = false;
        impl_->walk_eye_test = false;
        impl_->course_eye_test_environment.emplace(
            sim::CreatureBlueprint::humanoid(), 728314u);
        impl_->course_eye_test_environment->set_course(
            sim::CourseStage::uneven, 0.65f);
        impl_->course_eye_test_environment->set_course_motion_enabled(false);
        impl_->run_paused = true;
        impl_->camera_x = 18.5f;
        impl_->live_pixels_per_meter = view_camera::minimum_pixels_per_meter;
        impl_->live_zoom_factor = view_camera::minimum_zoom_factor;
        impl_->live_zoom_auto = false;
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
        impl_->art_eye_test = true;
        impl_->walk_eye_test = false;
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
        // Keep every subject on the immutable Human world scale. This value
        // frames the full 4.8 m Human in the production viewport without
        // normalizing compact animals up to Human screen height.
        impl_->live_pixels_per_meter = 84.0f;
        impl_->live_zoom_factor = 1.0f;
        impl_->live_zoom_auto = false;
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
    bool Application::prepare_walk_eye_test(std::string& error)
    {
        impl_->trainer.set_background_enabled(false);
        impl_->trainer.synchronize();
        impl_->art_eye_test = false;
        impl_->walk_eye_test = false;
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
        impl_->walk_eye_test = true;
        impl_->walk_eye_accumulator_seconds = 0.0;
        impl_->run_paused = false;
        const auto& particles = impl_->course_eye_test_environment->particles();
        const std::size_t root = impl_->course_eye_test_environment->blueprint().root_node;
        impl_->camera_x = root < particles.size()
            ? particles[root].position.x + 2.5f : proof.displayed_distance;
        impl_->live_pixels_per_meter = 105.0f;
        impl_->live_zoom_factor = 1.0f;
        impl_->live_zoom_auto = false;
        impl_->debug_skeleton_overlay = false;
        impl_->status = "PACKAGED RETAINED WALK PROOF - FRESH TRAINING, TEACHER 0 / REFLEX ASSISTED";
        impl_->status_time = 30.0f;
        error.clear();
        return true;
    }
    void Application::frame(const InputState& input, float dt, int width, int height)
    {
        impl_->frame(input, dt, width, height);
    }

    std::span<const render::Vertex> Application::vertices() const noexcept
    {
        return impl_->canvas.vertices();
    }

    bool Application::wants_quit() const noexcept
    {
        return impl_->quit;
    }
}
