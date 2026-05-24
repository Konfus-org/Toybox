#pragma once
#include "opengl_resources/internal/opengl_buffers_internal.h"
#include "opengl_resources/internal/opengl_shader_internal.h"
#include "opengl_resources/internal/opengl_texture_internal.h"
#include "opengl_resources/opengl_binding.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_cache.h"
#include "opengl_resources/opengl_shader.h"
#include "opengl_resources/opengl_state.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <glad/glad.h>
#include <string>
#include <string_view>
#include <vector>

namespace opengl_rendering::internal
{
    tbx::Result make_failure(std::string message);
    tbx::Result make_success();
    tbx::Result consume_gl_errors(std::string_view operation);
    bool has_clear_flag(tbx::GraphicsClearFlags value, tbx::GraphicsClearFlags flag);
    bool is_integer_vertex_format(tbx::GraphicsVertexFormat format);
    const tbx::GraphicsVertexBufferLayoutDesc* find_vertex_buffer_layout(
        const tbx::RasterPipelineDesc& desc,
        uint32 slot);
    GLenum to_gl_primitive_type(tbx::GraphicsPrimitiveType primitive_type);
    GLint get_vertex_component_count(tbx::GraphicsVertexFormat format);
    GLenum get_vertex_component_type(tbx::GraphicsVertexFormat format);

    bool is_same_buffer_slot_binding(
        const OpenGlBufferSlotBinding& binding,
        const tbx::Uuid& resource,
        uint64 offset,
        uint64 range);
    tbx::Result configure_vertex_array(
        GLuint vertex_array,
        const tbx::RasterPipelineDesc& desc,
        std::vector<OpenGlVertexBufferBinding>& out_vertex_buffers);
    const OpenGlVertexBufferBinding* find_vertex_buffer_binding(
        const OpenGlRasterPipelineResource& pipeline,
        uint32 slot);
    const OpenGlBindGroupLayoutEntry* find_bind_group_layout_entry(
        const std::vector<OpenGlBindGroupLayoutEntry>& layout,
        uint32 slot);
    bool can_bind_buffer_as(const OpenGlBufferResource& buffer, OpenGlBindEntryType type);
    tbx::Result require_buffer_capability(
        const OpenGlBufferResource& buffer,
        OpenGlBindEntryType type,
        std::string failure_message);
    OpenGlPipelineState make_pipeline_state(
        const tbx::Uuid& pipeline_resource_uuid,
        const tbx::RasterPipelineDesc& desc);
    void bind_buffer_slot(
        GLenum target,
        uint32 slot,
        const OpenGlGraphicsBuffer& buffer,
        uint64 offset,
        uint64 range);
    tbx::Result require_opengl_4_5_direct_state_access();
}
