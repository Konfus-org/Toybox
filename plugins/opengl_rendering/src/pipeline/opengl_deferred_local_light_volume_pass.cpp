#include "opengl_deferred_local_light_volume_pass.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/mesh.h"
#include <cmath>
#include <glad/glad.h>
#include <numbers>
#include <vector>

namespace opengl_rendering
{
    static constexpr uint64 POINT_VOLUME_MESH_SIGNATURE = 0x7000000000000001ULL;
    static constexpr uint64 SPOT_VOLUME_MESH_SIGNATURE = 0x7000000000000002ULL;
    static constexpr uint64 AREA_VOLUME_MESH_SIGNATURE = 0x7000000000000003ULL;

    static tbx::Mesh make_spot_cone_proxy_mesh()
    {
        static constexpr uint32 SEGMENT_COUNT = 24U;

        auto vertices = std::vector<Vertex> {};
        auto indices = IndexBuffer {};

        vertices.reserve(static_cast<size_t>(SEGMENT_COUNT) + 2U);
        indices.reserve(static_cast<size_t>(SEGMENT_COUNT) * 6U);

        const Vertex tip_vertex = Vertex {
            .position = tbx::Vec3(0.0F, 1.0F, 0.0F),
            .normal = tbx::Vec3(0.0F, 1.0F, 0.0F),
            .uv = tbx::Vec2(0.5F, 1.0F),
            .color = Color::WHITE,
        };
        const Vertex base_center_vertex = Vertex {
            .position = tbx::Vec3(0.0F, 0.0F, 0.0F),
            .normal = tbx::Vec3(0.0F, -1.0F, 0.0F),
            .uv = tbx::Vec2(0.5F, 0.5F),
            .color = Color::WHITE,
        };
        vertices.push_back(tip_vertex);
        vertices.push_back(base_center_vertex);

        for (uint32 segment_index = 0U; segment_index < SEGMENT_COUNT; ++segment_index)
        {
            const float t = static_cast<float>(segment_index) / static_cast<float>(SEGMENT_COUNT);
            const float angle = t * 2.0F * std::numbers::pi_v<float>;
            const float x = std::cos(angle);
            const float z = std::sin(angle);
            const auto side_normal = normalize(tbx::Vec3(x, 0.5F, z));
            vertices.push_back(
                Vertex {
                    .position = tbx::Vec3(x, 0.0F, z),
                    .normal = side_normal,
                    .uv = tbx::Vec2(t, 0.0F),
                    .color = Color::WHITE,
                });
        }

        const uint32 tip_index = 0U;
        const uint32 base_center_index = 1U;
        const uint32 ring_start_index = 2U;
        for (uint32 segment_index = 0U; segment_index < SEGMENT_COUNT; ++segment_index)
        {
            const uint32 next_segment_index = (segment_index + 1U) % SEGMENT_COUNT;
            const uint32 current_ring_index = ring_start_index + segment_index;
            const uint32 next_ring_index = ring_start_index + next_segment_index;

            indices.push_back(tip_index);
            indices.push_back(current_ring_index);
            indices.push_back(next_ring_index);

            indices.push_back(base_center_index);
            indices.push_back(next_ring_index);
            indices.push_back(current_ring_index);
        }

        const auto vertex_buffer = VertexBuffer(
            vertices,
            {{
                tbx::Vec3(0.0F),
                tbx::Color(),
                tbx::Vec3(0.0F),
                tbx::Vec2(0.0F),
            }});
        return tbx::Mesh(vertex_buffer, indices);
    }

    static void bind_local_lighting_inputs(const OpenGlRenderFrameContext& frame_context)
    {
        glBindTextureUnit(0U, frame_context.gbuffer->get_albedo_spec_texture_id());
        glBindTextureUnit(1U, frame_context.gbuffer->get_normal_texture_id());
        glBindTextureUnit(2U, frame_context.gbuffer->get_material_texture_id());
        glBindTextureUnit(3U, frame_context.gbuffer->get_depth_texture_id());

        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0U,
            frame_context.light_culling.packed_lights_buffer_id);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            4U,
            frame_context.local_light_volumes.local_light_indices_buffer_id);
    }

    static void unbind_local_lighting_inputs()
    {
        glBindTextureUnit(0U, 0U);
        glBindTextureUnit(1U, 0U);
        glBindTextureUnit(2U, 0U);
        glBindTextureUnit(3U, 0U);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0U, 0U);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4U, 0U);
    }

    static void draw_local_light_type(
        OpenGlShaderProgram& shader_program,
        OpenGlMesh& mesh,
        const int light_type,
        const uint32 instance_offset,
        const uint32 instance_count)
    {
        if (instance_count == 0U)
            return;

        shader_program.try_upload(
            MaterialParameter {
                .name = "u_light_type",
                .data = light_type,
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_instance_offset",
                .data = static_cast<int>(instance_offset),
            });
        mesh.draw_instanced(instance_count);
    }

    void OpenGlDeferredLocalLightVolumePass::execute(
        const OpenGlRenderFrameContext& frame_context,
        OpenGlResourceManager& resource_manager) const
    {
        if (!frame_context.is_local_light_volume_enabled)
            return;
        if (frame_context.local_light_volume_shader_program == nullptr)
            return;
        if (frame_context.gbuffer == nullptr || frame_context.lighting_target == nullptr)
            return;
        if (frame_context.light_culling.packed_lights_buffer_id == 0U
            || frame_context.local_light_volumes.local_light_indices_buffer_id == 0U)
        {
            return;
        }

        const uint32 total_local_light_count = frame_context.local_light_volumes.point_light_count
                                               + frame_context.local_light_volumes.spot_light_count
                                               + frame_context.local_light_volumes.area_light_count;
        if (total_local_light_count == 0U)
            return;

        static const auto spot_proxy_mesh = make_spot_cone_proxy_mesh();
        auto point_mesh =
            resource_manager.get_or_create_runtime_mesh(POINT_VOLUME_MESH_SIGNATURE, sphere);
        auto spot_mesh = resource_manager.get_or_create_runtime_mesh(
            SPOT_VOLUME_MESH_SIGNATURE,
            spot_proxy_mesh);
        auto area_mesh =
            resource_manager.get_or_create_runtime_mesh(AREA_VOLUME_MESH_SIGNATURE, cube);
        if (!point_mesh || !spot_mesh || !area_mesh)
            return;

        glBindFramebuffer(GL_FRAMEBUFFER, frame_context.lighting_target->get_framebuffer_id());
        const auto lighting_resolution = frame_context.lighting_target->get_resolution();
        glViewport(
            0,
            0,
            static_cast<GLsizei>(lighting_resolution.width),
            static_cast<GLsizei>(lighting_resolution.height));
        glDisable(GL_DEPTH_TEST);
        // Render only one face set per convex volume so each light contributes once per pixel.
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        auto& shader_program = *frame_context.local_light_volume_shader_program;
        auto shader_scope = GlResourceScope(shader_program);
        bind_local_lighting_inputs(frame_context);

        shader_program.try_upload(
            MaterialParameter {
                .name = "u_gbuffer_albedo_spec",
                .data = 0,
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_gbuffer_normal",
                .data = 1,
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_gbuffer_material",
                .data = 2,
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_scene_depth",
                .data = 3,
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_inverse_view_projection",
                .data = frame_context.inverse_view_projection,
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

        uint32 instance_offset = 0U;
        {
            auto mesh_scope = GlResourceScope(*point_mesh);
            draw_local_light_type(
                shader_program,
                *point_mesh,
                static_cast<int>(OpenGlPackedLightType::Point),
                instance_offset,
                frame_context.local_light_volumes.point_light_count);
            instance_offset += frame_context.local_light_volumes.point_light_count;
        }
        {
            auto mesh_scope = GlResourceScope(*spot_mesh);
            draw_local_light_type(
                shader_program,
                *spot_mesh,
                static_cast<int>(OpenGlPackedLightType::Spot),
                instance_offset,
                frame_context.local_light_volumes.spot_light_count);
            instance_offset += frame_context.local_light_volumes.spot_light_count;
        }
        {
            auto mesh_scope = GlResourceScope(*area_mesh);
            draw_local_light_type(
                shader_program,
                *area_mesh,
                static_cast<int>(OpenGlPackedLightType::Area),
                instance_offset,
                frame_context.local_light_volumes.area_light_count);
        }

        unbind_local_lighting_inputs();
        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
