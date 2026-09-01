#pragma once

#include <string_view>

namespace runner::runtime
{
    [[nodiscard]] inline bool is_headless_surface_error(
        std::string_view error) noexcept
    {
        return error.find("VK_KHR_surface") != std::string_view::npos
            || error.find("VK_KHR_win32_surface") != std::string_view::npos
            || error.find("VK_KHR_xcb_surface") != std::string_view::npos
            || error.find("VK_KHR_xlib_surface") != std::string_view::npos
            || error.find("No available video device") != std::string_view::npos
            || error.find("No dynamic Vulkan support") != std::string_view::npos;
    }
}
