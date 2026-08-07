#pragma once

#include "math.hpp"

namespace runner::ui_render
{
    inline constexpr Color transparent_fill{ 0.0f, 0.0f, 0.0f, 0.0f };

    [[nodiscard]] constexpr bool is_explicitly_transparent(Color color) noexcept
    {
        return color.a == 0.0f;
    }
}
