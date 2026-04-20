#pragma once
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/api.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/post_processing.h"
#include "tbx/graphics/render_resources.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/settings.h"
#include "tbx/graphics/texture.h"
#include "tbx/graphics/viewport.h"
#include "tbx/graphics/window.h"
#include "tbx/math/matrices.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tbx
{
    struct RenderFrameState;
    struct RenderPassOperation;
    struct RenderScene;

    /// @brief
    /// Purpose: Exposes active graphics context management for render backends.
    /// @details
    /// Ownership: Implementations own backend window/context state.
    /// Thread Safety: Not inherently thread-safe; callers should follow the implementation rules.
    class TBX_API IGraphicsContextManager
    {
      public:
        virtual ~IGraphicsContextManager() noexcept = default;

      public:
        virtual Result make_current(const Window& window) = 0;
        virtual Result present(const Window& window) = 0;
        virtual Result set_vsync(const VsyncMode& mode) = 0;
        virtual GraphicsProcAddress get_proc_address() const = 0;
    };

    /// @brief
    /// Purpose: Defines the backend contract used by the engine-owned render pipeline.
    /// @details
    /// Ownership: Implementations own backend renderer state for each tracked window.
    /// Thread Safety: Not inherently thread-safe; callers should use the render lane.
    class TBX_API IGraphicsBackend
    {
      public:
        virtual ~IGraphicsBackend() noexcept = default;

      public:
        virtual std::string_view get_backend_name() const = 0;
        virtual GraphicsApi get_api() const = 0;
        virtual Result initialize(GraphicsProcAddress loader) = 0;
        virtual Result try_upload(const Mesh& mesh, Uuid& out_resource) = 0;
        virtual Result try_upload(
            const Handle& material_handle,
            const Material& material,
            Uuid& out_resource) = 0;
        virtual Result try_upload(const Texture& texture, Uuid& out_resource) = 0;
        virtual Result try_upload(const TextureSettings& texture, Uuid& out_resource) = 0;
        virtual Result try_upload(const RenderFrameState& frame_state, Uuid& out_resource) = 0;
        virtual Result try_upload(const RenderPassOperation& operation, Uuid& out_resource) = 0;
        virtual Result try_unload(const Uuid& resource) = 0;
        virtual Result draw(const Uuid& resource) = 0;
        virtual Result begin_draw(
            const Uuid& render_texture_resource,
            const Camera& camera,
            const Vec2& resolution,
            const Viewport& viewport) = 0;
        virtual Result end_draw() = 0;
    };

    /// @brief
    /// Purpose: Exposes the engine-owned render flow as a service.
    /// @details
    /// Ownership: Implementations own backend/render-lane state.
    /// Thread Safety: Public calls are expected from the host thread unless documented otherwise.
    class TBX_API IRenderPipeline
    {
      public:
        virtual ~IRenderPipeline() noexcept = default;

      public:
        virtual GraphicsApi get_active_api() const = 0;
        virtual void render() = 0;
    };

    using IRendering = IRenderPipeline;

    /// @brief
    /// Purpose: Toybox implementation of the engine-owned render pipeline.
    /// @details
    /// Ownership: Owns backend/render-lane state and borrows engine services.
    /// Thread Safety: Public calls are expected from the host thread unless documented otherwise.
    class TBX_API RenderPipeline final : public IRenderPipeline
    {
      public:
        RenderPipeline(
            IMessageCoordinator& message_coordinator,
            ThreadManager& thread_manager,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            JobSystem& job_system,
            GraphicsSettings& settings,
            IWindowManager& window_manager,
            IGraphicsContextManager& context_manager,
            std::unique_ptr<IGraphicsBackend> backend);
        ~RenderPipeline() noexcept override;

      public:
        GraphicsApi get_active_api() const override;
        std::string_view get_backend_name() const;
        void render() override;

      private:
        RenderScene build_scene(const Size& viewport_size) const;
        void build_light_data(RenderScene& scene) const;
        void build_post_processing_data(RenderScene& scene) const;
        void build_shadow_data(RenderScene& scene) const;
        void collect_render_items(RenderScene& scene) const;
        void handle_message(Message& message);
        void process_asset_reload_queue();
        void sync_windows();

      private:
        IMessageCoordinator& _message_coordinator;
        ThreadManager& _thread_manager;
        EntityRegistry& _entity_registry;
        AssetManager& _asset_manager;
        JobSystem& _job_system;
        GraphicsSettings& _settings;
        IWindowManager& _window_manager;
        IGraphicsContextManager& _context_manager;
        std::unique_ptr<IGraphicsBackend> _backend = nullptr;
        RenderResourceManager _resource_manager;
        Uuid _message_handler_token = {};
        std::vector<Handle> _pending_asset_reloads = {};
        std::unordered_map<Window, Size> _windows = {};
        mutable bool _has_reported_missing_camera = false;
    };
}
