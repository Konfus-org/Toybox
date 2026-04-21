#pragma once
#include "tbx/common/handle.h"
#include "tbx/common/result.h"
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
#include "tbx/graphics/window.h"
#include "tbx/math/matrices.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    class AssetManager;
    class EntityRegistry;
    class JobSystem;
    class ThreadManager;

    /// @brief
    /// Purpose: Exposes active graphics context management for the engine render pipeline.
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

    using RenderMesh = std::variant<DynamicMesh, StaticMesh>;

    struct TBX_API RenderDrawItem
    {
        Uuid mesh_resource = {};
        Uuid material_resource = {};
        MaterialConfig material_config = {};
        ParamBindings material_parameters = {};
        TextureBindings material_textures = {};
        Mat4 transform = Mat4(1.0F);
        float camera_distance_squared = 0.0F;
    };

    struct TBX_API RenderShadowItem
    {
        Uuid mesh_resource = {};
        Mat4 transform = Mat4(1.0F);
        float bounds_radius = 0.0F;
        bool is_two_sided = false;
    };

    struct TBX_API RenderSky
    {
        Uuid mesh_resource = {};
        Uuid material_resource = {};
        MaterialConfig material_config = {};
        ParamBindings material_parameters = {};
        TextureBindings material_textures = {};
        Mat4 transform = Mat4(1.0F);
        float camera_distance_squared = 0.0F;
    };

    struct TBX_API RenderFallbacks
    {
        Uuid white_texture_resource = {};
        Uuid material_resource = {};
        Uuid mesh_resource = {};
    };

    struct TBX_API RenderUniformNames
    {
        std::string view_projection = "u_view_proj";
        std::string model = "u_model";
    };

    struct TBX_API DirectionalLightFrameData
    {
        Vec3 direction = Vec3(0.0F, 0.0F, -1.0F);
        float ambient_intensity = 0.03F;
        Vec3 radiance = Vec3(1.0F, 1.0F, 1.0F);
        float casts_shadows = 0.0F;
        uint32 shadow_cascade_offset = 0U;
        uint32 shadow_cascade_count = 0U;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
    };

    struct TBX_API ShadowCascadeFrameData
    {
        Mat4 light_view_projection = Mat4(1.0F);
        float split_depth = 0.0F;
        float normal_bias = 0.0F;
        float depth_bias = 0.0F;
        float blend_distance = 0.0F;
        uint32 texture_layer = 0U;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
    };

    struct TBX_API ProjectedShadowFrameData
    {
        Mat4 light_view_projection = Mat4(1.0F);
        float near_plane = 0.1F;
        float far_plane = 10.0F;
        float normal_bias = 0.0F;
        float depth_bias = 0.0F;
        uint32 texture_layer = 0U;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
        float padding2 = 0.0F;
    };

    struct TBX_API ShadowFrameData
    {
        uint32 directional_map_resolution = 2048U;
        uint32 local_map_resolution = 1024U;
        uint32 point_map_resolution = 1024U;
        float softness = 1.0F;
        float max_distance = 90.0F;
        std::vector<ShadowCascadeFrameData> directional_cascades = {};
        std::vector<ProjectedShadowFrameData> spot_maps = {};
        std::vector<ProjectedShadowFrameData> area_maps = {};
    };

    struct TBX_API PointLightFrameData
    {
        Vec3 position = Vec3(0.0F, 0.0F, 0.0F);
        float range = 10.0F;
        Vec3 radiance = Vec3(1.0F, 1.0F, 1.0F);
        float shadow_bias = 0.00035F;
        int shadow_index = -1;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
    };

    struct TBX_API SpotLightFrameData
    {
        Vec3 position = Vec3(0.0F, 0.0F, 0.0F);
        float range = 10.0F;
        Vec3 direction = Vec3(0.0F, 0.0F, -1.0F);
        float inner_cos = 0.93969262F;
        Vec3 radiance = Vec3(1.0F, 1.0F, 1.0F);
        float outer_cos = 0.81915206F;
        int shadow_index = -1;
        float shadow_bias = 0.00035F;
        float padding0 = 0.0F;
    };

    struct TBX_API AreaLightFrameData
    {
        Vec3 position = Vec3(0.0F, 0.0F, 0.0F);
        float range = 10.0F;
        Vec3 direction = Vec3(0.0F, 0.0F, -1.0F);
        float half_width = 0.5F;
        Vec3 radiance = Vec3(1.0F, 1.0F, 1.0F);
        float half_height = 0.5F;
        Vec3 right = Vec3(1.0F, 0.0F, 0.0F);
        float shadow_bias = 0.00045F;
        Vec3 up = Vec3(0.0F, 1.0F, 0.0F);
        int shadow_index = -1;
    };

    struct TBX_API ShadowRenderInfo
    {
        ShadowFrameData shadows = {};
        std::vector<PointLightFrameData> point_lights = {};
        std::vector<RenderShadowItem> draw_items = {};
        Uuid shadow_shader_program = {};
        RenderFallbacks fallbacks = {};
    };

    struct TBX_API GeometryRenderInfo
    {
        Mat4 view_projection = Mat4(1.0F);
        std::vector<RenderDrawItem> draw_items = {};
        RenderFallbacks fallbacks = {};
        RenderUniformNames uniforms = {};
    };

    struct TBX_API LightingRenderInfo
    {
        bool has_camera = false;
        Vec3 camera_position = Vec3(0.0F, 0.0F, 0.0F);
        Mat4 view_matrix = Mat4(1.0F);
        Mat4 projection_matrix = Mat4(1.0F);
        Mat4 view_projection = Mat4(1.0F);
        Mat4 inverse_view_projection = Mat4(1.0F);
        std::vector<DirectionalLightFrameData> directional_lights = {};
        std::vector<PointLightFrameData> point_lights = {};
        std::vector<SpotLightFrameData> spot_lights = {};
        std::vector<AreaLightFrameData> area_lights = {};
        ShadowFrameData shadows = {};
        RenderStage render_stage = RenderStage::FINAL_COLOR;
        Uuid lighting_shader_program = {};
        Uuid scratch_color_texture = {};
        RenderFallbacks fallbacks = {};
    };

    struct TBX_API TransparentRenderInfo
    {
        Mat4 view_projection = Mat4(1.0F);
        std::vector<RenderDrawItem> draw_items = {};
        RenderFallbacks fallbacks = {};
        RenderUniformNames uniforms = {};
    };

    struct TBX_API PostProcessingPass
    {
        std::optional<PostProcessing> post_processing = std::nullopt;
        Uuid post_shader_program = {};
        Uuid scratch_color_texture = {};
        RenderFallbacks fallbacks = {};
    };


    enum class TBX_API RenderPassStatus
    {
        Success,
        Degraded,
        Fatal,
    };

    struct TBX_API RenderPassOutcome
    {
        RenderPassStatus status = RenderPassStatus::Success;
        std::string diagnostics = {};

        bool is_success() const
        {
            return status == RenderPassStatus::Success;
        }

        bool is_degraded() const
        {
            return status == RenderPassStatus::Degraded;
        }

        bool is_fatal() const
        {
            return status == RenderPassStatus::Fatal;
        }

        static RenderPassOutcome success()
        {
            return {};
        }

        static RenderPassOutcome degraded(std::string diagnostics_text)
        {
            auto outcome = RenderPassOutcome {};
            outcome.status = RenderPassStatus::Degraded;
            outcome.diagnostics = std::move(diagnostics_text);
            return outcome;
        }

        static RenderPassOutcome fatal(std::string diagnostics_text)
        {
            auto outcome = RenderPassOutcome {};
            outcome.status = RenderPassStatus::Fatal;
            outcome.diagnostics = std::move(diagnostics_text);
            return outcome;
        }
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
        virtual GraphicsApi get_api() const = 0;
        virtual Result initialize(GraphicsProcAddress loader) = 0;
        virtual Result upload(const Mesh& mesh, Uuid& out_resource_uuid) = 0;
        virtual Result upload(const Material& material, Uuid& out_resource_uuid) = 0;
        virtual Result upload(const Texture& texture, Uuid& out_resource_uuid) = 0;
        virtual Result upload(
            const TextureSettings& texture_settings,
            Uuid& out_resource_uuid) = 0;
        virtual Result unload(const Uuid& resource_uuid) = 0;
        virtual Result begin_draw(
            const Window& window,
            const Camera& view,
            const Size& resolution) = 0;
        virtual RenderPassOutcome draw_shadows(const ShadowRenderInfo& shadows) = 0;
        virtual RenderPassOutcome draw_geometry(const GeometryRenderInfo& geo) = 0;
        virtual RenderPassOutcome draw_lighting(const LightingRenderInfo& lighting) = 0;
        virtual RenderPassOutcome draw_transparent(const TransparentRenderInfo& transparency) = 0;
        virtual RenderPassOutcome apply_post_processing(const PostProcessingPass& post) = 0;
        virtual Result clear(const Color& color) = 0;
        virtual Result end_draw() = 0;
    };

    /// @brief
    /// Purpose: Builds render scenes and drives backend pass execution for all open windows.
    /// @details
    /// Ownership: Borrows engine systems and the active graphics backend.
    /// Thread Safety: Public calls are expected from the main thread; rendering runs on the
    /// configured render lane.
    class TBX_API RenderingPipeline final
    {
      public:
        RenderingPipeline(
            IMessageCoordinator& message_coordinator,
            ThreadManager& thread_manager,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            JobSystem& job_system,
            GraphicsSettings& settings,
            IWindowManager& window_manager,
            IGraphicsContextManager& context_manager,
            IGraphicsBackend& backend);
        ~RenderingPipeline() noexcept;

      public:
        GraphicsApi get_active_api() const;
        void render();

      private:
        void handle_message(Message& message);
        void process_asset_reload_queue();
        void sync_windows();

      private:

        struct RenderPassLogState
        {
            RenderPassStatus status = RenderPassStatus::Success;
            std::string diagnostics = {};
        };

        struct WindowRenderLogState
        {
            RenderPassLogState shadows = {};
            RenderPassLogState geometry = {};
            RenderPassLogState lighting = {};
            RenderPassLogState transparency = {};
            RenderPassLogState post_processing = {};
            bool has_reported_fallback = false;
        };

        IMessageCoordinator& _message_coordinator;
        ThreadManager& _thread_manager;
        EntityRegistry& _entity_registry;
        AssetManager& _asset_manager;
        JobSystem& _job_system;
        GraphicsSettings& _settings;
        IWindowManager& _window_manager;
        IGraphicsContextManager& _context_manager;
        IGraphicsBackend& _backend;
        std::unique_ptr<RenderResourceManager> _resource_manager = nullptr;
        Uuid _message_handler_token = {};
        bool _is_backend_initialized = false;
        std::unordered_map<Window, Size> _windows = {};
        std::vector<Handle> _pending_asset_reloads = {};
        mutable bool _has_reported_missing_camera = false;
        std::unordered_map<Window, WindowRenderLogState> _window_render_log_state = {};
    };

    /// @brief
    /// Purpose: Exposes the engine-owned render flow as a service.
    /// @details
    /// Ownership: Owns the render pipeline and borrows the selected backend.
    /// Thread Safety: Public calls are expected from the main thread unless documented otherwise.
    class TBX_API IRendering
    {
      public:
        virtual ~IRendering() noexcept = default;

      public:
        virtual GraphicsApi get_active_api() const = 0;
        virtual void render() = 0;
        virtual void set_api(const GraphicsApi& api) = 0;
    };

    /// @brief
    /// Purpose: Toybox rendering service that owns the engine render pipeline.
    /// @details
    /// Ownership: Owns the render pipeline and borrows the selected backend.
    /// Thread Safety: Public calls are expected from the main thread unless documented otherwise.
    class TBX_API Rendering final : public IRendering
    {
      public:
        Rendering(
            IMessageCoordinator& message_coordinator,
            ThreadManager& thread_manager,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            JobSystem& job_system,
            GraphicsSettings& settings,
            IWindowManager& window_manager,
            IGraphicsContextManager& context_manager,
            IGraphicsBackend& backend);
        ~Rendering() noexcept override;

      public:
        GraphicsApi get_active_api() const override;
        void set_api(const GraphicsApi& api) override;
        void render() override;

      private:
        GraphicsSettings& _settings;
        IGraphicsBackend& _backend;
        std::unique_ptr<RenderingPipeline> _pipeline = nullptr;
    };
}
