#pragma once
#include "tbx/types/assets/asset.h"
#include "tbx/types/handle.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: A strongly-typed handle to an asset of type TAsset. Identity only — it wraps a Handle and
    /// never owns the asset; a caller resolves it to the live shared instance through an explicitly supplied
    /// AssetManager (assets.load<TAsset>(handle)), which is where load-on-demand, shared ownership and idle
    /// unloading already live.
    /// @details
    /// Ownership: Owns only the Handle identity, never the asset. Serializes exactly as its underlying Handle
    /// (a bare id, tolerant of both the bare and { "id": ... } forms on read) so a typed AssetHandle field is
    /// byte-identical on the wire and on disk to a plain Handle field — the type tag is compile-time only.
    /// Thread Safety: Matches Handle; safe to copy.
    [[serializable]];
    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    struct AssetHandle
    {
        AssetHandle() = default;

        /// @brief Wraps an existing handle (implicit, so an authored Handle assigns straight into a typed slot).
        explicit(false) AssetHandle(Handle asset_handle)
            : handle(std::move(asset_handle))
        {
        }

        /// @brief Whether the handle points at anything — an id, or a name the engine can resolve.
        bool is_valid() const
        {
            return handle.is_valid();
        }

        /// @brief The referenced asset id — the handle's persisted identity.
        Uuid get_id() const
        {
            return handle.id;
        }

        /// @brief Implicit view as the underlying handle, so a typed handle drops straight into the engine's
        /// handle-based asset APIs (assets.load<TAsset>(handle)) with no call-site churn.
        explicit(false) operator const Handle&() const
        {
            return handle;
        }

        bool operator==(const AssetHandle& other) const
        {
            return handle == other.handle;
        }

        Handle handle = {};
    };
}

// The handle serializes exactly as its underlying Handle (a bare id, tolerant of the { "id": ... }
// object form on read) through generated template serialize/deserialize; included after the definition
// so those function templates see the complete type.
#include "tbx/types/assets/asset_handle.generated.h"
