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

    struct OrientedArtTransform
    {
        Vec2 beginning{};
        Vec2 ending{};
        float thickness{};
    };

    struct SkinEnvelopeDimensions
    {
        float shoulder_width{};
        float chest_radius{};
        float pelvis_half_width{};
        float joint_overlap{};
    };

    [[nodiscard]] inline SkinEnvelopeDimensions skin_envelope_dimensions(
        float torso_length, float authored_shoulder_width) noexcept
    {
        const float torso = std::isfinite(torso_length) && torso_length > 0.0f
            ? std::clamp(torso_length, 12.0f, 240.0f) : 48.0f;
        const float authored = std::isfinite(authored_shoulder_width)
                && authored_shoulder_width > 0.0f
            ? std::clamp(authored_shoulder_width, 0.0f, torso * 1.30f) : 0.0f;
        const float shoulders = std::max(authored, torso * 0.94f);
        return { shoulders, std::max(torso * 0.29f, shoulders * 0.30f),
            std::max(torso * 0.22f, shoulders * 0.23f),
            std::clamp(torso * 0.085f, 3.5f, 14.0f) };
    }
    [[nodiscard]] inline float assembled_armor_scale(
        float torso_length, float authored_shoulder_width) noexcept
    {
        const SkinEnvelopeDimensions envelope = skin_envelope_dimensions(
            torso_length, authored_shoulder_width);
        const float torso = std::isfinite(torso_length) && torso_length > 0.0f
            ? std::clamp(torso_length, 12.0f, 240.0f) : 48.0f;
        const float silhouette_ratio = envelope.shoulder_width / torso;
        return std::clamp(1.30f + (0.98f - silhouette_ratio) * 0.20f,
            1.24f, 1.34f);
    }
    [[nodiscard]] inline Vec2 facing_presented_position(
        Vec2 position, Vec2 root, float facing_direction) noexcept
    {
        if (!std::isfinite(position.x) || !std::isfinite(position.y)
            || !std::isfinite(root.x) || !std::isfinite(root.y)
            || !std::isfinite(facing_direction) || facing_direction >= 0.0f)
            return position;
        return { root.x - (position.x - root.x), position.y };
    }
    [[nodiscard]] inline OrientedArtTransform oriented_box_transform(
        Vec2 center, Vec2 width_axis, float width, float height) noexcept
    {
        const Vec2 axis = normalized(width_axis, { 1.0f, 0.0f });
        const float bounded_width = std::max(0.0f, width);
        return { center - axis * (bounded_width * 0.5f),
            center + axis * (bounded_width * 0.5f), std::max(0.0f, height) };
    }

    [[nodiscard]] inline OrientedArtTransform support_boot_transform(
        Vec2 proximal, Vec2 support, float width, float height) noexcept
    {
        const Vec2 terminal = normalized(support - proximal, { 0.0f, 1.0f });
        const Vec2 forward{ terminal.y, -terminal.x };
        const Vec2 normal{ -forward.y, forward.x };
        const float bounded_width = std::max(0.0f, width);
        const float bounded_height = std::max(0.0f, height);
        const Vec2 center = support - normal * (bounded_height * 0.24f)
            + forward * (bounded_width * 0.26f);
        return oriented_box_transform(center, forward, bounded_width, bounded_height);
    }

    [[nodiscard]] bool load_p3_pixel_art(const std::filesystem::path& path,
        PixelArt& art, std::string& error);
}
