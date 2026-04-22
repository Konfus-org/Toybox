#include "opengl_utils.h"
#include "opengl_shader.h"
#include <algorithm>
#include <utility>

namespace opengl_rendering
{
    tbx::Result make_failure(std::string message)
    {
        auto result = tbx::Result {};
        result.flag_failure(std::move(message));
        return result;
    }

    tbx::Result make_success()
    {
        auto result = tbx::Result {};
        result.flag_success();
        return result;
    }

    bool has_clear_flag(const tbx::GraphicsClearFlags value, const tbx::GraphicsClearFlags flag)
    {
        return (static_cast<uint8>(value) & static_cast<uint8>(flag)) != 0U;
    }

    bool is_integer_vertex_format(const tbx::GraphicsVertexFormat format)
    {
        return format == tbx::GraphicsVertexFormat::UINT32
               || format == tbx::GraphicsVertexFormat::INT32;
    }

    const tbx::GraphicsVertexBufferLayoutDesc* find_vertex_buffer_layout(
        const tbx::GraphicsPipelineDesc& desc,
        const uint32 slot)
    {
        const auto it = std::ranges::find_if(
            desc.vertex_buffers,
            [slot](const tbx::GraphicsVertexBufferLayoutDesc& layout)
            {
                return layout.slot == slot;
            });
        return it == desc.vertex_buffers.end() ? nullptr : &(*it);
    }

    GLenum get_depth_attachment(const tbx::GraphicsTextureFormat format)
    {
        return format == tbx::GraphicsTextureFormat::DEPTH24_STENCIL8
                   ? GL_DEPTH_STENCIL_ATTACHMENT
                   : GL_DEPTH_ATTACHMENT;
    }

    GLenum get_texture_internal_format(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
                return GL_RGBA16F;
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
                return GL_RGBA32F;
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH24_STENCIL8;
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT32F;
            case tbx::GraphicsTextureFormat::RGBA8:
            default:
                return GL_RGBA8;
        }
    }

    GLenum get_texture_upload_format(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH_STENCIL;
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT;
            case tbx::GraphicsTextureFormat::RGBA8:
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
            default:
                return GL_RGBA;
        }
    }

    GLenum get_texture_upload_type(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_FLOAT;
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_UNSIGNED_INT_24_8;
            case tbx::GraphicsTextureFormat::RGBA8:
            default:
                return GL_UNSIGNED_BYTE;
        }
    }

    GLenum to_gl_buffer_target(const tbx::GraphicsBufferUsage usage)
    {
        switch (usage)
        {
            case tbx::GraphicsBufferUsage::INDEX:
                return GL_ELEMENT_ARRAY_BUFFER;
            case tbx::GraphicsBufferUsage::UNIFORM:
                return GL_UNIFORM_BUFFER;
            case tbx::GraphicsBufferUsage::STORAGE:
                return GL_SHADER_STORAGE_BUFFER;
            case tbx::GraphicsBufferUsage::VERTEX:
            default:
                return GL_ARRAY_BUFFER;
        }
    }

    GLenum to_gl_buffer_usage(const tbx::GraphicsBufferDesc& desc)
    {
        return desc.is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    }

    GLenum to_gl_index_type(const tbx::GraphicsIndexType index_type)
    {
        switch (index_type)
        {
            case tbx::GraphicsIndexType::UINT16:
                return GL_UNSIGNED_SHORT;
            case tbx::GraphicsIndexType::UINT32:
            default:
                return GL_UNSIGNED_INT;
        }
    }

    GLenum to_gl_primitive_type(const tbx::GraphicsPrimitiveType primitive_type)
    {
        switch (primitive_type)
        {
            case tbx::GraphicsPrimitiveType::LINES:
                return GL_LINES;
            case tbx::GraphicsPrimitiveType::POINTS:
                return GL_POINTS;
            case tbx::GraphicsPrimitiveType::TRIANGLES:
            default:
                return GL_TRIANGLES;
        }
    }

    GLint get_vertex_component_count(const tbx::GraphicsVertexFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsVertexFormat::VEC2:
                return 2;
            case tbx::GraphicsVertexFormat::VEC3:
                return 3;
            case tbx::GraphicsVertexFormat::VEC4:
                return 4;
            case tbx::GraphicsVertexFormat::FLOAT:
            case tbx::GraphicsVertexFormat::UINT32:
            case tbx::GraphicsVertexFormat::INT32:
            default:
                return 1;
        }
    }

    GLenum get_vertex_component_type(const tbx::GraphicsVertexFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsVertexFormat::UINT32:
                return GL_UNSIGNED_INT;
            case tbx::GraphicsVertexFormat::INT32:
                return GL_INT;
            case tbx::GraphicsVertexFormat::FLOAT:
            case tbx::GraphicsVertexFormat::VEC2:
            case tbx::GraphicsVertexFormat::VEC3:
            case tbx::GraphicsVertexFormat::VEC4:
            default:
                return GL_FLOAT;
        }
    }

    tbx::Result require_buffer_usage(
        const tbx::GraphicsBufferDesc& desc,
        const tbx::GraphicsBufferUsage usage,
        std::string failure_message)
    {
        if (desc.usage == usage)
            return make_success();

        return make_failure(std::move(failure_message));
    }

    tbx::Result create_shaders(
        const tbx::Shader& shader_desc,
        std::vector<std::shared_ptr<OpenGlShader>>& out_shaders)
    {
        if (shader_desc.sources.empty())
            return make_failure("OpenGL backend: pipeline has no shader sources.");

        out_shaders.reserve(shader_desc.sources.size());
        for (const auto& source : shader_desc.sources)
        {
            auto shader = std::make_shared<OpenGlShader>(source);
            if (!shader->compile())
                return make_failure("OpenGL backend: shader compilation failed.");

            out_shaders.push_back(std::move(shader));
        }

        return make_success();
    }

    uint64 get_texture_byte_size(const tbx::GraphicsTextureDesc& desc)
    {
        return static_cast<uint64>(desc.size.width) * static_cast<uint64>(desc.size.height)
               * get_texture_bytes_per_pixel(desc.format);
    }

    uint64 get_texture_bytes_per_pixel(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
                return 8U;
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
                return 16U;
            case tbx::GraphicsTextureFormat::RGBA8:
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
            default:
                return 4U;
        }
    }
}
