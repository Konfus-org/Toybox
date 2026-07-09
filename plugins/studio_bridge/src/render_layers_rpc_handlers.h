#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct EngineServices;
    struct RenderLayersState;

    /// @brief Registers the editor.setRenderLayers notification (the editor's Render Layers
    /// toolbar: collider wireframe modes, the post-processing toggle, and the render-stage debug
    /// view).
    void register_render_layers_handlers(
        const RpcRegistrar& registrar, RenderLayersState& layers, const EngineServices& services);
}
