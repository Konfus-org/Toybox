#pragma once
#include "opengl_backend.h"
#include "opengl_resources/opengl_utils.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

    static void apply_pipeline_state(const tbx::GraphicsPipelineDesc& desc)
    {
        desc.is_depth_test_enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        glDepthMask(desc.is_depth_write_enabled ? GL_TRUE : GL_FALSE);
        desc.is_blending_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        desc.is_culling_enabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
        if (desc.is_culling_enabled)
        {
            glCullFace(desc.cull_mode == tbx::GraphicsCullMode::FRONT ? GL_FRONT : GL_BACK);
        }

        if (desc.is_blending_enabled)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

}
