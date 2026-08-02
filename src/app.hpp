#pragma once

#include "renderer.hpp"

#include <filesystem>
#include <span>
#include <string>

namespace runner
{
    struct InputState
    {
        Vec2 mouse{};
        Vec2 mouse_delta{};
        float wheel{};
        bool left_down{};
        bool left_pressed{};
        bool left_released{};
        bool right_pressed{};
        bool shift{};
        bool control{};
        bool alt{};
        bool delete_pressed{};
        bool escape_pressed{};
        bool space_pressed{};
        bool key_1_pressed{};
        bool key_2_pressed{};
        bool key_3_pressed{};
        bool save_pressed{};
        bool load_pressed{};
        bool reset_pressed{};
    };

    class Application
    {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        [[nodiscard]] bool initialize(const std::filesystem::path& asset_directory, std::string& error);
        void frame(const InputState& input, float dt, int width, int height);
        [[nodiscard]] std::span<const render::Vertex> vertices() const noexcept;
        [[nodiscard]] bool wants_quit() const noexcept;

    private:
        struct Impl;
        Impl* impl_{};
    };
}
