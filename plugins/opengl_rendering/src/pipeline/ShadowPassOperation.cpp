#include "ShadowPassOperation.h"
#include "builtin_assets.generated.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    ShadowPassOperation::ShadowPassOperation(OpenGlResourceManager& resource_manager)
        : _resource_manager(resource_manager)
    {
    }

    ShadowPassOperation::~ShadowPassOperation() noexcept
    {
        if (_shadow_sampler != 0U)
            glDeleteSamplers(1, &_shadow_sampler);
        if (_shadow_texture != 0U)
            glDeleteTextures(1, &_shadow_texture);
        if (_framebuffer != 0U)
            glDeleteFramebuffers(1, &_framebuffer);
    }

    void ShadowPassOperation::execute(const std::any& payload)
    {
        const auto& frame_context = std::any_cast<const OpenGlFrameContext&>(payload);
        if (frame_context.shadows.cascade_count == 0U || frame_context.shadow_draw_calls.empty())
            return;
        if (!ensure_initialized() || !ensure_shadow_targets(frame_context) || !_shader_program)
            return;

        const auto viewport = frame_context.shadows.map_resolution;
        const auto depth_test_enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
        const auto cull_enabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
        GLint previous_viewport[4] = {};
        glGetIntegerv(GL_VIEWPORT, previous_viewport);

        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
        glViewport(0, 0, static_cast<GLsizei>(viewport), static_cast<GLsizei>(viewport));
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glClearDepth(1.0);

        auto shader_scope = OpenGlResourceScope(*_shader_program);
        {
            for (tbx::uint32 cascade_index = 0U;
                 cascade_index < frame_context.shadows.cascade_count;
                 ++cascade_index)
            {
                const auto& cascade = frame_context.shadows.cascades[cascade_index];
                _shader_program->try_upload(
                    tbx::MaterialParameter("light_view_projection", cascade.light_view_projection));
                glFramebufferTextureLayer(
                    GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    _shadow_texture,
                    0,
                    static_cast<GLint>(cascade_index));
                glClear(GL_DEPTH_BUFFER_BIT);

                for (const auto& draw_call : frame_context.shadow_draw_calls)
                {
                    if (draw_call.is_two_sided)
                        glDisable(GL_CULL_FACE);
                    else
                        glEnable(GL_CULL_FACE);

                    for (std::size_t draw_index = 0U; draw_index < draw_call.meshes.size();
                         ++draw_index)
                    {
                        _shader_program->try_upload(
                            tbx::MaterialParameter("model", draw_call.transforms[draw_index]));
                        auto mesh = std::shared_ptr<OpenGlMesh> {};
                        if (!_resource_manager.try_get<OpenGlMesh>(
                                draw_call.meshes[draw_index],
                                mesh))
                            continue;

                        auto mesh_scope = OpenGlResourceScope(*mesh);
                        {
                            mesh->draw();
                        }
                    }
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(
            previous_viewport[0],
            previous_viewport[1],
            previous_viewport[2],
            previous_viewport[3]);
        if (depth_test_enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        if (cull_enabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    tbx::uint32 ShadowPassOperation::get_shadow_texture() const
    {
        return _shadow_texture;
    }

    bool ShadowPassOperation::ensure_initialized()
    {
        if (_framebuffer == 0U)
            glCreateFramebuffers(1, &_framebuffer);
        if (_shadow_sampler == 0U)
        {
            glCreateSamplers(1, &_shadow_sampler);
            glSamplerParameteri(_shadow_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glSamplerParameteri(_shadow_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(_shadow_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(_shadow_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(_shadow_sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
        if (_shader_program)
            return true;

        auto shadow_material = tbx::MaterialInstance();
        shadow_material.handle = tbx::directional_shadow_map_material;
        const auto program_id = _resource_manager.add_material(shadow_material, true);
        if (!program_id.is_valid())
            return false;
        return _resource_manager.try_get<OpenGlShaderProgram>(program_id, _shader_program);
    }

    bool ShadowPassOperation::ensure_shadow_targets(const OpenGlFrameContext& frame_context)
    {
        const auto resolution = frame_context.shadows.map_resolution;
        if (_shadow_texture != 0U && _resolution == resolution)
            return true;

        if (_shadow_texture != 0U)
            glDeleteTextures(1, &_shadow_texture);

        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &_shadow_texture);
        glTextureStorage3D(
            _shadow_texture,
            1,
            GL_DEPTH_COMPONENT32F,
            static_cast<GLsizei>(resolution),
            static_cast<GLsizei>(resolution),
            static_cast<GLsizei>(frame_context.shadows.cascade_count));
        glTextureParameteri(_shadow_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_shadow_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_shadow_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_shadow_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_shadow_texture, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glNamedFramebufferTexture(_framebuffer, GL_DEPTH_ATTACHMENT, _shadow_texture, 0);
        glNamedFramebufferDrawBuffer(_framebuffer, GL_NONE);
        glNamedFramebufferReadBuffer(_framebuffer, GL_NONE);
        _resolution = resolution;
        return glCheckNamedFramebufferStatus(_framebuffer, GL_FRAMEBUFFER)
               == GL_FRAMEBUFFER_COMPLETE;
    }
}
