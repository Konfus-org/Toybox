#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct GameModeState;
    class RpcRegistrar;
    struct SyncEventState;
    struct ViewState;

    /// @brief Registers the sync.* editor RPC methods: the uniform path-addressed property verbs
    /// (served by the sync_path_ops free functions) and the sync.subscribe/sync.unsubscribe
    /// event-channel verbs (served by the sync_event_ops functions over the plugin's SyncEventState).
    /// @p game_mode lets a physics-event sync.subscribe made mid-play bind its forwarder immediately.
    void register_sync_handlers(
        const RpcRegistrar& registrar,
        const EngineServices& services,
        ViewState& views,
        SyncEventState& events,
        const GameModeState& game_mode);
}
