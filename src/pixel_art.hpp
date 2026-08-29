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
                // Lanczos-resampled keyed sprites can carry a one-pixel near-key
                // fringe. Hide only saturated magenta, never ordinary pink art.
                return color.r >= 0.94f && color.g <= 0.08f && color.b >= 0.94f
                    && std::abs(color.r - color.b) <= 0.08f;
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

    struct HandArtDimensions
    {
        float length{};
        float thickness{};
        float wrist_overlap{};
    };

    struct HelmetArtDimensions
    {
        float height{};
        float downward_offset{};
    };

    inline constexpr float reference_pixels_per_meter = 42.0f;

    [[nodiscard]] inline float presentation_pixel_scale(float pixels_per_meter) noexcept
    {
        if (!std::isfinite(pixels_per_meter) || pixels_per_meter <= 0.0f)
            return 1.0f;
        return std::clamp(pixels_per_meter / reference_pixels_per_meter, 0.25f, 4.0f);
    }

    [[nodiscard]] inline float scaled_pixels(float reference_pixels,
        float pixel_scale) noexcept
    {
        if (!std::isfinite(reference_pixels) || reference_pixels < 0.0f)
            return 0.0f;
        const float bounded_scale = std::isfinite(pixel_scale) && pixel_scale > 0.0f
            ? std::clamp(pixel_scale, 0.25f, 4.0f) : 1.0f;
        return reference_pixels * bounded_scale;
    }

    [[nodiscard]] inline bool presented_limb_transverse_mirror(
        float facing_direction) noexcept
    {
        return std::isfinite(facing_direction) && facing_direction < 0.0f;
    }

    [[nodiscard]] inline HelmetArtDimensions helmet_art_dimensions(
        float head_radius_pixels, float assembled_scale,
        float pixel_scale = 1.0f) noexcept
    {
        const float radius = std::isfinite(head_radius_pixels)
            && head_radius_pixels > 0.0f ? head_radius_pixels : 0.0f;
        const float assembly = std::isfinite(assembled_scale)
            && assembled_scale > 0.0f
            ? std::clamp(assembled_scale, 0.50f, 2.0f) : 1.0f;
        const float height = std::max(scaled_pixels(28.0f, pixel_scale),
            radius * 2.05f * assembly);
        return { height, height * 0.10f };
    }

    [[nodiscard]] inline HandArtDimensions hand_art_dimensions(
        float forearm_thickness, int source_width, int source_height,
        float pixel_scale = 1.0f) noexcept
    {
        const float minimum_thickness = scaled_pixels(28.0f, pixel_scale);
        const float maximum_thickness = scaled_pixels(64.0f, pixel_scale);
        const float thickness = std::clamp(
            std::isfinite(forearm_thickness) ? forearm_thickness * 0.98f
                : scaled_pixels(38.0f, pixel_scale),
            minimum_thickness, maximum_thickness);
        const float aspect = source_width > 0 && source_height > 0
            ? std::clamp(static_cast<float>(source_width)
                / static_cast<float>(source_height), 1.05f, 1.65f)
            : 1.40f;
        const float length = std::clamp(thickness * aspect,
            scaled_pixels(40.0f, pixel_scale), scaled_pixels(92.0f, pixel_scale));
        return { length, thickness, length * 0.16f };
    }

    [[nodiscard]] inline SkinEnvelopeDimensions skin_envelope_dimensions(
        float torso_length, float authored_shoulder_width,
        float pixel_scale = 1.0f) noexcept
    {
        const float torso = std::isfinite(torso_length) && torso_length > 0.0f
            ? std::clamp(torso_length, scaled_pixels(12.0f, pixel_scale),
                scaled_pixels(240.0f, pixel_scale))
            : scaled_pixels(48.0f, pixel_scale);
        const float authored = std::isfinite(authored_shoulder_width)
                && authored_shoulder_width > 0.0f
            ? std::clamp(authored_shoulder_width, 0.0f, torso * 1.30f) : 0.0f;
        const float shoulders = std::max(authored, torso * 0.94f);
        return { shoulders, std::max(torso * 0.29f, shoulders * 0.30f),
            std::max(torso * 0.22f, shoulders * 0.23f),
            std::clamp(torso * 0.085f, scaled_pixels(3.5f, pixel_scale),
                scaled_pixels(14.0f, pixel_scale)) };
    }
    [[nodiscard]] inline float assembled_armor_scale(
        float torso_length, float authored_shoulder_width,
        float pixel_scale = 1.0f) noexcept
    {
        const SkinEnvelopeDimensions envelope = skin_envelope_dimensions(
            torso_length, authored_shoulder_width, pixel_scale);
        const float torso = std::isfinite(torso_length) && torso_length > 0.0f
            ? std::clamp(torso_length, scaled_pixels(12.0f, pixel_scale),
                scaled_pixels(240.0f, pixel_scale))
            : scaled_pixels(48.0f, pixel_scale);
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
        Vec2 proximal, Vec2 support, float width, float height,
        float facing_direction = 1.0f) noexcept
    {
        const Vec2 terminal = normalized(support - proximal, { 0.0f, 1.0f });
        const float facing = std::isfinite(facing_direction)
                && facing_direction < 0.0f
            ? -1.0f : 1.0f;
        const Vec2 forward = Vec2{ terminal.y, -terminal.x } * facing;
        const float bounded_width = std::max(0.0f, width);
        const float bounded_height = std::max(0.0f, height);
        // Terminal points toward the support surface and therefore remains the
        // vertical/sole reference when the toe direction reverses.
        const Vec2 center = support - terminal * (bounded_height * 0.24f)
            + forward * (bounded_width * 0.26f);
        return oriented_box_transform(center, forward, bounded_width, bounded_height);
    }

    [[nodiscard]] bool load_p3_pixel_art(const std::filesystem::path& path,
        PixelArt& art, std::string& error);
}
