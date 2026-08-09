#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace runner::diagnostics
{
    struct CourseCompletionReport
    {
        bool launch_contact{};
        bool active_terrain{};
        bool safe_runway{};
        bool material_regions{};
        bool seed_variation{};
        bool water_and_holes{};
        bool observation_truth{};
        bool delayed_material_pressure{};
        bool climb_contract{};
        bool equipment_contract{};
        bool equipment_off_identity{};
        bool frame_independent{};
        float runway_minimum_feature_x{};
        float runway_distance{};
        float runway_elapsed_seconds{};
        std::uint32_t runway_gait_cycles{};
        std::uint32_t runway_material_events{};
        std::uint32_t runway_material_particles{};
        std::uint32_t runway_invalid_reason{};
        bool runway_unique_features{};

        [[nodiscard]] bool passed() const noexcept
        {
            return launch_contact && active_terrain && safe_runway
                && material_regions && seed_variation
                && water_and_holes && observation_truth
                && delayed_material_pressure && climb_contract
                && equipment_contract && equipment_off_identity
                && frame_independent;
        }
    };

    [[nodiscard]] CourseCompletionReport run_course_completion_diagnostic();
    [[nodiscard]] constexpr std::array<std::string_view, 12> course_completion_case_names() noexcept
    {
        return { "launch-contact", "active-terrain", "delayed-objects",
            "material-regions", "seed-variation",
            "water-and-holes", "observation-truth", "delayed-material-pressure",
            "climb-contract", "equipment-contract", "equipment-off-identity",
            "frame-independent" };
    }
}