#pragma once
#include "opengl_resources/opengl_utils.h"
#include "tbx/interfaces/graphics_backend.h"
#include <glad/glad.h>
#include <string>
#include <utility>

namespace opengl_rendering::internal
{
    static const char* get_gl_string(const GLenum name)
    {
        const auto* value = glGetString(name);
        return value ? reinterpret_cast<const char*>(value) : "unknown";
    }

    static tbx::Result require_opengl_4_5_direct_state_access()
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
