#include "app.hpp"
#include "autonomy.hpp"
#include "pixel_art.hpp"
#include "simulation.hpp"
#include "ui_layout.hpp"
#include "ui_font.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
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
        constexpr float ui_font_scale = 2.05f;

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

        void fill_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color color)
        {
            if (rect.size.x <= 0.0f || rect.size.y <= 0.0f)
                return;
            radius = std::clamp(radius, 0.0f,
                std::min(rect.size.x, rect.size.y) * 0.5f);
            if (radius <= 0.0f)
            {
                canvas.quad(rect.position, rect.position + rect.size, color);
                return;
            }
            const Vec2 minimum = rect.position;
            const Vec2 maximum = rect.position + rect.size;
            canvas.quad({ minimum.x + radius, minimum.y },
                { maximum.x - radius, maximum.y }, color);
            canvas.quad({ minimum.x, minimum.y + radius },
                { maximum.x, maximum.y - radius }, color);
            canvas.circle({ minimum.x + radius, minimum.y + radius }, radius, color, 12);
            canvas.circle({ maximum.x - radius, minimum.y + radius }, radius, color, 12);
            canvas.circle({ minimum.x + radius, maximum.y - radius }, radius, color, 12);
            canvas.circle({ maximum.x - radius, maximum.y - radius }, radius, color, 12);
        }

        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill,
            Color outline = {}, float border_width = 0.0f)
        {
            if (border_width <= 0.0f)
            {
                fill_rounded_rect(canvas, rect, radius, fill);
                return;
            }
            fill_rounded_rect(canvas, rect, radius, outline);
            const float inset = std::clamp(border_width, 0.0f,
                std::min(rect.size.x, rect.size.y) * 0.5f);
            fill_rounded_rect(canvas,
                { rect.position + Vec2{ inset, inset },
                  rect.size - Vec2{ inset * 2.0f, inset * 2.0f } },
                std::max(0.0f, radius - inset), fill);
        }

        void add_text(render::Canvas& canvas, Vec2 position, std::string_view text, float scale, Color color)
        {
            scale *= ui_font_scale;
            Vec2 cursor = position;
            const float start_x = position.x;
            for (const char character : text)
            {
                if (character == '\n')
                {
                    cursor.x = start_x;
                    cursor.y += static_cast<float>(font::line_advance) * scale;
                    continue;
                }
                const font::BitmapGlyph glyph = font::default_glyph(character);
                for (std::uint32_t row = 0; row < font::glyph_height; ++row)
                {
                    for (std::uint32_t column = 0; column < font::glyph_width; ++column)
                    {
                        if (!font::pixel_on(glyph, column, row))
                            continue;
                        const Vec2 minimum{
                            cursor.x + static_cast<float>(column) * scale,
                            cursor.y + static_cast<float>(row) * scale
                        };
                        canvas.quad(minimum, minimum + Vec2{ scale, scale }, color);
                    }
                }
                cursor.x += static_cast<float>(font::glyph_advance) * scale;
            }
        }

        [[nodiscard]] float fit_text_scale(std::string_view text, float requested_scale,
            float maximum_width, float minimum_scale = 1.05f) noexcept
        {
            float scale = requested_scale;
            while (scale > minimum_scale
                && font::measure_text(text, scale * ui_font_scale).x > maximum_width)
                scale -= 0.05f;
            return std::max(scale, minimum_scale);
        }

        void add_text_fit(render::Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float maximum_width, float minimum_scale = 1.05f)
        {
            add_text(canvas, position, text,
                fit_text_scale(text, scale, maximum_width, minimum_scale), color);
        }

        float add_wrapped_text(render::Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float maximum_width, float line_gap = 5.0f)
        {
            const float advance = static_cast<float>(font::line_advance) * scale * ui_font_scale + line_gap;
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
                    && font::measure_text(candidate, scale * ui_font_scale).x > maximum_width)
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

        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera_x,
            float pixels_per_meter, float ground_fraction = 0.72f) noexcept
        {
            const float ground_y = viewport.position.y + viewport.size.y * ground_fraction;
            return {
                viewport.position.x + viewport.size.x * 0.50f + (world.x - camera_x) * pixels_per_meter,
                ground_y - world.y * pixels_per_meter
            };
        }

        [[nodiscard]] Vec2 screen_to_world(Vec2 screen, Rect viewport, float camera_x,
            float pixels_per_meter) noexcept
        {
            const float ground_y = viewport.position.y + viewport.size.y * 0.72f;
            return {
                camera_x + (screen.x - (viewport.position.x + viewport.size.x * 0.50f)) / pixels_per_meter,
                (ground_y - screen.y) / pixels_per_meter
            };
        }
    }

    struct Application::Impl
    {
        enum class Mode : std::uint8_t { live, rig_lab };
        enum class RigPreset : std::uint8_t {
            humanoid, biped, chicken, quadruped, crawler4, hexapod, monoped, custom
        };
        enum class RigPanelPage : std::uint8_t { body, motor };
        enum class LivePanelPage : std::uint8_t { results, totals };
        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };

        render::Canvas canvas{};
        sim::CreatureBlueprint blueprint{ sim::CreatureBlueprint::humanoid() };
        rl::AutonomousTrainer trainer{ blueprint, 64 };
        Mode mode{ Mode::live };
        RigPreset rig_preset{ RigPreset::humanoid };
        JointTestGroup joint_test_group{ JointTestGroup::selected };
        RigPanelPage rig_panel_page{ RigPanelPage::body };
        LivePanelPage live_panel_page{ LivePanelPage::results };
        int selected_node{ -1 };
        int selected_motor{};
        bool dragging_node{};
        bool joint_auto_sweep{};
        bool run_paused{};
        ui_layout::DistanceUnits distance_units{ ui_layout::DistanceUnits::metric };
        float session_runtime_seconds{};
        float rig_lifetime_seconds{};
        std::uint64_t tracked_rig_signature{};
        std::uint64_t rig_start_update{};
        std::uint64_t rig_start_environment_steps{};
        std::uint64_t rig_start_episodes{};
        std::uint64_t rig_start_valid_episodes{};
        std::uint64_t rig_start_invalid_episodes{};
        std::uint64_t rig_start_steps{};
        std::uint64_t rig_start_falls{};
        std::uint64_t rig_start_collisions{};
        std::uint64_t rig_start_jumps{};
        std::uint64_t rig_start_landings{};
        std::uint64_t rig_start_flips{};
        std::uint64_t rig_start_obstacles{};
        double rig_start_distance{};
        double rig_start_training_seconds{};
        std::uint64_t rig_start_accepted_rigs{};
        std::uint64_t rig_start_rejected_rigs{};
        std::uint64_t rig_start_rollbacks{};
        std::uint64_t session_start_environment_steps{};
        std::uint64_t session_start_episodes{};
        std::uint64_t session_start_invalid_episodes{};
        std::uint64_t session_start_resets{};
        std::uint64_t session_start_collisions{};
        std::uint64_t session_start_jumps{};
        std::uint64_t session_start_flips{};
        std::uint64_t session_start_obstacles{};
        double session_start_distance{};
        double session_start_training_seconds{};
        std::uint64_t session_start_accepted_rigs{};
        std::uint64_t session_start_rejected_rigs{};
        std::uint64_t session_start_rollbacks{};
        std::uint8_t rig_best_stage{};
        bool session_stats_initialized{};
        bool rig_edit_pending{};
        std::string rig_edit_reason{};
        float joint_test_input{};
        float joint_test_phase{};
        float camera_x{};
        art::PixelArt original_runner_art{};
        std::string status{ "AUTOPILOT STARTING" };
        float status_time{ 4.0f };
        bool quit{};
        std::filesystem::path rig_path{ "creature.rig" };
        std::filesystem::path policy_path{ "creature.eppo" };
        std::filesystem::path autosave_policy_path{ "runner-v0715-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v0715-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v0715-autonomy.state" };

        [[nodiscard]] std::string_view preset_name() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::humanoid: return "HUMANOID";
            case RigPreset::biped: return "BASIC BIPED";
            case RigPreset::chicken: return "CHICKEN BIPED";
            case RigPreset::quadruped: return "QUADRUPED";
            case RigPreset::crawler4: return "FOUR-LEG CRAWLER";
            case RigPreset::hexapod: return "SIX-LEG HEXAPOD";
            case RigPreset::monoped: return "MONOPED";
            case RigPreset::custom: return "CUSTOM / EVOLVED";
            }
            return "CUSTOM / EVOLVED";
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

        [[nodiscard]] bool has_direct_bone(std::uint16_t a, std::uint16_t b) const noexcept
        {
            return std::ranges::any_of(blueprint.bones, [a, b](const sim::DistanceConstraint& bone)
            {
                return (bone.a == a && bone.b == b) || (bone.a == b && bone.b == a);
            });
        }

        void queue_rig_change(std::string_view reason)
        {
            rig_edit_pending = true;
            rig_edit_reason = reason;
        }

        [[nodiscard]] std::array<std::string_view, sim::action_count> motor_names() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::quadruped:
                return { "REAR NEAR LEG", "REAR FAR LEG", "FRONT NEAR LEG", "FRONT FAR LEG",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::crawler4:
                return { "REAR LEG", "MID-REAR LEG", "MID-FRONT LEG", "FRONT LEG",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::hexapod:
                return { "REAR PAIR A", "REAR PAIR B", "MID PAIR", "FRONT PAIR",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::monoped:
                return { "HIP", "KNEE", "LEFT FOOT", "RIGHT FOOT",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::humanoid:
                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE",
                    "LEFT SHOULDER", "LEFT ELBOW", "RIGHT SHOULDER", "RIGHT ELBOW" };
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

            float scale = 1.55f;
            Vec2 measured = font::measure_text(label, scale * ui_font_scale);
            while (measured.x > rect.size.x - 12.0f && scale > 1.05f)
            {
                scale -= 0.08f;
                measured = font::measure_text(label, scale * ui_font_scale);
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
            rig_preset = preset;
            switch (preset)
            {
            case RigPreset::humanoid: blueprint = sim::CreatureBlueprint::humanoid(); break;
            case RigPreset::biped: blueprint = sim::CreatureBlueprint::biped(); break;
            case RigPreset::chicken: blueprint = sim::CreatureBlueprint::chicken(); break;
            case RigPreset::quadruped: blueprint = sim::CreatureBlueprint::quadruped(); break;
            case RigPreset::crawler4: blueprint = sim::CreatureBlueprint::crawler4(); break;
            case RigPreset::hexapod: blueprint = sim::CreatureBlueprint::hexapod(); break;
            case RigPreset::monoped: blueprint = sim::CreatureBlueprint::monoped(); break;
            case RigPreset::custom: break;
            }
            selected_node = -1;
            selected_motor = 0;
            dragging_node = false;
            trainer.set_blueprint(blueprint, false);
            set_status(std::format("{} LOADED - AUTOPILOT STARTED A FRESH BALANCE LESSON", preset_name()));
        }

        void apply_small_rig_change(std::string_view reason)
        {
            blueprint.rebuild_rest_lengths();
            if (!blueprint.valid())
            {
                set_status("RIG CHANGE REJECTED - CONNECT EACH ENABLED MOTOR THROUGH TWO REAL BONES");
                return;
            }
            rig_preset = RigPreset::custom;
            trainer.set_blueprint(blueprint, true);
            set_status(std::format("{} - QUEUED; TRAINER RECALIBRATES WITHOUT BLOCKING", reason));
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
            add_text(canvas, { 18.0f, 13.0f }, "RUNNER v" RUNNER_VERSION, 2.10f, white);
            if (width >= 1080)
                add_text(canvas, { 20.0f, 50.0f }, "AUTONOMOUS PHYSICS LOCOMOTION LAB", 1.05f, muted);

            const float tab_width = width >= 1080 ? 184.0f : 164.0f;
            const float start_x = static_cast<float>(width) - tab_width * 2.0f - 18.0f;
            if (button({ { start_x, 16.0f }, { tab_width - 7.0f, 50.0f } },
                "LIVE AUTOPILOT", input, mode == Mode::live))
                mode = Mode::live;
            if (button({ { start_x + tab_width, 16.0f }, { tab_width - 7.0f, 50.0f } },
                "RIG LAB", input, mode == Mode::rig_lab))
                mode = Mode::rig_lab;
        }

        void draw_course_ground(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const sim::DeformableTerrain& terrain = environment.terrain();
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - sim::DeformableTerrain::macro_tile_size;
            const float right = camera + half_view + sim::DeformableTerrain::macro_tile_size;
            const int first_macro = static_cast<int>(std::floor(
                left / sim::DeformableTerrain::macro_tile_size));
            const int last_macro = static_cast<int>(std::ceil(
                right / sim::DeformableTerrain::macro_tile_size));

            auto material_color = [](sandhybrid::Material material)
            {
                const sandhybrid::Rgb8 source = sandhybrid::material_editor_color(
                    static_cast<std::uint32_t>(material));
                const std::uint32_t packed = (static_cast<std::uint32_t>(source.r) << 16u)
                    | (static_cast<std::uint32_t>(source.g) << 8u)
                    | static_cast<std::uint32_t>(source.b);
                return rgb(packed);
            };
            auto draw_world_cell = [&](float x0, float y0, float x1, float y1,
                sandhybrid::Material material)
            {
                const Vec2 minimum = world_to_screen({ x0, y0 }, viewport, camera, scale);
                const Vec2 maximum = world_to_screen({ x1, y1 }, viewport, camera, scale);
                canvas.quad({ minimum.x, maximum.y }, { maximum.x, minimum.y },
                    material_color(material));
            };

            for (int world_macro = first_macro; world_macro <= last_macro; ++world_macro)
            {
                const auto wrapped_macro = static_cast<std::size_t>((
                    world_macro % static_cast<int>(sim::DeformableTerrain::macro_columns)
                    + static_cast<int>(sim::DeformableTerrain::macro_columns))
                    % static_cast<int>(sim::DeformableTerrain::macro_columns));
                const float macro_x0 = static_cast<float>(world_macro)
                    * sim::DeformableTerrain::macro_tile_size;
                for (std::size_t macro_y = 0;
                    macro_y < sim::DeformableTerrain::macro_rows; ++macro_y)
                {
                    const sim::DeformableTerrain::MacroTile& tile =
                        terrain.macro_tile(wrapped_macro, macro_y);
                    if (tile.occupied_mask == 0u)
                        continue;
                    const float macro_y0 = sim::DeformableTerrain::world_bottom
                        + static_cast<float>(macro_y)
                            * sim::DeformableTerrain::macro_tile_size;
                    const float macro_y1 = macro_y0
                        + sim::DeformableTerrain::macro_tile_size;
                    bool near_surface = false;
                    for (std::size_t local_x = 0;
                        local_x < sim::DeformableTerrain::macro_cell_side; ++local_x)
                    {
                        const float sample_x = macro_x0
                            + (static_cast<float>(local_x) + 0.5f)
                                * sim::DeformableTerrain::fine_cell_spacing;
                        if (macro_y1 >= environment.ground_height_at(sample_x)
                            - sim::DeformableTerrain::fine_cell_spacing * 3.0f)
                        {
                            near_surface = true;
                            break;
                        }
                    }
                    if (tile.macro_ready && !tile.active && !near_surface)
                    {
                        draw_world_cell(macro_x0, macro_y0,
                            macro_x0 + sim::DeformableTerrain::macro_tile_size,
                            macro_y1, tile.uniform_material);
                        continue;
                    }

                    for (std::size_t local_y = 0;
                        local_y < sim::DeformableTerrain::macro_cell_side; ++local_y)
                    {
                        for (std::size_t local_x = 0;
                            local_x < sim::DeformableTerrain::macro_cell_side; ++local_x)
                        {
                            const std::size_t fine_x = wrapped_macro
                                * sim::DeformableTerrain::macro_cell_side + local_x;
                            const std::size_t fine_y = macro_y
                                * sim::DeformableTerrain::macro_cell_side + local_y;
                            const sim::DeformableTerrain::FineCell& cell =
                                terrain.fine_cell(fine_x, fine_y);
                            if (!cell.occupied())
                                continue;
                            const float x0 = macro_x0 + static_cast<float>(local_x)
                                * sim::DeformableTerrain::fine_cell_spacing;
                            const float y0 = sim::DeformableTerrain::row_world_bottom(fine_y);
                            draw_world_cell(x0, y0,
                                x0 + sim::DeformableTerrain::fine_cell_spacing,
                                y0 + sim::DeformableTerrain::fine_cell_spacing * cell.fill,
                                cell.material());
                        }
                    }
                }
            }

        }

        void draw_course_reference(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const float progress = environment.course_progress();
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - 2.0f;
            const float right = camera + half_view + 2.0f;


            const float marker_spacing = ui_layout::course_reference_marker_spacing_m(distance_units);
            const int first_marker = static_cast<int>(std::floor((left + progress) / marker_spacing));
            const int last_marker = static_cast<int>(std::ceil((right + progress) / marker_spacing));
            for (int index = first_marker; index <= last_marker; ++index)
            {
                if (index <= 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;
                const float x = distance - progress;
                const float ground = environment.ground_height_at(x);
                const Vec2 base = world_to_screen({ x, ground }, viewport, camera, scale);
                const Vec2 top = world_to_screen({ x, ground + 0.72f }, viewport, camera, scale);
                canvas.line(base, top, 4.0f, accent_dim);
                const Rect sign{ top + Vec2{ -62.0f, -28.0f }, { 124.0f, 28.0f } };
                add_rounded_rect(canvas, sign, 5.0f, rgb(0x102431, 0.96f), accent, 1.0f);
                const std::string marker_label = distance_units == ui_layout::DistanceUnits::metric
                    ? std::format("{:.2f} KM", distance / 1000.0f)
                    : std::format("{:.2f} MI", distance / 1609.344f);
                add_text(canvas, sign.position + Vec2{ 7.0f, 6.0f }, marker_label, 1.02f, white);
            }
        }

        void draw_course_features(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const Vec2 feature_screen = world_to_screen(feature.center, viewport, camera, scale);
                if (feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    const Color fill = feature.kind == sim::CourseFeatureKind::rock
                        ? rgb(0x6c747d)
                        : feature.kind == sim::CourseFeatureKind::projectile ? rgb(0xf06a3e) : danger;
                    canvas.circle(feature_screen, feature.radius * scale, fill, 24);
                    if (feature.kind == sim::CourseFeatureKind::projectile)
                    {
                        const Vec2 trail = feature_screen - feature.velocity * (scale * 0.20f);
                        canvas.line(trail, feature_screen, 3.0f, yellow);
                    }
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        viewport, camera, scale);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        viewport, camera, scale);
                    const Rect rect{
                        { minimum.x, maximum.y },
                        { maximum.x - minimum.x, minimum.y - maximum.y }
                    };
                    const Color feature_fill = feature.kind == sim::CourseFeatureKind::hurdle
                        ? yellow : feature.kind == sim::CourseFeatureKind::duck_press
                            ? rgb(0x315b70) : accent_dim;
                    const Color feature_outline = feature.kind == sim::CourseFeatureKind::hurdle
                        ? yellow : accent;
                    add_rounded_rect(canvas, rect, 4.0f,
                        feature_fill, feature_outline, 1.0f);
                }

                const std::string feature_label = feature.kind == sim::CourseFeatureKind::duck_press
                    ? std::format("TRAINER: {}", sim::course_feature_name(feature.kind))
                    : std::format("HAZARD: {}", sim::course_feature_name(feature.kind));
                add_text(canvas, feature_screen + Vec2{ -58.0f, -42.0f },
                    feature_label, 1.00f,
                    feature.kind == sim::CourseFeatureKind::projectile ? yellow : danger);
            }
        }

        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty())
                return;
            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            for (const sim::DistanceConstraint& bone : rig.bones)
            {
                if (bone.a >= particles.size() || bone.b >= particles.size())
                    continue;
                const float radius_a = bone.a < rig.radii.size() ? rig.radii[bone.a] : 0.15f;
                const float radius_b = bone.b < rig.radii.size() ? rig.radii[bone.b] : 0.15f;
                const float radius = std::max(0.055f, std::min(radius_a, radius_b) * 0.55f) * scale;
                canvas.capsule(point(bone.a), point(bone.b), radius, body, 16);
            }
            for (std::size_t index = 0; index < particles.size(); ++index)
            {
                const float radius = (index < rig.radii.size() ? rig.radii[index] : 0.15f) * scale;
                Color color = index == rig.head_node ? body_light : body;
                const bool primary_foot = rig.is_support_seed(index);
                if (primary_foot)
                {
                    color = leg;
                    const Vec2 center = point(index);
                    canvas.capsule(center - Vec2{ radius * 0.82f, 0.0f },
                        center + Vec2{ radius * 0.82f, 0.0f }, radius * 0.44f, color, 16);
                }
                else
                {
                    canvas.circle(point(index), radius, color, 22);
                }
                if (show_nodes)
                {
                    canvas.circle(point(index), 7.0f,
                        index == static_cast<std::size_t>(selected_node) ? accent : white, 18);
                    add_text(canvas, point(index) + Vec2{ 10.0f, -8.0f }, std::to_string(index), 1.05f, white);
                }
            }
        }

        void draw_training_pip(Rect rect)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019, 0.98f), accent_dim, 1.5f);
            add_text(canvas, rect.position + Vec2{ 13.0f, 9.0f },
                "LIVE TRAINING ENVIRONMENT", 1.00f, accent);

            if (!trainer.has_training_preview())
            {
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 43.0f },
                    "WAITING FOR FIRST INTACT TRAINING FRAME", 0.96f, muted,
                    rect.size.x - 26.0f);
                return;
            }

            const sim::Environment& environment = trainer.training_preview();
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty() || rig.root_node >= particles.size())
            {
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 43.0f },
                    "TRAINING FRAME HAS NO COMPLETE RIG", 0.96f, danger,
                    rect.size.x - 26.0f);
                return;
            }

            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(environment.course_stage(), environment);
            const bool foot_only = !environment.non_foot_grounded();
            const bool intact = environment.body_integrity_valid();
            const Color state_color = qualification.valid ? green
                : intact && foot_only ? yellow : danger;
            const std::string_view state_text = qualification.valid
                ? "STAGE VALID"
                : !intact ? "BROKEN RIG"
                : !foot_only ? "BODY TOUCHED GROUND"
                : rl::primary_motion_rejection_name(qualification.rejection_mask);
            add_text_fit(canvas, rect.position + Vec2{ rect.size.x - 154.0f, 9.0f },
                state_text, 0.82f, state_color, 141.0f, 0.68f);

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

            // Keep the rig large and readable. Show roughly 2 m behind and 4 m
            // ahead; a distant obstacle gets a distance label instead of forcing
            // the camera to zoom the body into a tiny cluster.
            float view_min_x = std::min(root_x - 2.0f, body_min_x - 0.35f);
            float view_max_x = std::max(root_x + 4.2f, body_max_x + 0.50f);
            float view_min_y = std::min(body_min_y - 0.18f,
                environment.ground_height_at(root_x) - 0.18f);
            float view_max_y = body_max_y + 0.32f;
            const float world_width = std::max(3.8f, view_max_x - view_min_x);
            const float world_height = std::max(1.5f, view_max_y - view_min_y);
            const float horizontal_scale = (inner.size.x - 12.0f) / world_width;
            const float vertical_scale = (inner.size.y * 0.78f) / world_height;
            const float scale = std::clamp(
                std::min(horizontal_scale, vertical_scale), 20.0f, 48.0f);
            const float camera = (view_min_x + view_max_x) * 0.5f;

            std::vector<Vec2> ground_points{};
            ground_points.reserve(81);
            for (int sample = 0; sample <= 80; ++sample)
            {
                const float fraction = static_cast<float>(sample) / 80.0f;
                const float world_x = camera
                    + (fraction - 0.5f) * inner.size.x / scale;
                ground_points.push_back(world_to_screen(
                    { world_x, environment.ground_height_at(world_x) },
                    inner, camera, scale, 0.82f));
            }
            canvas.polyline(ground_points, 3.0f, rgb(0x65727d));

            const sim::CourseFeature* next_feature = nullptr;
            float next_distance = std::numeric_limits<float>::infinity();
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const float distance = feature.center.x - root_x;
                if (distance >= -0.6f && distance < next_distance)
                {
                    next_distance = distance;
                    next_feature = &feature;
                }

                const Vec2 point = world_to_screen(feature.center,
                    inner, camera, scale, 0.82f);
                if (point.x < inner.position.x - 24.0f
                    || point.x > inner.position.x + inner.size.x + 24.0f)
                    continue;
                if (feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    canvas.circle(point, std::max(3.0f, feature.radius * scale),
                        feature.kind == sim::CourseFeatureKind::projectile ? danger : yellow, 18);
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        inner, camera, scale, 0.82f);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        inner, camera, scale, 0.82f);
                    add_rounded_rect(canvas,
                        { { minimum.x, maximum.y },
                          { maximum.x - minimum.x, minimum.y - maximum.y } },
                        3.0f, accent_dim, accent, 1.0f);
                }
            }

            draw_creature(environment, inner, camera, scale);

            if (next_feature != nullptr && next_distance > 4.2f)
            {
                add_text_fit(canvas,
                    inner.position + Vec2{ inner.size.x - 138.0f, 7.0f },
                    std::format("NEXT {:.1f}M >", next_distance),
                    0.78f, yellow, 128.0f, 0.64f);
            }

            if (!intact || !foot_only)
            {
                add_rounded_rect(canvas,
                    { inner.position + Vec2{ 5.0f, inner.size.y - 34.0f },
                      { inner.size.x - 10.0f, 28.0f } },
                    4.0f, rgb(0x3a0c10, 0.90f), danger, 1.0f);
                add_text_fit(canvas,
                    inner.position + Vec2{ 11.0f, inner.size.y - 28.0f },
                    !foot_only ? "REJECTED: ONLY FEET MAY TOUCH GROUND"
                        : "REJECTED: RIG LOST BODY INTEGRITY",
                    0.78f, white, inner.size.x - 22.0f, 0.62f);
            }

            const std::string pip_metrics = environment.course_stage() == sim::CourseStage::balance
                ? std::format("UPDATE {}  STANCE {:.1f}/{:.1f}S  SPIN {:.2f}  ARMS {:.0f} DEG",
                    trainer.metrics().update,
                    environment.longest_stable_stance_seconds(),
                    rl::standing_mastery_seconds,
                    environment.uncontrolled_spin_turns(),
                    environment.maximum_upper_body_motor_deviation() * 180.0f / pi)
                : environment.course_stage() == sim::CourseStage::moving_hazards
                    ? std::format("UPDATE {}  BURIAL {:.2f}M  IMPACT {:.1f}S  MATERIAL {}",
                        trainer.metrics().update, environment.burial_depth(),
                        environment.incoming_time_to_impact(), environment.material_event_count())
                    : std::format("UPDATE {}  CROUCH {:.1f}S  {:.2f}M  STEPS {}  PASSED {}",
                        trainer.metrics().update,
                        environment.crouch_walk_seconds(), environment.crouch_walk_distance(),
                        environment.gait_cycles(), environment.obstacles_passed());
            add_text_fit(canvas, rect.position + Vec2{ 12.0f, rect.size.y - 23.0f },
                pip_metrics, 0.72f, state_color, rect.size.x - 24.0f, 0.58f);
        }

        void draw_live_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            const float usable_width = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const rl::TrainingMetrics& metrics = trainer.metrics();

            add_text(canvas, cursor, "AUTONOMOUS RIG TRAINER", 1.72f, white);
            cursor.y += 42.0f;
            if (button({ cursor, { usable_width, 48.0f } },
                trainer.background_enabled() ? "AUTOPILOT ON - CLICK TO PAUSE" : "AUTOPILOT PAUSED - CLICK TO RUN",
                input, trainer.background_enabled()))
            {
                trainer.set_background_enabled(!trainer.background_enabled());
                set_status(trainer.background_enabled() ? "BACKGROUND TRAINING RESUMED" : "BACKGROUND TRAINING PAUSED");
            }
            cursor.y += 64.0f;

            add_text(canvas, cursor, "CURRENT LESSON", 1.05f, muted);
            cursor.y += 23.0f;
            add_text_fit(canvas, cursor, sim::course_stage_name(autonomy.stage), 2.05f,
                accent, usable_width, 1.30f);
            cursor.y += 38.0f;
            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   STRICT MASTERY {}/{}",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak,
                rl::mastery_lock_confirmations), 1.12f, white, usable_width);
            cursor.y += 29.0f;
            cursor.y += add_wrapped_text(canvas, cursor, autonomy.message, 1.00f,
                metrics.evaluation_valid || metrics.evaluation_count == 0 ? muted : danger,
                usable_width, 4.0f);
            cursor.y += 15.0f;

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
            const float half = (usable_width - 6.0f) * 0.5f;
            if (button({ cursor, { half, 36.0f } }, "METRIC / 0.25 KM", input,
                distance_units == ui_layout::DistanceUnits::metric))
                distance_units = ui_layout::DistanceUnits::metric;
            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 36.0f } },
                "IMPERIAL / 0.25 MI", input,
                distance_units == ui_layout::DistanceUnits::imperial))
                distance_units = ui_layout::DistanceUnits::imperial;
            cursor.y += 53.0f;

            if (button({ cursor, { half, 38.0f } }, "TRAINING RESULTS", input,
                live_panel_page == LivePanelPage::results))
                live_panel_page = LivePanelPage::results;
            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 38.0f } },
                "LIFETIME TOTALS", input, live_panel_page == LivePanelPage::totals))
                live_panel_page = LivePanelPage::totals;
            cursor.y += 47.0f;

            if (live_panel_page == LivePanelPage::results)
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 247.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, "TRAINING RESULTS", 1.18f, accent);
                cursor.y += 31.0f;
                add_text_fit(canvas, cursor, std::format("UPDATE {}   ENV STEPS {}",
                    metrics.update, metrics.environment_steps), 1.10f, white, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("EVAL {:+.2f}   DIST {}",
                    metrics.evaluation_score, format_distance(metrics.evaluation_distance)),
                    1.10f, metrics.evaluation_valid ? green : danger, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("BEST {:+.2f} @ UPDATE {}",
                    metrics.best_evaluation_score, metrics.best_update),
                    1.10f, accent, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("SURVIVAL {:.1f} S   STRIDES {:.1f}",
                    metrics.evaluation_survival, metrics.evaluation_stride_events),
                    1.08f, white, usable_width);
                cursor.y += 29.0f;
                if (autonomy.stage == sim::CourseStage::balance)
                {
                    add_text_fit(canvas, cursor,
                        std::format("STANCE CUR {:.1f} LONG {:.1f} TARGET {:.1f} S",
                            metrics.evaluation_stable_stance,
                            metrics.evaluation_longest_stance,
                            rl::standing_mastery_seconds),
                        1.00f, metrics.evaluation_valid ? green : danger, usable_width);
                    cursor.y += 29.0f;
                    const std::uint32_t valid_seeds = 6u
                        - std::min<std::uint32_t>(metrics.evaluation_invalid_runs, 6u);
                    add_text_fit(canvas, cursor,
                        std::format("SPIN {:.2f}/{:.2f}   VALID SEEDS {}/6",
                            metrics.evaluation_spin_turns,
                            rl::standing_mastery_spin_limit, valid_seeds),
                        0.98f, rl::strict_balance_mastery(metrics) ? green : yellow,
                        usable_width, 0.82f);
                }
                else
                {
                    add_text_fit(canvas, cursor,
                        std::format("STANCE {:.1f}/{:.1f} S   DUCK REC {:.1f}",
                            metrics.evaluation_stable_stance,
                            metrics.evaluation_longest_stance,
                            metrics.evaluation_duck_recoveries),
                        1.05f, metrics.evaluation_valid ? green : danger, usable_width);
                    cursor.y += 29.0f;
                    add_text_fit(canvas, cursor, std::format("QUALITY {:016X}   {}",
                        metrics.evaluation_quality_key,
                        rl::primary_motion_rejection_name(metrics.evaluation_rejection_mask)),
                        0.98f, metrics.evaluation_valid ? accent : danger,
                        usable_width, 0.82f);
                }
            }
            else
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 318.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, "RIG / SESSION / ALL-TIME TOTALS", 1.08f, accent);
                cursor.y += 27.0f;
                add_text_fit(canvas, cursor, std::format("{} WORKERS {} ENV {:.2f} UPDATES/S {}",
                    autonomy.rollout_threads, autonomy.environment_count,
                    autonomy.updates_per_second, autonomy.speed_mode),
                    0.76f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG {} UPD {} ENV {} BEST STAGE {}",
                    format_duration(rig_lifetime_seconds),
                    ui_layout::lifetime_delta(metrics.update, rig_start_update),
                    ui_layout::lifetime_delta(metrics.environment_steps,
                        rig_start_environment_steps),
                    static_cast<unsigned>(rig_best_stage) + 1u),
                    0.78f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG EPS {} VALID {} BAD {} DIST {}",
                    ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                    ui_layout::lifetime_delta(metrics.total_valid_episodes,
                        rig_start_valid_episodes),
                    ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                        rig_start_invalid_episodes),
                    format_distance(static_cast<float>(std::max(0.0,
                        metrics.total_distance - rig_start_distance)))),
                    0.76f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG STEP {} FALL {} COLL {} OBS {}",
                    ui_layout::lifetime_delta(metrics.total_alternating_steps, rig_start_steps),
                    ui_layout::lifetime_delta(metrics.total_falls, rig_start_falls),
                    ui_layout::lifetime_delta(metrics.total_collisions, rig_start_collisions),
                    ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                        rig_start_obstacles)), 0.76f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG JUMP {} LAND {} FLIP {} TRAIN {}",
                    ui_layout::lifetime_delta(metrics.total_powered_jumps, rig_start_jumps),
                    ui_layout::lifetime_delta(metrics.total_landed_jumps, rig_start_landings),
                    ui_layout::lifetime_delta(metrics.total_landed_flips, rig_start_flips),
                    format_duration(static_cast<float>(std::max(0.0,
                        metrics.total_training_seconds - rig_start_training_seconds)))),
                    0.74f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG ACCEPT {} REJECT {} ROLLBACK {}",
                    ui_layout::lifetime_delta(autonomy.accepted_rig_changes,
                        rig_start_accepted_rigs),
                    ui_layout::lifetime_delta(autonomy.rejected_rig_changes,
                        rig_start_rejected_rigs),
                    ui_layout::lifetime_delta(autonomy.rollback_count, rig_start_rollbacks)),
                    0.74f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("SESSION {} TRAIN {} ENV {} EPS {} BAD {}",
                    format_duration(session_runtime_seconds),
                    format_duration(static_cast<float>(std::max(0.0,
                        metrics.total_training_seconds - session_start_training_seconds))),
                    ui_layout::lifetime_delta(metrics.total_environment_steps,
                        session_start_environment_steps),
                    ui_layout::lifetime_delta(metrics.total_episodes, session_start_episodes),
                    ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                        session_start_invalid_episodes)), 0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("SESSION DIST {} COLL {} JUMP {} FLIP {} OBS {}",
                    format_distance(static_cast<float>(std::max(0.0,
                        metrics.total_distance - session_start_distance))),
                    ui_layout::lifetime_delta(metrics.total_collisions,
                        session_start_collisions),
                    ui_layout::lifetime_delta(metrics.total_powered_jumps, session_start_jumps),
                    ui_layout::lifetime_delta(metrics.total_landed_flips, session_start_flips),
                    ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                        session_start_obstacles)), 0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("SESSION ACCEPT {} REJECT {} RESET {} RB {}",
                    ui_layout::lifetime_delta(autonomy.accepted_rig_changes,
                        session_start_accepted_rigs),
                    ui_layout::lifetime_delta(autonomy.rejected_rig_changes,
                        session_start_rejected_rigs),
                    ui_layout::lifetime_delta(metrics.total_resets, session_start_resets),
                    ui_layout::lifetime_delta(autonomy.rollback_count,
                        session_start_rollbacks)), 0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("ALL TRAIN {} UPD {} ENV {} EPS {} RESET {}",
                    format_duration(static_cast<float>(metrics.total_training_seconds)),
                    metrics.total_updates, metrics.total_environment_steps,
                    metrics.total_episodes, metrics.total_resets),
                    0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("ALL DIST {} COLL {} JUMP {} FLIP {} OBS {}",
                    format_distance(static_cast<float>(metrics.total_distance)),
                    metrics.total_collisions, metrics.total_powered_jumps,
                    metrics.total_landed_flips, metrics.total_obstacles_passed),
                    0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("ALL ACCEPT {} REJECT {} ROLLBACK {}",
                    autonomy.accepted_rig_changes, autonomy.rejected_rig_changes,
                    autonomy.rollback_count), 0.70f, muted, usable_width);
            }
        }

        void draw_live_world(Rect viewport, float dt)
        {
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
            constexpr float live_pixels_per_meter = 22.0f;
            if (!environment.particles().empty())
                camera_x = lerp(camera_x,
                    environment.particles()[environment.blueprint().root_node].position.x + 5.5f, 0.035f);
            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            draw_course_ground(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_reference(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_features(environment, viewport, camera_x, live_pixels_per_meter);
            draw_creature(environment, viewport, camera_x, live_pixels_per_meter);

            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const float overlay_width = std::max(260.0f, viewport.size.x - 48.0f);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 22.0f },
                std::format("{}  /  {:.0f}%", sim::course_stage_name(autonomy.stage), autonomy.difficulty * 100.0f),
                1.95f, white, overlay_width, 1.25f);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 61.0f },
                std::format("{}   ACTUAL {}",
                    format_speed(environment.forward_speed()),
                    format_distance(environment.distance_travelled())),
                1.22f, environment.valid_motion() ? green : danger, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 90.0f },
                std::format("COURSE {}   {}", format_distance(environment.course_progress()),
                    sim::invalid_motion_name(environment.invalid_reason())),
                1.16f, environment.valid_motion() ? green : danger, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 119.0f },
                std::format("STEPS {}  DUCK {:.1f} S  JUMP {}/{}  FLIP {:.1f}  SPIN {:.1f}  PASSED {}",
                    environment.alternating_steps(), environment.duck_seconds(),
                    environment.powered_jumps(), environment.landed_jumps(),
                    environment.maximum_flip_turns(), environment.uncontrolled_spin_turns(),
                    environment.obstacles_passed()),
                1.02f, environment.recovering() ? yellow : muted, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 38.0f },
                trainer.has_best_policy()
                    ? "BEST STAGE-VALID CONTROLLER   v" RUNNER_VERSION "   BACKGROUND TRAINING ACTIVE"
                    : "CURRENT POLICY UNVERIFIED   v" RUNNER_VERSION "   SEARCHING FOR VALID STANCE",
                1.05f, trainer.has_best_policy() ? muted : danger, overlay_width, 1.00f);

            const ui_layout::Box pip = ui_layout::training_pip_box({
                viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y });
            draw_training_pip({ { pip.x, pip.y }, { pip.width, pip.height } });
        }

        void draw_joint_lab(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x0b1721, 0.98f), accent_dim, 1.0f);
            const auto names = motor_names();
            add_text(canvas, rect.position + Vec2{ 14.0f, 10.0f },
                std::format("JOINT TEST - {}", names[static_cast<std::size_t>(selected_motor)]), 1.45f, white);
            const float group_width = (rect.size.x - 28.0f) * 0.25f;
            Vec2 row = rect.position + Vec2{ 14.0f, 39.0f };
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

            row.y += 39.0f;
            if (button({ row, { group_width - 4.0f, 31.0f } }, "MIN", input))
            {
                joint_auto_sweep = false;
                joint_test_input = -1.0f;
            }
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "REST", input))
            {
                joint_auto_sweep = false;
                joint_test_input = 0.0f;
            }
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "MAX", input))
            {
                joint_auto_sweep = false;
                joint_test_input = 1.0f;
            }
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, 31.0f } },
                joint_auto_sweep ? "STOP" : "SWEEP", input, joint_auto_sweep))
                joint_auto_sweep = !joint_auto_sweep;
            joint_test_input = slider({ rect.position + Vec2{ 14.0f, 119.0f }, { rect.size.x - 28.0f, 36.0f } },
                "TEST INPUT  -1 MIN / 0 REST / +1 MAX", joint_test_input, -1.0f, 1.0f, input);
        }

        void draw_blueprint(Rect viewport, const InputState& input)
        {
            constexpr float scale = 86.0f;
            const float ground_y = world_to_screen({ 0.0f, 0.0f }, viewport, 0.0f, scale).y;
            canvas.quad({ viewport.position.x, ground_y }, viewport.position + viewport.size, rgb(0x111820));
            canvas.line({ viewport.position.x, ground_y }, { viewport.position.x + viewport.size.x, ground_y },
                3.0f, rgb(0x475762));

            auto screen = [&](std::size_t index)
            {
                return world_to_screen(blueprint.nodes[index], viewport, 0.0f, scale);
            };
            std::vector<Vec2> preview = blueprint.nodes;
            for (int motor_index = 0; motor_index < static_cast<int>(sim::action_count); ++motor_index)
            {
                if (!test_motor_active(motor_index))
                    continue;
                const sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(motor_index)];
                if (!motor.enabled || motor.a >= preview.size() || motor.pivot >= preview.size() || motor.c >= preview.size())
                    continue;
                const Vec2 pivot = preview[motor.pivot];
                const float current = signed_angle(preview[motor.a] - pivot, preview[motor.c] - pivot);
                const float delta = wrap_angle(sim::motor_target_angle(motor, joint_test_input) - current);
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
                return world_to_screen(preview[index], viewport, 0.0f, scale);
            };
            for (std::size_t index = 0; index < blueprint.bones.size(); ++index)
            {
                const sim::DistanceConstraint& bone = blueprint.bones[index];
                if (bone.a >= preview.size() || bone.b >= preview.size())
                    continue;
                const Vec2 a = preview_screen(bone.a);
                const Vec2 b = preview_screen(bone.b);
                canvas.line(a, b, 1.25f, with_alpha(accent, 0.24f));
                const float packet_phase = std::fmod(session_runtime_seconds * 0.55f
                    + static_cast<float>(index) * 0.137f, 1.0f);
                canvas.circle(a + (b - a) * packet_phase, 2.4f,
                    with_alpha(body_light, 0.68f), 12);
            }
            for (std::size_t index = 0; index < preview.size(); ++index)
            {
                if (index != blueprint.root_node && index != blueprint.torso_node
                    && index != blueprint.head_node && !blueprint.is_support_seed(index))
                    continue;
                const Vec2 center = preview_screen(index);
                const float radius = 13.0f + std::sin(session_runtime_seconds * 2.2f
                    + static_cast<float>(index)) * 2.0f;
                std::array<Vec2, 25> halo{};
                for (std::size_t point_index = 0; point_index < halo.size(); ++point_index)
                {
                    const float angle = static_cast<float>(point_index)
                        / static_cast<float>(halo.size() - 1u) * pi * 2.0f;
                    halo[point_index] = center
                        + Vec2{ std::cos(angle), std::sin(angle) } * radius;
                }
                canvas.polyline(halo, 1.25f, with_alpha(accent, 0.44f));
            }
            for (const sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a < preview.size() && bone.b < preview.size())
                    canvas.line(preview_screen(bone.a), preview_screen(bone.b), 9.0f, with_alpha(accent, 0.34f));
            }

            for (const sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a < blueprint.nodes.size() && bone.b < blueprint.nodes.size())
                    canvas.line(screen(bone.a), screen(bone.b), 17.0f, rgb(0x835927));
            }
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const float radius = (index < blueprint.radii.size() ? blueprint.radii[index] : 0.15f) * scale;
                Color color = index == blueprint.head_node ? body_light : body;
                if (blueprint.is_support_seed(index))
                    color = leg;
                canvas.circle(screen(index), radius, color, 24);
                canvas.circle(screen(index), 7.0f,
                    index == static_cast<std::size_t>(selected_node) ? accent : white, 18);
                add_text(canvas, screen(index) + Vec2{ 10.0f, -8.0f }, std::to_string(index), 1.05f, white);
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
                    arc.push_back(world_to_screen(pivot_world + rotate(reference, angle) * arm_length,
                        viewport, 0.0f, scale));
                }
                canvas.polyline(arc, 4.0f, accent);
                const Vec2 pivot_screen = screen(motor.pivot);
                auto ray = [&](float angle, Color color, float width)
                {
                    canvas.line(pivot_screen,
                        world_to_screen(pivot_world + rotate(reference, angle) * arm_length,
                            viewport, 0.0f, scale), width, color);
                };
                ray(motor.minimum_angle, danger, 2.5f);
                ray(motor.maximum_angle, danger, 2.5f);
                ray(motor.neutral_angle, white, 3.0f);
                ray(sim::motor_target_angle(motor, joint_test_input), yellow, 4.0f);
                add_text(canvas, screen(motor.a) + Vec2{ 8.0f, -15.0f }, "A / PARENT", 1.05f, accent);
                add_text(canvas, pivot_screen + Vec2{ 8.0f, -15.0f }, "PIVOT", 1.05f, white);
                add_text(canvas, screen(motor.c) + Vec2{ 8.0f, -15.0f }, "C / DRIVEN", 1.05f, yellow);
            }

            const Rect joint_rect{ { viewport.position.x + 20.0f, viewport.position.y + viewport.size.y - 174.0f },
                { std::min(850.0f, viewport.size.x - 40.0f), 154.0f } };
            const bool over_joint_lab = contains(joint_rect, input.mouse);
            if (input.left_pressed && contains(viewport, input.mouse) && !over_joint_lab)
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
                    blueprint.nodes.push_back(screen_to_world(input.mouse, viewport, 0.0f, scale));
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
                blueprint.nodes[static_cast<std::size_t>(selected_node)] = screen_to_world(input.mouse, viewport, 0.0f, scale);
            if (dragging_node && input.left_released)
            {
                dragging_node = false;
                apply_small_rig_change("NODE MOVED");
            }
            draw_joint_lab(joint_rect, input);
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
            apply_small_rig_change("NODE DELETED; AFFECTED MOTORS DISABLED");
            return true;
        }

        void draw_rig_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            add_text(canvas, cursor, "GUIDED RIG LAB", 1.80f, white);
            cursor.y += 38.0f;
            const float page_width = (rect.size.x - 42.0f) * 0.5f;
            if (button({ cursor, { page_width, 36.0f } }, "BODY / FILES", input,
                rig_panel_page == RigPanelPage::body))
                rig_panel_page = RigPanelPage::body;
            if (button({ cursor + Vec2{ page_width + 6.0f, 0.0f }, { page_width, 36.0f } }, "JOINT / MOTOR", input,
                rig_panel_page == RigPanelPage::motor))
                rig_panel_page = RigPanelPage::motor;
            cursor.y += 49.0f;

            if (rig_panel_page == RigPanelPage::body)
            {
                add_text(canvas, cursor, "CHOOSE A BODY; AUTOPILOT RESTARTS AT BALANCE", 1.02f, muted);
                cursor.y += 27.0f;
                const float fourth = (rect.size.x - 54.0f) / 4.0f;
                if (button({ cursor, { fourth, 35.0f } }, "HUMANOID", input, rig_preset == RigPreset::humanoid))
                    use_preset(RigPreset::humanoid);
                if (button({ cursor + Vec2{ fourth + 6.0f, 0.0f }, { fourth, 35.0f } }, "BIPED", input,
                    rig_preset == RigPreset::biped))
                    use_preset(RigPreset::biped);
                if (button({ cursor + Vec2{ (fourth + 6.0f) * 2.0f, 0.0f }, { fourth, 35.0f } }, "QUADRUPED", input,
                    rig_preset == RigPreset::quadruped))
                    use_preset(RigPreset::quadruped);
                if (button({ cursor + Vec2{ (fourth + 6.0f) * 3.0f, 0.0f }, { fourth, 35.0f } }, "4-LEG", input,
                    rig_preset == RigPreset::crawler4))
                    use_preset(RigPreset::crawler4);
                cursor.y += 43.0f;
                const float third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "CHICKEN", input, rig_preset == RigPreset::chicken))
                    use_preset(RigPreset::chicken);
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 35.0f } }, "6-LEG", input,
                    rig_preset == RigPreset::hexapod))
                    use_preset(RigPreset::hexapod);
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 35.0f } }, "MONOPED", input,
                    rig_preset == RigPreset::monoped))
                    use_preset(RigPreset::monoped);
                cursor.y += 48.0f;

                const float file_third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { file_third, 35.0f } }, "SAVE RIG", input) || input.save_pressed)
                {
                    std::string error{};
                    set_status(blueprint.save(rig_path, error) ? "RIG SAVED" : error);
                }
                if (button({ cursor + Vec2{ file_third + 6.0f, 0.0f }, { file_third, 35.0f } }, "LOAD RIG", input)
                    || input.load_pressed)
                {
                    std::string error{};
                    blueprint = sim::CreatureBlueprint::load(rig_path, error);
                    rig_preset = RigPreset::custom;
                    trainer.set_blueprint(blueprint, false);
                    set_status(error.empty() ? "RIG LOADED - FRESH BALANCE LESSON STARTED" : error);
                }
                if (button({ cursor + Vec2{ (file_third + 6.0f) * 2.0f, 0.0f }, { file_third, 35.0f } },
                    "USE EVOLVED", input))
                {
                    blueprint = trainer.blueprint();
                    rig_preset = RigPreset::custom;
                    set_status("CURRENT AUTOPILOT-EVOLVED RIG COPIED INTO LAB");
                }
                cursor.y += 50.0f;

                add_text(canvas, cursor, std::format("SELECTED NODE: {}", selected_node), 1.20f, muted);
                if (button({ cursor + Vec2{ rect.size.x - 151.0f, -6.0f }, { 115.0f, 32.0f } },
                    "DELETE", input, false, selected_node >= 0))
                    delete_selected_node();
                cursor.y += 33.0f;
                if (selected_node >= 0 && static_cast<std::size_t>(selected_node) < blueprint.radii.size())
                {
                    float& radius = blueprint.radii[static_cast<std::size_t>(selected_node)];
                    const float updated = slider({ cursor, { rect.size.x - 36.0f, 38.0f } },
                        "NODE SIZE", radius, 0.08f, 0.60f, input);
                    if (updated != radius)
                    {
                        radius = updated;
                        queue_rig_change("NODE SIZE UPDATED");
                    }
                    cursor.y += 50.0f;

                    add_text(canvas, cursor, "SELECTED NODE ROLE", 1.08f, muted);
                    cursor.y += 23.0f;
                    const float role_width = (rect.size.x - 52.0f) * 0.20f;
                    auto role = [&](int slot, std::string_view label, std::uint16_t& target)
                    {
                        if (button({ cursor + Vec2{ role_width * static_cast<float>(slot), 0.0f },
                            { role_width - 4.0f, 32.0f } }, label, input, target == selected_node))
                        {
                            target = static_cast<std::uint16_t>(selected_node);
                            apply_small_rig_change("NODE ROLE UPDATED");
                        }
                    };
                    role(0, "ROOT", blueprint.root_node);
                    role(1, "TORSO", blueprint.torso_node);
                    role(2, "HEAD", blueprint.head_node);
                    role(3, "FOOT L", blueprint.left_contact_node);
                    role(4, "FOOT R", blueprint.right_contact_node);
                    cursor.y += 44.0f;
                }
                add_text(canvas, cursor, "SELECT OR DRAG NODES IN THE VIEW. SHIFT ADDS; CTRL CONNECTS.", 1.00f, muted);
            }
            else
            {
                add_text(canvas, cursor, "A = PARENT REFERENCE   PIVOT = JOINT   C = DRIVEN CHILD", 1.00f, muted);
                cursor.y += 29.0f;
                const float motor_width = (rect.size.x - 48.0f) * 0.25f;
                for (int index = 0; index < static_cast<int>(sim::action_count); ++index)
                {
                    const int column = index % 4;
                    const int row = index / 4;
                    const bool motor_available = static_cast<std::size_t>(index) < blueprint.active_motor_count;
                    if (button({ cursor + Vec2{ motor_width * static_cast<float>(column),
                        static_cast<float>(row) * 41.0f }, { motor_width - 4.0f, 35.0f } },
                        std::to_string(index + 1), input, selected_motor == index, motor_available))
                    {
                        selected_motor = index;
                        joint_test_group = JointTestGroup::selected;
                    }
                }
                cursor.y += 87.0f;
                const auto names = motor_names();
                add_text(canvas, cursor, names[static_cast<std::size_t>(selected_motor)], 1.55f, white);
                cursor.y += 30.0f;

                sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(selected_motor)];
                const float endpoint = (rect.size.x - 48.0f) / 3.0f;
                auto set_endpoint = [&](Vec2 position, std::string_view label, std::uint16_t& value)
                {
                    if (!button({ position, { endpoint, 34.0f } }, label, input, false, selected_node >= 0))
                        return;
                    value = static_cast<std::uint16_t>(selected_node);
                    const bool distinct = motor.a != motor.pivot && motor.pivot != motor.c && motor.a != motor.c;
                    const bool connected = distinct && has_direct_bone(motor.a, motor.pivot)
                        && has_direct_bone(motor.pivot, motor.c);
                    motor.enabled = connected;
                    if (connected)
                    {
                        const float negative = std::max(2.0f,
                            (motor.neutral_angle - motor.minimum_angle) * 180.0f / pi);
                        const float positive = std::max(2.0f,
                            (motor.maximum_angle - motor.neutral_angle) * 180.0f / pi);
                        blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor), negative, positive, motor.strength);
                        apply_small_rig_change("MOTOR ENDPOINT UPDATED");
                    }
                    else
                    {
                        set_status("MOTOR DISABLED - A-PIVOT AND PIVOT-C MUST EACH BE REAL BONES");
                    }
                };
                set_endpoint(cursor, "SET A / PARENT", motor.a);
                set_endpoint(cursor + Vec2{ endpoint + 6.0f, 0.0f }, "SET PIVOT", motor.pivot);
                set_endpoint(cursor + Vec2{ (endpoint + 6.0f) * 2.0f, 0.0f }, "SET C / DRIVEN", motor.c);
                cursor.y += 44.0f;

                const bool endpoints_valid = motor.a < blueprint.nodes.size() && motor.pivot < blueprint.nodes.size()
                    && motor.c < blueprint.nodes.size() && motor.a != motor.pivot
                    && motor.pivot != motor.c && motor.a != motor.c;
                const bool connected = endpoints_valid && has_direct_bone(motor.a, motor.pivot)
                    && has_direct_bone(motor.pivot, motor.c);
                add_text(canvas, cursor, std::format("A {}  PIVOT {}  C {}  {}", motor.a, motor.pivot, motor.c,
                    connected ? (motor.enabled ? "READY" : "DISABLED") : "NOT CONNECTED"), 1.12f,
                    connected ? accent : danger);
                cursor.y += 30.0f;

                const float third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { third, 34.0f } }, "SET REST", input, false, connected))
                {
                    const float negative = std::max(2.0f, (motor.neutral_angle - motor.minimum_angle) * 180.0f / pi);
                    const float positive = std::max(2.0f, (motor.maximum_angle - motor.neutral_angle) * 180.0f / pi);
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor), negative, positive, motor.strength);
                    apply_small_rig_change("REST POSE RECALIBRATED");
                }
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 34.0f } }, "SAFE RANGE", input,
                    false, connected))
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor), 16.0f, 18.0f, 0.050f);
                    apply_small_rig_change("SAFE JOINT DEFAULT APPLIED");
                }
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 34.0f } }, "SWAP A/C", input,
                    false, connected))
                {
                    std::swap(motor.a, motor.c);
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor), 16.0f, 18.0f, motor.strength);
                    apply_small_rig_change("MOTOR DIRECTION SWAPPED");
                }
                cursor.y += 46.0f;

                if (button({ cursor, { rect.size.x - 36.0f, 34.0f } },
                    motor.enabled ? "DISABLE THIS MOTOR" : "ENABLE THIS MOTOR", input, motor.enabled, connected))
                {
                    motor.enabled = !motor.enabled;
                    apply_small_rig_change(motor.enabled ? "MOTOR ENABLED" : "MOTOR DISABLED");
                }
                cursor.y += 47.0f;

                float negative = (motor.neutral_angle - motor.minimum_angle) * 180.0f / pi;
                float positive = (motor.maximum_angle - motor.neutral_angle) * 180.0f / pi;
                const float updated_negative = slider({ cursor, { rect.size.x - 36.0f, 38.0f } },
                    "BACKWARD TRAVEL", negative, 2.0f, 60.0f, input, " DEG");
                if (updated_negative != negative)
                {
                    motor.minimum_angle = motor.neutral_angle - updated_negative * pi / 180.0f;
                    queue_rig_change("BACKWARD TRAVEL UPDATED");
                }
                cursor.y += 49.0f;
                const float updated_positive = slider({ cursor, { rect.size.x - 36.0f, 38.0f } },
                    "FORWARD TRAVEL", positive, 2.0f, 60.0f, input, " DEG");
                if (updated_positive != positive)
                {
                    motor.maximum_angle = motor.neutral_angle + updated_positive * pi / 180.0f;
                    queue_rig_change("FORWARD TRAVEL UPDATED");
                }
                cursor.y += 49.0f;
                const float updated_power = slider({ cursor, { rect.size.x - 36.0f, 38.0f } },
                    "JOINT SPEED / POWER", motor.strength, 0.02f, 0.10f, input);
                if (updated_power != motor.strength)
                {
                    motor.strength = updated_power;
                    queue_rig_change("JOINT POWER UPDATED");
                }
            }
        }

        void process_shortcuts(const InputState& input)
        {
            if (input.key_1_pressed) mode = Mode::live;
            if (input.key_2_pressed || input.key_3_pressed) mode = Mode::rig_lab;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
            if (input.reset_pressed)
            {
                trainer.reset_preview();
                camera_x = 0.0f;
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
            if (!session_stats_initialized)
            {
                session_stats_initialized = true;
                session_start_environment_steps = current_metrics.total_environment_steps;
                session_start_episodes = current_metrics.total_episodes;
                session_start_invalid_episodes = current_metrics.total_invalid_episodes;
                session_start_resets = current_metrics.total_resets;
                session_start_collisions = current_metrics.total_collisions;
                session_start_jumps = current_metrics.total_powered_jumps;
                session_start_flips = current_metrics.total_landed_flips;
                session_start_obstacles = current_metrics.total_obstacles_passed;
                session_start_distance = current_metrics.total_distance;
                session_start_training_seconds = current_metrics.total_training_seconds;
                session_start_accepted_rigs = current_autonomy.accepted_rig_changes;
                session_start_rejected_rigs = current_autonomy.rejected_rig_changes;
                session_start_rollbacks = current_autonomy.rollback_count;
            }
            const std::uint64_t current_signature = trainer.rig_signature();
            if (tracked_rig_signature == 0u || tracked_rig_signature != current_signature)
            {
                tracked_rig_signature = current_signature;
                rig_lifetime_seconds = 0.0f;
                rig_start_update = current_metrics.update;
                rig_start_environment_steps = current_metrics.environment_steps;
                rig_start_episodes = current_metrics.total_episodes;
                rig_start_valid_episodes = current_metrics.total_valid_episodes;
                rig_start_invalid_episodes = current_metrics.total_invalid_episodes;
                rig_start_steps = current_metrics.total_alternating_steps;
                rig_start_falls = current_metrics.total_falls;
                rig_start_collisions = current_metrics.total_collisions;
                rig_start_jumps = current_metrics.total_powered_jumps;
                rig_start_landings = current_metrics.total_landed_jumps;
                rig_start_flips = current_metrics.total_landed_flips;
                rig_start_obstacles = current_metrics.total_obstacles_passed;
                rig_start_distance = current_metrics.total_distance;
                rig_start_training_seconds = current_metrics.total_training_seconds;
                rig_start_accepted_rigs = current_autonomy.accepted_rig_changes;
                rig_start_rejected_rigs = current_autonomy.rejected_rig_changes;
                rig_start_rollbacks = current_autonomy.rollback_count;
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
            const Rect content{ { layout_content.x, layout_content.y },
                { layout_content.width, layout_content.height } };
            if (!ui_layout::supported_window(static_cast<float>(width), static_cast<float>(height)))
            {
                add_text(canvas, { 24.0f, 100.0f },
                    "WINDOW TOO SMALL - MINIMUM CONTENT 1080 X 800", 2.0f, danger);
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
                draw_live_world(world, dt);
                draw_live_panel(side, input);
            }
            else
            {
                const float panel_width = std::clamp(content.size.x * 0.42f, 680.0f, 760.0f);
                const Rect side{ content.position, { panel_width, content.size.y } };
                const Rect world{ { content.position.x + panel_width + 10.0f, content.position.y },
                    { content.size.x - panel_width - 10.0f, content.size.y } };
                draw_rig_panel(side, input);
                add_rounded_rect(canvas, world, 11.0f, rgb(0x0a131d), border, 1.0f);
                draw_blueprint(world, input);
                add_text(canvas, world.position + Vec2{ 20.0f, 18.0f },
                    "CLICK SELECT / DRAG MOVE / SHIFT ADD / CTRL CONNECT / DELETE REMOVE", 1.12f, muted);
            }

            if (input.left_released && rig_edit_pending)
            {
                const std::string reason = rig_edit_reason;
                rig_edit_pending = false;
                rig_edit_reason.clear();
                apply_small_rig_change(reason);
            }

            if (status_time > 0.0f)
            {
                const float scale = 1.30f * ui_font_scale;
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
        std::string artwork_error{};
        if (!art::load_p3_pixel_art(asset_directory / "chicken.ppm",
                impl_->original_runner_art, artwork_error))
        {
            impl_->original_runner_art = {};
            impl_->status = "ARTWORK WARNING - " + artwork_error;
            impl_->status_time = 9.0f;
        }

        impl_->trainer.set_autosave_paths(impl_->autosave_policy_path,
            impl_->autosave_rig_path, impl_->autosave_state_path);
        std::string message{};
        const bool resumed = impl_->trainer.load_autosave(message);
        impl_->trainer.synchronize();
        impl_->blueprint = impl_->trainer.blueprint();
        impl_->rig_preset = resumed ? Impl::RigPreset::custom : Impl::RigPreset::humanoid;
        impl_->trainer.set_background_enabled(true);
        if (impl_->original_runner_art.loaded())
        {
            impl_->status = message;
            impl_->status_time = 6.0f;
        }
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
