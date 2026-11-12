#pragma once

#include <filesystem>
#include <string>

namespace runner::persistence
{
    // Replace a file within one filesystem without deleting the accepted
    // destination first. The caller must completely close `temporary` before
    // invoking this function. A failed replacement leaves an existing
    // destination untouched.
    [[nodiscard]] bool atomic_replace_file(
        const std::filesystem::path& temporary,
        const std::filesystem::path& destination, std::string& error);
}
