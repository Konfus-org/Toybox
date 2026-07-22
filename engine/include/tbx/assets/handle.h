#pragma once
#include "tbx/utils/uuid.h"
#include <string>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Typed reference to an asset. Identity is the uuid from the identity-only
    /// .meta sidecar ({id, version, type}) so renames never break references; a handle may
    /// also be authored straight from a relative path — AssetHandle<gpu::Texture>("MyTexture.png")
    /// — and resolves to its id on first load. The id is always the first member: serialized
    /// handles are exactly their uuid.
    template <typename TAsset>
    struct AssetHandle
    {
        Uuid id = {};
        std::string path = {};

        AssetHandle() = default;

        explicit(false) AssetHandle(const Uuid& asset_id)
            : id(asset_id)
        {
        }

        explicit AssetHandle(std::string relative_path)
            : path(std::move(relative_path))
        {
        }

        AssetHandle(const Uuid& asset_id, std::string relative_path)
            : id(asset_id)
            , path(std::move(relative_path))
        {
        }

        /// @brief
        /// Purpose: True when the handle references anything at all (id or path).
        bool is_set() const
        {
            return id.is_valid() || !path.empty();
        }

        /// @brief
        /// Purpose: True when the identity is resolved (kit references, loaded handles).
        bool is_valid() const
        {
            return id.is_valid();
        }
    };
}
