#pragma once
#include "tbx/core/result.h"
#include <cstddef>
#include <filesystem>
#include <vector>

// Plain file IO — stateless, so it is a namespace, not a class. The FileWatcher (script/asset
// hot-reload) arrives with the asset milestone.
namespace tbx::files
{
    /// @brief
    /// Purpose: Reads a whole file as raw bytes.
    Result<std::vector<std::byte>> read_bytes(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Reads a whole file as text (no encoding conversion).
    Result<std::string> read_text(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Writes text to a file, creating parent directories as needed.
    Result<void> write_text(const std::filesystem::path& path, std::string_view text);
}
