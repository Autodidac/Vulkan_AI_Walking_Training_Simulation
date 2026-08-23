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

    [[nodiscard]] constexpr SpeciesArtProfile species_art_profile(
        sim::CreatureSpecies species) noexcept
    {
        switch (species)
        {
        case sim::CreatureSpecies::chicken:
            return { 0.34f, 0.055f, 0.15f, 0.035f,
                0.62f, 0.18f, 0.36f, 0.03f, 0.04f,
                0.15f, 0.09f, 0.54f, 0.88f, 0.050f, false,
                5.10f, 0.40f, 0.68f, 0.82f, 0.065f, 0.050f };
        case sim::CreatureSpecies::dog:
            return { 0.42f, 0.075f, 0.18f, 0.060f,
                0.52f, 0.22f, 0.42f, 0.08f, 0.07f,
                0.22f, 0.105f, 0.66f, 0.82f, 0.065f, false,
                5.00f, 0.46f, 0.78f, 0.84f, 0.095f, 0.050f };
        case sim::CreatureSpecies::hexapod:
            return { 0.33f, 0.060f, 0.16f, 0.045f,
                0.46f, 0.18f, 0.38f, 0.05f, 0.05f,
                0.16f, 0.08f, 0.0f, 0.0f, 0.0f, false,
                2.80f, 0.22f, 0.38f, 0.72f, 0.0f, 0.0f };
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