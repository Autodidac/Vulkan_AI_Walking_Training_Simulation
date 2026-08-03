#pragma once

#include "math.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace runner::art
{
    struct PixelArt
    {
        int width{};
        int height{};
        std::vector<Color> pixels{};

        [[nodiscard]] bool loaded() const noexcept
        {
            return width > 0 && height > 0
                && pixels.size() == static_cast<std::size_t>(width * height);
        }
    };

    [[nodiscard]] bool load_p3_pixel_art(const std::filesystem::path& path,
        PixelArt& art, std::string& error);
}
