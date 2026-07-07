#include "lifecycle_rpc_handlers.h"
#include "engine_services.h"
#include "game_mode_manager.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/json.h"
#include <utility>

namespace tbx::studio_bridge
{
    // v2 adds the sync.* namespace (per-property get/set/isDefault).
    constexpr int PROTOCOL_VERSION = 2;

    void register_lifecycle_handlers(
        const RpcRegistrar& registrar,
        const EngineServices& services,
        GameModeManager& game_mode,
        std::function<void(bool paused)> set_paused,
        std::function<void()> request_shutdown)
    {
        registrar.add(
            Wire::EDITOR_HELLO,
            [&services](const tbx::Json&, tbx::RpcResponder& r)
            {
                auto result = tbx::Json::object();
                result["protocolVersion"] = PROTOCOL_VERSION;
                result["engine"] = "Toybox";
                result["app"] = services.app_name;
                r.result(result);
            });
        registrar.add(
            Wire::ENGINE_PING,
            [](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(tbx::Json::object());
            });
        registrar.add(
            Wire::ENGINE_SET_PAUSED,
            [set_paused = std::move(set_paused)](const tbx::Json& params, tbx::RpcResponder& r)
            {
                set_paused(params.value("isPaused", false));
                r.result(tbx::Json::object());
            });
        registrar.add(
            Wire::ENGINE_SET_PLAYING,
            [&game_mode](const tbx::Json& params, tbx::RpcResponder& r)
            {
                game_mode.set_playing(params.value("isPlaying", false));
                r.result(tbx::Json::object());
            });
        registrar.add(
            Wire::ENGINE_SHUTDOWN,
            [request_shutdown = std::move(request_shutdown)](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(tbx::Json::object());
                TBX_TRACE_INFO("StudioBridge: shutdown requested by the editor.");
                request_shutdown();
            });
    }
}
