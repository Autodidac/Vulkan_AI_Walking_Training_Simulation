#include "app.hpp"
#include "autonomy.hpp"
#include "simulation.hpp"

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

import epoch.gui;
import epoch.gui.font;
import epoch.gui.rounded_rect;

#ifndef EPOCHRUNNER_VERSION
#define EPOCHRUNNER_VERSION "development"
#endif

namespace epochrunner
{
    namespace gui = epochengine::gui_lib;
    namespace font = epochengine::gui_lib::font;
    namespace rounded = epochengine::gui_lib::rounded_rect;

    namespace
    {
        constexpr float ui_font_scale = 1.22f;

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

        [[nodiscard]] gui::Rect to_gui(Rect rect) noexcept
        {
            return { { rect.position.x, rect.position.y }, { rect.size.x, rect.size.y } };
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

        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill,
            Color outline = {}, float border_width = 0.0f)
        {
            rounded::RoundedRectOptions options{};
            options.bounds = to_gui(rect);
            options.radii = { radius, radius, radius, radius };
            options.border_width = border_width;
            options.segments_per_corner = 6;
            const rounded::RoundedRectMesh mesh = rounded::make_rounded_rect_mesh(options);
            if (!mesh.valid)
            {
                canvas.quad(rect.position, rect.position + rect.size, fill);
                return;
            }
            for (std::size_t index = 0; index + 2 < mesh.fill_indices.size(); index += 3)
            {
                const gui::Vec2 a = mesh.vertices[mesh.fill_indices[index]];
                const gui::Vec2 b = mesh.vertices[mesh.fill_indices[index + 1]];
                const gui::Vec2 c = mesh.vertices[mesh.fill_indices[index + 2]];
                canvas.triangle({ a.x, a.y }, { b.x, b.y }, { c.x, c.y }, fill);
            }
            if (border_width <= 0.0f)
                return;
            for (std::size_t index = 0; index + 2 < mesh.border_indices.size(); index += 3)
            {
                const gui::Vec2 a = mesh.vertices[mesh.border_indices[index]];
                const gui::Vec2 b = mesh.vertices[mesh.border_indices[index + 1]];
                const gui::Vec2 c = mesh.vertices[mesh.border_indices[index + 2]];
                canvas.triangle({ a.x, a.y }, { b.x, b.y }, { c.x, c.y }, outline);
            }
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

        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera_x,
            float pixels_per_meter, float ground_fraction = 0.84f) noexcept
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
            const float ground_y = viewport.position.y + viewport.size.y * 0.84f;
            return {
                camera_x + (screen.x - (viewport.position.x + viewport.size.x * 0.50f)) / pixels_per_meter,
                (ground_y - screen.y) / pixels_per_meter
            };
        }
    }

    struct Application::Impl
    {
        enum class Mode : std::uint8_t { live, rig_lab };
        enum class RigPreset : std::uint8_t { humanoid, biped, chicken, quadruped, monoped, custom };
        enum class RigPanelPage : std::uint8_t { body, motor };
        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };

        render::Canvas canvas{};
        sim::CreatureBlueprint blueprint{ sim::CreatureBlueprint::humanoid() };
        rl::AutonomousTrainer trainer{ blueprint, 64 };
        Mode mode{ Mode::live };
        RigPreset rig_preset{ RigPreset::humanoid };
        JointTestGroup joint_test_group{ JointTestGroup::selected };
        RigPanelPage rig_panel_page{ RigPanelPage::body };
        int selected_node{ -1 };
        int selected_motor{};
        bool dragging_node{};
        bool joint_auto_sweep{};
        bool run_paused{};
        bool rig_edit_pending{};
        std::string rig_edit_reason{};
        float joint_test_input{};
        float joint_test_phase{};
        float camera_x{};
        std::string status{ "AUTOPILOT STARTING" };
        float status_time{ 4.0f };
        bool quit{};
        std::filesystem::path rig_path{ "creature.epochrig" };
        std::filesystem::path policy_path{ "creature.eppo" };
        std::filesystem::path autosave_policy_path{ "epochrunner-v050-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "epochrunner-v050-evolved.epochrig" };
        std::filesystem::path autosave_state_path{ "epochrunner-v050-autonomy.state" };

        [[nodiscard]] std::string_view preset_name() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::humanoid: return "HUMANOID";
            case RigPreset::biped: return "BASIC BIPED";
            case RigPreset::chicken: return "CHICKEN BIPED";
            case RigPreset::quadruped: return "QUADRUPED";
            case RigPreset::monoped: return "MONOPED";
            case RigPreset::custom: return "CUSTOM / EVOLVED";
            }
            return "CUSTOM / EVOLVED";
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

        [[nodiscard]] std::array<std::string_view, 4> motor_names() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::quadruped:
                return { "REAR HIP", "REAR KNEE", "FRONT SHOULDER", "FRONT KNEE" };
            case RigPreset::monoped:
                return { "HIP", "KNEE", "LEFT FOOT", "RIGHT FOOT" };
            case RigPreset::humanoid:
            case RigPreset::biped:
            case RigPreset::chicken:
                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE" };
            case RigPreset::custom:
                return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4" };
            }
            return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4" };
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
            gui::Vec2 measured = font::measure_text(label, scale * ui_font_scale);
            while (measured.x > rect.size.x - 12.0f && scale > 0.95f)
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
            case JointTestGroup::pair_a: return index < 2;
            case JointTestGroup::pair_b: return index >= 2;
            case JointTestGroup::all: return true;
            }
            return false;
        }

        void draw_top_bar(const InputState& input, int width)
        {
            canvas.quad({ 0.0f, 0.0f }, { static_cast<float>(width), 64.0f }, rgb(0x0b1119));
            add_text(canvas, { 18.0f, 17.0f }, "EPOCH RUNNER v" EPOCHRUNNER_VERSION, 2.25f, white);
            add_text(canvas, { 286.0f, 23.0f }, "AUTONOMOUS LOCOMOTION LAB", 1.30f, muted);

            const float tab_width = 190.0f;
            const float start_x = static_cast<float>(width) - tab_width * 2.0f - 18.0f;
            if (button({ { start_x, 10.0f }, { tab_width - 7.0f, 43.0f } },
                "LIVE AUTOPILOT", input, mode == Mode::live))
                mode = Mode::live;
            if (button({ { start_x + tab_width, 10.0f }, { tab_width - 7.0f, 43.0f } },
                "RIG LAB", input, mode == Mode::rig_lab))
                mode = Mode::rig_lab;
        }

        void draw_course_ground(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            std::vector<Vec2> surface{};
            constexpr int samples = 120;
            surface.reserve(samples);
            for (int sample = 0; sample < samples; ++sample)
            {
                const float t = static_cast<float>(sample) / static_cast<float>(samples - 1);
                const float x = camera + (t - 0.5f) * viewport.size.x / scale;
                surface.push_back(world_to_screen({ x, environment.ground_height_at(x) }, viewport, camera, scale));
            }
            std::vector<Vec2> fill{};
            fill.reserve(surface.size() + 2);
            fill.push_back({ viewport.position.x, viewport.position.y + viewport.size.y });
            fill.insert(fill.end(), surface.begin(), surface.end());
            fill.push_back(viewport.position + viewport.size);
            for (std::size_t index = 1; index + 1 < fill.size(); ++index)
                canvas.triangle(fill[0], fill[index], fill[index + 1], rgb(0x111820));
            canvas.polyline(surface, 3.0f, rgb(0x475762));
        }

        void draw_course_reference(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const float progress = environment.course_progress();
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - 2.0f;
            const float right = camera + half_view + 2.0f;

            constexpr float dash_spacing = 1.6f;
            const int first_dash = static_cast<int>(std::floor((left + progress) / dash_spacing));
            const int last_dash = static_cast<int>(std::ceil((right + progress) / dash_spacing));
            for (int index = first_dash; index <= last_dash; ++index)
            {
                const float x0 = static_cast<float>(index) * dash_spacing - progress;
                const float x1 = x0 + 0.72f;
                const Vec2 start = world_to_screen(
                    { x0, environment.ground_height_at(x0) + 0.035f }, viewport, camera, scale);
                const Vec2 end = world_to_screen(
                    { x1, environment.ground_height_at(x1) + 0.035f }, viewport, camera, scale);
                canvas.line(start, end, 3.0f, rgb(0xd6d9c4, 0.82f));
            }

            constexpr float marker_spacing = 10.0f;
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
                const Vec2 top = world_to_screen({ x, ground + 0.72f }, viewport, camera, scale);
                canvas.line(base, top, 4.0f, accent_dim);
                const Rect sign{ top + Vec2{ -43.0f, -22.0f }, { 86.0f, 21.0f } };
                add_rounded_rect(canvas, sign, 4.0f, rgb(0x102431, 0.94f), accent, 1.0f);
                add_text(canvas, sign.position + Vec2{ 5.0f, 5.0f },
                    std::format("{:.0f} M / {:.3f} MI", distance, distance / 1609.344f),
                    0.76f, white);
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
                    add_rounded_rect(canvas, rect, 4.0f,
                        feature.kind == sim::CourseFeatureKind::hurdle ? yellow : accent_dim,
                        feature.kind == sim::CourseFeatureKind::hurdle ? yellow : accent, 1.0f);
                }

                add_text(canvas, feature_screen + Vec2{ -42.0f, -36.0f },
                    sim::course_feature_name(feature.kind), 0.82f,
                    feature.kind == sim::CourseFeatureKind::projectile ? yellow : muted);
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
                if (index == rig.left_contact_node || index == rig.right_contact_node)
                    color = leg;
                canvas.circle(point(index), radius, color, 22);
                if (show_nodes)
                {
                    canvas.circle(point(index), 7.0f,
                        index == static_cast<std::size_t>(selected_node) ? accent : white, 18);
                    add_text(canvas, point(index) + Vec2{ 10.0f, -8.0f }, std::to_string(index), 1.05f, white);
                }
            }
        }

        void draw_live_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const rl::TrainingMetrics& metrics = trainer.metrics();

            add_text(canvas, cursor, "AUTONOMOUS TRAINER", 1.80f, white);
            cursor.y += 39.0f;
            if (button({ cursor, { rect.size.x - 36.0f, 42.0f } },
                trainer.background_enabled() ? "AUTOPILOT ON - CLICK TO PAUSE" : "AUTOPILOT PAUSED - CLICK TO RUN",
                input, trainer.background_enabled()))
            {
                trainer.set_background_enabled(!trainer.background_enabled());
                set_status(trainer.background_enabled() ? "BACKGROUND TRAINING RESUMED" : "BACKGROUND TRAINING PAUSED");
            }
            cursor.y += 54.0f;

            add_text(canvas, cursor, "CURRENT LESSON", 1.15f, muted);
            cursor.y += 21.0f;
            add_text(canvas, cursor, sim::course_stage_name(autonomy.stage), 2.10f, accent);
            cursor.y += 34.0f;
            add_text(canvas, cursor, std::format("DIFFICULTY {:.0f}%   MASTERY {}/3",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak), 1.20f, white);
            cursor.y += 25.0f;
            add_text(canvas, cursor, autonomy.message, 1.05f,
                metrics.evaluation_valid || metrics.evaluation_count == 0 ? muted : danger);
            cursor.y += 38.0f;

            const float third = (rect.size.x - 48.0f) / 3.0f;
            if (button({ cursor, { third, 35.0f } }, "NORMAL", input, trainer.updates_per_cycle() == 1))
                trainer.set_updates_per_cycle(1);
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 35.0f } },
                "FASTER", input, trainer.updates_per_cycle() == 2))
                trainer.set_updates_per_cycle(2);
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 35.0f } },
                "MAX CPU", input, trainer.updates_per_cycle() == 4))
                trainer.set_updates_per_cycle(4);
            cursor.y += 48.0f;

            add_text(canvas, cursor, std::format("UPDATE {}   STEPS {}", metrics.update, metrics.environment_steps), 1.20f, white);
            cursor.y += 24.0f;
            add_text(canvas, cursor, std::format("EVAL SCORE {:+.2f}   DIST {:.2f} M",
                metrics.evaluation_score, metrics.evaluation_distance), 1.20f,
                metrics.evaluation_valid ? green : danger);
            cursor.y += 24.0f;
            add_text(canvas, cursor, std::format("BEST SCORE {:+.2f} @ {}", metrics.best_evaluation_score,
                metrics.best_update), 1.20f, accent);
            cursor.y += 24.0f;
            add_text(canvas, cursor, std::format("SURVIVAL {:.1f} S   STEPS {:.1f}",
                metrics.evaluation_survival, metrics.evaluation_stride_events), 1.20f, white);
            cursor.y += 24.0f;
            add_text(canvas, cursor, std::format("COLLISIONS {:.1f}   AIRBORNE {:.0f}%",
                metrics.evaluation_collisions, metrics.evaluation_airborne_ratio * 100.0f), 1.20f, white);
            cursor.y += 31.0f;

            add_text(canvas, cursor, std::format("{} CPU ROLLOUT THREADS / {} ENVIRONMENTS",
                autonomy.rollout_threads, autonomy.environment_count), 1.12f, muted);
            cursor.y += 23.0f;
            add_text(canvas, cursor, std::format("TRAIN {:.2f} UPDATES/S   MODE {}   QUEUED {}",
                autonomy.updates_per_second, autonomy.speed_mode, autonomy.pending_commands), 1.10f, white);
            cursor.y += 23.0f;
            add_text(canvas, cursor, autonomy.worker_busy ? "TRAINER BUSY" : "TRAINER IDLE", 1.04f,
                autonomy.worker_busy ? yellow : green);
            cursor.y += 23.0f;
            add_text(canvas, cursor, "QUADRUPED-STABLE MOTORS / REAL FEET / SOFT START", 1.04f, muted);
            cursor.y += 23.0f;
            add_text(canvas, cursor, "GPU: VULKAN PRESENTS ONLY THE LIVE BEST AGENT", 1.04f, muted);
            cursor.y += 23.0f;
            add_text(canvas, cursor, std::format("RIG GEN {}   ACCEPTED {}   REJECTED {}",
                autonomy.rig_generation, autonomy.accepted_rig_changes, autonomy.rejected_rig_changes), 1.10f, white);
            cursor.y += 23.0f;
            add_text(canvas, cursor, std::format("AUTO ROLLBACKS {}   NO FLY / FLIP / >50 KM/H",
                autonomy.rollback_count), 1.08f, white);
            cursor.y += 34.0f;

            add_text(canvas, cursor, "CHECKPOINTS / ROLLBACK / EXPLORATION / RIG EVOLUTION: AUTOMATIC", 1.02f, muted);
            cursor.y += 25.0f;
            add_text(canvas, cursor, "A NEW VERIFIED BEST IS APPLIED AT THE NEXT LIVE RUN", 1.02f, muted);
        }

        void draw_live_world(Rect viewport, float dt)
        {
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
            if (!environment.particles().empty())
                camera_x = lerp(camera_x,
                    environment.particles()[environment.blueprint().root_node].position.x + 1.8f, 0.045f);
            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            draw_course_ground(environment, viewport, camera_x, 90.0f);
            draw_course_reference(environment, viewport, camera_x, 90.0f);
            draw_course_features(environment, viewport, camera_x, 90.0f);
            draw_creature(environment, viewport, camera_x, 90.0f);

            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            add_text(canvas, viewport.position + Vec2{ 24.0f, 22.0f },
                std::format("{}  /  {:.0f}%", sim::course_stage_name(autonomy.stage), autonomy.difficulty * 100.0f),
                2.20f, white);
            add_text(canvas, viewport.position + Vec2{ 24.0f, 61.0f },
                std::format("{:.1f} KM/H   ACTUAL {:.1f} M   COURSE {:.1f} M   {}",
                    environment.forward_speed() * 3.6f, environment.distance_travelled(),
                    environment.course_progress(), sim::invalid_motion_name(environment.invalid_reason())),
                1.45f, environment.valid_motion() ? green : danger);
            add_text(canvas, viewport.position + Vec2{ 24.0f, 88.0f },
                std::format("RECOVERY {}   {}/{} SUCCESS",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.recovery_successes(), environment.recovery_events()),
                1.12f, environment.recovering() ? yellow : muted);
            add_text(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 36.0f },
                "LIVE BEST CONTROLLER   v" EPOCHRUNNER_VERSION "   TRAINING STAYS IN THE BACKGROUND", 1.20f, muted);
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
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "PAIR 1+2", input,
                joint_test_group == JointTestGroup::pair_a))
                joint_test_group = JointTestGroup::pair_a;
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "PAIR 3+4", input,
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
                if (index == blueprint.left_contact_node || index == blueprint.right_contact_node)
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
                const float third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "HUMANOID", input, rig_preset == RigPreset::humanoid))
                    use_preset(RigPreset::humanoid);
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 35.0f } }, "BIPED", input,
                    rig_preset == RigPreset::biped))
                    use_preset(RigPreset::biped);
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 35.0f } }, "QUADRUPED", input,
                    rig_preset == RigPreset::quadruped))
                    use_preset(RigPreset::quadruped);
                cursor.y += 43.0f;
                const float half = (rect.size.x - 42.0f) * 0.5f;
                if (button({ cursor, { half, 35.0f } }, "CHICKEN", input, rig_preset == RigPreset::chicken))
                    use_preset(RigPreset::chicken);
                if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 35.0f } }, "MONOPED", input,
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
                for (int index = 0; index < 4; ++index)
                {
                    if (button({ cursor + Vec2{ motor_width * static_cast<float>(index), 0.0f },
                        { motor_width - 4.0f, 35.0f } }, std::to_string(index + 1), input, selected_motor == index))
                    {
                        selected_motor = index;
                        joint_test_group = JointTestGroup::selected;
                    }
                }
                cursor.y += 46.0f;
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
            status_time = std::max(0.0f, status_time - dt);
            if (joint_auto_sweep)
            {
                joint_test_phase += dt;
                joint_test_input = std::sin(joint_test_phase * 1.55f);
            }
            process_shortcuts(input);
            draw_top_bar(input, width);

            const Rect content{ { 10.0f, 74.0f },
                { static_cast<float>(width) - 20.0f, static_cast<float>(height) - 84.0f } };
            if (content.size.x < 700.0f || content.size.y < 500.0f)
            {
                add_text(canvas, { 24.0f, 100.0f }, "WINDOW TOO SMALL", 2.0f, danger);
                return;
            }

            if (mode == Mode::live)
            {
                const float panel_width = std::clamp(content.size.x * 0.30f, 390.0f, 470.0f);
                const Rect world{ content.position, { content.size.x - panel_width - 10.0f, content.size.y } };
                const Rect side{ { content.position.x + world.size.x + 10.0f, content.position.y },
                    { panel_width, content.size.y } };
                draw_live_world(world, dt);
                draw_live_panel(side, input);
            }
            else
            {
                const float panel_width = std::clamp(content.size.x * 0.34f, 470.0f, 560.0f);
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
                const gui::Vec2 measured = font::measure_text(status, scale);
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

    bool Application::initialize(const std::filesystem::path&, std::string& error)
    {
        impl_->trainer.set_autosave_paths(impl_->autosave_policy_path,
            impl_->autosave_rig_path, impl_->autosave_state_path);
        std::string message{};
        const bool resumed = impl_->trainer.load_autosave(message);
        impl_->trainer.synchronize();
        impl_->blueprint = impl_->trainer.blueprint();
        impl_->rig_preset = resumed ? Impl::RigPreset::custom : Impl::RigPreset::humanoid;
        impl_->trainer.set_background_enabled(true);
        impl_->status = message;
        impl_->status_time = 6.0f;
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
