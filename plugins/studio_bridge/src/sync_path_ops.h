#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct ViewState;

    // The editor's uniform sync.* { path } addressing (see EngineAddress on the editor side):
    // resolves each verb's path (describe/set/reset/isDefault) and routes it to the implementation
    // that owns what the path names — entity/component/world ops in world_ops, asset describes in
    // asset_ops. These functions own the path grammar; the targets own the behavior. Main-thread
    // only (request handling).

    /// @brief Reads the object addressed by { path } as its describe body — the editor's sync.describe.
    /// The path names an object (an entity, a component, …); see EngineAddress on the editor side.
    Result sync_describe_path(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);

    /// @brief Writes the field addressed by { path } to { value } — the editor's sync.set.
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
