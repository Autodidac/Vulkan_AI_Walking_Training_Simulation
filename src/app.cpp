#include "app.hpp"

#include "autonomy.hpp"
#include "font.hpp"
#include "ui_font.hpp"
#include "ui_layout.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <numbers>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef RUNNER_VERSION
#define RUNNER_VERSION "0.7.4"
#endif

namespace runner
{
    namespace
    {
        constexpr float pi = std::numbers::pi_v<float>;

        [[nodiscard]] Color rgb(std::uint32_t hex, float alpha = 1.0f) noexcept
        {
            return {
                static_cast<float>((hex >> 16u) & 0xffu) / 255.0f,
                static_cast<float>((hex >> 8u) & 0xffu) / 255.0f,
                static_cast<float>(hex & 0xffu) / 255.0f,
                alpha
            };
        }

        [[nodiscard]] bool contains(Rect rect, Vec2 point) noexcept
        {
            return point.x >= rect.position.x && point.y >= rect.position.y
                && point.x <= rect.position.x + rect.size.x
                && point.y <= rect.position.y + rect.size.y;
        }

        [[nodiscard]] float clamp(float value, float minimum, float maximum) noexcept
        {
            return std::clamp(value, minimum, maximum);
        }

        [[nodiscard]] float lerp(float a, float b, float t) noexcept
        {
            return a + (b - a) * t;
        }

        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera,
            float scale, float vertical_anchor = 0.82f) noexcept
        {
            return {
                viewport.position.x + viewport.size.x * 0.5f + (world.x - camera) * scale,
                viewport.position.y + viewport.size.y * vertical_anchor - world.y * scale
            };
        }

        [[nodiscard]] Rect inset(Rect rect, float amount) noexcept
        {
            return {
                rect.position + Vec2{ amount, amount },
                rect.size - Vec2{ amount * 2.0f, amount * 2.0f }
            };
        }

        [[nodiscard]] std::string format_duration(float seconds)
        {
            const int total = std::max(0, static_cast<int>(seconds));
            return std::format("{:02}:{:02}:{:02}", total / 3600,
                (total / 60) % 60, total % 60);
        }

        [[nodiscard]] std::string format_distance(float metres,
            ui_layout::DistanceUnits units = ui_layout::DistanceUnits::metric)
        {
            if (units == ui_layout::DistanceUnits::imperial)
                return std::format("{:.2f} MI", metres / 1609.344f);
            if (metres >= 1000.0f)
                return std::format("{:.2f} KM", metres / 1000.0f);
            return std::format("{:.1f} M", metres);
        }

        void add_rounded_rect(Canvas& canvas, Rect rect, float radius, Color fill,
            Color outline = {}, float outline_width = 0.0f)
        {
            canvas.rounded_rect(rect, radius, fill, outline, outline_width);
        }

        void add_text(Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color)
        {
            font::draw_text(canvas, position, text, scale * ui_font::scale(), color);
        }

        void add_text_fit(Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float available_width)
        {
            float adjusted = scale;
            while (font::measure_text(text, adjusted * ui_font::scale()).x > available_width
                && adjusted > 0.68f)
                adjusted -= 0.04f;
            add_text(canvas, position, text, adjusted, color);
        }
    }

    struct Application::Impl
    {
        enum class Mode
        {
            live,
            rig_lab
        };

        enum class RigPreset
        {
            humanoid,
            biped,
            chicken,
            quadruped,
            crawler4,
            hexapod,
            monoped,
            custom
        };

        enum class JointTestGroup
        {
            selected,
            pair_a,
            pair_b,
            all
        };

        Canvas& canvas;
        rl::PpoTrainer trainer;
        sim::Environment live_environment;
        Mode mode{ Mode::live };
        RigPreset rig_preset{ RigPreset::humanoid };
        sim::CreatureBlueprint blueprint;
        std::string status{ "AUTONOMOUS TRAINING ACTIVE" };
        float status_time{};
        float ui_font_scale{ 1.0f };
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
        std::uint64_t session_start_episodes{};
        std::uint64_t session_start_invalid_episodes{};
        std::uint64_t session_start_resets{};
        std::uint64_t session_start_collisions{};
        std::uint64_t session_start_jumps{};
        std::uint64_t session_start_flips{};
        std::uint64_t session_start_obstacles{};
        double session_start_distance{};
        std::uint8_t rig_best_stage{};
        bool session_stats_initialized{};
        bool rig_edit_pending{};
        int selected_node{ -1 };
        int selected_motor{};
        bool dragging_node{};
        bool joint_auto_sweep{};
        float joint_sweep_phase{};
        JointTestGroup joint_test_group{ JointTestGroup::selected };
        rl::SpeedMode speed_mode{ rl::SpeedMode::normal };
        bool camera_follow{ true };
        float camera_x{};
        float zoom{ 95.0f };
        float training_preview_camera{};
        float training_preview_zoom{ 82.0f };

        explicit Impl(Canvas& canvas_reference)
            : canvas(canvas_reference),
              trainer(sim::CreatureBlueprint::humanoid()),
              live_environment(sim::CreatureBlueprint::humanoid(), 0x1234u),
              blueprint(sim::CreatureBlueprint::humanoid())
        {
            trainer.start();
            tracked_rig_signature = trainer.rig_signature();
        }

        ~Impl()
        {
            trainer.stop();
        }

        [[nodiscard]] const char* preset_name() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::humanoid: return "HUMANOID";
            case RigPreset::biped: return "BIPED";
            case RigPreset::chicken: return "CHICKEN";
            case RigPreset::quadruped: return "QUADRUPED";
            case RigPreset::crawler4: return "FOUR-LEG CRAWLER";
            case RigPreset::hexapod: return "SIX-LEG RIG";
            case RigPreset::monoped: return "MONOPED";
            case RigPreset::custom: return "CUSTOM";
            }
            return "CUSTOM";
        }

        [[nodiscard]] std::array<const char*, sim::action_count> motor_names() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::quadruped:
            case RigPreset::crawler4:
                return { "FRONT LEFT HIP", "FRONT LEFT KNEE", "FRONT RIGHT HIP", "FRONT RIGHT KNEE",
                    "REAR LEFT HIP", "REAR LEFT KNEE", "REAR RIGHT HIP", "REAR RIGHT KNEE" };
            case RigPreset::hexapod:
                return { "FRONT LEFT", "FRONT RIGHT", "MID LEFT", "MID RIGHT",
                    "REAR LEFT", "REAR RIGHT", "UNUSED 7", "UNUSED 8" };
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
                canvas.line(point(bone.a), point(bone.b), 5.0f, rgb(0xb8c0c8));
            }
            for (std::size_t index = 0; index < particles.size(); ++index)
            {
                const sim::Particle& particle = particles[index];
                Color fill = rgb(0x8fc4d7);
                if (rig.is_support_seed(index))
                    fill = rgb(0xe8c76c);
                else if (index == rig.head_node)
                    fill = rgb(0xd8a1a8);
                else if (index == rig.torso_node || index == rig.root_node)
                    fill = rgb(0x84a6b8);
                canvas.circle(point(index), particle.radius * scale, fill, 24);
                if (show_nodes)
                    add_text(canvas, point(index) + Vec2{ 8.0f, -9.0f },
                        std::format("{}", index), 0.84f, white);
            }
        }

        void draw_training_preview(Rect viewport)
        {
            const auto publication = trainer.publication();
            if (!publication || publication->particles.empty()
                || publication->particles.size() != publication->blueprint.nodes.size()
                || !publication->body_integrity_valid)
            {
                add_rounded_rect(canvas, viewport, 8.0f, panel_alt, border, 1.0f);
                add_text(canvas, viewport.position + Vec2{ 18.0f, 18.0f },
                    "TRAINING PREVIEW WAITING FOR A CONNECTED FULL RIG", 0.92f, muted);
                return;
            }

            float minimum_x = std::numeric_limits<float>::max();
            float maximum_x = std::numeric_limits<float>::lowest();
            float minimum_y = std::numeric_limits<float>::max();
            float maximum_y = std::numeric_limits<float>::lowest();
            for (std::size_t index = 0; index < publication->particles.size(); ++index)
            {
                const sim::Particle& particle = publication->particles[index];
                if (!std::isfinite(particle.position.x) || !std::isfinite(particle.position.y))
                    return;
                minimum_x = std::min(minimum_x, particle.position.x - particle.radius);
                maximum_x = std::max(maximum_x, particle.position.x + particle.radius);
                minimum_y = std::min(minimum_y, particle.position.y - particle.radius);
                maximum_y = std::max(maximum_y, particle.position.y + particle.radius);
            }
            const float world_width = std::max(0.8f, maximum_x - minimum_x);
            const float world_height = std::max(0.8f, maximum_y - minimum_y);
            training_preview_camera = 0.5f * (minimum_x + maximum_x);
            training_preview_zoom = std::min(
                (viewport.size.x - 38.0f) / world_width,
                (viewport.size.y - 46.0f) / world_height);
            training_preview_zoom = clamp(training_preview_zoom, 32.0f, 112.0f);

            add_rounded_rect(canvas, viewport, 8.0f, panel_alt, border, 1.0f);
            sim::Environment preview(publication->blueprint, 0x66u);
            preview.apply_publication(*publication);
            draw_course_ground(preview, inset(viewport, 8.0f),
                training_preview_camera, training_preview_zoom);
            draw_creature(preview, inset(viewport, 8.0f),
                training_preview_camera, training_preview_zoom);
        }

        void draw_live_panel(const InputState& input, Rect panel)
        {
            add_rounded_rect(canvas, panel, 10.0f, panel, border, 1.0f);
            Vec2 cursor = panel.position + Vec2{ 18.0f, 18.0f };
            const float usable_width = panel.size.x - 36.0f;
            const rl::TrainingMetrics& metrics = trainer.metrics();
            const rl::AutonomyStatus autonomy = trainer.autonomy_status();

            add_text(canvas, cursor, "AUTONOMOUS RIG TRAINER", 1.34f, white);
            cursor.y += 31.0f;
            add_text_fit(canvas, cursor,
                std::format("STAGE {}  {}", static_cast<unsigned>(autonomy.stage) + 1u,
                    sim::course_stage_name(autonomy.stage)),
                0.98f, accent, usable_width);
            cursor.y += 27.0f;
            add_text_fit(canvas, cursor,
                std::format("REWARD {:.3f}  SPEED {:.2f} M/S",
                    metrics.mean_reward, metrics.mean_speed),
                0.94f, white, usable_width);
            cursor.y += 26.0f;
            add_text_fit(canvas, cursor,
                std::format("VALID {:.0f}%  QUALITY {:.3f}",
                    metrics.valid_ratio * 100.0f, metrics.mean_quality),
                0.94f, white, usable_width);
            cursor.y += 30.0f;

            const float button_width = (usable_width - 12.0f) / 3.0f;
            if (button({ cursor, { button_width, 42.0f } }, "NORMAL", input,
                    speed_mode == rl::SpeedMode::normal))
            {
                speed_mode = rl::SpeedMode::normal;
                trainer.set_speed_mode(speed_mode);
            }
            if (button({ cursor + Vec2{ button_width + 6.0f, 0.0f },
                    { button_width, 42.0f } }, "FASTER", input,
                    speed_mode == rl::SpeedMode::faster))
            {
                speed_mode = rl::SpeedMode::faster;
                trainer.set_speed_mode(speed_mode);
            }
            if (button({ cursor + Vec2{ (button_width + 6.0f) * 2.0f, 0.0f },
                    { button_width, 42.0f } }, "MAX", input,
                    speed_mode == rl::SpeedMode::maximum))
            {
                speed_mode = rl::SpeedMode::maximum;
                trainer.set_speed_mode(speed_mode);
            }
            cursor.y += 56.0f;

            const std::string unit_button = distance_units == ui_layout::DistanceUnits::metric
                ? "METRIC 0.25 KM" : "IMPERIAL 0.25 MI";
            if (button({ cursor, { usable_width, 40.0f } }, unit_button, input, true))
                distance_units = distance_units == ui_layout::DistanceUnits::metric
                    ? ui_layout::DistanceUnits::imperial
                    : ui_layout::DistanceUnits::metric;
            cursor.y += 53.0f;

            add_text_fit(canvas, cursor, std::format("RIG {}  UPD {}  ENV {}  BEST STAGE {}",
                format_duration(rig_lifetime_seconds),
                ui_layout::lifetime_delta(metrics.update, rig_start_update),
                ui_layout::lifetime_delta(metrics.environment_steps, rig_start_environment_steps),
                static_cast<unsigned>(rig_best_stage) + 1u), 0.94f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("RIG EPS {}  VALID {}  INVALID {}  DIST {}",
                ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                ui_layout::lifetime_delta(metrics.total_valid_episodes, rig_start_valid_episodes),
                ui_layout::lifetime_delta(metrics.total_invalid_episodes, rig_start_invalid_episodes),
                format_distance(static_cast<float>(std::max(0.0,
                    metrics.total_distance - rig_start_distance)), distance_units)),
                0.90f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("RIG STEPS {}  FALLS {}  COLL {}  OBS {}",
                ui_layout::lifetime_delta(metrics.total_alternating_steps, rig_start_steps),
                ui_layout::lifetime_delta(metrics.total_falls, rig_start_falls),
                ui_layout::lifetime_delta(metrics.total_collisions, rig_start_collisions),
                ui_layout::lifetime_delta(metrics.total_obstacles_passed, rig_start_obstacles)),
                0.90f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("RIG JUMP {} / LAND {}  FLIPS {}",
                ui_layout::lifetime_delta(metrics.total_powered_jumps, rig_start_jumps),
                ui_layout::lifetime_delta(metrics.total_landed_jumps, rig_start_landings),
                ui_layout::lifetime_delta(metrics.total_landed_flips, rig_start_flips)),
                0.90f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("SESSION {}  EPS {}  BAD {}  DIST {}",
                format_duration(session_runtime_seconds),
                ui_layout::lifetime_delta(metrics.total_episodes, session_start_episodes),
                ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                    session_start_invalid_episodes),
                format_distance(static_cast<float>(std::max(0.0,
                    metrics.total_distance - session_start_distance)), distance_units)),
                0.88f, muted, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("ALL UPD {} ENV {} EPS {} RESET {} ROLLBACK {}",
                metrics.update, metrics.environment_steps, metrics.total_episodes,
                metrics.total_resets, autonomy.rollback_count), 0.86f, muted, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("ALL DIST {} COLL {} JUMP {} FLIP {} OBS {}",
                format_distance(static_cast<float>(metrics.total_distance), distance_units),
                metrics.total_collisions, metrics.total_powered_jumps,
                metrics.total_landed_flips, metrics.total_obstacles_passed),
                0.84f, muted, usable_width);
            cursor.y += 34.0f;

            add_text_fit(canvas, cursor,
                std::format("IMI {:.2f}  PERT {:.2f}  ROLLBACKS {}",
                    autonomy.imitation_weight, autonomy.perturbation_scale,
                    autonomy.rollback_count), 0.88f, muted, usable_width);
            cursor.y += 26.0f;
            add_text_fit(canvas, cursor,
                std::format("ACCEPT {}  REJECT {}  INVALID {}",
                    autonomy.accepted_rig_changes, autonomy.rejected_rig_changes,
                    autonomy.invalid_episode_count), 0.88f, muted, usable_width);
            cursor.y += 30.0f;

            if (status_time > 0.0f)
                add_text_fit(canvas, cursor, status, 0.88f, yellow, usable_width);
        }

        void draw_rig_lab(const InputState& input, Rect viewport, Rect panel)
        {
            add_rounded_rect(canvas, panel, 10.0f, panel_alt, border, 1.0f);
            Vec2 cursor = panel.position + Vec2{ 18.0f, 18.0f };
            const float usable_width = panel.size.x - 36.0f;

            add_text(canvas, cursor, "RIG PRESETS", 1.30f, white);
            cursor.y += 35.0f;
            const std::array<std::pair<RigPreset, const char*>, 7> presets = {
                std::pair{ RigPreset::humanoid, "HUMANOID" },
                std::pair{ RigPreset::biped, "BIPED" },
                std::pair{ RigPreset::chicken, "CHICKEN" },
                std::pair{ RigPreset::quadruped, "QUADRUPED" },
                std::pair{ RigPreset::crawler4, "CRAWLER 4" },
                std::pair{ RigPreset::hexapod, "HEXAPOD" },
                std::pair{ RigPreset::monoped, "MONOPED" }
            };
            for (const auto& [preset, label] : presets)
            {
                if (button({ cursor, { usable_width, 38.0f } }, label, input,
                        rig_preset == preset))
                    use_preset(preset);
                cursor.y += 45.0f;
            }

            cursor.y += 8.0f;
            add_text(canvas, cursor, "JOINT TEST", 1.24f, white);
            cursor.y += 32.0f;
            const auto names = motor_names();
            selected_motor = std::clamp(selected_motor, 0,
                static_cast<int>(sim::action_count - 1));
            if (button({ cursor, { usable_width, 38.0f } }, names[selected_motor], input, true))
                selected_motor = (selected_motor + 1) % static_cast<int>(sim::action_count);
            cursor.y += 48.0f;

            const float half = (usable_width - 8.0f) * 0.5f;
            if (button({ cursor, { half, 38.0f } }, "SELECTED", input,
                    joint_test_group == JointTestGroup::selected))
                joint_test_group = JointTestGroup::selected;
            if (button({ cursor + Vec2{ half + 8.0f, 0.0f }, { half, 38.0f } },
                    "PAIR A", input, joint_test_group == JointTestGroup::pair_a))
                joint_test_group = JointTestGroup::pair_a;
            cursor.y += 46.0f;
            if (button({ cursor, { half, 38.0f } }, "PAIR B", input,
                    joint_test_group == JointTestGroup::pair_b))
                joint_test_group = JointTestGroup::pair_b;
            if (button({ cursor + Vec2{ half + 8.0f, 0.0f }, { half, 38.0f } },
                    "ALL", input, joint_test_group == JointTestGroup::all))
                joint_test_group = JointTestGroup::all;
            cursor.y += 52.0f;

            joint_auto_sweep = button({ cursor, { usable_width, 40.0f } },
                joint_auto_sweep ? "STOP JOINT SWEEP" : "START JOINT SWEEP",
                input, joint_auto_sweep) ? !joint_auto_sweep : joint_auto_sweep;
            cursor.y += 52.0f;

            const sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(selected_motor)];
            add_text_fit(canvas, cursor,
                std::format("LIMIT {:.0f}..{:.0f} DEG  STRENGTH {:.2f}",
                    motor.minimum_angle * 180.0f / pi,
                    motor.maximum_angle * 180.0f / pi,
                    motor.strength), 0.88f, muted, usable_width);
            cursor.y += 28.0f;
            add_text_fit(canvas, cursor,
                std::format("NODE {} / PIVOT {} / NODE {}",
                    motor.a, motor.pivot, motor.c), 0.88f, muted, usable_width);
            cursor.y += 34.0f;

            if (button({ cursor, { usable_width, 38.0f } },
                    "TEST JOINT LIMITS", input, false,
                    blueprint.active_motor_count > 0))
            {
                joint_auto_sweep = true;
                set_status("JOINT LIMIT TEST RUNNING WITHOUT BLOCKING TRAINING");
            }
            cursor.y += 48.0f;

            if (button({ cursor, { usable_width, 38.0f } },
                    "DELETE SELECTED NODE", input, false,
                    selected_node >= 0 && selected_node < static_cast<int>(blueprint.nodes.size())))
            {
                if (!delete_selected_node())
                    set_status("NODE DELETE REJECTED");
            }

            draw_rig_editor(input, viewport);
        }

        [[nodiscard]] bool delete_selected_node()
        {
            if (selected_node < 0 || selected_node >= static_cast<int>(blueprint.nodes.size()))
                return false;
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
            if (removed < blueprint.radii.size())
                blueprint.radii.erase(blueprint.radii.begin() + selected_node);
            blueprint.bones.erase(std::remove_if(blueprint.bones.begin(), blueprint.bones.end(),
                [removed](const sim::DistanceConstraint& bone)
                {
                    return bone.a == removed || bone.b == removed;
                }), blueprint.bones.end());
            for (sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a > removed) --bone.a;
                if (bone.b > removed) --bone.b;
            }
            for (sim::MotorConstraint& motor : blueprint.motors)
            {
                if (motor.a > removed) --motor.a;
                if (motor.pivot > removed) --motor.pivot;
                if (motor.c > removed) --motor.c;
            }
            auto remap = [removed](std::uint16_t& node)
            {
                if (node > removed) --node;
            };
            remap(blueprint.root_node);
            remap(blueprint.torso_node);
            remap(blueprint.head_node);
            remap(blueprint.left_contact_node);
            remap(blueprint.right_contact_node);
            for (std::uint16_t& node : blueprint.additional_left_contact_nodes) remap(node);
            for (std::uint16_t& node : blueprint.additional_right_contact_nodes) remap(node);
            selected_node = -1;
            apply_small_rig_change("NODE DELETED");
            return true;
        }

        void draw_rig_editor(const InputState& input, Rect viewport)
        {
            add_rounded_rect(canvas, viewport, 10.0f, rgb(0x0c131c), border, 1.0f);
            const Rect inner = inset(viewport, 16.0f);
            draw_course_ground(live_environment, inner, camera_x, zoom);
            draw_creature(live_environment, inner, camera_x, zoom, true);

            if (input.left_pressed)
            {
                float best_distance = 18.0f;
                int best = -1;
                for (std::size_t index = 0; index < live_environment.particles().size(); ++index)
                {
                    const Vec2 screen = world_to_screen(live_environment.particles()[index].position,
                        inner, camera_x, zoom);
                    const float distance = length(screen - input.mouse);
                    if (distance < best_distance)
                    {
                        best_distance = distance;
                        best = static_cast<int>(index);
                    }
                }
                selected_node = best;
                dragging_node = selected_node >= 0;
            }

            if (!input.left_down)
                dragging_node = false;
            if (dragging_node && selected_node >= 0
                && selected_node < static_cast<int>(blueprint.nodes.size()))
            {
                Vec2 world{
                    camera_x + (input.mouse.x - inner.position.x - inner.size.x * 0.5f) / zoom,
                    (inner.position.y + inner.size.y * 0.82f - input.mouse.y) / zoom
                };
                blueprint.nodes[static_cast<std::size_t>(selected_node)] = world;
                apply_small_rig_change("NODE MOVED");
            }
        }

        void update(const InputState& input, float dt, int width, int height)
        {
            status_time = std::max(0.0f, status_time - dt);
            session_runtime_seconds += std::max(0.0f, dt);
            ui_font_scale = ui_font::scale();

            const rl::TrainingMetrics& current_metrics = trainer.metrics();
            if (!session_stats_initialized)
            {
                session_stats_initialized = true;
                session_start_episodes = current_metrics.total_episodes;
                session_start_invalid_episodes = current_metrics.total_invalid_episodes;
                session_start_resets = current_metrics.total_resets;
                session_start_collisions = current_metrics.total_collisions;
                session_start_jumps = current_metrics.total_powered_jumps;
                session_start_flips = current_metrics.total_landed_flips;
                session_start_obstacles = current_metrics.total_obstacles_passed;
                session_start_distance = current_metrics.total_distance;
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
                rig_best_stage = static_cast<std::uint8_t>(trainer.autonomy_status().stage);
            }
            else
            {
                rig_lifetime_seconds += std::max(0.0f, dt);
                rig_best_stage = std::max(rig_best_stage,
                    static_cast<std::uint8_t>(trainer.autonomy_status().stage));
            }
            if (joint_auto_sweep)
            {
                joint_sweep_phase += dt * 0.75f;
                const auto names = motor_names();
                std::array<float, sim::action_count> action{};
                for (std::size_t index = 0; index < action.size(); ++index)
                {
                    if (test_motor_active(static_cast<int>(index)))
                        action[index] = std::sin(joint_sweep_phase + static_cast<float>(index) * 0.4f);
                }
                (void)names;
                (void)live_environment.step(action);
            }
            else
            {
                const auto publication = trainer.publication();
                if (publication && publication->body_integrity_valid)
                    live_environment.apply_publication(*publication);
            }

            if (camera_follow)
                camera_x = live_environment.particles().empty() ? 0.0f
                    : live_environment.particles()[live_environment.blueprint().root_node].position.x;

            if (input.wheel_y != 0.0f)
                zoom = clamp(zoom + input.wheel_y * 7.0f, 42.0f, 145.0f);
            if (input.middle_down)
            {
                camera_follow = false;
                camera_x -= input.mouse_delta.x / zoom;
            }

            draw(width, height, input);
        }

        void draw(int width, int height, const InputState& input)
        {
            canvas.clear(background);
            draw_top_bar(input, width);

            const float panel_width = width >= 1440 ? 430.0f : 380.0f;
            const Rect panel{ { 14.0f, 86.0f },
                { panel_width, static_cast<float>(height) - 100.0f } };
            const Rect viewport{ { panel.position.x + panel.size.x + 14.0f, 86.0f },
                { static_cast<float>(width) - panel.size.x - 42.0f,
                  static_cast<float>(height) - 100.0f } };

            if (mode == Mode::live)
            {
                draw_live_panel(input, panel);
                draw_course_ground(live_environment, viewport, camera_x, zoom);
                draw_course_reference(live_environment, viewport, camera_x, zoom);
                draw_course_features(live_environment, viewport, camera_x, zoom);
                draw_creature(live_environment, viewport, camera_x, zoom);
                draw_training_preview({
                    { viewport.position.x + 18.0f, viewport.position.y + 18.0f },
                    { std::min(420.0f, viewport.size.x * 0.42f),
                      std::min(290.0f, viewport.size.y * 0.38f) }
                });
            }
            else
            {
                draw_rig_lab(input, viewport, panel);
            }
        }
    };

    Application::Application(Canvas& canvas)
        : impl_(std::make_unique<Impl>(canvas))
    {
    }

    Application::~Application() = default;

    void Application::frame(const InputState& input, float dt, int width, int height)
    {
        impl_->update(input, dt, width, height);
    }
}
