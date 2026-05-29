#pragma once
#include "tbx/systems/messaging/observable.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Defines global world streaming and chunking settings.
    /// @details
    /// Ownership: Owns all configuration values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    struct TBX_API WorldSettings
    {
        WorldSettings();
        WorldSettings(std::weak_ptr<IMessageDispatcher> dispatcher);

        template <typename TOwner>
        WorldSettings(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            Observable<TOwner, WorldSettings>& parent)
            : chunk_size(dispatcher, parent, *this, &WorldSettings::chunk_size, 32.0F)
            , unload_radius_chunks(
                  dispatcher,
                  parent,
                  *this,
                  &WorldSettings::unload_radius_chunks,
                  6U)
        {
        }

        Observable<WorldSettings, float> chunk_size;
        Observable<WorldSettings, uint32> unload_radius_chunks;
    };
}
