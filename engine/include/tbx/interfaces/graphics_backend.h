#pragma once
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/graphics/api.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/color.h"
#include "tbx/types/size.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/viewport.h"
#include "tbx/utils/result.h"
#include <limits>
#include <string>
#include <vector>

namespace tbx
{
    using GpuId = uint64;
    inline constexpr GpuId INVALID_GPU_ID = std::numeric_limits<GpuId>::max();

    /// @brief
    /// Purpose: Defines how a GPU buffer will be bound by a graphics backend.
    /// @details
    /// Ownership: Enum values are copied by value by resource systems.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class BufferUsage : uint32
    {
        VERTEX = 1U << 0U,
        INDEX = 1U << 1U,
        UNIFORM = 1U << 2U,
        STORAGE = 1U << 3U,
        INDIRECT_ARGS = 1U << 4U,
        COPY_SRC = 1U << 5U,
        COPY_DST = 1U << 6U,
    };

    constexpr BufferUsage operator|(
        const BufferUsage left,
        const BufferUsage right)
    {
        return static_cast<BufferUsage>(
            static_cast<uint32>(left) | static_cast<uint32>(right));
    }

    constexpr BufferUsage operator&(
        const BufferUsage left,
        const BufferUsage right)
    {
        return static_cast<BufferUsage>(
            static_cast<uint32>(left) & static_cast<uint32>(right));
    }

    /// @brief
    /// Purpose: Defines the primitive topology consumed by a draw command.
    /// @details
    /// Ownership: Enum values are copied by value by draw submissions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class PrimitiveType
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
    enum class CullMode
    {
        BACK,
        FRONT,
    };

    /// @brief
    /// Purpose: Defines one vertex attribute's packed data format.
    /// @details
    /// Ownership: Enum values are copied by value by pipeline descriptions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class VertexFormat
    {
        FLOAT,
        VEC2,
        VEC3,
        VEC4,
        UINT32,
        INT32,
    };

    /// @brief
    /// Purpose: Defines color, depth, and stencil buffers cleared at pass start.
    /// @details
    /// Ownership: Enum values are copied by value by pass submissions.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class ClearFlags : uint8
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
    struct TBX_API BufferDesc
    {
        BufferUsage usage = BufferUsage::VERTEX;
        uint64 size = 0U;
        bool is_dynamic = false;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes one texture resource created by a graphics backend.
    /// @details
    /// Ownership: Owns descriptive values by copy; upload data is supplied separately.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API TextureDesc
    {
        TextureUsage usage = TextureUsage::SAMPLED;
        TextureFormat format = TextureFormat::RGBA8;
        Size size = {1U, 1U};
        uint32 mip_count = 1U;
        uint32 array_layer_count = 1U;
        bool is_depth_comparison_enabled = false;
        bool is_linear_filtering_enabled = true;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes a texture update region.
    /// @details
    /// Ownership: Owns region values by copy; upload data is supplied separately.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API TextureUpdateDesc
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
    struct TBX_API SamplerDesc
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
        GpuId resource = INVALID_GPU_ID;
    };

    /// @brief
    /// Purpose: Describes one concrete resource bound into a bind group slot.
    /// @details
    /// Ownership: Stores backend resource identifiers by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API ResourceBinding
    {
        uint32 binding_slot = 0U;
        GpuId resource_handle = INVALID_GPU_ID;
        uint64 offset = 0U;
        uint64 range = 0U;
    };

    /// @brief
    /// Purpose: Describes a concrete bind group instance submitted by command recording.
    /// @details
    /// Ownership: Owns copied binding metadata and references backend resources by id.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally. The backend
    /// resolves each binding's resource class from the bound resource's creation usage.
    struct TBX_API BindGroupDesc
    {
        std::vector<ResourceBinding> bindings = {};
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes how one bound vertex buffer is stepped during drawing.
    /// @details
    /// Ownership: Owns layout values by copy.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API VertexBufferLayoutDesc
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
    struct TBX_API VertexAttributeDesc
    {
        uint32 location = 0U;
        uint32 buffer_slot = 0U;
        uint32 offset = 0U;
        VertexFormat format = VertexFormat::FLOAT;
    };

    /// @brief
    /// Purpose: Describes fixed pipeline state realized by a graphics backend.
    /// @details
    /// Ownership: Owns state and shader sources by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RasterPipelineDesc
    {
        std::vector<Shader> shaders = {};
        std::vector<VertexBufferLayoutDesc> vertex_buffers = {};
        std::vector<VertexAttributeDesc> vertex_attributes = {};
        PrimitiveType primitive_type = PrimitiveType::TRIANGLES;
        MaterialDepthFunction depth_function = MaterialDepthFunction::LESS;
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        bool is_blending_enabled = false;
        bool is_culling_enabled = true;
        float depth_bias_constant = 0.0F;
        float depth_bias_slope = 0.0F;
        CullMode cull_mode = CullMode::BACK;
        std::string debug_name = {};
    };

    /// @brief
    /// Purpose: Describes a render pass target and clear behavior.
    /// @details
    /// Ownership: Stores resource identifiers and clear values by copy. An empty color target list
    /// means the backend should render to the active frame output when supported.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderPassDesc
    {
        std::vector<GpuId> color_targets = {};
        GpuId depth_stencil_target = INVALID_GPU_ID;
        int32 depth_stencil_layer = -1;
        Viewport viewport = {};
        Color clear_color = Color::BLACK;
        float clear_depth = 1.0F;
        uint32 clear_stencil = 0U;
        ClearFlags clear_flags = ClearFlags::NONE;
        bool is_color_write_enabled = true;
        std::string debug_name = {};
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
        virtual Result begin_render_pass(const RenderPassDesc& pass) = 0;
        virtual Result end_render_pass() = 0;

        virtual Result destroy_resource(const GpuId& resource_uuid) = 0;

        virtual Result create_bind_group(const BindGroupDesc& desc, GpuId& out_resource_uuid) = 0;
        virtual Result create_buffer(const BufferDesc& desc, GpuId& out_resource_uuid) = 0;
        virtual Result create_raster_pipeline(
            const RasterPipelineDesc& desc,
            GpuId& out_resource_uuid) = 0;
        virtual Result create_sampler(
            const SamplerDesc& desc,
            GpuId& out_resource_uuid) = 0;
        virtual Result create_texture(
            const TextureDesc& desc,
            GpuId& out_resource_uuid) = 0;

        /// @brief Whether the backend supports referencing textures by resident bindless handle.
        virtual bool supports_bindless_textures() const = 0;
        /// @brief Returns a resident bindless handle for a sampled texture, indexable from shaders.
        /// @details Fails if bindless is unsupported or the resource is not a sampled texture.
        virtual Result get_texture_bindless_handle(
            const GpuId& texture_uuid,
            uint64& out_handle) = 0;

        virtual Result write_buffer(
            const GpuId& resource_uuid,
            const void* data,
            uint64 data_size,
            uint64 offset) = 0;
        virtual Result write_texture(
            const GpuId& resource_uuid,
            const TextureUpdateDesc& desc,
            const void* data,
            uint64 data_size) = 0;

        virtual Result present() = 0;
        virtual void wait_for_idle() = 0;

        virtual Result bind_group(uint32 set_index, const GpuId& group_resource_uuid) = 0;
        virtual Result bind_raster_pipeline(const GpuId& pipeline_resource_uuid) = 0;

        virtual Result draw(
            uint32 index_count,
            uint32 instance_count,
            uint32 first_index,
            int32 vertex_offset,
            uint32 first_instance) = 0;
        virtual Result draw_indirect(
            const GpuId& argument_buffer,
            uint64 offset,
            uint32 draw_count,
            uint32 stride) = 0;
    };
}
