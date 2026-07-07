#pragma once
#include <functional>

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct EngineServices;
    class GameModeManager;

    /// @brief Registers the handshake + engine-lifecycle editor RPC methods. @p set_paused gates the
    /// engine simulation and @p request_shutdown asks the application to exit (both wired to the
    /// plugin's message posting, which a free function can't reach directly).
    void register_lifecycle_handlers(
        const RpcRegistrar& registrar,
        const EngineServices& services,
        GameModeManager& game_mode,
        std::function<void(bool paused)> set_paused,
        std::function<void()> request_shutdown);
}
