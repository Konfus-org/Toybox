#pragma once
#include "engine_services.h"
#include "view_manager.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Each frame, projects every active-world entity to screen space for each editor view and
    /// pushes the positions to the editor (view.billboards). The editor draws the actual overlay — a
    /// name label per entity plus a stack of component icons ([[tbx::viewport_icon]]) — and handles
    /// clicks. The engine owns only the projection; everything visual and interactive is editor-driven.
    /// @details
    /// Ownership: Stateless; borrows the engine services and view manager. Thread Safety: Main thread
    /// only; called once per frame from the bridge update after the views render.
    class BillboardPublisher
    {
      public:
        BillboardPublisher(EngineServices& services, ViewManager& views);

        // Projects + pushes a view.billboards notification for every editor view (no-op without a
        // connected editor or active world).
        void publish();

      private:
        EngineServices& _services;
        ViewManager& _views;
    };
}
