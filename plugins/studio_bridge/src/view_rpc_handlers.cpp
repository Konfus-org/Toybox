#include "view_rpc_handlers.h"
#include "rpc_registrar.h"
#include "view_ops.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"
#include <string>

namespace tbx::studio_bridge
{
    // App-specific JSON-RPC error code: a view RPC arrived but the target view/surface isn't available.
    // (The standard protocol codes live in tbx/interfaces/rpc_router.h.)
    constexpr int RPC_VIEW_UNAVAILABLE_CODE = -32000;

    void register_view_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views)
    {
        registrar.add(
            Wire::VIEW_START,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // The kind selects which view stream is created; only an asset-preview view uses the
                // asset id (it selects the asset loaded into the view's isolated preview world).
                const auto kind_token = params.value("kind", std::string());
                const auto asset_id = params.value(Wire::ASSET_ID, 0U);
                // Asset-preview only: auto-orbit turntable + a render-resolution scale (0..1] for a cheaper
                // small preview such as the browser's hover card.
                const auto turntable = params.value("turntable", false);
                const auto render_scale = params.value("renderScale", 1.0F);
                auto view_name = std::string();
                // World id 0 = the active editing world; an asset-preview view fills in its isolated
                // world's id so the editor can target it for world.describe / entity.create.
                auto world_id = 0U;
                auto view_result =
                    kind_token == "game"  ? start_game_view(views, services, view_name)
                    : kind_token == "asset"
                        ? start_asset_preview_view(
                              views, services, asset_id, turntable, render_scale, view_name, world_id)
                        : start_editor_view(views, services, view_name);
                if (view_result)
                {
                    auto view_info = tbx::Json::object();
                    view_info[Wire::NAME] = view_name;
                    view_info[Wire::FORMAT] = "bgra8";
                    view_info[Wire::WORLD_ASSET_ID] = world_id;
                    r.result(view_info);
                }
                else
                {
                    r.error(RPC_VIEW_UNAVAILABLE_CODE, view_result.get_report());
                }
            });
        registrar.add(
            Wire::VIEW_STOP,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                const auto name = params.value(Wire::NAME, std::string());
                if (name.empty())
                    stop_all_views(views, services);
                else
                    stop_view(views, services, name);
                r.result(tbx::Json::object());
            });
        registrar.add(
            Wire::VIEW_INPUT,
            [&views](const tbx::Json& params, tbx::RpcResponder&)
            {
                // High-frequency notification from the focused editor viewport; no response.
                apply_view_input(views, params);
            });
        registrar.add(
            Wire::VIEW_FRAME_ASSET_PREVIEW,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // Frames the orbit camera of an asset-preview world to the renderable bounds the editor
                // built in it; called after the editor creates/swaps the previewed entity.
                const auto world_id = params.value(Wire::WORLD_ASSET_ID, 0U);
                r.respond(frame_asset_preview(views, services, world_id));
            });
    }
}
