#pragma once
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/graphics/api.h"
#include "tbx/tbx_api.h"
#include "tbx/types/color.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/shader.h"
#include "tbx/types/size.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/viewport.h"
#include "tbx/utils/result.h"
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Defines how a GPU buffer will be bound by a graphics backend.
    /// @details
    /// Ownership: Enum values are copied by value by resource systems.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsBufferUsage : uint32
    {
        VERTEX = 1U << 0U,
        INDEX = 1U << 1U,
        UNIFORM = 1U << 2U,
        STORAGE = 1U << 3U,
        INDIRECT_ARGS = 1U << 4U,
        COPY_SRC = 1U << 5U,
        COPY_DST = 1U << 6U,
    };

    constexpr GraphicsBufferUsage operator|(
        const GraphicsBufferUsage left,
        const GraphicsBufferUsage right)
    {
        return static_cast<GraphicsBufferUsage>(
            static_cast<uint32>(left) | static_cast<uint32>(right));
    }

    constexpr GraphicsBufferUsage operator&(
        const GraphicsBufferUsage left,
        const GraphicsBufferUsage right)
    {
        return static_cast<GraphicsBufferUsage>(
            static_cast<uint32>(left) & static_cast<uint32>(right));
    }

    /// @brief
    /// Purpose: Defines which immutable pipeline category a backend resource represents.
    /// @details
    /// Ownership: Enum values are copied by value by pipeline descriptions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class PipelineType
    {
        RASTER,
        COMPUTE,
    };

    /// @brief
    /// Purpose: Defines the resource class expected at one bind group slot.
    /// @details
    /// Ownership: Enum values are copied by value by bind group descriptions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class BindingType
    {
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        STORAGE_BUFFER_DYNAMIC,
        SAMPLED_TEXTURE,
        STORAGE_TEXTURE,
    };

    /// @brief
    /// Purpose: Describes explicit resource synchronization states for backend barriers.
    /// @details
    /// Ownership: Enum values are copied by value by command submissions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class ResourceState
    {
        UNDEFINED,
        RENDER_TARGET,
        DEPTH_WRITE,
        DEPTH_READ,
        SHADER_READ_ONLY,
        UNORDERED_ACCESS,
        INDIRECT_ARGUMENT,
    };

    inline constexpr uint32 SHADER_STAGE_VERTEX = 1U << 0U;
    inline constexpr uint32 SHADER_STAGE_TESSELATION = 1U << 1U;
    inline constexpr uint32 SHADER_STAGE_GEOMETRY = 1U << 2U;
    inline constexpr uint32 SHADER_STAGE_FRAGMENT = 1U << 3U;
    inline constexpr uint32 SHADER_STAGE_COMPUTE = 1U << 4U;

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
    /// Purpose: Selects which winding side is rejected when face culling is enabled.
    /// @details
    /// Ownership: Enum values are copied by value by pipeline descriptions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class GraphicsCullMode
    {
        BACK,
        FRONT,
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
        bool is_depth_comparison_enabled = false;
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
    /// Purpose: Describes a resource bound to one backend binding slot.
    /// @details
    /// Ownership: Stores resource identifiers by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsResourceBinding
    {
        uint32 slot = 0U;
        Uuid resource = {};
    };

    /// @brief
    /// Purpose: Describes one resource expected by a bind group layout.
    /// @details
    /// Ownership: Owns binding metadata by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API BindGroupLayoutEntry
    {
        uint32 binding_slot = 0U;
        BindingType type = BindingType::UNIFORM_BUFFER;
        uint32 shader_stages = 0U;
    };

    /// @brief
    /// Purpose: Defines the resource layout expected by one bind group set.
    /// @details
    /// Ownership: Owns binding metadata and debug names by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API BindGroupLayoutDesc
    {
        std::vector<BindGroupLayoutEntry> entries = {};
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes one concrete resource bound into a bind group slot.
    /// @details
    /// Ownership: Stores backend resource identifiers by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API ResourceBinding
    {
        uint32 binding_slot = 0U;
        Uuid resource_handle = {};
        uint64 offset = 0U;
        uint64 range = 0U;
    };

    /// @brief
    /// Purpose: Describes a concrete bind group instance submitted by command recording.
    /// @details
    /// Ownership: Owns copied binding metadata and references backend resources by id.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API BindGroupDesc
    {
        Uuid layout_handle = {};
        std::vector<ResourceBinding> bindings = {};
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
    struct TBX_API RasterPipelineDesc
    {
        ShaderProgram shader = {};
        std::vector<BindGroupLayoutDesc> bind_group_layouts = {};
        std::vector<GraphicsVertexBufferLayoutDesc> vertex_buffers = {};
        std::vector<GraphicsVertexAttributeDesc> vertex_attributes = {};
        GraphicsPrimitiveType primitive_type = GraphicsPrimitiveType::TRIANGLES;
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        bool is_blending_enabled = false;
        bool is_culling_enabled = true;
        GraphicsCullMode cull_mode = GraphicsCullMode::BACK;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes immutable compute pipeline state realized by a graphics backend.
    /// @details
    /// Ownership: Owns state and shader sources by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API ComputePipelineDesc
    {
        ShaderProgram compute_shader = {};
        std::vector<BindGroupLayoutDesc> bind_group_layouts = {};
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes a render pass target and clear behavior.
    /// @details
    /// Ownership: Stores resource identifiers and clear values by copy. An empty color target list
    /// means the backend should render to the active frame output when supported.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsRenderPassDesc
    {
        std::vector<Uuid> color_targets = {};
        Uuid depth_stencil_target = {};
        int32 depth_stencil_layer = -1;
        Viewport viewport = {};
        Color clear_color = Color::BLACK;
        float clear_depth = 1.0F;
        uint32 clear_stencil = 0U;
        GraphicsClearFlags clear_flags = GraphicsClearFlags::NONE;
        bool is_color_write_enabled = true;
        std::string debug_name = {};
    };

    using GraphicsPassDesc = GraphicsRenderPassDesc;

    /// @brief
    /// Purpose: Describes a compute pass scope.
    /// @details
    /// Ownership: Owns descriptive values by copy.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsComputePassDesc
    {
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
    /// Purpose: Describes one explicit resource state transition.
    /// @details
    /// Ownership: Stores backend resource identifiers by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API PipelineBarrierDesc
    {
        Uuid resource_handle = {};
        ResourceState state_before = ResourceState::UNDEFINED;
        ResourceState state_after = ResourceState::UNDEFINED;
    };

    /// @brief
    /// Purpose: Defines the explicit resource and command contract implemented by graphics
    /// backends.
    /// @details
    /// Ownership: Implementations own backend state and realized GPU resources; Toybox owns render
    /// pass logic and issues backend-neutral commands.
    /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
    class TBX_API IGraphicsBackend
    {
      public:
        virtual ~IGraphicsBackend() noexcept = default;

      public:
        virtual GraphicsApi get_api() const = 0;
        virtual VsyncMode get_vsync() const = 0;
        virtual Result set_vsync(VsyncMode mode) = 0;

        virtual Result begin_frame(const Window& output_target) = 0;
        virtual Result end_frame() = 0;

        virtual Result create_bind_group(const BindGroupDesc& desc, Uuid& out_resource_uuid) = 0;
        virtual Result create_bind_group_layout(
            const BindGroupLayoutDesc& desc,
            Uuid& out_resource_uuid) = 0;
        virtual Result create_buffer(const GraphicsBufferDesc& desc, Uuid& out_resource_uuid) = 0;
        virtual Result create_compute_pipeline(
            const ComputePipelineDesc& desc,
            Uuid& out_resource_uuid) = 0;
        virtual Result create_raster_pipeline(
            const RasterPipelineDesc& desc,
            Uuid& out_resource_uuid) = 0;
        virtual Result create_sampler(const GraphicsSamplerDesc& desc, Uuid& out_resource_uuid) = 0;
        virtual Result create_texture(const GraphicsTextureDesc& desc, Uuid& out_resource_uuid) = 0;
        virtual Result destroy_resource(const Uuid& resource_uuid) = 0;

        virtual Result begin_render_pass(const GraphicsRenderPassDesc& pass) = 0;
        virtual Result end_render_pass() = 0;
        virtual Result begin_compute_pass(const GraphicsComputePassDesc& pass) = 0;
        virtual Result end_compute_pass() = 0;

        virtual Result present() = 0;
        virtual void wait_for_idle() = 0;

        virtual Result bind_group(uint32 set_index, const Uuid& group_resource_uuid) = 0;
        virtual Result bind_compute_pipeline(const Uuid& pipeline_resource_uuid) = 0;
        virtual Result bind_raster_pipeline(const Uuid& pipeline_resource_uuid) = 0;

        virtual Result draw(
            uint32 index_count,
            uint32 instance_count,
            uint32 first_index,
            int32 vertex_offset,
            uint32 first_instance) = 0;
        virtual Result draw_indirect(
            const Uuid& argument_buffer,
            uint64 offset,
            uint32 draw_count,
            uint32 stride) = 0;
        virtual Result dispatch_compute(
            uint32 group_count_x,
            uint32 group_count_y,
            uint32 group_count_z) = 0;
        virtual Result pipeline_barrier(const std::vector<PipelineBarrierDesc>& barriers) = 0;
        virtual Result write_buffer(
            const Uuid& resource_uuid,
            const void* data,
            uint64 data_size,
            uint64 offset) = 0;
        virtual Result write_texture(
            const Uuid& resource_uuid,
            const GraphicsTextureUpdateDesc& desc,
            const void* data,
            uint64 data_size) = 0;
    };
}
