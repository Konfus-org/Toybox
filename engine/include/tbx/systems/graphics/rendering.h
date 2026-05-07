#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/graphics/viewport.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <vector>

namespace tbx
{
    struct RenderFrameContext;

    /// @brief
    /// Purpose: Orchestrates the per-frame render loop — builds a frame context, drives the
    /// prepare/execute phases of registered operations, and manages begin/end frame lifecycle.
    /// @details
    /// Ownership: Owns the resource manager and registered operations. Borrows all services.
    /// Thread Safety: Not inherently thread-safe; callers should synchronize backend access.
    class TBX_API Rendering
    {
      public:
        Rendering(
            IGraphicsBackend& backend,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            IWindowManager& window_manager,
            Window output_window,
            const GraphicsSettings& settings);
        ~Rendering() noexcept;

        Rendering(const Rendering&) = delete;
        Rendering& operator=(const Rendering&) = delete;
        Rendering(Rendering&&) noexcept = delete;
        Rendering& operator=(Rendering&&) noexcept = delete;

        void render();

      private:
        Result begin_frame_and_view(const Camera& camera, const Viewport& viewport);
        Result end_view_and_frame();
        Size get_render_resolution() const;
        void release_operations();

        std::reference_wrapper<IGraphicsBackend> _backend;
        std::reference_wrapper<EntityRegistry> _entity_registry;
        std::reference_wrapper<IWindowManager> _window_manager;
        Window _output_window = {};
        Size _requested_resolution = {};
        std::unique_ptr<GraphicsResourceManager> _resource_manager = {};
        std::vector<std::unique_ptr<IRenderOperation>> _operations = {};
        uint64 _render_frame = 0U;
        Result _initialization_result = {};
    };
}
