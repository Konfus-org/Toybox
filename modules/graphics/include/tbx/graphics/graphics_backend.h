#pragma once
#include "tbx/common/result.h"
#include "tbx/common/typedefs.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/api.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/settings.h"
#include "tbx/graphics/shader.h"
#include "tbx/graphics/viewport.h"
#include "tbx/graphics/window.h"
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Defines how a GPU buffer will be bound by a graphics backend.
    /// @details
    /// Ownership: Enum values are copied by value by resource systems.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsBufferUsage
    {
        VERTEX,
        INDEX,
        UNIFORM,
        STORAGE,
    };

    /// @brief
    /// Purpose: Defines the type stored in a bound index buffer.
    /// @details
    /// Ownership: Enum values are copied by value by draw submissions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsIndexType
    {
        UINT16,
        UINT32,
    };

    /// @brief
    /// Purpose: Defines the primitive topology consumed by a draw command.
    /// @details
    /// Ownership: Enum values are copied by value by draw submissions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsPrimitiveType
    {
        TRIANGLES,
        LINES,
        POINTS,
    };

    /// @brief
    /// Purpose: Defines one vertex attribute's packed data format.
    /// @details
    /// Ownership: Enum values are copied by value by pipeline descriptions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsVertexFormat
    {
        FLOAT,
        VEC2,
        VEC3,
        VEC4,
        UINT32,
        INT32,
    };

    /// @brief
    /// Purpose: Defines how a texture resource can be used by a graphics backend.
    /// @details
    /// Ownership: Enum values are copied by value by resource systems.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsTextureUsage : uint8
    {
        SAMPLED = 1U << 0U,
        RENDER_TARGET = 1U << 1U,
        DEPTH_STENCIL = 1U << 2U,
        STORAGE = 1U << 3U,
        SAMPLED_RENDER_TARGET = (1U << 0U) | (1U << 1U),
        SAMPLED_DEPTH_STENCIL = (1U << 0U) | (1U << 2U),
    };

    /// @brief
    /// Purpose: Defines a backend-neutral texture pixel format.
    /// @details
    /// Ownership: Enum values are copied by value by resource systems.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsTextureFormat
    {
        RGBA8,
        RGBA16_FLOAT,
        RGBA32_FLOAT,
        DEPTH24_STENCIL8,
        DEPTH32_FLOAT,
    };

    /// @brief
    /// Purpose: Defines color, depth, and stencil buffers cleared at pass start.
    /// @details
    /// Ownership: Enum values are copied by value by pass submissions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsClearFlags : uint8
    {
        NONE = 0U,
        COLOR = 1U << 0U,
        DEPTH = 1U << 1U,
        STENCIL = 1U << 2U,
        COLOR_DEPTH = (1U << 0U) | (1U << 1U),
    };

    /// @brief
    /// Purpose: Describes frame-level render state shared by all views in a frame.
    /// @details
    /// Ownership: Owns submission state by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsFrameInfo
    {
        Window output_window = {};
        Size render_resolution = {};
        Size output_resolution = {};
    };

    /// @brief
    /// Purpose: Describes one camera viewport rendered within a graphics frame.
    /// @details
    /// Ownership: Owns camera and viewport data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsView
    {
        Camera camera = {};
        Viewport viewport = {};
    };

    /// @brief
    /// Purpose: Describes one buffer resource created by a graphics backend.
    /// @details
    /// Ownership: Owns descriptive values by copy; upload data is supplied separately.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsBufferDesc
    {
        GraphicsBufferUsage usage = GraphicsBufferUsage::VERTEX;
        uint64 size = 0U;
        bool is_dynamic = false;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes one texture resource created by a graphics backend.
    /// @details
    /// Ownership: Owns descriptive values by copy; upload data is supplied separately.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsTextureDesc
    {
        GraphicsTextureUsage usage = GraphicsTextureUsage::SAMPLED;
        GraphicsTextureFormat format = GraphicsTextureFormat::RGBA8;
        Size size = {1U, 1U};
        uint32 mip_count = 1U;
        uint32 array_layer_count = 1U;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes a texture update region.
    /// @details
    /// Ownership: Owns region values by copy; upload data is supplied separately.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsTextureUpdateDesc
    {
        uint32 x = 0U;
        uint32 y = 0U;
        uint32 width = 0U;
        uint32 height = 0U;
        uint32 mip_level = 0U;
        uint32 array_layer = 0U;
    };

    /// @brief
    /// Purpose: Describes one sampler resource created by a graphics backend.
    /// @details
    /// Ownership: Owns sampler values by copy.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsSamplerDesc
    {
        bool is_linear_filtering_enabled = true;
        bool is_mipmapping_enabled = true;
        bool is_repeating = true;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes how one bound vertex buffer is stepped during drawing.
    /// @details
    /// Ownership: Owns layout values by copy.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsVertexBufferLayoutDesc
    {
        uint32 slot = 0U;
        uint32 stride = 0U;
        bool is_per_instance = false;
    };

    /// @brief
    /// Purpose: Describes one shader input attribute read from a vertex buffer.
    /// @details
    /// Ownership: Owns attribute values by copy.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsVertexAttributeDesc
    {
        uint32 location = 0U;
        uint32 buffer_slot = 0U;
        uint32 offset = 0U;
        GraphicsVertexFormat format = GraphicsVertexFormat::FLOAT;
    };

    /// @brief
    /// Purpose: Describes fixed pipeline state realized by a graphics backend.
    /// @details
    /// Ownership: Owns state and shader sources by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsPipelineDesc
    {
        Shader shader = {};
        std::vector<GraphicsVertexBufferLayoutDesc> vertex_buffers = {};
        std::vector<GraphicsVertexAttributeDesc> vertex_attributes = {};
        GraphicsPrimitiveType primitive_type = GraphicsPrimitiveType::TRIANGLES;
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        bool is_blending_enabled = false;
        bool is_culling_enabled = true;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes a render pass target and clear behavior.
    /// @details
    /// Ownership: Stores resource identifiers and clear values by copy. An empty color target list
    /// means the backend should render to the active frame output when supported.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsPassDesc
    {
        std::vector<Uuid> color_targets = {};
        Uuid depth_stencil_target = {};
        Color clear_color = Color::BLACK;
        float clear_depth = 1.0F;
        uint32 clear_stencil = 0U;
        GraphicsClearFlags clear_flags = GraphicsClearFlags::NONE;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes one indexed draw command.
    /// @details
    /// Ownership: Owns draw ranges by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsDrawIndexedDesc
    {
        GraphicsPrimitiveType primitive_type = GraphicsPrimitiveType::TRIANGLES;
        GraphicsIndexType index_type = GraphicsIndexType::UINT32;
        uint32 index_count = 0U;
        uint32 index_offset = 0U;
        int32 vertex_offset = 0;
        uint32 instance_count = 1U;
        uint32 first_instance = 0U;
    };

    /// @brief
    /// Purpose: Defines the explicit command and resource contract implemented by graphics backends.
    /// @details
    /// Ownership: Implementations own backend state and realized GPU resources; Toybox owns render
    /// pass logic and issues backend-neutral commands.
    /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
    class TBX_API IGraphicsBackend
    {
      public:
        virtual ~IGraphicsBackend() noexcept = default;

      public:
        virtual Result initialize(const GraphicsSettings& settings) = 0;
        virtual void shutdown() = 0;

        virtual GraphicsApi get_api() const = 0;

        virtual void wait_for_idle() = 0;

        /// @brief
        /// Purpose: Begins rendering a frame that may contain one or more views.
        /// @details
        /// Ownership: The backend reads frame data during the call and owns any derived state.
        /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
        virtual Result begin_frame(const GraphicsFrameInfo& frame) = 0;

        /// @brief
        /// Purpose: Begins rendering a camera viewport within the active frame.
        /// @details
        /// Ownership: The backend reads view data during the call and owns any derived state.
        /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
        virtual Result begin_view(const GraphicsView& view) = 0;

        /// @brief
        /// Purpose: Ends the active frame after all views have been rendered and presented.
        /// @details
        /// Ownership: The backend owns frame resources and synchronization.
        /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
        virtual Result end_frame() = 0;

        /// @brief
        /// Purpose: Ends the active camera viewport.
        /// @details
        /// Ownership: The backend owns view resources and synchronization.
        /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
        virtual Result end_view() = 0;

        /// @brief
        /// Purpose: Presents the active frame to the configured output target.
        /// @details
        /// Ownership: The backend owns swapchain or presentation resources.
        /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
        virtual Result present() = 0;

        /// @brief
        /// Purpose: Begins a render pass against backend-owned targets.
        /// @details
        /// Ownership: The backend reads pass data during the call and owns any derived state.
        /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
        virtual Result begin_pass(const GraphicsPassDesc& pass) = 0;

        /// @brief
        /// Purpose: Ends the active render pass.
        /// @details
        /// Ownership: The backend owns pass synchronization and resource transitions.
        /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
        virtual Result end_pass() = 0;

        virtual Result set_viewport(const Viewport& viewport) = 0;
        virtual Result set_scissor(const Viewport& scissor) = 0;
        virtual Result bind_pipeline(const Uuid& pipeline_resource_uuid) = 0;
        virtual Result bind_vertex_buffer(uint32 slot, const Uuid& buffer_resource_uuid) = 0;
        virtual Result bind_index_buffer(
            const Uuid& buffer_resource_uuid,
            GraphicsIndexType index_type) = 0;
        virtual Result bind_uniform_buffer(uint32 slot, const Uuid& buffer_resource_uuid) = 0;
        virtual Result bind_storage_buffer(uint32 slot, const Uuid& buffer_resource_uuid) = 0;
        virtual Result bind_texture(uint32 slot, const Uuid& texture_resource_uuid) = 0;
        virtual Result bind_sampler(uint32 slot, const Uuid& sampler_resource_uuid) = 0;
        virtual Result draw(uint32 vertex_count, uint32 vertex_offset) = 0;
        virtual Result draw_indexed(const GraphicsDrawIndexedDesc& draw) = 0;

        virtual Result unload(const Uuid& resource_uuid) = 0;
        virtual Result upload_buffer(
            const GraphicsBufferDesc& desc,
            const void* data,
            uint64 data_size,
            Uuid& out_resource_uuid) = 0;
        virtual Result upload_pipeline(
            const GraphicsPipelineDesc& desc,
            Uuid& out_resource_uuid) = 0;
        virtual Result upload_sampler(
            const GraphicsSamplerDesc& desc,
            Uuid& out_resource_uuid) = 0;
        virtual Result upload_texture(
            const GraphicsTextureDesc& desc,
            const void* data,
            uint64 data_size,
            Uuid& out_resource_uuid) = 0;

        virtual Result update_settings(const GraphicsSettings& settings) = 0;
        virtual Result update_buffer(
            const Uuid& resource_uuid,
            const void* data,
            uint64 data_size,
            uint64 offset) = 0;
        virtual Result update_texture(
            const Uuid& resource_uuid,
            const GraphicsTextureUpdateDesc& desc,
            const void* data,
            uint64 data_size) = 0;
    };
}
