#pragma once
#include "tbx/common/uuid.h"
#include "tbx/files/filesystem.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>

namespace tbx
{
    /// <summary>
    /// Purpose: Captures metadata associated with an asset on disk.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns its path and name strings; stores an id value by copy.
    /// Thread Safety: Safe to copy between threads; immutable accessors are thread-safe.
    /// </remarks>
    struct AssetMeta
    {
        std::filesystem::path asset_path;
        Uuid id = {};
        std::string name;
    };

    /// <summary>
    /// Purpose: Reads asset metadata from JSON sidecar files.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own filesystem resources; reads from the provided IFileSystem.
    /// Thread Safety: Safe to call concurrently as long as the filesystem is thread-safe.
    /// </remarks>
    class TBX_API AssetMetaReader final
    {
      public:
        /// <summary>
        /// Purpose: Attempts to read metadata from the asset's .meta JSON sidecar.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes metadata into the caller-provided struct on success.
        /// Thread Safety: Safe to call concurrently when the filesystem is thread-safe.
        /// </remarks>
        bool try_read(
            const IFileSystem& file_system,
            const std::filesystem::path& asset_path,
            AssetMeta& out_meta) const;
    };
}
