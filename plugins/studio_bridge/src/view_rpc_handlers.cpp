#include "view_rpc_handlers.h"
#include "data_plane_ops.h"
#include "glass_ops.h"
#include "glass_state.h"
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
        const RpcRegistrar& registrar,
        const EngineServices& services,
        ViewState& views,
        GlassState& glass,
        DataPlaneState& data_plane)
    {
        registrar.add(
            Wire::VIEW_START,
            [&services, &views, &data_plane](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // The kind selects which view stream is created.
                const auto kind_token = params.value("kind", std::string());
                // Asset-preview only: auto-orbit turntable + a render-resolution scale (0..1] for a cheaper
                // small preview such as the browser's hover card.
                const auto turntable = params.value("turntable", false);
                const auto render_scale = params.value("renderScale", 1.0F);
                // Editor views: the viewport's on-screen device-pixel size, so the engine renders the pane
                // at its own resolution instead of the full graphics resolution (0 = full, e.g. before the
                // pane is laid out). Re-sent as a restart when the pane resizes.
                const auto pane_width = params.value("renderWidth", 0U);
                const auto pane_height = params.value("renderHeight", 0U);
                auto view_name = std::string();
                // World id 0 = the active editing world. An editor/game view can be bound to a specific
                // loaded world by passing its id; an asset-preview view REQUIRES one — the preview world the
                // editor loaded (world.load) and populated. The reply echoes it so the editor can target it
                // for world.describe / entity.create / framing.
                const auto world_id = params.value(Wire::WORLD_ASSET_ID, 0U);
                auto view_result =
                    kind_token == "game" ? start_game_view(views, services, world_id, view_name)
                    : kind_token == "asset"
                        ? start_asset_preview_view(
                              views, services, world_id, turntable, render_scale, view_name)
                        : start_editor_view(
                              views, services, world_id, pane_width, pane_height, view_name);
                if (view_result)
                {
                    auto view_info = tbx::Json::object();
                    view_info[Wire::NAME] = view_name;
                    view_info[Wire::FORMAT] = "bgra8";
                    view_info[Wire::WORLD_ASSET_ID] = world_id;
                    // The view's data-plane slot (or -1 when the plane is unavailable/full — the
                    // editor then keeps this view's hot traffic on the RPC fallbacks).
                    const auto slot = acquire_view_slot(data_plane, view_name);
                    view_info[Wire::SLOT] = slot;
                    view_info[Wire::GENERATION] = view_slot_generation(data_plane, slot);
                    r.result(view_info);
                }
                else
                {
                    r.error(RPC_VIEW_UNAVAILABLE_CODE, view_result.get_report());
                }
            });
        registrar.add(
            Wire::VIEW_STOP,
            [&services, &views, &glass, &data_plane](const tbx::Json& params, tbx::RpcResponder& r)
            {
                const auto name = params.value(Wire::NAME, std::string());
                if (name.empty())
                {
                    stop_all_views(views, services);
                    clear_all_glass(glass, services);
                    release_all_view_slots(data_plane);
                }
                else
                {
                    stop_view(views, services, name);
                    clear_view_glass(glass, services, name);
                    release_view_slot(data_plane, name);
                }
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
            Wire::VIEW_SET_GLASS,
            [&services, &glass](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // The editor pushes its overlay cards' footprints whenever they change; the engine
                // blurs the scene under them (the frosted-glass backdrop).
                r.respond(set_view_glass(glass, services, params));
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
