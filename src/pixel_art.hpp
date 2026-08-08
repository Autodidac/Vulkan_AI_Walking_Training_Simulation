#pragma once

#include "math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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
        Color transparent_key{};
        bool chroma_keyed{};

        [[nodiscard]] bool loaded() const noexcept
        {
            return width > 0 && height > 0
                && pixels.size() == static_cast<std::size_t>(width * height);
        }

        [[nodiscard]] bool transparent(Color color) const noexcept
        {
            if (chroma_keyed)
            {
                constexpr float tolerance = 0.5f / 255.0f;
                return std::abs(color.r - transparent_key.r) <= tolerance
                    && std::abs(color.g - transparent_key.g) <= tolerance
                    && std::abs(color.b - transparent_key.b) <= tolerance;
            }
            return std::max({ color.r, color.g, color.b }) < 0.035f;
        }
    };

    [[nodiscard]] bool load_p3_pixel_art(const std::filesystem::path& path,
        PixelArt& art, std::string& error);
}
