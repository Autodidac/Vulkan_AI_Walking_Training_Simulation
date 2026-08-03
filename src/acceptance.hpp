#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace runner::acceptance
{
    struct CaseResult
    {
        std::string name{};
        bool passed{};
        std::string detail{};
    };

    struct Report
    {
        std::vector<CaseResult> cases{};

        [[nodiscard]] bool passed() const noexcept;
        [[nodiscard]] std::size_t passed_count() const noexcept;
    };

    [[nodiscard]] Report run_live_acceptance_matrix();
}
