#pragma once

#include "simulation.hpp"

namespace runner::art
{
    struct SpeciesArtProfile
    {
        float limb_ratio;
        float limb_minimum;
        float limb_maximum;
        float joint_overlap;
        float body_ratio;
        float body_minimum;
        float body_maximum;
        float body_rear_overlap;
        float body_front_overlap;
        float foot_length;
        float foot_thickness;
        float tail_length;
        float tail_ratio;
        float tail_anchor_up;
        bool tail_flip_vertical;
        float head_scale;
        float head_minimum;
        float head_maximum;
        float head_ratio;
        float head_anchor_forward;
        float head_anchor_up;
    };


    struct SpeciesArtTopology
    {
        std::uint16_t body_rear{};
        std::uint16_t body_front{};
        std::uint16_t head_attachment{};
        std::uint16_t head_tip{};
        std::uint16_t tail_tip{};
        bool physical_tail{};
    };

    [[nodiscard]] constexpr SpeciesArtTopology species_art_topology(
        sim::CreatureSpecies species) noexcept
    {
        switch (species)
        {
        case sim::CreatureSpecies::chicken:
            // Body 0->2, head/neck 2->4, and tail 0->5 share exact
            // physical landmarks instead of treating the tail tip as a torso.
            return { 0u, 2u, 2u, 4u, 5u, true };
        case sim::CreatureSpecies::dog:
            return { 0u, 1u, 1u, 2u, 0u, false };
        case sim::CreatureSpecies::hexapod:
            return { 0u, 2u, 2u, 3u, 0u, false };
        default:
            return {};
        }
    }
    [[nodiscard]] constexpr SpeciesArtProfile species_art_profile(
        sim::CreatureSpecies species) noexcept
    {
        switch (species)
        {
        case sim::CreatureSpecies::chicken:
            return { 0.40f, 0.070f, 0.18f, 0.085f,
                0.68f, 0.24f, 0.42f, 0.045f, 0.060f,
                0.20f, 0.11f, 0.64f, 0.92f, 0.0f, false,
                5.80f, 0.48f, 0.76f, 0.90f, 0.030f, 0.0f };
        case sim::CreatureSpecies::dog:
            return { 0.48f, 0.090f, 0.21f, 0.095f,
                0.58f, 0.28f, 0.47f, 0.095f, 0.095f,
                0.25f, 0.12f, 0.58f, 0.72f, 0.035f, true,
                5.40f, 0.55f, 0.88f, 0.88f, 0.035f, 0.0f };
        case sim::CreatureSpecies::hexapod:
            return { 0.38f, 0.070f, 0.18f, 0.075f,
                0.52f, 0.22f, 0.42f, 0.070f, 0.070f,
                0.18f, 0.09f, 0.0f, 0.0f, 0.0f, false,
                2.80f, 0.24f, 0.40f, 0.74f, 0.025f, 0.0f };
        default:
            return { 0.30f, 0.050f, 0.18f, 0.040f,
                0.45f, 0.14f, 0.42f, 0.04f, 0.04f,
                0.18f, 0.08f, 0.30f, 0.60f, 0.0f, false,
                2.80f, 0.22f, 0.40f, 0.74f, 0.0f, 0.0f };
        }
    }

    [[nodiscard]] constexpr bool tail_transverse_mirror(
        const SpeciesArtProfile& profile, bool transverse_mirror) noexcept
    {
        return profile.tail_flip_vertical ? !transverse_mirror : transverse_mirror;
    }
}