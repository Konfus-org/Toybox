#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Provides metadata common to serialized engine assets.
    /// @details
    /// Ownership: Value type inherited by asset payloads.
    /// Thread Safety: Safe to copy between threads; synchronize shared mutation externally.
    struct TBX_API Asset
    {
        virtual ~Asset() noexcept = default;

        [[meta]]
        Uuid id = {};

        [[meta]]
        uint32 version = 1U;

        operator Handle()
        {
            return Handle(id);
        }
    };

    /// @brief
    /// Purpose: Provides default read parameters for asset types without custom load options.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct DefaultAssetLoadParameters
    {
        bool operator==(const DefaultAssetLoadParameters& other) const = default;
    };

    // Maps an asset type to its load-parameter type without a central traits table. Each asset
    // header that needs custom parameters declares its own `load_parameters_of` overload next to
    // its struct; any asset without one resolves to DefaultAssetLoadParameters through this base
    // overload. These functions are declared but never defined — they exist purely to compute
    // AssetLoadParameters<T> in unevaluated context.
    DefaultAssetLoadParameters load_parameters_of(const Asset&);

    template <typename TAsset>
    using AssetLoadParameters = decltype(load_parameters_of(std::declval<const TAsset&>()));
}
