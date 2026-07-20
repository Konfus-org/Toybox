#pragma once
#include "tbx/core/result.h"
#include <cstddef>
#include <filesystem>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Plain file IO. The FileWatcher (script/asset hot-reload) arrives with the asset
    /// milestone.
    /// @details
    /// Ownership: Stateless today; owned by Engine so later path-root/watcher state has a home.
    class Files final
    {
      public:
        /// @brief
        /// Purpose: Reads a whole file as raw bytes.
        Result<std::vector<std::byte>> read_bytes(const std::filesystem::path& path) const;

        /// @brief
        /// Purpose: Reads a whole file as text (no encoding conversion).
        Result<std::string> read_text(const std::filesystem::path& path) const;

        /// @brief
        /// Purpose: Writes text to a file, creating parent directories as needed.
        Result<void> write_text(const std::filesystem::path& path, std::string_view text) const;
    };
}
