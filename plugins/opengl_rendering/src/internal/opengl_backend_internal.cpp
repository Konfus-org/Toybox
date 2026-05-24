#include "opengl_backend_internal.h"
#include <algorithm>
#include <utility>

namespace opengl_rendering::internal
{
    static std::string gl_error_to_string(const GLenum error)
    {
        switch (error)
        {
            case GL_INVALID_ENUM:
                return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE:
                return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION:
                return "GL_INVALID_OPERATION";
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                return "GL_INVALID_FRAMEBUFFER_OPERATION";
            case GL_OUT_OF_MEMORY:
                return "GL_OUT_OF_MEMORY";
            case GL_STACK_UNDERFLOW:
                return "GL_STACK_UNDERFLOW";
            case GL_STACK_OVERFLOW:
                return "GL_STACK_OVERFLOW";
            default:
                return "0x" + std::to_string(static_cast<uint32>(error));
        }
    }

    static const char* get_gl_string(const GLenum name)
    {
        const auto* value = glGetString(name);
        return value ? reinterpret_cast<const char*>(value) : "unknown";
    }

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

    tbx::Result consume_gl_errors(const std::string_view operation)
    {
        auto message = std::string {};
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            if (message.empty())
            {
                message = "OpenGL backend: ";
                message.append(operation);
                message.append(" failed with OpenGL error(s): ");
            }
            else
            {
                message.append(", ");
            }

            message.append(gl_error_to_string(error));
        }

        if (!message.empty())
            return make_failure(std::move(message));

        return make_success();
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
        const tbx::RasterPipelineDesc& desc,
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

    bool is_same_buffer_slot_binding(
        const OpenGlBufferSlotBinding& binding,
        const tbx::Uuid& resource,
        const uint64 offset,
        const uint64 range)
    {
        return binding.resource == resource && binding.offset == offset && binding.range == range;
    }

    tbx::Result configure_vertex_array(
        const GLuint vertex_array,
        const tbx::RasterPipelineDesc& desc,
        std::vector<OpenGlVertexBufferBinding>& out_vertex_buffers)
    {
        out_vertex_buffers.clear();
        out_vertex_buffers.reserve(desc.vertex_buffers.size());
        for (const auto& layout : desc.vertex_buffers)
        {
            glVertexArrayBindingDivisor(
                vertex_array,
                layout.slot,
                layout.is_per_instance ? 1U : 0U);
            out_vertex_buffers.push_back(
                OpenGlVertexBufferBinding {
                    .slot = layout.slot,
                    .stride = static_cast<GLsizei>(layout.stride),
                });
        }

        for (const auto& attribute : desc.vertex_attributes)
        {
            const auto* layout = find_vertex_buffer_layout(desc, attribute.buffer_slot);
            if (layout == nullptr)
            {
                return make_failure(
                    "OpenGL backend: vertex attribute references a missing vertex buffer slot.");
            }

            glEnableVertexArrayAttrib(vertex_array, attribute.location);
            glVertexArrayAttribBinding(vertex_array, attribute.location, attribute.buffer_slot);

            if (is_integer_vertex_format(attribute.format))
            {
                glVertexArrayAttribIFormat(
                    vertex_array,
                    attribute.location,
                    get_vertex_component_count(attribute.format),
                    get_vertex_component_type(attribute.format),
                    attribute.offset);
            }
            else
            {
                glVertexArrayAttribFormat(
                    vertex_array,
                    attribute.location,
                    get_vertex_component_count(attribute.format),
                    get_vertex_component_type(attribute.format),
                    GL_FALSE,
                    attribute.offset);
            }
        }

        return make_success();
    }

    const OpenGlVertexBufferBinding* find_vertex_buffer_binding(
        const OpenGlRasterPipelineResource& pipeline,
        const uint32 slot)
    {
        const auto it = std::ranges::find_if(
            pipeline.vertex_buffers,
            [slot](const OpenGlVertexBufferBinding& binding)
            {
                return binding.slot == slot;
            });
        return it == pipeline.vertex_buffers.end() ? nullptr : &(*it);
    }

    const OpenGlBindGroupLayoutEntry* find_bind_group_layout_entry(
        const std::vector<OpenGlBindGroupLayoutEntry>& layout,
        const uint32 slot)
    {
        const auto it = std::ranges::find_if(
            layout,
            [slot](const OpenGlBindGroupLayoutEntry& entry)
            {
                return entry.slot == slot;
            });
        return it == layout.end() ? nullptr : &(*it);
    }

    bool can_bind_buffer_as(const OpenGlBufferResource& buffer, const OpenGlBindEntryType type)
    {
        switch (type)
        {
            case OpenGlBindEntryType::VERTEX_BUFFER:
                return buffer.is_vertex_buffer;
            case OpenGlBindEntryType::INDEX_BUFFER:
                return buffer.is_index_buffer;
            case OpenGlBindEntryType::UNIFORM_BUFFER:
                return buffer.is_uniform_buffer;
            case OpenGlBindEntryType::STORAGE_BUFFER:
                return buffer.is_storage_buffer;
            default:
                return false;
        }
    }

    tbx::Result require_buffer_capability(
        const OpenGlBufferResource& buffer,
        const OpenGlBindEntryType type,
        std::string failure_message)
    {
        if (can_bind_buffer_as(buffer, type))
            return make_success();

        return make_failure(std::move(failure_message));
    }

    OpenGlPipelineState make_pipeline_state(
        const tbx::Uuid& pipeline_resource_uuid,
        const tbx::RasterPipelineDesc& desc)
    {
        return OpenGlPipelineState {
            .id = pipeline_resource_uuid,
            .is_depth_test_enabled = desc.is_depth_test_enabled,
            .is_depth_write_enabled = desc.is_depth_write_enabled,
            .is_blending_enabled = desc.is_blending_enabled,
            .is_culling_enabled = desc.is_culling_enabled,
            .cull_mode = desc.cull_mode,
        };
    }

    void bind_buffer_slot(
        const GLenum target,
        const uint32 slot,
        const OpenGlGraphicsBuffer& buffer,
        const uint64 offset,
        const uint64 range)
    {
        if (range > 0U)
        {
            glBindBufferRange(
                target,
                slot,
                buffer.get_buffer_id(),
                static_cast<GLintptr>(offset),
                static_cast<GLsizeiptr>(range));
            return;
        }

        glBindBufferBase(target, slot, buffer.get_buffer_id());
    }

    tbx::Result require_opengl_4_5_direct_state_access()
    {
        if (GLAD_GL_VERSION_4_5 && glCreateBuffers && glNamedBufferData && glNamedBufferSubData
            && glCreateVertexArrays && glVertexArrayVertexBuffer && glVertexArrayElementBuffer
            && glCreateFramebuffers && glNamedFramebufferTexture && glNamedFramebufferDrawBuffers
            && glCreateTextures)
            return make_success();

        auto message = std::string("OpenGL backend requires OpenGL 4.5 direct state access. ");
        message += "Driver reported version '";
        message += get_gl_string(GL_VERSION);
        message += "', renderer '";
        message += get_gl_string(GL_RENDERER);
        message += "'.";
        return make_failure(std::move(message));
    }
}
