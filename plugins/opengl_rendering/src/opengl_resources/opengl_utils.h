#pragma once
#include "tbx/common/result.h"
#include "tbx/graphics/graphics_backend.h"
#include <glad/glad.h>
#include <memory>
#include <string>
#include <vector>

namespace opengl_rendering
{
    class OpenGlShader;

    tbx::Result make_failure(std::string message);
    tbx::Result make_success();
    bool has_clear_flag(tbx::GraphicsClearFlags value, tbx::GraphicsClearFlags flag);
    bool is_integer_vertex_format(tbx::GraphicsVertexFormat format);
    const tbx::GraphicsVertexBufferLayoutDesc* find_vertex_buffer_layout(
        const tbx::GraphicsPipelineDesc& desc,
        uint32 slot);
    GLenum get_depth_attachment(tbx::GraphicsTextureFormat format);
    GLenum get_texture_internal_format(tbx::GraphicsTextureFormat format);
    GLenum get_texture_upload_format(tbx::GraphicsTextureFormat format);
    GLenum get_texture_upload_type(tbx::GraphicsTextureFormat format);
    GLenum to_gl_buffer_target(tbx::GraphicsBufferUsage usage);
    GLenum to_gl_buffer_usage(const tbx::GraphicsBufferDesc& desc);
    GLenum to_gl_index_type(tbx::GraphicsIndexType index_type);
    GLenum to_gl_primitive_type(tbx::GraphicsPrimitiveType primitive_type);
    GLint get_vertex_component_count(tbx::GraphicsVertexFormat format);
    GLenum get_vertex_component_type(tbx::GraphicsVertexFormat format);
    tbx::Result require_buffer_usage(
        const tbx::GraphicsBufferDesc& desc,
        tbx::GraphicsBufferUsage usage,
        std::string failure_message);
    tbx::Result create_shaders(
        const tbx::Shader& shader_desc,
        std::vector<std::shared_ptr<OpenGlShader>>& out_shaders);
    uint64 get_texture_byte_size(const tbx::GraphicsTextureDesc& desc);
    uint64 get_texture_bytes_per_pixel(tbx::GraphicsTextureFormat format);
}
