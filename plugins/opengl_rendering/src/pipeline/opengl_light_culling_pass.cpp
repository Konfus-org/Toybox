#include "opengl_light_culling_pass.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    static constexpr uint32 LIGHT_CULLING_WORKGROUP_SIZE_X = 8U;
    static constexpr uint32 LIGHT_CULLING_WORKGROUP_SIZE_Y = 8U;

    static uint32 divide_round_up(const uint32 numerator, const uint32 denominator)
    {
        if (denominator == 0U)
            return 0U;

        return (numerator + denominator - 1U) / denominator;
    }

    void OpenGlLightCullingPass::execute(const OpenGlRenderFrameContext& frame_context) const
    {
        if (!frame_context.is_compute_culling_enabled)
            return;
        if (frame_context.light_culling_shader_program == nullptr)
            return;
        if (frame_context.light_culling.tile_count_x == 0U
            || frame_context.light_culling.tile_count_y == 0U)
        {
            return;
        }
        if (frame_context.light_culling.packed_lights_buffer_id == 0U
            || frame_context.light_culling.tile_headers_buffer_id == 0U
            || frame_context.light_culling.tile_light_indices_buffer_id == 0U
            || frame_context.light_culling.tile_overflow_counter_buffer_id == 0U)
        {
            return;
        }

        auto& shader_program = *frame_context.light_culling_shader_program;
        shader_program.bind();

        const GLuint clear_value = 0U;
        glClearNamedBufferData(
            frame_context.light_culling.tile_overflow_counter_buffer_id,
            GL_R32UI,
            GL_RED_INTEGER,
            GL_UNSIGNED_INT,
            &clear_value);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0U, frame_context.light_culling.packed_lights_buffer_id);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            1U,
            frame_context.light_culling.tile_headers_buffer_id);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            2U,
            frame_context.light_culling.tile_light_indices_buffer_id);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            3U,
            frame_context.light_culling.tile_overflow_counter_buffer_id);

        shader_program.try_upload(
            MaterialParameter {
                .name = "u_screen_size",
                .data = tbx::Vec2(
                    static_cast<float>(frame_context.render_resolution.width),
                    static_cast<float>(frame_context.render_resolution.height)),
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_tile_size",
                .data = static_cast<int>(frame_context.light_culling.tile_size),
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_tile_count_x",
                .data = static_cast<int>(frame_context.light_culling.tile_count_x),
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_tile_count_y",
                .data = static_cast<int>(frame_context.light_culling.tile_count_y),
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_max_lights_per_tile",
                .data = static_cast<int>(frame_context.light_culling.max_lights_per_tile),
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_packed_light_count",
                .data = static_cast<int>(frame_context.light_culling.packed_light_count),
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_camera_position",
                .data = frame_context.camera_world_position,
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_view_projection",
                .data = frame_context.view_projection,
            });

        const uint32 dispatch_x = divide_round_up(
            frame_context.light_culling.tile_count_x,
            LIGHT_CULLING_WORKGROUP_SIZE_X);
        const uint32 dispatch_y = divide_round_up(
            frame_context.light_culling.tile_count_y,
            LIGHT_CULLING_WORKGROUP_SIZE_Y);
        glDispatchCompute(dispatch_x, dispatch_y, 1U);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0U, 0U);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1U, 0U);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2U, 0U);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3U, 0U);
        shader_program.unbind();
    }
}
