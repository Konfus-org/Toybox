#pragma once

namespace tbx::studio_bridge
{
    struct DataPlaneState;
    struct EngineServices;
    struct GlassState;
    class RpcRegistrar;
    struct ViewState;

    /// @brief Registers the render-view editor RPC methods served by the view_ops free functions,
    /// plus view.setGlass (glass_ops) — a stopped view's glass pass is cleared with the view.
    /// @p data_plane assigns each started view a data-plane slot (view.start replies with
    /// slot + generation) and releases it at view.stop.
    void register_view_handlers(
        const RpcRegistrar& registrar,
        const EngineServices& services,
        ViewState& views,
        GlassState& glass,
        DataPlaneState& data_plane);
}
