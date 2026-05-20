#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/draw_command_factory.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_tracker.h"
#include "tbx/systems/graphics/resource_uploader.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/mesh.h"
#include "tbx/utils/result.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Extracts ECS renderables and builds the frame's executable render-pass list.
    /// @details
    /// Ownership: Owns command construction, frame shader data, and frame-local upload helpers.
    /// Unload policy remains owned by Rendering.
    /// Thread Safety: Call on the render lane.
    class TBX_API RenderingPassFactory final
    {
      public:
        RenderingPassFactory(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<EntityRegistry> entity_registry,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<IWindowManager> window_manager,
            Size configured_resolution);
        ~RenderingPassFactory() = default;

      public:
        RenderingPassFactory(const RenderingPassFactory&) = delete;
        RenderingPassFactory& operator=(const RenderingPassFactory&) = delete;
        RenderingPassFactory(RenderingPassFactory&&) noexcept = delete;
        RenderingPassFactory& operator=(RenderingPassFactory&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Creates all render passes for the current frame.
        Result create(
            uint64 frame_index,
            RenderingResourceTracker& resource_tracker,
            std::vector<RenderPass>& out_render_passes);

        /// @brief
        /// Purpose: Builds frame shader and render-target state before GPU frame upload.
        Result build_frame_data(
            DeltaTime delta_time,
            RenderTarget& out_render_target,
            RenderView& out_view);

      private:
        Result build_frame_data(
            EntityRegistry& entity_registry,
            const IWindowManager& window_manager,
            DeltaTime delta_time,
            RenderTarget& out_render_target,
            RenderView& out_view);

        std::weak_ptr<EntityRegistry> _entity_registry = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        Size _configured_resolution = {};
        ResourceUploader _resource_uploader;
        RenderingDrawCommandFactory _draw_command_factory = {};
        std::shared_ptr<Mesh> _sky_dome_mesh = {};
        FrameShaderData _frame_shader_data = {};
        CameraShaderData _camera_shader_data = {};
        LightShaderData _light_shader_data = {};
    };
}
