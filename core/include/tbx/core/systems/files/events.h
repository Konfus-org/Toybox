#pragma once
#include "tbx/core/tbx_api.h"
#include <filesystem>

namespace tbx
{
    /// @brief
    /// Purpose: Identifies the type of filesystem change detected for a watched file.
    /// @details
    /// Ownership: Not applicable; value-type enum.
    /// Thread Safety: Safe to copy between threads.
    enum class FileWatchChangeType
    {
        CREATED,
        MODIFIED,
        REMOVED
    };

    /// @brief
    /// Purpose: Describes a single file change detected while diffing two watch snapshots.
    /// @details
    /// Ownership: Owns its path value.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API FileWatchChange
    {
        std::filesystem::path path = {};
        FileWatchChangeType type = FileWatchChangeType::MODIFIED;
    };
}
