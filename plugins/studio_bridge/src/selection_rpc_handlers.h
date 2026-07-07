#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    class EntitySelectionHandler;
    class Selection;
    struct EngineServices;
    class ViewManager;

    /// @brief Registers the picking + selection editor RPC methods served by the EntitySelectionHandler.
    void register_selection_handlers(
        const RpcRegistrar& registrar,
        EntitySelectionHandler& selection_handler,
        Selection& selection,
        const EngineServices& services,
        ViewManager& views);
}
