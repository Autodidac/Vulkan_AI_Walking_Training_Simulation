#include "atomic_file.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace runner::persistence
{
    bool atomic_replace_file(const std::filesystem::path& temporary,
        const std::filesystem::path& destination, std::string& error)
    {
        if (temporary.empty() || destination.empty())
        {
            error = "Atomic file replacement paths are empty.";
            return false;
        }
        std::error_code filesystem_error{};
        if (!std::filesystem::is_regular_file(temporary, filesystem_error)
            || filesystem_error)
        {
            error = "Atomic replacement source is not a regular file: "
                + temporary.string();
            return false;
        }
#if defined(_WIN32)
        if (::MoveFileExW(temporary.c_str(), destination.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
        {
            error = "Could not replace file atomically (Windows error "
                + std::to_string(::GetLastError()) + "): "
                + destination.string();
            return false;
        }
#else
        std::filesystem::rename(temporary, destination, filesystem_error);
        if (filesystem_error)
        {
            error = "Could not replace file atomically: "
                + filesystem_error.message();
            return false;
        }
#endif
        error.clear();
        return true;
    }
}
