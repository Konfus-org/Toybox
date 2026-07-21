#pragma once
#include <functional>

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct DataPlaneState;
    struct EngineServices;
    struct GameModeState;
    struct SyncEventState;

    /// @brief Registers the handshake + engine-lifecycle editor RPC methods. @p set_paused gates the
    /// engine simulation, @p request_step advances one paused tick (the game view's next-frame
    /// button), and @p request_shutdown asks the application to exit (all wired to the plugin's message
    /// posting, which a free function can't reach directly). @p events lets the engine.setPlaying
    /// handler (re)bind the editor's physics-event forwarders on the play toggle. @p data_plane is
    /// lazily created by the editor.hello handler and advertised in its reply.
    void register_lifecycle_handlers(
        const RpcRegistrar& registrar,
        const EngineServices& services,
        GameModeState& game_mode,
        SyncEventState& events,
        DataPlaneState& data_plane,
        std::function<void(bool paused)> set_paused,
        std::function<void()> request_step,
        std::function<void()> request_shutdown);
}
