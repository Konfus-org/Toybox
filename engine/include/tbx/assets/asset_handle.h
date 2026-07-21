#pragma once
#include "tbx/core/uuid.h"

namespace tbx
{
    /// @brief
    /// Purpose: Typed reference to a loaded asset; the id comes from the identity-only .meta
    /// sidecar ({id, version, type}), so renames never break references.
    template <typename TAsset>
    struct AssetHandle
    {
        Uuid id = {};

        bool is_valid() const
        {
            return !id.is_nil();
        }
    };
}
