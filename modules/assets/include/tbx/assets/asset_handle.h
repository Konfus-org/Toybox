#pragma once
#include "tbx/common/uuid.h"
#include <filesystem>

namespace tbx
{
    /// <summary>
    /// Purpose: Encapsulates an asset identifier that can be a path or UUID.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores an owned path copy and UUID value.
    /// Thread Safety: Safe to copy between threads; immutable accessors are thread-safe.
    /// </remarks>
    struct AssetHandle
    {
        AssetHandle() = default;

        /// <summary>
        /// Purpose: Constructs a handle from an asset path.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the provided path into the handle.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        AssetHandle(std::filesystem::path asset_path)
            : path(std::move(asset_path))
        {
        }

        /// <summary>
        /// Purpose: Constructs a handle from an asset UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the provided UUID value by copy.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        AssetHandle(Uuid asset_id)
            : id(asset_id)
        {
        }

        /// <summary>
        /// Purpose: Returns true when the handle references a non-empty path.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool has_path() const
        {
            return !path.empty();
        }

        /// <summary>
        /// Purpose: Returns true when the handle references a valid UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool has_id() const
        {
            return id.is_valid();
        }

        /// <summary>
        /// Purpose: Returns true when the handle has either a path or UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool is_valid() const
        {
            return has_path() || has_id();
        }

        std::filesystem::path path;
        Uuid id = {};
    };
}
