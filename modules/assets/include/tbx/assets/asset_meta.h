#pragma once
#include "tbx/common/uuid.h"
#include "tbx/files/file_operator.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <string_view>

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
    /// Purpose: Parses asset metadata from JSON sidecar files or raw JSON.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own filesystem resources; reads from disk using FileOperator.
    /// Thread Safety: Safe to call concurrently as long as the file operator is thread-safe.
    /// </remarks>
    class TBX_API AssetMetaParser final
    {
      public:
        /// <summary>
        /// Purpose: Attempts to read metadata from the asset's .meta JSON sidecar.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes metadata into the caller-provided struct on success.
        /// Thread Safety: Safe to call concurrently when the file operator is thread-safe.
        /// </remarks>
        bool try_parse_from_disk(
            const std::filesystem::path& working_directory,
            const std::filesystem::path& asset_path,
            AssetMeta& out_meta) const;

        /// <summary>
        /// Purpose: Parses metadata from raw JSON text.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes metadata into the caller-provided struct on success.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool try_parse_from_source(
            std::string_view meta_text,
            const std::filesystem::path& asset_path,
            AssetMeta& out_meta) const;
    };
}
