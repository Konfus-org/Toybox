#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct ViewState;

    // The editor's uniform sync.* { address } addressing (see EngineAddress on the editor side): resolves
    // each verb's address (describe/set/reset/isDefault) and routes it to the implementation that owns
    // what the address names — entity/component/world ops in world_ops, asset describes in asset_ops.
    // These functions own the address grammar; the targets own the behavior. Main-thread only (request
    // handling).

    /// @brief Reads the object addressed by { address } as its describe body — the editor's sync.describe.
    /// The address names an object (an entity, a component, an asset, a world).
    Result sync_describe_path(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);

    /// @brief Writes the field addressed by { address } to { value } — the editor's sync.set.
    Result sync_set_path(
        const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Resets the field addressed by { path } to its default — the editor's sync.reset.
    Result sync_reset_path(
        const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Whether the field addressed by { path } currently holds its default — sync.isDefault.
    Result sync_is_default_path(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        bool& out_is_default);
}
