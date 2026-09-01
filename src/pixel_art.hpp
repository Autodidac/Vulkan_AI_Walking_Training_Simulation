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

    struct BootArtDimensions
    {
        float width{};
        float height{};
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

    [[nodiscard]] inline float human_limb_layer_opacity(
        bool foreground) noexcept
    {
        // Side-view anatomy still renders both physical chains, but the far
        // chain must read as depth instead of a duplicate translucent rig.
        return foreground ? 0.98f : 0.22f;
    }

    [[nodiscard]] inline float human_limb_thickness_ratio(
        bool support_limb, bool has_distal_motor) noexcept
    {
        if (support_limb)
            return has_distal_motor ? 0.50f : 0.46f;
        // Human arms remain visibly articulated without letting the forearm
        // and hand dominate the saved neutral silhouette.
        return has_distal_motor ? 0.31f : 0.17f;
    }

    [[nodiscard]] inline float fitted_joint_overlap(float limb_span,
        float limb_thickness, bool support_limb) noexcept
    {
        const float span = std::isfinite(limb_span) ? std::max(0.0f, limb_span) : 0.0f;
        const float thickness = std::isfinite(limb_thickness)
            ? std::max(0.0f, limb_thickness) : 0.0f;
        const float span_limit = span * (support_limb ? 0.075f : 0.055f);
        return std::min(span_limit,
            thickness * (support_limb ? 0.22f : 0.16f));
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
        float pixel_scale = 1.0f, float forearm_span = 0.0f) noexcept
    {
        const bool fitted_to_bone = std::isfinite(forearm_span)
            && forearm_span > 1.0e-4f;
        const float minimum_thickness = fitted_to_bone
            ? forearm_span * 0.10f : scaled_pixels(10.0f, pixel_scale);
        const float maximum_thickness = fitted_to_bone
            ? forearm_span * 0.27f : scaled_pixels(30.0f, pixel_scale);
        const float thickness = std::clamp(
            std::isfinite(forearm_thickness) ? forearm_thickness * 0.48f
                : scaled_pixels(16.0f, pixel_scale),
            minimum_thickness, maximum_thickness);
        const float aspect = source_width > 0 && source_height > 0
            ? std::clamp(static_cast<float>(source_width)
                / static_cast<float>(source_height), 1.05f, 1.65f)
            : 1.40f;
        const float minimum_length = fitted_to_bone
            ? forearm_span * 0.20f : scaled_pixels(17.0f, pixel_scale);
        const float maximum_length = fitted_to_bone
            ? forearm_span * 0.43f : scaled_pixels(38.0f, pixel_scale);
        const float hand_length = std::clamp(
            thickness * aspect, minimum_length, maximum_length);
        const float wrist_overlap = fitted_to_bone
            ? hand_length * 0.14f
            : std::min(hand_length * 0.18f,
                std::max(scaled_pixels(3.0f, pixel_scale), hand_length * 0.14f));
        return { hand_length, thickness, wrist_overlap };
    }

    [[nodiscard]] inline BootArtDimensions articulated_boot_dimensions(
        float heel_to_toe_span, float lower_leg_span, float joint_radius) noexcept
    {
        const float plate = std::isfinite(heel_to_toe_span)
            ? std::max(0.0f, heel_to_toe_span) : 0.0f;
        const float lower = std::isfinite(lower_leg_span)
            ? std::max(0.0f, lower_leg_span) : 0.0f;
        const float radius = std::isfinite(joint_radius)
            ? std::max(0.0f, joint_radius) : 0.0f;
        const float maximum_width = std::max(radius * 1.42f, lower * 0.88f);
        const float width = std::clamp(
            std::max(plate * 1.08f, radius * 1.72f),
            radius * 1.40f, maximum_width);
        const float height = std::clamp(width * 0.46f,
            radius * 0.82f, std::max(radius * 0.84f, lower * 0.40f));
        return { width, height };
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
        float facing_direction = 1.0f, bool grounded = false) noexcept
    {
        const Vec2 terminal = normalized(support - proximal, { 0.0f, 1.0f });
        const float facing = std::isfinite(facing_direction)
                && facing_direction < 0.0f
            ? -1.0f : 1.0f;
        const Vec2 articulated_forward = Vec2{ terminal.y, -terminal.x } * facing;
        const Vec2 forward = grounded
            ? Vec2{ facing, 0.0f } : articulated_forward;
        const float bounded_width = std::max(0.0f, width);
        const float bounded_height = std::max(0.0f, height);
        // Terminal points toward the support surface and therefore remains the
        // vertical/sole reference when the toe direction reverses.
        // A loaded boot has a physical sole: keep that sole horizontal and on
        // the contact point instead of rotating the whole foot perpendicular
        // to the shin. Airborne boots retain the articulated swing pitch.
        const Vec2 center = grounded
            ? support + forward * (bounded_width * 0.22f)
                + Vec2{ 0.0f, -bounded_height * 0.48f }
            : support - terminal * (bounded_height * 0.24f)
                + forward * (bounded_width * 0.26f);
        return oriented_box_transform(center, forward, bounded_width, bounded_height);
    }


    [[nodiscard]] inline OrientedArtTransform articulated_boot_transform(
        Vec2 heel, Vec2 toe, float width, float height,
        float facing_direction = 1.0f) noexcept
    {
        const float facing = std::isfinite(facing_direction)
                && facing_direction < 0.0f ? -1.0f : 1.0f;
        Vec2 forward = normalized(toe - heel, { facing, 0.0f });
        if (forward.x * facing < 0.0f)
            forward *= -1.0f;
        const float bounded_width = std::max(0.0f, width);
        const float bounded_height = std::max(0.0f, height);
        const Vec2 physical_center = (heel + toe) * 0.5f;
        const Vec2 center = physical_center + Vec2{ 0.0f,
            -bounded_height * 0.48f };
        return oriented_box_transform(center, forward,
            bounded_width, bounded_height);
    }
    [[nodiscard]] bool load_p3_pixel_art(const std::filesystem::path& path,
        PixelArt& art, std::string& error);
}
