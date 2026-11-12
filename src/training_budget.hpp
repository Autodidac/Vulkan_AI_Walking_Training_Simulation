#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace runner::rl::training_budget
{
    inline constexpr std::size_t default_worker_ceiling = 4u;

    [[nodiscard]] constexpr std::size_t active_workers(int mode,
        std::size_t capacity) noexcept
    {
        capacity = std::max<std::size_t>(1u, capacity);
        return mode <= 1 ? 1u : mode <= 2
            ? std::min<std::size_t>(2u, capacity) : capacity;
    }

    // Budget the training pipeline, not rendering or whole-process CPU percent.
    // Reserve arithmetic headroom for chrono deadline addition on corrupt input.
    [[nodiscard]] constexpr std::int64_t idle_nanoseconds(int mode,
        std::int64_t work_nanoseconds) noexcept
    {
        if (work_nanoseconds <= 0 || mode > 2)
            return 0;
        const std::int64_t factor = mode <= 1 ? 3 : 1;
        constexpr auto maximum = std::numeric_limits<std::int64_t>::max() / 4;
        return std::min(work_nanoseconds, maximum / factor) * factor;
    }
}
