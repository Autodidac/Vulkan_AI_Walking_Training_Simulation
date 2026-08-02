#pragma once

#include "math.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

struct SDL_Window;

namespace runner::render
{
    struct Vertex
    {
        Vec2 position{};
        Color color{};
    };

    class Canvas
    {
    public:
        void clear() noexcept { vertices_.clear(); }
        void reserve(std::size_t vertex_count) { vertices_.reserve(vertex_count); }
        [[nodiscard]] std::span<const Vertex> vertices() const noexcept { return vertices_; }

        void triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
        void quad(Vec2 minimum, Vec2 maximum, Color color);
        void line(Vec2 a, Vec2 b, float thickness, Color color);
        void circle(Vec2 center, float radius, Color color, std::uint32_t segments = 24);
        void capsule(Vec2 a, Vec2 b, float radius, Color color, std::uint32_t segments = 16);
        void polyline(std::span<const Vec2> points, float thickness, Color color);

    private:
        std::vector<Vertex> vertices_{};
    };

    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;
        VulkanRenderer(VulkanRenderer&&) = delete;
        VulkanRenderer& operator=(VulkanRenderer&&) = delete;

        [[nodiscard]] bool initialize(SDL_Window* window, const std::filesystem::path& shader_directory, std::string& error);
        void shutdown() noexcept;
        [[nodiscard]] bool render(std::span<const Vertex> vertices, int drawable_width, int drawable_height, std::string& error);
        void wait_idle() noexcept;

    private:
        struct Impl;
        Impl* impl_{};
    };
}
