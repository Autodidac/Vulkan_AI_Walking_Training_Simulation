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

    struct OpaquePixelBounds
    {
        int left{};
        int top{};
        int right{};
        int bottom{};

        [[nodiscard]] bool valid() const noexcept
        {
            return right > left && bottom > top;
        }
    };

    [[nodiscard]] inline OpaquePixelBounds opaque_pixel_bounds(
        const PixelArt& art) noexcept
    {
        if (!art.loaded())
            return {};
        OpaquePixelBounds bounds{ art.width, art.height, 0, 0 };
        for (int y = 0; y < art.height; ++y)
        {
            for (int x = 0; x < art.width; ++x)
            {
                const Color color = art.pixels[static_cast<std::size_t>(
                    y * art.width + x)];
                if (art.transparent(color))
                    continue;
                bounds.left = std::min(bounds.left, x);
                bounds.top = std::min(bounds.top, y);
                bounds.right = std::max(bounds.right, x + 1);
                bounds.bottom = std::max(bounds.bottom, y + 1);
            }
        }
        return bounds.valid() ? bounds : OpaquePixelBounds{};
    }

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
        // Side-view anatomy still renders both physical chains. Keep the rear
        // chain materially opaque so it reads as a second solid limb instead
        // of a ghost copy; physical draw order and independently stepped
        // anchors provide the depth cue.
        return foreground ? 0.99f : 0.96f;
    }

    [[nodiscard]] inline float human_arm_layer_opacity(
        bool foreground) noexcept
    {
        // A strict side view can put two physical arm chains almost directly
        // on top of one another. Preserve their anchors and draw order, while
        // giving the rear arm enough depth separation for both hands to remain
        // readable around held equipment.
        return foreground ? 0.99f : 0.94f;
    }

    [[nodiscard]] inline Color opaque_depth_tint(Color source, Color tint,
        float strength) noexcept
    {
        // Depth treatment is deliberately a color-only operation. The caller
        // applies it after rejecting transparent source pixels, so it cannot
        // manufacture a rectangular silhouette around irregular authored art.
        const float amount = std::clamp(
            std::isfinite(strength) ? strength : 0.0f, 0.0f, 1.0f);
        const auto channel = [amount](float source_channel,
            float tint_channel) noexcept
        {
            const float finite_source = std::isfinite(source_channel)
                ? source_channel : 0.0f;
            const float finite_tint = std::isfinite(tint_channel)
                ? tint_channel : finite_source;
            return std::clamp(lerp(finite_source, finite_tint, amount),
                0.0f, 1.0f);
        };
        return {
            channel(source.r, tint.r),
            channel(source.g, tint.g),
            channel(source.b, tint.b),
            std::isfinite(source.a) ? std::clamp(source.a, 0.0f, 1.0f) : 0.0f
        };
    }

    // These are authored opaque-pixel occupancies, not substitute geometry.
    // Keeping the fit tied to the supplied irregular silhouettes prevents the
    // differently padded source canvases from making one physical arm segment
    // look wire-thin while the adjacent segment looks disproportionately wide.
    inline constexpr float human_upper_arm_opaque_transverse_fraction =
        7.0f / 20.0f;
    inline constexpr float human_forearm_opaque_transverse_fraction =
        12.0f / 20.0f;
    inline constexpr float human_hand_opaque_transverse_fraction =
        13.0f / 28.0f;
    // The arm sources are much more heavily padded than the leg sources.
    // Target a substantial visible silhouette after that padding is removed.
    // The earlier 0.36/0.41 forearm/upper-arm result still read as a wire next
    // to the chest and disappeared around a held weapon at native scale.
    // Preserve a visible anatomical taper, but keep both supplied-pixel pieces
    // broad enough to read as one continuous armored manipulator.
    inline constexpr float human_arm_opaque_thickness_ratio = 0.53f;
    inline constexpr float human_arm_opaque_taper_ratio = 0.035f;

    // The supplied boot's last opaque row is 24/28 through its canvas. The
    // neutral Human support surface lies about 35% of the fitted boot height
    // below the heel/toe particle centers, matching their capped contact depth.
    inline constexpr float human_boot_opaque_sole_fraction = 24.0f / 28.0f;
    inline constexpr float human_boot_contact_depth_fraction = 0.35f;

    enum class HumanFootEdgeKind
    {
        ordinary,
        triangle_brace,
        ordered_path
    };

    struct HumanFootEdgeStyle
    {
        float width{};
        float opacity{};
    };

    [[nodiscard]] inline HumanFootEdgeKind human_foot_edge_kind(
        int first_slot, int second_slot) noexcept
    {
        if (first_slot < 0 || first_slot > 3
            || second_slot < 0 || second_slot > 3
            || first_slot == second_slot)
            return HumanFootEdgeKind::ordinary;
        const int first = std::min(first_slot, second_slot);
        const int second = std::max(first_slot, second_slot);
        if (second == first + 1)
            return HumanFootEdgeKind::ordered_path;
        if (first == 0 && second == 2)
            return HumanFootEdgeKind::triangle_brace;
        return HumanFootEdgeKind::ordinary;
    }

    [[nodiscard]] inline HumanFootEdgeStyle human_foot_edge_style(
        HumanFootEdgeKind kind, bool foreground) noexcept
    {
        switch (kind)
        {
        case HumanFootEdgeKind::triangle_brace:
            return foreground ? HumanFootEdgeStyle{ 0.60f, 0.28f }
                : HumanFootEdgeStyle{ 0.40f, 0.18f };
        case HumanFootEdgeKind::ordered_path:
            return foreground ? HumanFootEdgeStyle{ 2.65f, 0.98f }
                : HumanFootEdgeStyle{ 1.65f, 0.80f };
        case HumanFootEdgeKind::ordinary:
            break;
        }
        return {};
    }

    [[nodiscard]] inline float human_limb_thickness_ratio(
        bool support_limb, bool has_distal_motor) noexcept
    {
        if (support_limb)
            return has_distal_motor ? 0.50f : 0.46f;
        // Normalize the differently padded source canvases while retaining a
        // deliberate upper-arm -> cuff taper. The physical shoulder/elbow/
        // wrist anchors still own both ends and no opaque capsule is made.
        const float visible_target = human_arm_opaque_thickness_ratio
            + (has_distal_motor ? human_arm_opaque_taper_ratio
                                : -human_arm_opaque_taper_ratio);
        return visible_target
            / (has_distal_motor
                ? human_upper_arm_opaque_transverse_fraction
                : human_forearm_opaque_transverse_fraction);
    }

    [[nodiscard]] inline float fitted_joint_overlap(float limb_span,
        float limb_thickness, bool support_limb) noexcept
    {
        const float span = std::isfinite(limb_span) ? std::max(0.0f, limb_span) : 0.0f;
        const float thickness = std::isfinite(limb_thickness)
            ? std::max(0.0f, limb_thickness) : 0.0f;
        // Arm source pixels taper at both ends. Let them overlap the physical
        // shoulder/elbow seams while retaining a smaller support-limb overlap.
        const float span_limit = span * (support_limb ? 0.10f : 0.085f);
        return std::min(span_limit,
            thickness * (support_limb ? 0.34f : 0.30f));
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
            ? forearm_span * 0.23f : scaled_pixels(10.0f, pixel_scale);
        // Only 13/28 of the supplied hand canvas is opaque transversely. Its
        // bone-fitted request may therefore occupy 76% of the forearm span.
        // Keep its longitudinal fit compact so the glove reads as a separate
        // hand/cuff mass instead of extending the forearm into one rigid bar.
        const float maximum_thickness = fitted_to_bone
            ? forearm_span * 0.76f : scaled_pixels(30.0f, pixel_scale);
        const float fitted_factor = fitted_to_bone ? 1.50f : 0.58f;
        const float thickness = std::clamp(
            std::isfinite(forearm_thickness) ? forearm_thickness * fitted_factor
                : scaled_pixels(16.0f, pixel_scale),
            minimum_thickness, maximum_thickness);
        const float aspect = source_width > 0 && source_height > 0
            ? std::clamp(static_cast<float>(source_width)
                / static_cast<float>(source_height), 1.05f, 1.65f)
            : 1.40f;
        const float minimum_length = fitted_to_bone
            ? forearm_span * 0.24f : scaled_pixels(17.0f, pixel_scale);
        const float maximum_length = fitted_to_bone
            ? forearm_span * 0.38f : scaled_pixels(38.0f, pixel_scale);
        const float hand_length = std::clamp(
            thickness * aspect, minimum_length, maximum_length);
        const float wrist_overlap = fitted_to_bone
            ? hand_length * 0.28f
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
        if (plate <= 1.0e-4f && lower <= 1.0e-4f
            && radius <= 1.0e-4f)
            return {};
        // The physical heel->toe plate is the complete longitudinal sole.
        // Fitting the sprite's opaque span beyond either endpoint created the
        // needle-like toe/black spike seen in strict side view. A valid plate
        // therefore owns the width exactly; knee->ankle and contact radius
        // influence only the bounded transverse boot envelope. The fallback
        // remains finite for legacy/malformed non-articulated callers.
        const float width = plate > 1.0e-4f
            ? plate : std::max(radius * 1.70f, lower * 0.42f);
        const float height = std::clamp(width * 0.69f,
            radius * 1.05f, std::max({ radius * 1.35f,
                lower * 0.45f, width * 0.74f }));
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

    // The supplied weapon's physical hand grip is five pixels from the left
    // edge of its 30-pixel opaque longitudinal span and two thirds of the way
    // down its 24-pixel canvas. Keep both source-art landmarks fixed for every
    // equipment state; moving the hand along the same sprite or centering the
    // receiver on the forearm makes safe carry collapse into a rigid stick.
    inline constexpr float equipment_primary_grip_fraction = 1.0f / 6.0f;
    inline constexpr float equipment_primary_grip_normal_fraction = 2.0f / 3.0f;

    [[nodiscard]] inline OrientedArtTransform grip_anchored_art_transform(
        Vec2 grip, Vec2 direction, float span, float thickness,
        float grip_fraction = equipment_primary_grip_fraction,
        float grip_normal_fraction = equipment_primary_grip_normal_fraction,
        bool mirror_vertical = false) noexcept
    {
        const Vec2 axis = normalized(direction, { 1.0f, 0.0f });
        Vec2 normal{ -axis.y, axis.x };
        if (mirror_vertical)
            normal = normal * -1.0f;
        const float bounded_span = std::isfinite(span)
            ? std::max(0.0f, span) : 0.0f;
        const float bounded_thickness = std::isfinite(thickness)
            ? std::max(0.0f, thickness) : 0.0f;
        const float bounded_grip = std::isfinite(grip_fraction)
            ? std::clamp(grip_fraction, 0.0f, 1.0f)
            : equipment_primary_grip_fraction;
        const float bounded_grip_normal = std::isfinite(grip_normal_fraction)
            ? std::clamp(grip_normal_fraction, 0.0f, 1.0f)
            : equipment_primary_grip_normal_fraction;
        const Vec2 centerline_grip = grip - normal
            * ((bounded_grip_normal - 0.5f) * bounded_thickness);
        return { centerline_grip - axis * (bounded_span * bounded_grip),
            centerline_grip + axis * (bounded_span * (1.0f - bounded_grip)),
            bounded_thickness };
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
        if (!std::isfinite(heel.x) || !std::isfinite(heel.y)
            || !std::isfinite(toe.x) || !std::isfinite(toe.y)
            || !std::isfinite(width) || !std::isfinite(height)
            || width <= 0.0f || height <= 0.0f)
            return {};
        const float facing = std::isfinite(facing_direction)
                && facing_direction < 0.0f ? -1.0f : 1.0f;
        const Vec2 forward = normalized(toe - heel, { facing, 0.0f });
        // draw_authored_pixel_art mirrors the transverse source axis when the
        // whole rig faces left. Reconstruct that presented normal here so the
        // bottom of the supplied 20-high opaque boot inside its 28-high canvas
        // lands on the real heel->toe contact plane at every physical pitch.
        // Human support particles are centers with a capped contact depth;
        // retaining that depth below the ordered anchors prevents a FLAT foot
        // from visibly hovering without moving either physical endpoint.
        Vec2 presented_normal{ -forward.y, forward.x };
        if (facing < 0.0f)
            presented_normal *= -1.0f;
        const float bounded_width = width;
        const float bounded_height = height;
        const Vec2 physical_center = (heel + toe) * 0.5f;
        const Vec2 center = physical_center - presented_normal
            * (bounded_height
                * (human_boot_opaque_sole_fraction - 0.5f
                    - human_boot_contact_depth_fraction));
        return oriented_box_transform(center, forward,
            bounded_width, bounded_height);
    }
    [[nodiscard]] bool load_p3_pixel_art(const std::filesystem::path& path,
        PixelArt& art, std::string& error);
}
