#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/render_pipeline.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Owns Toybox-side render submission against a graphics backend.
    /// @details
    /// Ownership: Owns render pipeline resources; borrows runtime services.
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

      public:
        Rendering(const Rendering&) = delete;
        Rendering& operator=(const Rendering&) = delete;
        Rendering(Rendering&&) noexcept = delete;
        Rendering& operator=(Rendering&&) noexcept = delete;

      public:
        Result render();

      private:
        Result begin_frame_and_view();
        Result end_view_and_frame();
        Result ensure_geometry_buffers(
            const std::vector<float>& vertices,
            const std::vector<uint32>& indices);
        Result ensure_geometry_pipeline();
        Size get_render_resolution() const;
        void release_resources();
        void setup_geometry_pass(uint32 index_count);

      private:
        std::reference_wrapper<IGraphicsBackend> _backend;
        std::reference_wrapper<EntityRegistry> _entity_registry;
        std::reference_wrapper<AssetManager> _asset_manager;
        std::reference_wrapper<IWindowManager> _window_manager;
        Window _output_window = {};
        Size _requested_resolution = {};
        std::unique_ptr<GraphicsRenderPipeline> _pipeline = {};
        Uuid _geometry_index_buffer = {};
        Uuid _geometry_pipeline = {};
        Uuid _geometry_vertex_buffer = {};
        uint64 _geometry_index_buffer_size = 0U;
        uint64 _geometry_vertex_buffer_size = 0U;
        Result _initialization_result = {};
    };
}
