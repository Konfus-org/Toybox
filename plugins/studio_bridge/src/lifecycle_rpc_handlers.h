#pragma once
#include <functional>

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct EngineServices;
    struct GameModeState;
    struct SyncEventState;

    /// @brief Registers the handshake + engine-lifecycle editor RPC methods. @p set_paused gates the
    /// engine simulation and @p request_shutdown asks the application to exit (both wired to the
    /// plugin's message posting, which a free function can't reach directly). @p events lets the
    /// engine.setPlaying handler (re)bind the editor's physics-event forwarders on the play toggle.
    void register_lifecycle_handlers(
        const RpcRegistrar& registrar,
        const EngineServices& services,
        GameModeState& game_mode,
        SyncEventState& events,
        std::function<void(bool paused)> set_paused,
        std::function<void()> request_shutdown);
}
