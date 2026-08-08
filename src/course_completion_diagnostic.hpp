#pragma once

#include <array>
#include <string_view>

namespace runner::diagnostics
{
    struct CourseCompletionReport
    {
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

        [[nodiscard]] bool passed() const noexcept
        {
            return safe_runway && material_regions && seed_variation
                && water_and_holes && observation_truth
                && delayed_material_pressure && climb_contract
                && equipment_contract && equipment_off_identity
                && frame_independent;
        }
    };

    [[nodiscard]] CourseCompletionReport run_course_completion_diagnostic();
    [[nodiscard]] constexpr std::array<std::string_view, 10> course_completion_case_names() noexcept
    {
        return { "safe-runway", "material-regions", "seed-variation",
            "water-and-holes", "observation-truth", "delayed-material-pressure",
            "climb-contract", "equipment-contract", "equipment-off-identity",
            "frame-independent" };
    }
}