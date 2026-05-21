#pragma once
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/assets/registry.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/messages.h"
#include <filesystem>

namespace tbx::internal
{
    static constexpr double ASSET_UNLOAD_INTERVAL_SECONDS = 1.0;
    static constexpr auto ASSET_UNLOAD_IDLE_GRACE = std::chrono::seconds(5);

    static Handle build_asset_handle(const AssetRegistryEntry& entry)
    {
        return Handle(entry.normalized_path, entry.asset_id);
    }
}
