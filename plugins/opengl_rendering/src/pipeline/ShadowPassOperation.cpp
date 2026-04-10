#include "ShadowPassOperation.h"
#include "opengl_resources/opengl_mesh.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/frustum.h"
#include "tbx/math/matrices.h"
#include "tbx/math/trig.h"
#include <array>
#include <cstdint>
#include <glad/glad.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace opengl_rendering
{
    static constexpr auto LightViewProjectionUniformName = "u_light_view_projection";
    static constexpr auto ModelUniformName = "u_model";
    static constexpr auto WriteLinearDepthUniformName = "u_write_linear_depth";
    static constexpr auto PointLightPositionUniformName = "u_point_light_position";
    static constexpr auto PointLightFarPlaneUniformName = "u_point_light_far_plane";
    static constexpr auto ShadowPointFaceCount = uint32 {6U};

    static uint32 take_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0U);
    }

    static void delete_texture(uint32& texture_id) noexcept
    {
        if (texture_id == 0U)
            return;

        auto texture = take_gl_handle(texture_id);
        glDeleteTextures(1, &texture);
    }

    static std::shared_ptr<OpenGlShaderProgram> build_shadow_shader_program()
    {
        auto shader_sources = std::array {
            tbx::ShaderSource(
                R"(#version 450 core
layout(location = 0) in vec3 a_position;

uniform mat4 u_light_view_projection;
uniform mat4 u_model;

out vec3 v_world_position;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    v_world_position = world_position.xyz;
    gl_Position = u_light_view_projection * world_position;
}
)",
                tbx::ShaderType::VERTEX),
            tbx::ShaderSource(
                R"(#version 450 core

in vec3 v_world_position;

uniform int u_write_linear_depth;
uniform vec3 u_point_light_position;
uniform float u_point_light_far_plane;

void main()
{
    if (u_write_linear_depth != 0)
        gl_FragDepth =
            length(v_world_position - u_point_light_position) / max(u_point_light_far_plane, 0.0001);
}
)",
                tbx::ShaderType::FRAGMENT),
        };

        auto shaders = std::vector<std::shared_ptr<OpenGlShader>> {};
        shaders.reserve(shader_sources.size());
        for (const auto& shader_source : shader_sources)
        {
            auto shader = std::make_shared<OpenGlShader>(shader_source);
            if (!shader->compile())
                return nullptr;
            shaders.push_back(std::move(shader));
        }

        auto shader_program = std::make_shared<OpenGlShaderProgram>(shaders);
        if (shader_program->get_program_id() == 0U)
            return nullptr;

        return shader_program;
    }

    static void configure_shadow_depth_array(
        uint32& texture_id,
        uint32& cached_resolution,
        uint32& cached_layer_capacity,
        const uint32 required_resolution,
        const uint32 required_layers)
    {
        if (required_layers == 0U)
            return;
        if (texture_id != 0U && cached_resolution == required_resolution
            && cached_layer_capacity == required_layers)
            return;

        delete_texture(texture_id);
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texture_id);
        glTextureStorage3D(
            texture_id,
            1,
            GL_DEPTH_COMPONENT32F,
            static_cast<GLsizei>(required_resolution),
            static_cast<GLsizei>(required_resolution),
            static_cast<GLsizei>(required_layers));
        glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTextureParameteri(texture_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        const auto border_color = std::array {1.0F, 1.0F, 1.0F, 1.0F};
        glTextureParameterfv(texture_id, GL_TEXTURE_BORDER_COLOR, border_color.data());
        cached_resolution = required_resolution;
        cached_layer_capacity = required_layers;
    }

    static void configure_shadow_cube_array(
        uint32& texture_id,
        uint32& cached_resolution,
        uint32& cached_light_capacity,
        const uint32 required_resolution,
        const uint32 required_light_count)
    {
        if (required_light_count == 0U)
            return;
        if (texture_id != 0U && cached_resolution == required_resolution
            && cached_light_capacity == required_light_count)
            return;

        delete_texture(texture_id);
        glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &texture_id);
        glTextureStorage3D(
            texture_id,
            1,
            GL_DEPTH_COMPONENT32F,
            static_cast<GLsizei>(required_resolution),
            static_cast<GLsizei>(required_resolution),
            static_cast<GLsizei>(required_light_count * ShadowPointFaceCount));
        glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        cached_resolution = required_resolution;
        cached_light_capacity = required_light_count;
    }

    enum class ShadowFramebufferPass : uint8_t
    {
        Directional = 0U,
        Spot = 1U,
        Area = 2U,
        Point = 3U,
    };

    static const char* to_string(const ShadowFramebufferPass pass)
    {
        switch (pass)
        {
            case ShadowFramebufferPass::Directional:
                return "directional";
            case ShadowFramebufferPass::Spot:
                return "spot";
            case ShadowFramebufferPass::Area:
                return "area";
            case ShadowFramebufferPass::Point:
                return "point";
            default:
                return "unknown";
        }
    }

    static const char* to_string(const GLenum framebuffer_status)
    {
        switch (framebuffer_status)
        {
            case GL_FRAMEBUFFER_COMPLETE:
                return "GL_FRAMEBUFFER_COMPLETE";
            case GL_FRAMEBUFFER_UNDEFINED:
                return "GL_FRAMEBUFFER_UNDEFINED";
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            case GL_FRAMEBUFFER_UNSUPPORTED:
                return "GL_FRAMEBUFFER_UNSUPPORTED";
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
            default:
                return "GL_FRAMEBUFFER_STATUS_UNKNOWN";
        }
    }

    static bool is_shadow_framebuffer_complete(
        const uint32 framebuffer,
        const ShadowFramebufferPass pass,
        const int layer_index)
    {
        const auto status = glCheckNamedFramebufferStatus(framebuffer, GL_FRAMEBUFFER);
        if (status == GL_FRAMEBUFFER_COMPLETE)
            return true;

        // Keep logs actionable without flooding every frame.
        static auto reported_failures = std::unordered_set<std::uint64_t> {};
        const auto key = (static_cast<std::uint64_t>(pass) << 32U)
                         | static_cast<std::uint64_t>(status);
        if (reported_failures.emplace(key).second)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: shadow framebuffer incomplete for {} shadows "
                "(example layer {}). Status: {} ({:#x}).",
                to_string(pass),
                layer_index,
                to_string(status),
                static_cast<uint32>(status));
        }

        return false;
    }

    static float get_distance_squared(const tbx::Vec3& a, const tbx::Vec3& b)
    {
        const auto delta = a - b;
        return (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z);
    }

    static bool intersects_point_shadow_influence(
        const tbx::Vec3& light_position,
        const float light_range,
        const tbx::Vec3& bounds_center,
        const float bounds_radius)
    {
        const auto max_distance = light_range + bounds_radius;
        return get_distance_squared(light_position, bounds_center) <= (max_distance * max_distance);
    }

    static bool should_render_shadow_caster(
        const tbx::Vec3& bounds_center,
        const float bounds_radius,
        const tbx::Frustum* shadow_frustum,
        const tbx::Vec3* point_light_position,
        const float point_light_range)
    {
        const auto shadow_bounds = tbx::Sphere {.center = bounds_center, .radius = bounds_radius};
        if (shadow_frustum != nullptr && !shadow_frustum->intersects(shadow_bounds))
            return false;
        if (point_light_position != nullptr
            && !intersects_point_shadow_influence(
                *point_light_position,
                point_light_range,
                bounds_center,
                bounds_radius))
            return false;

        return true;
    }

    static void render_shadow_batches(
        const OpenGlFrameContext& frame_context,
        const OpenGlResourceManager& resource_manager,
        const std::shared_ptr<OpenGlShaderProgram>& shader_program,
        const tbx::Mat4& light_view_projection,
        const tbx::Frustum* shadow_frustum,
        const tbx::Vec3* point_light_position,
        const float point_light_range,
        std::unordered_map<tbx::Uuid, std::shared_ptr<OpenGlMesh>>& mesh_cache)
    {
        if (!shader_program)
            return;

        shader_program->bind();
        shader_program->try_upload(
            tbx::MaterialParameter(LightViewProjectionUniformName, light_view_projection));

        auto currently_bound_mesh = tbx::Uuid {};
        auto is_cull_face_enabled = true;

        for (const auto& shadow_draw_call : frame_context.shadow_draw_calls)
        {
            const auto should_enable_cull_face = !shadow_draw_call.is_two_sided;
            if (should_enable_cull_face != is_cull_face_enabled)
            {
                if (should_enable_cull_face)
                    glEnable(GL_CULL_FACE);
                else
                    glDisable(GL_CULL_FACE);
                is_cull_face_enabled = should_enable_cull_face;
            }

            for (std::size_t draw_index = 0U; draw_index < shadow_draw_call.meshes.size(); ++draw_index)
            {
                if (!should_render_shadow_caster(
                        shadow_draw_call.bounds_centers[draw_index],
                        shadow_draw_call.bounds_radii[draw_index],
                        shadow_frustum,
                        point_light_position,
                        point_light_range))
                    continue;

                shader_program->try_upload(
                    tbx::MaterialParameter(
                        ModelUniformName,
                        shadow_draw_call.transforms[draw_index]));

                const auto mesh_key = shadow_draw_call.meshes[draw_index];
                auto mesh = std::shared_ptr<OpenGlMesh> {};
                if (const auto cached_mesh = mesh_cache.find(mesh_key); cached_mesh != mesh_cache.end())
                {
                    mesh = cached_mesh->second;
                }
                else if (resource_manager.try_get<OpenGlMesh>(mesh_key, mesh))
                {
                    mesh_cache.emplace(mesh_key, mesh);
                }
                else
                {
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: mesh resource '{}' is unavailable for shadow pass.",
                        mesh_key.value);
                    continue;
                }

                if (currently_bound_mesh != mesh_key)
                {
                    mesh->bind();
                    currently_bound_mesh = mesh_key;
                }

                mesh->draw_bound();
            }
        }

        glEnable(GL_CULL_FACE);
    }

    ShadowPassOperation::ShadowPassOperation(OpenGlResourceManager& resource_manager)
        : _resource_manager(resource_manager)
    {
    }

    ShadowPassOperation::~ShadowPassOperation() noexcept
    {
        delete_texture(_area_shadow_texture);
        delete_texture(_spot_shadow_texture);
        delete_texture(_point_shadow_texture);
        delete_texture(_directional_shadow_texture);
        if (_framebuffer != 0U)
        {
            auto framebuffer = take_gl_handle(_framebuffer);
            glDeleteFramebuffers(1, &framebuffer);
        }
    }

    void ShadowPassOperation::execute(const std::any& payload)
    {
        const auto& frame_context = std::any_cast<const OpenGlFrameContext&>(payload);
        if (!ensure_initialized())
            return;

        const auto directional_layer_count =
            static_cast<uint32>(frame_context.shadows.directional_cascades.size());
        const auto spot_layer_count =
            static_cast<uint32>(frame_context.shadows.spot_maps.size());
        const auto area_layer_count =
            static_cast<uint32>(frame_context.shadows.area_maps.size());
        auto point_shadow_count = uint32 {0U};
        for (const auto& point_light : frame_context.point_lights)
            if (point_light.shadow_index >= 0)
                point_shadow_count =
                    tbx::max(point_shadow_count, static_cast<uint32>(point_light.shadow_index + 1));

        if (directional_layer_count == 0U && spot_layer_count == 0U && area_layer_count == 0U
            && point_shadow_count == 0U)
            return;

        configure_shadow_depth_array(
            _directional_shadow_texture,
            _directional_shadow_resolution,
            _directional_shadow_layer_capacity,
            frame_context.shadows.directional_map_resolution,
            directional_layer_count);
        configure_shadow_cube_array(
            _point_shadow_texture,
            _point_shadow_resolution,
            _point_shadow_light_capacity,
            frame_context.shadows.point_map_resolution,
            point_shadow_count);
        configure_shadow_depth_array(
            _spot_shadow_texture,
            _spot_shadow_resolution,
            _spot_shadow_layer_capacity,
            frame_context.shadows.local_map_resolution,
            spot_layer_count);
        configure_shadow_depth_array(
            _area_shadow_texture,
            _area_shadow_resolution,
            _area_shadow_layer_capacity,
            frame_context.shadows.local_map_resolution,
            area_layer_count);

        auto previous_viewport = std::array<GLint, 4U> {};
        glGetIntegerv(GL_VIEWPORT, previous_viewport.data());

        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0F, 1.0F);

        auto mesh_cache = std::unordered_map<tbx::Uuid, std::shared_ptr<OpenGlMesh>> {};
        mesh_cache.reserve(frame_context.shadow_draw_calls.size() * 4U);

        if (_directional_shadow_texture != 0U)
        {
            _shader_program->bind();
            _shader_program->try_upload(tbx::MaterialParameter(WriteLinearDepthUniformName, 0));
            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame_context.shadows.directional_map_resolution),
                static_cast<GLsizei>(frame_context.shadows.directional_map_resolution));
            for (const auto& shadow_cascade : frame_context.shadows.directional_cascades)
            {
                glNamedFramebufferTextureLayer(
                    _framebuffer,
                    GL_DEPTH_ATTACHMENT,
                    _directional_shadow_texture,
                    0,
                    static_cast<GLint>(shadow_cascade.texture_layer));
                if (!is_shadow_framebuffer_complete(
                        _framebuffer,
                        ShadowFramebufferPass::Directional,
                        static_cast<int>(shadow_cascade.texture_layer)))
                    continue;

                glClear(GL_DEPTH_BUFFER_BIT);
                render_shadow_batches(
                    frame_context,
                    _resource_manager,
                    _shader_program,
                    shadow_cascade.light_view_projection,
                    nullptr,
                    nullptr,
                    0.0F,
                    mesh_cache);
            }
        }

        if (_spot_shadow_texture != 0U)
        {
            _shader_program->bind();
            _shader_program->try_upload(tbx::MaterialParameter(WriteLinearDepthUniformName, 0));
            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame_context.shadows.local_map_resolution),
                static_cast<GLsizei>(frame_context.shadows.local_map_resolution));
            for (const auto& spot_map : frame_context.shadows.spot_maps)
            {
                glNamedFramebufferTextureLayer(
                    _framebuffer,
                    GL_DEPTH_ATTACHMENT,
                    _spot_shadow_texture,
                    0,
                    static_cast<GLint>(spot_map.texture_layer));
                if (!is_shadow_framebuffer_complete(
                        _framebuffer,
                        ShadowFramebufferPass::Spot,
                        static_cast<int>(spot_map.texture_layer)))
                    continue;

                glClear(GL_DEPTH_BUFFER_BIT);
                const auto shadow_frustum = tbx::Frustum(spot_map.light_view_projection);
                render_shadow_batches(
                    frame_context,
                    _resource_manager,
                    _shader_program,
                    spot_map.light_view_projection,
                    &shadow_frustum,
                    nullptr,
                    0.0F,
                    mesh_cache);
            }
        }

        if (_area_shadow_texture != 0U)
        {
            _shader_program->bind();
            _shader_program->try_upload(tbx::MaterialParameter(WriteLinearDepthUniformName, 0));
            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame_context.shadows.local_map_resolution),
                static_cast<GLsizei>(frame_context.shadows.local_map_resolution));
            for (const auto& area_map : frame_context.shadows.area_maps)
            {
                glNamedFramebufferTextureLayer(
                    _framebuffer,
                    GL_DEPTH_ATTACHMENT,
                    _area_shadow_texture,
                    0,
                    static_cast<GLint>(area_map.texture_layer));
                if (!is_shadow_framebuffer_complete(
                        _framebuffer,
                        ShadowFramebufferPass::Area,
                        static_cast<int>(area_map.texture_layer)))
                    continue;

                glClear(GL_DEPTH_BUFFER_BIT);
                const auto shadow_frustum = tbx::Frustum(area_map.light_view_projection);
                render_shadow_batches(
                    frame_context,
                    _resource_manager,
                    _shader_program,
                    area_map.light_view_projection,
                    &shadow_frustum,
                    nullptr,
                    0.0F,
                    mesh_cache);
            }
        }

        if (_point_shadow_texture != 0U)
        {
            const auto point_face_directions = std::array {
                tbx::Vec3(1.0F, 0.0F, 0.0F),
                tbx::Vec3(-1.0F, 0.0F, 0.0F),
                tbx::Vec3(0.0F, 1.0F, 0.0F),
                tbx::Vec3(0.0F, -1.0F, 0.0F),
                tbx::Vec3(0.0F, 0.0F, 1.0F),
                tbx::Vec3(0.0F, 0.0F, -1.0F),
            };
            const auto point_face_up_vectors = std::array {
                tbx::Vec3(0.0F, -1.0F, 0.0F),
                tbx::Vec3(0.0F, -1.0F, 0.0F),
                tbx::Vec3(0.0F, 0.0F, 1.0F),
                tbx::Vec3(0.0F, 0.0F, -1.0F),
                tbx::Vec3(0.0F, -1.0F, 0.0F),
                tbx::Vec3(0.0F, -1.0F, 0.0F),
            };

            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame_context.shadows.point_map_resolution),
                static_cast<GLsizei>(frame_context.shadows.point_map_resolution));
            for (const auto& point_light : frame_context.point_lights)
            {
                if (point_light.shadow_index < 0)
                    continue;

                const auto point_projection = tbx::perspective_projection(
                    tbx::to_radians(90.0F),
                    1.0F,
                    0.05F,
                    point_light.range);
                for (uint32 face_index = 0U; face_index < ShadowPointFaceCount; ++face_index)
                {
                    const auto light_view = tbx::look_at(
                        point_light.position,
                        point_light.position + point_face_directions[face_index],
                        point_face_up_vectors[face_index]);
                    const auto light_view_projection = point_projection * light_view;
                    const auto texture_layer =
                        static_cast<uint32>(point_light.shadow_index) * ShadowPointFaceCount
                        + face_index;
                    glNamedFramebufferTextureLayer(
                        _framebuffer,
                        GL_DEPTH_ATTACHMENT,
                        _point_shadow_texture,
                        0,
                        static_cast<GLint>(texture_layer));
                    if (!is_shadow_framebuffer_complete(
                            _framebuffer,
                            ShadowFramebufferPass::Point,
                            static_cast<int>(texture_layer)))
                        continue;

                    glClear(GL_DEPTH_BUFFER_BIT);
                    _shader_program->bind();
                    _shader_program->try_upload(
                        tbx::MaterialParameter(WriteLinearDepthUniformName, 1));
                    _shader_program->try_upload(
                        tbx::MaterialParameter(PointLightPositionUniformName, point_light.position));
                    _shader_program->try_upload(
                        tbx::MaterialParameter(PointLightFarPlaneUniformName, point_light.range));
                    const auto shadow_frustum = tbx::Frustum(light_view_projection);
                    render_shadow_batches(
                        frame_context,
                        _resource_manager,
                        _shader_program,
                        light_view_projection,
                        &shadow_frustum,
                        &point_light.position,
                        point_light.range,
                        mesh_cache);
                }
            }
        }

        _shader_program->bind();
        _shader_program->try_upload(tbx::MaterialParameter(WriteLinearDepthUniformName, 0));
        glDisable(GL_POLYGON_OFFSET_FILL);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(
            previous_viewport[0],
            previous_viewport[1],
            previous_viewport[2],
            previous_viewport[3]);
    }

    uint32 ShadowPassOperation::get_directional_shadow_texture() const
    {
        return _directional_shadow_texture;
    }

    uint32 ShadowPassOperation::get_point_shadow_texture() const
    {
        return _point_shadow_texture;
    }

    uint32 ShadowPassOperation::get_spot_shadow_texture() const
    {
        return _spot_shadow_texture;
    }

    uint32 ShadowPassOperation::get_area_shadow_texture() const
    {
        return _area_shadow_texture;
    }

    bool ShadowPassOperation::ensure_initialized()
    {
        if (_framebuffer == 0U)
        {
            glCreateFramebuffers(1, &_framebuffer);
            glNamedFramebufferDrawBuffer(_framebuffer, GL_NONE);
            glNamedFramebufferReadBuffer(_framebuffer, GL_NONE);
        }

        if (_shader_program)
            return true;

        _shader_program = build_shadow_shader_program();
        if (_shader_program)
        {
            _has_reported_initialization_failure = false;
            return true;
        }

        if (!_has_reported_initialization_failure)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: failed to initialize the realtime shadow shader program.");
            _has_reported_initialization_failure = true;
        }

        return false;
    }
}
