#include "app.hpp"
#include "ppo.hpp"
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
import epoch.gui.image;
import epoch.gui.input;
import epoch.gui.rounded_rect;

namespace epochrunner
{
    namespace gui = epochengine::gui_lib;
    namespace font = epochengine::gui_lib::font;
    namespace image = epochengine::gui_lib::image;
    namespace gui_input = epochengine::gui_lib::input;
    namespace rounded = epochengine::gui_lib::rounded_rect;

    namespace
    {
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

        [[nodiscard]] Rect from_gui(gui::Rect rect) noexcept
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
        constexpr Color muted{ 0.55f, 0.60f, 0.67f, 1.0f };
        constexpr Color panel{ 0.064f, 0.076f, 0.098f, 0.97f };
        constexpr Color panel_alt{ 0.085f, 0.100f, 0.126f, 1.0f };
        constexpr Color border{ 0.16f, 0.19f, 0.24f, 1.0f };
        constexpr Color accent{ 0.20f, 0.72f, 0.92f, 1.0f };
        constexpr Color accent_dim{ 0.12f, 0.35f, 0.48f, 1.0f };
        constexpr Color danger{ 0.93f, 0.28f, 0.30f, 1.0f };
        constexpr Color yellow{ 0.95f, 0.74f, 0.18f, 1.0f };
        constexpr Color chicken_body{ 0.88f, 0.62f, 0.20f, 1.0f };
        constexpr Color chicken_light{ 0.98f, 0.83f, 0.38f, 1.0f };
        constexpr Color chicken_leg{ 0.88f, 0.40f, 0.13f, 1.0f };

        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill, Color outline = {}, float border_width = 0.0f)
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
            if (border_width > 0.0f)
            {
                for (std::size_t index = 0; index + 2 < mesh.border_indices.size(); index += 3)
                {
                    const gui::Vec2 a = mesh.vertices[mesh.border_indices[index]];
                    const gui::Vec2 b = mesh.vertices[mesh.border_indices[index + 1]];
                    const gui::Vec2 c = mesh.vertices[mesh.border_indices[index + 2]];
                    canvas.triangle({ a.x, a.y }, { b.x, b.y }, { c.x, c.y }, outline);
                }
            }
        }

        void add_text(render::Canvas& canvas, Vec2 position, std::string_view text, float scale, Color color)
        {
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

        void add_image(render::Canvas& canvas, const image::Image& bitmap, Rect viewport)
        {
            const image::RasterImageLayout layout = image::make_raster_image_layout(bitmap, to_gui(viewport), image::ImageFit::contain, 2.0f);
            if (!layout.valid)
                return;
            for (std::uint32_t y = 0; y < bitmap.height; ++y)
            {
                for (std::uint32_t x = 0; x < bitmap.width; ++x)
                {
                    const image::Rgba8* pixel = bitmap.pixel(x, y);
                    if (pixel == nullptr || pixel->a == 0)
                        continue;
                    const gui::Rect pixel_rect = image::raster_pixel_rect(layout, x, y, 0.4f);
                    const Color color{
                        static_cast<float>(pixel->r) / 255.0f,
                        static_cast<float>(pixel->g) / 255.0f,
                        static_cast<float>(pixel->b) / 255.0f,
                        static_cast<float>(pixel->a) / 255.0f
                    };
                    const Rect rect = from_gui(pixel_rect);
                    canvas.quad(rect.position, rect.position + rect.size, color);
                }
            }
        }

        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera_x, float pixels_per_meter) noexcept
        {
            const float ground_y = viewport.position.y + viewport.size.y * 0.84f;
            return {
                viewport.position.x + viewport.size.x * 0.50f + (world.x - camera_x) * pixels_per_meter,
                ground_y - world.y * pixels_per_meter
            };
        }

        [[nodiscard]] Vec2 screen_to_world(Vec2 screen, Rect viewport, float camera_x, float pixels_per_meter) noexcept
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
        enum class Mode : std::uint8_t { editor, training, run };

        render::Canvas canvas{};
        sim::CreatureBlueprint blueprint{ sim::CreatureBlueprint::chicken() };
        rl::PpoTrainer trainer{ blueprint, 32 };
        image::Image icon{};
        gui_input::InputTracker input_tracker{};
        Mode mode{ Mode::editor };
        bool training{};
        bool run_paused{};
        int updates_per_frame{ 1 };
        int selected_node{ -1 };
        int selected_motor{};
        bool dragging_node{};
        float camera_x{};
        std::string status{ "READY" };
        float status_time{};
        bool quit{};
        std::filesystem::path asset_directory{};
        std::filesystem::path rig_path{ "chicken.epochrig" };
        std::filesystem::path policy_path{ "chicken.eppo" };
        std::array<float, 4> scripted_phase{};

        [[nodiscard]] bool button(Rect rect, std::string_view label, const InputState& input, bool active = false, bool enabled = true)
        {
            const bool hovered = contains(rect, input.mouse);
            Color fill = active ? accent_dim : panel_alt;
            if (hovered && enabled)
                fill = active ? rgb(0x1b7998) : rgb(0x1a2531);
            if (!enabled)
                fill = rgb(0x10151d);
            add_rounded_rect(canvas, rect, 7.0f, fill, active ? accent : border, 1.0f);
            const gui::Vec2 measured = font::measure_text(label, 1.7f);
            add_text(canvas,
                { rect.position.x + (rect.size.x - measured.x) * 0.5f, rect.position.y + (rect.size.y - measured.y) * 0.5f },
                label, 1.7f, enabled ? white : muted);
            return enabled && hovered && input.left_pressed;
        }

        float slider(Rect rect, std::string_view label, float value, float minimum, float maximum, const InputState& input)
        {
            add_text(canvas, rect.position, label, 1.45f, muted);
            Rect track{ { rect.position.x, rect.position.y + 19.0f }, { rect.size.x, 8.0f } };
            add_rounded_rect(canvas, track, 4.0f, rgb(0x101820), border, 1.0f);
            float fraction = (value - minimum) / std::max(0.0001f, maximum - minimum);
            fraction = clamp(fraction, 0.0f, 1.0f);
            add_rounded_rect(canvas, { track.position, { track.size.x * fraction, track.size.y } }, 4.0f, accent);
            canvas.circle({ track.position.x + track.size.x * fraction, track.position.y + 4.0f }, 7.0f, white, 16);
            if (input.left_down && contains({ { track.position.x - 8.0f, track.position.y - 8.0f }, { track.size.x + 16.0f, 24.0f } }, input.mouse))
            {
                fraction = clamp((input.mouse.x - track.position.x) / track.size.x, 0.0f, 1.0f);
                value = lerp(minimum, maximum, fraction);
            }
            add_text(canvas, { rect.position.x + rect.size.x - 64.0f, rect.position.y }, std::format("{:.2f}", value), 1.4f, white);
            return value;
        }

        void graph(Rect rect, std::string_view title, std::span<const float> values, Color graph_color)
        {
            add_rounded_rect(canvas, rect, 9.0f, rgb(0x0d141d), border, 1.0f);
            add_text(canvas, rect.position + Vec2{ 10.0f, 8.0f }, title, 1.45f, muted);
            if (values.size() < 2)
                return;
            float minimum = *std::min_element(values.begin(), values.end());
            float maximum = *std::max_element(values.begin(), values.end());
            if (std::abs(maximum - minimum) < 1.0e-4f)
            {
                minimum -= 1.0f;
                maximum += 1.0f;
            }
            std::vector<Vec2> points{};
            points.reserve(values.size());
            const Rect plot{ rect.position + Vec2{ 10.0f, 30.0f }, rect.size - Vec2{ 20.0f, 40.0f } };
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                const float x = plot.position.x + plot.size.x * static_cast<float>(index) / static_cast<float>(values.size() - 1);
                const float normalized_value = (values[index] - minimum) / (maximum - minimum);
                const float y = plot.position.y + plot.size.y * (1.0f - normalized_value);
                points.push_back({ x, y });
            }
            canvas.polyline(points, 2.0f, graph_color);
            add_text(canvas, { plot.position.x, plot.position.y }, std::format("{:.1f}", maximum), 1.1f, muted);
            add_text(canvas, { plot.position.x, plot.position.y + plot.size.y - 8.0f }, std::format("{:.1f}", minimum), 1.1f, muted);
        }

        void set_status(std::string text)
        {
            status = std::move(text);
            status_time = 3.0f;
        }

        void draw_top_bar(const InputState& input, int width)
        {
            canvas.quad({ 0.0f, 0.0f }, { static_cast<float>(width), 56.0f }, rgb(0x0b1119));
            add_text(canvas, { 18.0f, 16.0f }, "EPOCH RUNNER", 2.25f, white);
            add_text(canvas, { 186.0f, 20.0f }, "VULKAN + EPOCHGUI + PPO", 1.35f, muted);

            const float tab_width = 116.0f;
            const float start_x = static_cast<float>(width) - tab_width * 3.0f - 18.0f;
            if (button({ { start_x, 10.0f }, { tab_width - 6.0f, 36.0f } }, "EDITOR", input, mode == Mode::editor))
                mode = Mode::editor;
            if (button({ { start_x + tab_width, 10.0f }, { tab_width - 6.0f, 36.0f } }, "TRAIN", input, mode == Mode::training))
                mode = Mode::training;
            if (button({ { start_x + tab_width * 2.0f, 10.0f }, { tab_width - 6.0f, 36.0f } }, "RUN", input, mode == Mode::run))
                mode = Mode::run;
        }

        void draw_ground(Rect viewport, float camera, float scale)
        {
            const float ground_y = world_to_screen({ camera, 0.0f }, viewport, camera, scale).y;
            canvas.quad({ viewport.position.x, ground_y }, viewport.position + viewport.size, rgb(0x111820));
            canvas.line({ viewport.position.x, ground_y }, { viewport.position.x + viewport.size.x, ground_y }, 3.0f, rgb(0x41505b));
            const float first = std::floor((camera - viewport.size.x / scale * 0.5f) / 2.0f) * 2.0f;
            const float last = camera + viewport.size.x / scale * 0.5f;
            for (float marker = first; marker <= last; marker += 2.0f)
            {
                const Vec2 screen = world_to_screen({ marker, 0.0f }, viewport, camera, scale);
                canvas.line({ screen.x, ground_y }, { screen.x, ground_y + 12.0f }, 1.0f, rgb(0x34404a));
            }
        }

        void draw_creature(const sim::Environment& environment, Rect viewport, float camera, float scale, float alpha = 1.0f, bool joints = false)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.size() < 8)
                return;
            auto p = [&](std::size_t index) { return world_to_screen(particles[index].position, viewport, camera, scale); };
            for (const sim::DistanceConstraint& bone : rig.bones)
            {
                if (bone.a >= particles.size() || bone.b >= particles.size())
                    continue;
                Color color = chicken_body;
                float radius = 0.17f * scale;
                if ((bone.a == 0 && (bone.b == 3 || bone.b == 5)) || bone.a == 3 || bone.a == 5)
                {
                    color = chicken_leg;
                    radius = 0.11f * scale;
                }
                if (bone.b == 2)
                {
                    color = chicken_light;
                    radius = 0.22f * scale;
                }
                canvas.capsule(p(bone.a), p(bone.b), radius, with_alpha(color, alpha), 14);
            }
            canvas.circle(p(1), 0.44f * scale, with_alpha(chicken_body, alpha), 24);
            canvas.circle(p(2), 0.32f * scale, with_alpha(chicken_light, alpha), 24);
            const Vec2 head_position = p(2);
            canvas.triangle(head_position + Vec2{ 0.26f * scale, -0.05f * scale },
                head_position + Vec2{ 0.52f * scale, 0.05f * scale },
                head_position + Vec2{ 0.26f * scale, 0.15f * scale }, with_alpha(chicken_leg, alpha));
            canvas.circle(head_position + Vec2{ 0.12f * scale, -0.08f * scale }, 0.035f * scale, with_alpha(rgb(0x15181d), alpha), 12);
            canvas.circle(head_position + Vec2{ -0.10f * scale, -0.30f * scale }, 0.08f * scale, with_alpha(danger, alpha), 12);
            canvas.line(p(4), p(4) + Vec2{ 0.35f * scale, 0.0f }, 0.07f * scale, with_alpha(chicken_leg, alpha));
            canvas.line(p(6), p(6) + Vec2{ 0.35f * scale, 0.0f }, 0.07f * scale, with_alpha(chicken_leg, alpha));
            if (joints)
            {
                for (std::size_t index = 0; index < particles.size(); ++index)
                {
                    const Vec2 screen = p(index);
                    canvas.circle(screen, 6.0f, index == static_cast<std::size_t>(selected_node) ? accent : white, 16);
                    canvas.circle(screen, 2.4f, rgb(0x0b1119), 12);
                }
            }
        }

        void draw_blueprint(Rect viewport, const InputState& input)
        {
            constexpr float scale = 88.0f;
            draw_ground(viewport, 0.0f, scale);
            auto screen_position = [&](std::size_t index)
            {
                return world_to_screen(blueprint.nodes[index], viewport, 0.0f, scale);
            };

            for (const sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a < blueprint.nodes.size() && bone.b < blueprint.nodes.size())
                    canvas.line(screen_position(bone.a), screen_position(bone.b), 18.0f, rgb(0x835927));
            }
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const Vec2 position = screen_position(index);
                const float radius = index < blueprint.radii.size() ? blueprint.radii[index] * scale : 12.0f;
                canvas.circle(position, radius, index == 2 ? chicken_light : chicken_body, 24);
                canvas.circle(position, 7.0f, index == static_cast<std::size_t>(selected_node) ? accent : white, 16);
                add_text(canvas, position + Vec2{ 10.0f, -7.0f }, std::to_string(index), 1.2f, white);
            }

            if (input.left_pressed && contains(viewport, input.mouse))
            {
                int hit = -1;
                float best_distance = 18.0f;
                for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
                {
                    const float distance = length(screen_position(index) - input.mouse);
                    if (distance < best_distance)
                    {
                        best_distance = distance;
                        hit = static_cast<int>(index);
                    }
                }
                if (input.shift && hit < 0 && blueprint.nodes.size() < 128)
                {
                    blueprint.nodes.push_back(screen_to_world(input.mouse, viewport, 0.0f, scale));
                    blueprint.radii.push_back(0.16f);
                    selected_node = static_cast<int>(blueprint.nodes.size() - 1);
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
                        trainer.set_blueprint(blueprint);
                        set_status("BONE CONNECTED");
                    }
                }
                else
                {
                    selected_node = hit;
                    dragging_node = hit >= 0;
                }
            }
            if (dragging_node && input.left_down && selected_node >= 0 && static_cast<std::size_t>(selected_node) < blueprint.nodes.size())
                blueprint.nodes[static_cast<std::size_t>(selected_node)] = screen_to_world(input.mouse, viewport, 0.0f, scale);
            if (dragging_node && input.left_released)
            {
                dragging_node = false;
                blueprint.rebuild_rest_lengths();
                trainer.set_blueprint(blueprint);
                set_status("RIG UPDATED");
            }
            if (input.delete_pressed && selected_node >= 8
                && static_cast<std::size_t>(selected_node) < blueprint.nodes.size())
            {
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
                blueprint.rebuild_rest_lengths();
                trainer.set_blueprint(blueprint);
                selected_node = -1;
                dragging_node = false;
                set_status("NODE DELETED");
            }

            add_text(canvas, viewport.position + Vec2{ 20.0f, 18.0f }, "SHIFT CLICK: ADD   CTRL CLICK: CONNECT   DRAG: MOVE   DELETE: REMOVE", 1.35f, muted);
        }

        void draw_editor_panel(Rect panel_rect, const InputState& input)
        {
            add_rounded_rect(canvas, panel_rect, 10.0f, panel, border, 1.0f);
            Vec2 cursor = panel_rect.position + Vec2{ 16.0f, 16.0f };
            add_text(canvas, cursor, "CREATURE EDITOR", 1.8f, white);
            cursor.y += 34.0f;
            if (icon.valid())
            {
                add_image(canvas, icon, { cursor, { panel_rect.size.x - 32.0f, 78.0f } });
                cursor.y += 88.0f;
            }
            const float button_width = (panel_rect.size.x - 42.0f) * 0.5f;
            if (button({ cursor, { button_width, 34.0f } }, "RESET RIG", input))
            {
                blueprint = sim::CreatureBlueprint::chicken();
                trainer.set_blueprint(blueprint);
                selected_node = -1;
                set_status("DEFAULT CHICKEN RESTORED");
            }
            if (button({ cursor + Vec2{ button_width + 10.0f, 0.0f }, { button_width, 34.0f } }, "SAVE RIG", input) || input.save_pressed)
            {
                std::string error{};
                set_status(blueprint.save(rig_path, error) ? "RIG SAVED" : error);
            }
            cursor.y += 44.0f;
            if (button({ cursor, { panel_rect.size.x - 32.0f, 34.0f } }, "LOAD RIG", input) || input.load_pressed)
            {
                std::string error{};
                blueprint = sim::CreatureBlueprint::load(rig_path, error);
                trainer.set_blueprint(blueprint);
                set_status(error.empty() ? "RIG LOADED" : error);
            }
            cursor.y += 50.0f;
            bool blueprint_changed = false;
            add_text(canvas, cursor, std::format("SELECTED NODE: {}", selected_node), 1.45f, muted);
            cursor.y += 27.0f;
            if (selected_node >= 0 && static_cast<std::size_t>(selected_node) < blueprint.radii.size())
            {
                float& radius = blueprint.radii[static_cast<std::size_t>(selected_node)];
                const float updated_radius = slider(
                    { cursor, { panel_rect.size.x - 32.0f, 38.0f } }, "NODE RADIUS", radius, 0.08f, 0.60f, input);
                blueprint_changed = blueprint_changed || updated_radius != radius;
                radius = updated_radius;
                cursor.y += 49.0f;
            }
            add_text(canvas, cursor, "MOTOR", 1.45f, muted);
            cursor.y += 23.0f;
            for (int index = 0; index < 4; ++index)
            {
                const float width = (panel_rect.size.x - 44.0f) * 0.25f;
                if (button({ cursor + Vec2{ width * static_cast<float>(index), 0.0f }, { width - 4.0f, 30.0f } }, std::to_string(index + 1), input, selected_motor == index))
                    selected_motor = index;
            }
            cursor.y += 42.0f;
            sim::MotorConstraint& motor = blueprint.motors[static_cast<std::size_t>(selected_motor)];
            auto update_motor_value = [&](float& value, std::string_view label, float minimum, float maximum)
            {
                const float updated = slider({ cursor, { panel_rect.size.x - 32.0f, 38.0f } }, label, value, minimum, maximum, input);
                blueprint_changed = blueprint_changed || updated != value;
                value = updated;
                cursor.y += 48.0f;
            };
            update_motor_value(motor.minimum_angle, "MIN ANGLE", -pi, pi);
            update_motor_value(motor.maximum_angle, "MAX ANGLE", -pi, pi);
            update_motor_value(motor.neutral_angle, "NEUTRAL", -pi, pi);
            update_motor_value(motor.strength, "STRENGTH", 0.05f, 1.0f);
            if (motor.minimum_angle > motor.maximum_angle)
                std::swap(motor.minimum_angle, motor.maximum_angle);
            if (blueprint_changed)
                trainer.set_blueprint(blueprint);
        }

        void draw_training_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 10.0f, panel, border, 1.0f);
            Vec2 cursor = rect.position + Vec2{ 16.0f, 16.0f };
            add_text(canvas, cursor, "PPO TRAINING", 1.8f, white);
            cursor.y += 35.0f;
            if (button({ cursor, { rect.size.x - 32.0f, 38.0f } }, training ? "PAUSE TRAINING" : "START TRAINING", input, training))
                training = !training;
            cursor.y += 48.0f;
            const float third = (rect.size.x - 44.0f) / 3.0f;
            if (button({ cursor, { third, 32.0f } }, "X1", input, updates_per_frame == 1)) updates_per_frame = 1;
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 32.0f } }, "X2", input, updates_per_frame == 2)) updates_per_frame = 2;
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 32.0f } }, "X4", input, updates_per_frame == 4)) updates_per_frame = 4;
            cursor.y += 43.0f;
            if (button({ cursor, { rect.size.x - 32.0f, 34.0f } }, "SINGLE PPO UPDATE", input))
                trainer.train_one_update();
            cursor.y += 44.0f;
            if (button({ cursor, { rect.size.x - 32.0f, 34.0f } }, "RESET POLICY", input))
            {
                trainer.reset_policy();
                set_status("POLICY RESET");
            }
            cursor.y += 46.0f;
            const float half = (rect.size.x - 42.0f) * 0.5f;
            if (button({ cursor, { half, 34.0f } }, "SAVE AI", input))
            {
                std::string error{};
                set_status(trainer.policy().save(policy_path, error) ? "POLICY SAVED" : error);
            }
            if (button({ cursor + Vec2{ half + 10.0f, 0.0f }, { half, 34.0f } }, "LOAD AI", input))
            {
                std::string error{};
                set_status(trainer.policy().load(policy_path, error) ? "POLICY LOADED" : error);
            }

            const rl::TrainingMetrics& metrics = trainer.metrics();
            cursor.y += 54.0f;
            add_text(canvas, cursor, std::format("UPDATE       {}", metrics.update), 1.35f, white); cursor.y += 22.0f;
            add_text(canvas, cursor, std::format("STEPS        {}", metrics.environment_steps), 1.35f, white); cursor.y += 22.0f;
            add_text(canvas, cursor, std::format("MEAN REWARD  {:.2f}", metrics.mean_reward), 1.35f, white); cursor.y += 22.0f;
            add_text(canvas, cursor, std::format("MEAN SPEED   {:.2f} KM/H", metrics.mean_speed * 3.6f), 1.35f, white); cursor.y += 22.0f;
            add_text(canvas, cursor, std::format("DISTANCE     {:.2f} M", metrics.mean_episode_distance), 1.35f, white); cursor.y += 22.0f;
            add_text(canvas, cursor, std::format("POLICY LOSS  {:.4f}", metrics.policy_loss), 1.35f, muted); cursor.y += 22.0f;
            add_text(canvas, cursor, std::format("VALUE LOSS   {:.4f}", metrics.value_loss), 1.35f, muted); cursor.y += 30.0f;

            const float available = rect.position.y + rect.size.y - cursor.y - 12.0f;
            if (available > 170.0f)
            {
                const float graph_height = (available - 10.0f) * 0.5f;
                graph({ cursor, { rect.size.x - 32.0f, graph_height } }, "REWARD", trainer.reward_history(), accent);
                cursor.y += graph_height + 10.0f;
                graph({ cursor, { rect.size.x - 32.0f, graph_height } }, "SPEED KM/H", trainer.speed_history(), yellow);
            }
        }

        void draw_training_world(Rect viewport)
        {
            add_rounded_rect(canvas, viewport, 10.0f, rgb(0x09101a), border, 1.0f);
            constexpr int columns = 4;
            constexpr int rows = 2;
            const Vec2 cell_size{ viewport.size.x / static_cast<float>(columns), viewport.size.y / static_cast<float>(rows) };
            const auto environments = trainer.environments();
            for (int row = 0; row < rows; ++row)
            {
                for (int column = 0; column < columns; ++column)
                {
                    const std::size_t index = static_cast<std::size_t>(row * columns + column);
                    if (index >= environments.size())
                        continue;
                    const Rect cell{ viewport.position + Vec2{ cell_size.x * static_cast<float>(column), cell_size.y * static_cast<float>(row) }, cell_size };
                    canvas.line({ cell.position.x, cell.position.y + cell.size.y }, cell.position + cell.size, 1.0f, border);
                    const auto& env = environments[index];
                    const float camera = env.particles().empty() ? 0.0f : env.particles()[0].position.x;
                    draw_ground(cell, camera, 38.0f);
                    draw_creature(env, cell, camera, 38.0f, index == 0 ? 1.0f : 0.64f, false);
                    add_text(canvas, cell.position + Vec2{ 8.0f, 8.0f }, std::format("AGENT {:02}", index + 1), 1.15f, muted);
                }
            }
            add_text(canvas, viewport.position + Vec2{ 18.0f, viewport.size.y - 28.0f },
                std::format("{} PARALLEL ENVIRONMENTS - {} ROLLOUT STEPS PER UPDATE", trainer.environment_count(), trainer.environment_count() * 128),
                1.25f, muted);
        }

        void draw_run_world(Rect viewport, float dt)
        {
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
            if (!environment.particles().empty())
                camera_x = lerp(camera_x, environment.particles()[0].position.x + 1.5f, 0.04f);
            add_rounded_rect(canvas, viewport, 10.0f, rgb(0x0a1520), border, 1.0f);
            draw_ground(viewport, camera_x, 92.0f);
            draw_creature(environment, viewport, camera_x, 92.0f, 1.0f, false);
            add_text(canvas, viewport.position + Vec2{ 24.0f, 22.0f },
                std::format("{:.2f} KM/H", environment.forward_speed() * 3.6f), 3.0f, white);
            add_text(canvas, viewport.position + Vec2{ 24.0f, 58.0f },
                std::format("DISTANCE {:.1f} M", environment.distance_travelled()), 1.55f, muted);
            add_text(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 30.0f }, "R: RESET   SPACE: PAUSE/RUN", 1.35f, muted);
        }

        void process_shortcuts(const InputState& input)
        {
            if (input.key_1_pressed) mode = Mode::editor;
            if (input.key_2_pressed) mode = Mode::training;
            if (input.key_3_pressed) mode = Mode::run;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed && mode == Mode::training) training = !training;
            if (input.space_pressed && mode == Mode::run) run_paused = !run_paused;
            if (input.reset_pressed && mode == Mode::run)
            {
                trainer.reset_preview();
                camera_x = 0.0f;
            }
        }

        void frame(const InputState& input, float dt, int width, int height)
        {
            canvas.clear();
            canvas.reserve(120000);
            status_time = std::max(0.0f, status_time - dt);
            process_shortcuts(input);

            input_tracker.set_pointer_position({ input.mouse.x, input.mouse.y });
            input_tracker.set_pointer_button(gui_input::PointerButton::left, input.left_down);
            input_tracker.set_pointer_button(gui_input::PointerButton::right, input.right_pressed);
            input_tracker.set_modifiers({ input.shift, input.control, input.alt, false });

            draw_top_bar(input, width);
            const Rect content{ { 10.0f, 66.0f }, { static_cast<float>(width) - 20.0f, static_cast<float>(height) - 76.0f } };
            if (content.size.x <= 300.0f || content.size.y <= 200.0f)
            {
                add_text(canvas, { 20.0f, 90.0f }, "WINDOW TOO SMALL", 2.0f, danger);
                input_tracker.finish_frame();
                return;
            }

            if (mode == Mode::editor)
            {
                gui::SplitterLayoutOptions split_options{};
                split_options.area = to_gui(content);
                split_options.axis = gui::SplitterAxis::vertical;
                split_options.split_fraction = clamp(300.0f / content.size.x, 0.20f, 0.42f);
                split_options.thickness = 10.0f;
                split_options.min_before = 260.0f;
                split_options.min_after = 400.0f;
                const gui::SplitterLayout split = gui::make_splitter_layout(split_options);
                const Rect left = from_gui(split.before);
                const Rect viewport = from_gui(split.after);
                draw_editor_panel(left, input);
                add_rounded_rect(canvas, viewport, 10.0f, rgb(0x0a131d), border, 1.0f);
                draw_blueprint(viewport, input);
            }
            else if (mode == Mode::training)
            {
                if (training)
                {
                    for (int update = 0; update < updates_per_frame; ++update)
                        trainer.train_one_update();
                }
                gui::SplitterLayoutOptions split_options{};
                split_options.area = to_gui(content);
                split_options.axis = gui::SplitterAxis::vertical;
                split_options.split_fraction = clamp(1.0f - 340.0f / content.size.x, 0.52f, 0.78f);
                split_options.thickness = 10.0f;
                split_options.min_before = 480.0f;
                split_options.min_after = 310.0f;
                const gui::SplitterLayout split = gui::make_splitter_layout(split_options);
                draw_training_world(from_gui(split.before));
                draw_training_panel(from_gui(split.after), input);
            }
            else
            {
                draw_run_world(content, dt);
            }

            if (status_time > 0.0f)
            {
                const gui::Vec2 text_size = font::measure_text(status, 1.45f);
                const Rect toast{ { 20.0f, static_cast<float>(height) - 54.0f }, { text_size.x + 28.0f, 34.0f } };
                add_rounded_rect(canvas, toast, 8.0f, rgb(0x10202b, 0.96f), accent, 1.0f);
                add_text(canvas, toast.position + Vec2{ 14.0f, 10.0f }, status, 1.45f, white);
            }
            input_tracker.finish_frame();
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

    bool Application::initialize(const std::filesystem::path& asset_directory, std::string& error)
    {
        impl_->asset_directory = asset_directory;
        const image::ImageResult loaded = image::load_ppm_file((asset_directory / "chicken.ppm").string());
        if (loaded)
            impl_->icon = loaded.image;
        else
            impl_->status = "CHICKEN ICON NOT FOUND - PROCEDURAL VISUALS ACTIVE";
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
