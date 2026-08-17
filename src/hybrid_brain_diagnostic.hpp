#pragma once

namespace runner::diagnostics
{
    struct HybridBrainReport
    {
        bool hole_escape{};
        bool falling_dodge{};
        bool harmless_object{};
        bool blocked_exit{};
        bool policy_bounds{};
        bool multi_topology{};
        bool frame_independent{};

        [[nodiscard]] bool passed() const noexcept
        {
            return hole_escape && falling_dodge && harmless_object
                && blocked_exit && policy_bounds && multi_topology
                && frame_independent;
        }
    };

    [[nodiscard]] HybridBrainReport run_hybrid_brain_diagnostic();
}
