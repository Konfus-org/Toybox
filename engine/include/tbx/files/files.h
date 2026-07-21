#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

// Plain file IO — stateless, so it is a namespace, not a class. The FileWatcher (script/asset
// hot-reload) arrives with the asset milestone.
namespace tbx::files
{
    /// @brief
    /// Purpose: Reads a whole file as raw bytes.
    TBX_API Result<std::vector<std::byte>> read_bytes(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Reads a whole file as text (no encoding conversion).
    TBX_API Result<std::string> read_text(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Writes raw bytes to a file, creating parent directories as needed.
    TBX_API Result<void> write_bytes(const std::filesystem::path& path, std::span<const std::byte> bytes);

    /// @brief
    /// Purpose: Writes text to a file, creating parent directories as needed.
    TBX_API Result<void> write_text(const std::filesystem::path& path, std::string_view text);
}
