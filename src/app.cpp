#include "app.hpp"
#include "autonomy.hpp"
#include "pixel_art.hpp"
#include "simulation.hpp"
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
                    for (std::uint32_t column = 0;
                        column < font::glyph_width; ++column)
                    {
                        if (!font::pixel_on(glyph, column, row))
                            continue;
                        const Vec2 minimum{
                            cursor.x + static_cast<float>(column)
                                * metrics.cell_size,
                            cursor.y + static_cast<float>(row)
                                * metrics.cell_size
                        };
                        canvas.quad(minimum,
                            minimum + Vec2{ metrics.cell_size,
                                metrics.cell_size }, color);
                    }
                }
                cursor.x += metrics.advance;
            }
        }

        void draw_pixel_art(render::Canvas& canvas, const art::PixelArt& art,
            Rect target, float alpha = 1.0f)
        {
            if (!art.loaded() || target.size.x <= 0.0f || target.size.y <= 0.0f)
                return;
            const float pixel_width = target.size.x / static_cast<float>(art.width);
            const float pixel_height = target.size.y / static_cast<float>(art.height);
            for (int y = 0; y < art.height; ++y)
            {
                for (int x = 0; x < art.width; ++x)
                {
                    Color color = art.pixels[static_cast<std::size_t>(
                        y * art.width + x)];
                    if (std::max({ color.r, color.g, color.b }) < 0.035f)
                        continue;
                    color.a *= alpha;
                    const Vec2 minimum = target.position + Vec2{
                        static_cast<float>(x) * pixel_width,
                        static_cast<float>(y) * pixel_height
                    };
                    canvas.quad(minimum,
                        minimum + Vec2{ pixel_width + 0.35f,
                            pixel_height + 0.35f }, color);
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

        [[nodiscard]] Vec2 screen_to_world(Vec2 screen, Rect viewport, float camera_x,
            float pixels_per_meter) noexcept
        {
            const float ground_y = viewport.position.y
                + viewport.size.y * view_camera::live_ground_fraction;
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
            scaffold, humanoid, biped, chicken, quadruped, crawler4, hexapod, monoped, custom
        };
        enum class RigPanelPage : std::uint8_t { presets, structure, motors, test };
        enum class LivePanelPage : std::uint8_t { summary, totals, advanced };
        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };

        render::Canvas canvas{};
        sim::CreatureBlueprint blueprint{ sim::CreatureBlueprint::humanoid() };
        rl::AutonomousTrainer trainer{ blueprint, 64 };
        Mode mode{ Mode::live };
        RigPreset rig_preset{ RigPreset::humanoid };
        JointTestGroup joint_test_group{ JointTestGroup::selected };
        RigPanelPage rig_panel_page{ RigPanelPage::presets };
        LivePanelPage live_panel_page{ LivePanelPage::summary };
        int selected_node{ -1 };
        int selected_bone{ -1 };
        int selected_motor{};
        bool dragging_node{};
        bool joint_auto_sweep{};
        bool right_leg_near{ true };
        bool rig_test_loose_ground{};
        sim::RigTestPattern rig_test_pattern{ sim::RigTestPattern::manual };
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
        float live_pixels_per_meter{ view_camera::default_pixels_per_meter };
        float live_zoom_factor{ 1.0f };
        bool live_zoom_auto{ true };
        art::PixelArt original_runner_art{};
        art::PixelArt optional_foot_art{};
        art::PixelArt optional_helmet_art{};
        art::PixelArt optional_torso_art{};
        art::PixelArt optional_weapon_art{};
        bool optional_art_enabled{ false };
        bool debug_skeleton_overlay{};
        std::string status{ "AUTOPILOT STARTING" };
        float status_time{ 4.0f };
        bool quit{};
        std::filesystem::path rig_path{ "creature.rig" };
        std::filesystem::path policy_path{ "creature.eppo" };
        std::filesystem::path autosave_policy_path{ "runner-v0725-rig-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v0725-rig-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v0725-rig-autonomy.state" };

        [[nodiscard]] std::string_view preset_name() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::scaffold: return "SCAFFOLD";
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
                return { "REAR PHASE A", "REAR PHASE B",
                    "MIDDLE PHASE A", "MIDDLE PHASE B",
                    "FRONT PHASE A", "FRONT PHASE B",
                    "UNUSED 7", "UNUSED 8" };
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
            rig_preset = preset;
            switch (preset)
            {
            case RigPreset::scaffold: blueprint = sim::CreatureBlueprint::scaffold(); break;
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
            selected_bone = -1;
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
            const sim::DeformableTerrain& terrain = environment.terrain();
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
            auto draw_world_cell = [&](float x0, float y0, float x1, float y1,
                sandhybrid::Material material)
            {
                const Vec2 minimum = world_to_screen({ x0, y0 }, viewport, camera, scale);
                const Vec2 maximum = world_to_screen({ x1, y1 }, viewport, camera, scale);
                canvas.quad({ minimum.x, maximum.y }, { maximum.x, minimum.y },
                    material_color(material));
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

            const float progress = environment.course_progress();
            const float source_left = sim::terrain_sample_x(left, progress);
            const float source_right = sim::terrain_sample_x(right, progress);
            const int first_source_macro = static_cast<int>(std::floor(
                source_left / sim::DeformableTerrain::macro_tile_size));
            const int last_source_macro = static_cast<int>(std::ceil(
                source_right / sim::DeformableTerrain::macro_tile_size));

            for (int source_macro = first_source_macro;
                source_macro <= last_source_macro; ++source_macro)
            {
                const auto wrapped_macro = static_cast<std::size_t>((
                    source_macro % static_cast<int>(sim::DeformableTerrain::macro_columns)
                    + static_cast<int>(sim::DeformableTerrain::macro_columns))
                    % static_cast<int>(sim::DeformableTerrain::macro_columns));
                const float source_macro_x0 = static_cast<float>(source_macro)
                    * sim::DeformableTerrain::macro_tile_size;
                const float macro_x0 = sim::terrain_world_x(source_macro_x0, progress);
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
                if (index < 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;
                const float x = distance - progress;
                const float ground = environment.ground_height_at(x);
                const Vec2 base = world_to_screen({ x, ground }, viewport, camera, scale);
                const Vec2 top = world_to_screen({ x, ground + 0.66f }, viewport, camera, scale);
                canvas.line(base, top, 3.0f, accent_dim);
                const Rect sign{ top + Vec2{ -46.0f, -25.0f }, { 92.0f, 25.0f } };
                add_rounded_rect(canvas, sign, 5.0f, rgb(0x102431, 0.97f), accent, 1.0f);
                const std::string marker_label = index == 0 ? "START"
                    : distance_units == ui_layout::DistanceUnits::metric
                        ? (distance >= 1000.0f
                            ? std::format("{:.2f} KM", distance / 1000.0f)
                            : std::format("{:.0f} M", distance))
                        : (distance >= 1609.344f
                            ? std::format("{:.2f} MI", distance / 1609.344f)
                            : std::format("{:.0f} FT", distance * 3.2808399f));
                add_text_fit(canvas, sign.position + Vec2{ 6.0f, 5.0f },
                    marker_label, 0.88f, white, sign.size.x - 12.0f, 0.78f);
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
            auto leg_side = [&](std::size_t index) noexcept
            {
                if (rig.is_left_support_seed(index))
                    return -1;
                if (rig.is_right_support_seed(index))
                    return 1;
                if (rig.paired_leg_chains())
                {
                    if (index == rig.motors[0].pivot || index == rig.motors[0].c
                        || index == rig.motors[1].pivot || index == rig.motors[1].c)
                        return -1;
                    if (index == rig.motors[2].pivot || index == rig.motors[2].c
                        || index == rig.motors[3].pivot || index == rig.motors[3].c)
                        return 1;
                }
                return 0;
            };
            auto draw_bones = [&](int pass)
            {
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    if (bone.a >= particles.size() || bone.b >= particles.size())
                        continue;
                    const int side_a = leg_side(bone.a);
                    const int side_b = leg_side(bone.b);
                    const int side = side_a != 0 ? side_a : side_b;
                    const bool near = side != 0 && ((side > 0) == right_leg_near);
                    const int layer = side == 0 ? 1 : near ? 2 : 0;
                    if (layer != pass)
                        continue;
                    const float radius_a = bone.a < rig.radii.size()
                        ? rig.radii[bone.a] : 0.15f;
                    const float radius_b = bone.b < rig.radii.size()
                        ? rig.radii[bone.b] : 0.15f;
                    const float radius = std::max(0.035f,
                        std::min(radius_a, radius_b) * 0.34f) * scale;
                    const Color color = side == 0 ? body
                        : near ? leg : rgb(0x5f493b);
                    canvas.capsule(point(bone.a), point(bone.b), radius, color, 16);
                }
            };
            auto draw_nodes = [&](int pass)
            {
                for (std::size_t index = 0; index < particles.size(); ++index)
                {
                    const int side = leg_side(index);
                    const bool near = side != 0 && ((side > 0) == right_leg_near);
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
                        const Vec2 center = point(index);
                        if (optional_art_enabled && optional_foot_art.loaded())
                        {
                            const float width = std::max(34.0f, scale * 0.78f);
                            const float height = width
                                * static_cast<float>(optional_foot_art.height)
                                / static_cast<float>(optional_foot_art.width);
                            draw_pixel_art(canvas, optional_foot_art,
                                { center + Vec2{ -width * 0.24f, -height * 0.74f },
                                  { width, height } },
                                near || side == 0 ? 1.0f : 0.58f);
                        }
                        else
                        {
                            const float height = std::max(7.0f, radius * 0.55f);
                            canvas.capsule(center - Vec2{ radius * 0.18f, 0.0f },
                                center + Vec2{ radius * 1.45f, 0.0f },
                                height, color, 14);
                        }
                    }
                    else
                    {
                        const float visual_radius = index == rig.head_node
                            ? std::clamp(radius, 9.0f, 18.0f)
                            : std::clamp(radius * 0.48f, 4.5f, 10.0f);
                        canvas.circle(point(index), visual_radius, color, 22);
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

            draw_bones(0);
            draw_nodes(0);
            draw_bones(1);
            draw_nodes(1);

            if (optional_art_enabled
                && rig.root_node < particles.size()
                && rig.torso_node < particles.size())
            {
                // COMPACT SEGMENTED BODY ARMOR: keep the approved helmet and
                // feet, but replace the oversized translucent bitmap sheet with
                // node-attached geometry that follows the real body and arms.
                const Vec2 root = point(rig.root_node);
                const Vec2 torso = point(rig.torso_node);
                const Vec2 body_axis = normalized(torso - root, { 0.0f, -1.0f });
                const Vec2 body_right{ -body_axis.y, body_axis.x };
                const float torso_length = std::max(24.0f, length(torso - root));

                float shoulder_span = torso_length * 0.82f;
                Vec2 left_shoulder = torso - body_right * shoulder_span * 0.5f;
                Vec2 right_shoulder = torso + body_right * shoulder_span * 0.5f;
                if (rig.active_motor_count >= 8u
                    && rig.motors[4].pivot < particles.size()
                    && rig.motors[6].pivot < particles.size())
                {
                    left_shoulder = point(rig.motors[4].pivot);
                    right_shoulder = point(rig.motors[6].pivot);
                    shoulder_span = std::max(24.0f,
                        length(right_shoulder - left_shoulder));
                }

                const Vec2 chest_bottom = root + body_axis * (torso_length * 0.20f);
                const Vec2 chest_top = torso - body_axis * (torso_length * 0.10f);
                const float chest_radius = std::clamp(
                    std::min(shoulder_span * 0.25f, torso_length * 0.28f),
                    10.0f, 27.0f);
                canvas.capsule(chest_bottom, chest_top, chest_radius + 2.0f,
                    rgb(0x33414c, 0.96f), 20);
                canvas.capsule(chest_bottom, chest_top, chest_radius,
                    rgb(0x8c9aa5, 0.94f), 20);
                canvas.capsule(chest_bottom + body_axis * 2.0f,
                    chest_top - body_axis * 3.0f,
                    std::max(7.0f, chest_radius * 0.62f),
                    rgb(0xaeb9c1, 0.78f), 18);

                const Vec2 indicator_center =
                    chest_bottom + (chest_top - chest_bottom) * 0.55f;
                const float indicator_half = std::clamp(
                    chest_radius * 0.56f, 6.0f, 14.0f);
                canvas.capsule(indicator_center - body_right * indicator_half,
                    indicator_center + body_right * indicator_half,
                    3.2f, rgb(0x0ed7e9), 12);

                const float shoulder_cap_radius = std::clamp(
                    shoulder_span * 0.15f, 7.0f, 14.0f);
                auto shoulder_cap = [&](Vec2 center)
                {
                    canvas.circle(center, shoulder_cap_radius + 1.5f,
                        rgb(0x34434f, 0.96f), 20);
                    canvas.circle(center, shoulder_cap_radius,
                        rgb(0x8b99a5, 0.92f), 20);
                };
                shoulder_cap(left_shoulder);
                shoulder_cap(right_shoulder);

                auto forearm_guard = [&](std::size_t motor_index)
                {
                    if (motor_index >= rig.active_motor_count)
                        return;
                    const sim::MotorConstraint& motor = rig.motors[motor_index];
                    if (!motor.enabled || motor.pivot >= particles.size()
                        || motor.c >= particles.size())
                        return;
                    const Vec2 elbow = point(motor.pivot);
                    const Vec2 hand = point(motor.c);
                    const Vec2 start = elbow + (hand - elbow) * 0.22f;
                    const Vec2 finish = elbow + (hand - elbow) * 0.78f;
                    const float guard_radius = std::clamp(
                        length(hand - elbow) * 0.10f, 3.8f, 7.0f);
                    canvas.capsule(start, finish, guard_radius + 1.0f,
                        rgb(0x34434f, 0.94f), 14);
                    canvas.capsule(start, finish, guard_radius,
                        rgb(0x7f8e9a, 0.88f), 14);
                };
                forearm_guard(5u);
                forearm_guard(7u);
            }
            if (optional_art_enabled && optional_helmet_art.loaded()
                && rig.head_node < particles.size())
            {
                const Vec2 center = point(rig.head_node);
                const float height = std::max(38.0f,
                    particles[rig.head_node].radius * scale * 2.55f);
                const float width = height
                    * static_cast<float>(optional_helmet_art.width)
                    / static_cast<float>(optional_helmet_art.height);
                draw_pixel_art(canvas, optional_helmet_art,
                    { center - Vec2{ width * 0.50f, height * 0.54f },
                      { width, height } }, 0.92f);
            }
            draw_bones(2);
            draw_nodes(2);

            if (show_nodes && optional_art_enabled && optional_weapon_art.loaded()
                && rig.active_motor_count >= 8u)
            {
                std::size_t hand = rig.motors[7].c;
                if (hand < particles.size())
                {
                    const Vec2 anchor = point(hand);
                    const float width = 92.0f;
                    const float height = width
                        * static_cast<float>(optional_weapon_art.height)
                        / static_cast<float>(optional_weapon_art.width);
                    draw_pixel_art(canvas, optional_weapon_art,
                        { anchor + Vec2{ -12.0f, -height * 0.58f },
                          { width, height } }, 0.90f);
                }
            }
        }

        void draw_training_pip(Rect rect)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019, 0.99f), accent_dim, 1.5f);
            add_text(canvas, rect.position + Vec2{ 12.0f, 9.0f },
                "LIVE TRAINING ENVIRONMENT", 0.88f, accent);
            if (!trainer.has_training_preview())
            {
                add_text_fit(canvas, rect.position + Vec2{ 12.0f, 42.0f },
                    "WAITING FOR FIRST INTACT TRAINING FRAME", 0.90f, muted,
                    rect.size.x - 24.0f);
                return;
            }

            const sim::Environment& environment = trainer.training_preview();
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
            const Color state_color = qualification.valid ? green
                : intact && foot_only ? yellow : danger;
            const std::string_view state_text = qualification.valid
                ? "STAGE VALID" : !intact ? "BROKEN RIG"
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
            const float world_height = std::max(1.5f, view_max_y - view_min_y);
            const float scale = view_camera::pip_pixels_per_meter(
                (inner.size.x - 12.0f) / world_width,
                (inner.size.y * 0.78f) / world_height);
            const float camera = (view_min_x + view_max_x) * 0.5f;

            canvas.push_clip(inner.position, inner.position + inner.size);
            std::vector<Vec2> ground_points{};
            ground_points.reserve(81);
            for (int sample = 0; sample <= 80; ++sample)
            {
                const float fraction = static_cast<float>(sample) / 80.0f;
                const float world_x = camera + (fraction - 0.5f) * inner.size.x / scale;
                ground_points.push_back(world_to_screen(
                    { world_x, environment.ground_height_at(world_x) },
                    inner, camera, scale, 0.82f));
            }
            canvas.polyline(ground_points, 3.0f, rgb(0x65727d));
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const Vec2 point = world_to_screen(feature.center,
                    inner, camera, scale, 0.82f);
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
            canvas.pop_clip();

            const std::string pip_metrics = std::format(
                "TOTAL UPDATES {}  LESSON UPDATE {}  DISTANCE {:.1f} M  STEPS {}",
                trainer.metrics().total_updates, trainer.metrics().update,
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
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
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
            add_text_fit(canvas, cursor,
                std::format("DIFFICULTY {:.0f}%   MASTERY TESTS {} / {}",
                    autonomy.difficulty * 100.0f, autonomy.mastery_streak,
                    rl::required_mastery_confirmations(autonomy.stage)),
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
            const std::string training_work_label = progress.sample_budget_complete
                ? std::string("TRAINING SAMPLES READY")
                : std::format("TRAINING WORK {}%",
                    static_cast<int>(std::lround(
                        progress.training_work * 100.0f)));
            add_text_fit(canvas, cursor,
                std::format("{}   MASTERY PASSES {} / {}",
                    training_work_label, autonomy.mastery_streak,
                    rl::required_mastery_confirmations(autonomy.stage)),
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
                    std::format("TOTAL UPDATES {}   COMPLETION {}%",
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
                case sim::CourseStage::uneven:
                    evidence = std::format(
                        "CURRENT EVIDENCE: DISTANCE {}   STRIDE EVENTS {:.0f}   SURVIVAL {:.1f} S",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events,
                        metrics.evaluation_survival);
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
                add_text(canvas, cursor, "THIS RIG", 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("RUNNING TIME {}   LEARNING UPDATES {}",
                        format_duration(rig_lifetime_seconds),
                        ui_layout::lifetime_delta(metrics.update, rig_start_update)),
                    0.76f, white, usable_width, 0.64f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("SIMULATED RUNS {}   PASSED STAGE CHECKS {}   FAILED STAGE CHECKS {}",
                        ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                        ui_layout::lifetime_delta(metrics.total_valid_episodes,
                            rig_start_valid_episodes),
                        ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                            rig_start_invalid_episodes)),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("DISTANCE {}   STEPS {}   FALLS {}",
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
                        static_cast<unsigned>(rig_best_stage) + 1u),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 28.0f;

                add_text(canvas, cursor, "THIS SESSION", 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("RUN TIME {}   TRAINING TIME {}",
                        format_duration(session_runtime_seconds),
                        format_duration(static_cast<float>(std::max(0.0,
                            metrics.total_training_seconds - session_start_training_seconds)))),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("SIMULATED RUNS {}   RESETS {}   ROLLBACKS {}",
                        ui_layout::lifetime_delta(metrics.total_episodes,
                            session_start_episodes),
                        ui_layout::lifetime_delta(metrics.total_resets,
                            session_start_resets),
                        ui_layout::lifetime_delta(autonomy.rollback_count,
                            session_start_rollbacks)),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("DISTANCE {}   COLLISIONS {}   FEATURES CLEARED {}",
                        format_distance(static_cast<float>(std::max(0.0,
                            metrics.total_distance - session_start_distance))),
                        ui_layout::lifetime_delta(metrics.total_collisions,
                            session_start_collisions),
                        ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                            session_start_obstacles)),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 28.0f;

                add_text(canvas, cursor, "ALL TIME", 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("TOTAL UPDATES {}   SIMULATED RUNS {}   PASSED STAGE CHECKS {}",
                        metrics.total_updates, metrics.total_episodes,
                        metrics.total_valid_episodes),
                    0.76f, white, usable_width, 0.62f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("DISTANCE {}   RESETS {}   ROLLBACKS {}",
                        format_distance(static_cast<float>(metrics.total_distance)),
                        metrics.total_resets, autonomy.rollback_count),
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
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 365.0f } },
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
                add_text_fit(canvas, cursor,
                    std::format("RAW TEST SCORE {}   BEST RAW SCORE {}",
                        raw_number(metrics.evaluation_score),
                        raw_number(metrics.best_evaluation_score)),
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
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
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
            const float target_pixels_per_meter = view_camera::fitted_pixels_per_meter(
                viewport.size.y, rig_height, live_zoom_factor);
            live_pixels_per_meter = view_camera::smooth_zoom(
                live_pixels_per_meter, target_pixels_per_meter, dt);
            if (!particles.empty())
            {
                const std::size_t root = environment.blueprint().root_node;
                if (root < particles.size())
                {
                    const float target_camera = particles[root].position.x
                        + view_camera::lookahead_meters(
                            viewport.size.x, live_pixels_per_meter);
                    camera_x = view_camera::smooth_camera(
                        camera_x, target_camera, live_pixels_per_meter, dt);
                }
            }

            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            canvas.push_clip(viewport.position + Vec2{ 1.0f, 1.0f },
                viewport.position + viewport.size - Vec2{ 1.0f, 1.0f });
            draw_course_ground(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_reference(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_features(environment, viewport, camera_x, live_pixels_per_meter);
            draw_creature(environment, viewport, camera_x, live_pixels_per_meter);

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
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const float text_width = telemetry.size.x - 24.0f;
            Vec2 line = telemetry.position + Vec2{ 12.0f, 11.0f };
            add_text_fit(canvas, line,
                std::format("{}  /  {:.0f}%", sim::course_stage_name(autonomy.stage),
                    autonomy.difficulty * 100.0f),
                1.42f, white, text_width, 1.00f);
            line.y += 31.0f;
            add_text_fit(canvas, line,
                std::format("SPEED {}   DIST {}   COURSE {}",
                    format_speed(environment.forward_speed()),
                    format_distance(environment.distance_travelled()),
                    format_distance(environment.course_progress())),
                0.92f, environment.valid_motion() ? green : danger, text_width);
            line.y += 24.0f;
            add_text_fit(canvas, line,
                std::format("REAL STEPS {}   LEG CROSSINGS {}   HEEL STRIKES {}   TOE LIFTS {}",
                    environment.alternating_steps(), environment.limb_crossings(),
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
                std::format("MOTION {}   TOTAL UPDATES {}   LESSON UPDATE {}",
                    sim::invalid_motion_name(environment.invalid_reason()),
                    trainer.metrics().total_updates, trainer.metrics().update),
                0.80f, environment.valid_motion() ? accent : danger, text_width);

            draw_training_pip({ { pip_box.x, pip_box.y },
                { pip_box.width, pip_box.height } });
            add_rounded_rect(canvas, bottom, 8.0f,
                rgb(0x07111b, 0.96f), border, 1.0f);
            add_text_fit(canvas, bottom.position + Vec2{ 11.0f, 10.0f },
                std::format("{}   v{}   VIEW {:.0f} PX/M {}   {}",
                    trainer.has_best_policy()
                        ? "RETAINED CHAMPION PREVIEW"
                        : "CURRENT EXPLORATORY POLICY",
                    RUNNER_VERSION, live_pixels_per_meter,
                    live_zoom_auto ? "AUTO" : "MANUAL",
                    trainer.background_enabled() ? "TRAINING" : "PAUSED"),
                0.86f, trainer.has_best_policy() ? green : yellow,
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

            row.y += 39.0f;
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
            add_text(canvas, rect.position + Vec2{ 14.0f, 119.0f },
                std::format("TRACTION TEST RETENTION {:.3f}  {}",
                    friction, rig_test_loose_ground ? "LOOSE" : "FIRM"),
                0.92f, rig_test_loose_ground ? yellow : green);
            joint_test_input = slider({ rect.position + Vec2{ 14.0f, 157.0f }, { rect.size.x - 28.0f, 36.0f } },
                "MANUAL INPUT  -1 MIN / 0 REST / +1 MAX", joint_test_input, -1.0f, 1.0f, input);
        }

        void draw_blueprint(Rect viewport, const InputState& input)
        {
            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            float maximum_y = 0.0f;
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const float radius = index < blueprint.radii.size()
                    ? blueprint.radii[index] : 0.15f;
                minimum_x = std::min(minimum_x, blueprint.nodes[index].x - radius);
                maximum_x = std::max(maximum_x, blueprint.nodes[index].x + radius);
                maximum_y = std::max(maximum_y, blueprint.nodes[index].y + radius);
            }
            if (!std::isfinite(minimum_x) || !std::isfinite(maximum_x))
            {
                minimum_x = -1.0f;
                maximum_x = 1.0f;
            }
            const float blueprint_camera = 0.5f * (minimum_x + maximum_x);
            const float horizontal_scale = (viewport.size.x - 90.0f)
                / std::max(1.0f, maximum_x - minimum_x + 0.50f);
            const float vertical_scale = (viewport.size.y * 0.70f - 45.0f)
                / std::max(1.0f, maximum_y + 0.30f);
            const float scale = std::clamp(
                std::min(horizontal_scale, vertical_scale), 42.0f, 106.0f);
            const float ground_y = world_to_screen({ 0.0f, 0.0f }, viewport,
                blueprint_camera, scale).y;
            canvas.quad({ viewport.position.x, ground_y }, viewport.position + viewport.size, rgb(0x111820));
            canvas.line({ viewport.position.x, ground_y }, { viewport.position.x + viewport.size.x, ground_y },
                3.0f, rgb(0x475762));

            auto screen = [&](std::size_t index)
            {
                return world_to_screen(blueprint.nodes[index], viewport, blueprint_camera, scale);
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
                return world_to_screen(preview[index], viewport, blueprint_camera, scale);
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

            for (std::size_t bone_index = 0; bone_index < blueprint.bones.size(); ++bone_index)
            {
                const sim::DistanceConstraint& bone = blueprint.bones[bone_index];
                if (bone.a < blueprint.nodes.size() && bone.b < blueprint.nodes.size())
                    canvas.line(screen(bone.a), screen(bone.b),
                        bone_index == static_cast<std::size_t>(selected_bone) ? 22.0f : 17.0f,
                        bone_index == static_cast<std::size_t>(selected_bone) ? accent : rgb(0x835927));
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
                std::string_view foot_label{};
                if (index == blueprint.left_contact_node) foot_label = "L HEEL";
                else if (index == blueprint.right_contact_node) foot_label = "R HEEL";
                else if (blueprint.additional_left_contact_nodes.size() >= 1u
                    && index == blueprint.additional_left_contact_nodes[0]) foot_label = "L BALL";
                else if (blueprint.additional_left_contact_nodes.size() >= 2u
                    && index == blueprint.additional_left_contact_nodes[1]) foot_label = "L TOE";
                else if (blueprint.additional_right_contact_nodes.size() >= 1u
                    && index == blueprint.additional_right_contact_nodes[0]) foot_label = "R BALL";
                else if (blueprint.additional_right_contact_nodes.size() >= 2u
                    && index == blueprint.additional_right_contact_nodes[1]) foot_label = "R TOE";
                if (!foot_label.empty())
                    add_text(canvas, screen(index) + Vec2{ 10.0f, 10.0f }, foot_label, 0.82f, yellow);
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
                        viewport, blueprint_camera, scale));
                }
                canvas.polyline(arc, 4.0f, accent);
                const Vec2 pivot_screen = screen(motor.pivot);
                auto ray = [&](float angle, Color color, float width)
                {
                    canvas.line(pivot_screen,
                        world_to_screen(pivot_world + rotate(reference, angle) * arm_length,
                            viewport, blueprint_camera, scale), width, color);
                };
                ray(motor.minimum_angle, danger, 2.5f);
                ray(motor.maximum_angle, danger, 2.5f);
                ray(motor.neutral_angle, white, 3.0f);
                ray(sim::motor_target_angle(motor, joint_test_input), yellow, 4.0f);
                add_text(canvas, screen(motor.a) + Vec2{ 8.0f, -15.0f }, "A / PARENT", 1.05f, accent);
                add_text(canvas, pivot_screen + Vec2{ 8.0f, -15.0f }, "PIVOT", 1.05f, white);
                add_text(canvas, screen(motor.c) + Vec2{ 8.0f, -15.0f }, "C / DRIVEN", 1.05f, yellow);
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
                    blueprint.nodes.push_back(screen_to_world(input.mouse, viewport, blueprint_camera, scale));
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
                blueprint.nodes[static_cast<std::size_t>(selected_node)] = screen_to_world(input.mouse, viewport, blueprint_camera, scale);
            if (dragging_node && input.left_released)
            {
                dragging_node = false;
                apply_small_rig_change("NODE MOVED");
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
            const float tab = (usable - 18.0f) * 0.25f;
            auto page_button = [&](int slot, std::string_view label, RigPanelPage page)
            {
                if (button({ cursor + Vec2{ static_cast<float>(slot) * (tab + 6.0f), 0.0f },
                    { tab, 35.0f } }, label, input, rig_panel_page == page))
                    rig_panel_page = page;
            };
            page_button(0, "PRESETS", RigPanelPage::presets);
            page_button(1, "STRUCTURE", RigPanelPage::structure);
            page_button(2, "MOTORS", RigPanelPage::motors);
            page_button(3, "TEST", RigPanelPage::test);
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
                preset(0, 0, "HUMANOID", RigPreset::humanoid);
                preset(0, 1, "BIPED", RigPreset::biped);
                preset(1, 0, "SCAFFOLD", RigPreset::scaffold);
                preset(1, 1, "CHICKEN", RigPreset::chicken);
                preset(2, 0, "QUADRUPED", RigPreset::quadruped);
                preset(2, 1, "FOUR-LEG CRAWLER", RigPreset::crawler4);
                preset(3, 0, "HEXAPOD", RigPreset::hexapod);
                preset(3, 1, "MONOPED", RigPreset::monoped);
                cursor.y += 176.0f;
                add_wrapped_text(canvas, cursor,
                    "Selecting a preset restores its authored anatomy. Automatic training tunes control parameters only; it never changes limb length or adds body parts.",
                    0.73f, muted, usable, 2.0f);
                cursor.y += 55.0f;

                const float third = (usable - 12.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "SAVE RIG", input)
                    || input.save_pressed)
                {
                    std::string error{};
                    set_status(blueprint.save(rig_path, error) ? "RIG SAVED" : error);
                }
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f },
                    { third, 35.0f } }, "LOAD RIG", input) || input.load_pressed)
                {
                    std::string error{};
                    blueprint = sim::CreatureBlueprint::load(rig_path, error);
                    rig_preset = RigPreset::custom;
                    trainer.set_blueprint(blueprint, false);
                    set_status(error.empty()
                        ? "CUSTOM RIG LOADED - FRESH BALANCE LESSON STARTED"
                        : error);
                }
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 35.0f } }, "COPY TRAINING RIG", input))
                {
                    blueprint = trainer.blueprint();
                    rig_preset = RigPreset::custom;
                    set_status("CURRENT TRAINING RIG COPIED INTO STRUCTURE EDITOR");
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
                    std::format("CONTROL TUNING {}   ACCEPTED {}   REJECTED {}   ROLLBACKS {}",
                        autonomy.rig_generation, autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes, autonomy.rollback_count),
                    0.74f, accent, usable, 0.60f);
            }
            else if (rig_panel_page == RigPanelPage::structure)
            {
                add_text(canvas, cursor, "MANUAL STRUCTURE EDITING", 1.02f, accent);
                cursor.y += 25.0f;
                add_wrapped_text(canvas, cursor,
                    "Only manual edits change anatomy. Select and drag nodes in the viewport. Shift adds a node, Ctrl connects it, Alt selects a bone.",
                    0.72f, muted, usable, 2.0f);
                cursor.y += 58.0f;
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
                    if (stiffness != selected.stiffness)
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
                for (int index = 0; index < static_cast<int>(sim::action_count); ++index)
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
                float negative = (motor.neutral_angle - motor.minimum_angle)
                    * 180.0f / pi;
                float positive = (motor.maximum_angle - motor.neutral_angle)
                    * 180.0f / pi;
                const float updated_negative = slider({ cursor, { usable, 38.0f } },
                    "NEGATIVE RANGE", negative, 2.0f, 120.0f, input, " DEG");
                if (updated_negative != negative)
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, positive, motor.strength);
                cursor.y += 50.0f;
                const float updated_positive = slider({ cursor, { usable, 38.0f } },
                    "POSITIVE RANGE", positive, 2.0f, 120.0f, input, " DEG");
                if (updated_positive != positive)
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, updated_positive, motor.strength);
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
            else
            {
                add_text(canvas, cursor, "JOINT AND TRACTION TESTS", 1.02f, accent);
                cursor.y += 28.0f;
                const Rect test_card{ cursor, { usable, 205.0f } };
                draw_joint_lab(test_card, input);
                cursor.y += 220.0f;
                add_wrapped_text(canvas, cursor,
                    "GAIT CYCLE is a side-view fore/aft check. A credited walking step must lift from behind, pass the stance support, and land ahead. Test controls never change the saved training policy.",
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
                const ui_layout::Box layout_side =
                    ui_layout::rig_lab_panel_box(layout_content);
                const ui_layout::Box layout_world =
                    ui_layout::rig_lab_world_box(layout_content);
                const Rect side{ { layout_side.x, layout_side.y },
                    { layout_side.width, layout_side.height } };
                const Rect world{ { layout_world.x, layout_world.y },
                    { layout_world.width, layout_world.height } };
                draw_rig_panel(side, input);
                add_rounded_rect(canvas, world, 11.0f, rgb(0x0a131d), border, 1.0f);
                canvas.push_clip(world.position + Vec2{ 1.0f, 1.0f },
                    world.position + world.size - Vec2{ 1.0f, 1.0f });
                draw_blueprint(world, input);
                canvas.pop_clip();
                add_text_fit(canvas, world.position + Vec2{ 18.0f, 16.0f },
                    "SIDE VIEW  |  DRAG NODE  |  SHIFT ADD  |  CTRL CONNECT  |  ALT SELECT BONE",
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
        load_optional("weapon_side.ppm", impl_->optional_weapon_art);

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
