#include "opengl_renderer.h"
#include "builtin_assets.generated.h"
#include "opengl_fallbacks.h"
#include "opengl_resources/opengl_bindless.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_shader.h"
#include "opengl_resources/opengl_texture.h"
#include "pipeline/OpenGlFrameContext.h"
#include "pipeline/opengl_render_pipeline.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/frustum.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/matrices.h"
#include "tbx/math/trig.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glad/glad.h>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace opengl_rendering
{
    static constexpr tbx::uint32 ShadowCascadeCount = 3U;
    static constexpr tbx::uint32 DirectionalShadowMapResolution = 2048U;
    static constexpr tbx::uint32 LocalShadowMapResolution = 1024U;
    static constexpr tbx::uint32 PointShadowMapResolution = 512U;
    static constexpr float ShadowRenderDistance = 90.0F;
    static constexpr float ShadowFilterSoftness = 0.6F;
    static constexpr float ShadowNearPlane = 0.1F;
    static constexpr float ShadowSplitLambda = 0.6F;
    static constexpr float ShadowStabilizationPadding = 6.0F;
    static constexpr float LocalShadowDepthBias = 0.00008F;
    static constexpr float LocalShadowNormalBias = 0.00035F;
    static constexpr float AreaShadowDepthPadding = 2.0F;
    static const auto DefaultClearColor = tbx::Color(0.07F, 0.08F, 0.11F, 1.0F);

    static void gl_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        const GLsizei length,
        const GLchar* message,
        const void* _)
    {
        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:
            {
                TBX_TRACE_WARNING(
                    "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}:",
                    source,
                    type,
                    id,
                    severity,
                    message,
                    std::string_view(message, length));
                break;
            }
            case GL_DEBUG_SEVERITY_MEDIUM:
            {
                TBX_TRACE_WARNING(
                    "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}:",
                    source,
                    type,
                    id,
                    severity,
                    message,
                    std::string_view(message, length));
                break;
            }
            case GL_DEBUG_SEVERITY_LOW:
            {
                TBX_TRACE_WARNING(
                    "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}:",
                    source,
                    type,
                    id,
                    severity,
                    message,
                    std::string_view(message, length));
                break;
            }
            default:
                break;
        }
    }

    static const tbx::Material* try_get_material_defaults(
        const tbx::Handle& material_handle,
        tbx::AssetManager& asset_manager,
        std::unordered_map<tbx::Uuid, tbx::Material>& material_defaults_cache)
    {
        if (!material_handle.is_valid())
            return nullptr;

        if (const auto cached_material = material_defaults_cache.find(material_handle.id);
            cached_material != material_defaults_cache.end())
            return &cached_material->second;

        const auto material_asset = asset_manager.load<tbx::Material>(material_handle);
        if (!material_asset)
            return nullptr;

        const auto cache_it =
            material_defaults_cache.emplace(material_handle.id, *material_asset).first;
        return &cache_it->second;
    }

    static tbx::Vec3 to_vec3(const tbx::Color& color)
    {
        return tbx::Vec3(color.r, color.g, color.b);
    }

    static tbx::Vec3 get_normalized_light_color(const tbx::Color& color)
    {
        const auto rgb = to_vec3(color);
        const auto length = tbx::sqrt((rgb.x * rgb.x) + (rgb.y * rgb.y) + (rgb.z * rgb.z));
        if (length <= 0.0001F)
            return tbx::Vec3(0.0F, 0.0F, 0.0F);
        return rgb / length;
    }

    static tbx::Vec3 get_light_radiance(const tbx::Light& light)
    {
        return get_normalized_light_color(light.color) * tbx::max(light.intensity, 0.0F);
    }

    static bool has_light_radiance(const tbx::Light& light)
    {
        if (tbx::max(light.intensity, 0.0F) <= 0.0001F)
            return false;

        return light.color.r > 0.0001F || light.color.g > 0.0001F || light.color.b > 0.0001F;
    }

    static tbx::Vec3 get_light_direction(const tbx::Transform& transform)
    {
        return tbx::normalize(transform.rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));
    }

    static tbx::Vec3 get_light_right(const tbx::Transform& transform)
    {
        return tbx::normalize(transform.rotation * tbx::Vec3(1.0F, 0.0F, 0.0F));
    }

    static tbx::Vec3 get_light_up(const tbx::Transform& transform)
    {
        return tbx::normalize(transform.rotation * tbx::Vec3(0.0F, 1.0F, 0.0F));
    }

    static float get_max_component(const tbx::Vec3& value)
    {
        return tbx::max(value.x, tbx::max(value.y, value.z));
    }

    static float get_distance_squared(const tbx::Vec3& a, const tbx::Vec3& b)
    {
        const auto delta = a - b;
        return (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z);
    }

    static bool intersects_light_influence(
        const tbx::Frustum& view_frustum,
        const tbx::Vec3& position,
        const float radius)
    {
        return view_frustum.intersects(
            tbx::Sphere {
                .center = position,
                .radius = tbx::max(radius, 0.001F),
            });
    }

    static float get_area_light_culling_radius(const tbx::AreaLight& light)
    {
        const auto emitter_radius = tbx::sqrt(
            (light.area_size.x * light.area_size.x * 0.25F)
            + (light.area_size.y * light.area_size.y * 0.25F));
        return tbx::max(light.range, 0.001F) + emitter_radius;
    }

    static tbx::Handle resolve_renderer_mesh_handle(
        const tbx::Renderer& renderer,
        const float camera_distance)
    {
        if (renderer.lods.empty())
            return {};

        auto selected_handle = renderer.lods.back().handle;
        auto selected_max_distance = std::numeric_limits<float>::max();
        for (const auto& lod : renderer.lods)
        {
            if (!lod.handle.is_valid())
                continue;

            if (camera_distance > lod.max_distance)
                continue;

            if (lod.max_distance < selected_max_distance)
            {
                selected_max_distance = lod.max_distance;
                selected_handle = lod.handle;
            }
        }

        return selected_handle;
    }

    static tbx::Uuid get_default_texture_for_binding(
        std::string_view binding_name,
        OpenGlResourceManager& resource_manager)
    {
        if (binding_name == "u_normal_map")
            return get_flat_normal_texture(resource_manager);
        if (binding_name == "u_specular_map")
            return get_fallback_texture(resource_manager);
        if (binding_name == "u_shininess_map")
            return get_fallback_texture(resource_manager);
        if (binding_name == "u_emissive_map")
            return get_fallback_texture(resource_manager);
        return get_fallback_texture(resource_manager);
    }

    static float get_material_float_parameter_or_default(
        const tbx::MaterialParameterBindings& parameters,
        const std::string_view name,
        const float fallback)
    {
        const auto* parameter = parameters.get(name);
        if (!parameter)
            return fallback;

        if (const auto* value = std::get_if<float>(&parameter->data))
            return *value;
        if (const auto* value = std::get_if<double>(&parameter->data))
            return static_cast<float>(*value);
        if (const auto* value = std::get_if<int>(&parameter->data))
            return static_cast<float>(*value);

        return fallback;
    }

    template <typename TLight>
    static bool light_specifies_no_shadows(const TLight& light)
    {
        if constexpr (requires { light.casts_shadows; })
        {
            using CastsShadowsType = std::remove_cvref_t<decltype(light.casts_shadows)>;
            if constexpr (std::is_same_v<CastsShadowsType, bool>)
                return !light.casts_shadows;
            else
                return light.casts_shadows == 0;
        }

        if constexpr (requires { light.shadows_enabled; })
        {
            using ShadowsEnabledType = std::remove_cvref_t<decltype(light.shadows_enabled)>;
            if constexpr (std::is_same_v<ShadowsEnabledType, bool>)
                return !light.shadows_enabled;
            else
                return light.shadows_enabled == 0;
        }

        if constexpr (requires { light.enable_shadows; })
        {
            using EnableShadowsType = std::remove_cvref_t<decltype(light.enable_shadows)>;
            if constexpr (std::is_same_v<EnableShadowsType, bool>)
                return !light.enable_shadows;
            else
                return light.enable_shadows == 0;
        }

        if constexpr (requires { light.shadow_mode; })
            return light.shadow_mode == tbx::ShadowMode::None;

        return false;
    }

    static std::array<tbx::Vec3, 8> get_world_frustum_corners(
        const OpenGlFrameContext& frame_context,
        const float near_depth,
        const float far_depth)
    {
        auto corners = std::array<tbx::Vec3, 8> {};
        auto corner_index = std::size_t {0U};
        const auto inverse_view = tbx::inverse(frame_context.view_matrix);
        const auto clamped_near = tbx::max(near_depth, 0.001F);
        const auto clamped_far = tbx::max(far_depth, clamped_near + 0.001F);

        if (frame_context.is_camera_perspective)
        {
            const auto tan_half_vertical_fov =
                tbx::tan(tbx::to_radians(frame_context.camera_vertical_fov_degrees) * 0.5F);
            const auto near_half_height = clamped_near * tan_half_vertical_fov;
            const auto near_half_width = near_half_height * frame_context.camera_aspect;
            const auto far_half_height = clamped_far * tan_half_vertical_fov;
            const auto far_half_width = far_half_height * frame_context.camera_aspect;

            for (const auto depth_and_extent :
                 {std::tuple(clamped_near, near_half_width, near_half_height),
                  std::tuple(clamped_far, far_half_width, far_half_height)})
            {
                const auto depth = std::get<0>(depth_and_extent);
                const auto half_width = std::get<1>(depth_and_extent);
                const auto half_height = std::get<2>(depth_and_extent);
                for (const auto view_y : {-half_height, half_height})
                    for (const auto view_x : {-half_width, half_width})
                    {
                        const auto world_corner =
                            inverse_view * tbx::Vec4(view_x, view_y, -depth, 1.0F);
                        corners[corner_index++] = tbx::Vec3(world_corner);
                    }
            }

            return corners;
        }

        const auto half_height = frame_context.camera_vertical_fov_degrees * 0.5F;
        const auto half_width = half_height * frame_context.camera_aspect;
        for (const auto depth : {clamped_near, clamped_far})
            for (const auto view_y : {-half_height, half_height})
                for (const auto view_x : {-half_width, half_width})
                {
                    const auto world_corner =
                        inverse_view * tbx::Vec4(view_x, view_y, -depth, 1.0F);
                    corners[corner_index++] = tbx::Vec3(world_corner);
                }

        return corners;
    }

    static std::vector<float> build_shadow_splits(const float near_plane, const float max_distance)
    {
        auto splits = std::vector<float>(ShadowCascadeCount, max_distance);
        for (tbx::uint32 cascade_index = 0U; cascade_index < ShadowCascadeCount; ++cascade_index)
        {
            const auto ratio =
                static_cast<float>(cascade_index + 1U) / static_cast<float>(ShadowCascadeCount);
            const auto logarithmic =
                near_plane * static_cast<float>(std::pow(max_distance / near_plane, ratio));
            const auto uniform = near_plane + ((max_distance - near_plane) * ratio);
            splits[cascade_index] =
                (ShadowSplitLambda * logarithmic) + ((1.0F - ShadowSplitLambda) * uniform);
        }

        return splits;
    }

    static tbx::Mat4 build_local_light_view(const tbx::Vec3& position, const tbx::Vec3& direction)
    {
        auto light_up = tbx::Vec3(0.0F, 1.0F, 0.0F);
        if (std::abs(tbx::dot(light_up, direction)) > 0.95F)
            light_up = tbx::Vec3(1.0F, 0.0F, 0.0F);

        return tbx::look_at(position, position + direction, light_up);
    }

    static ProjectedShadowFrameData build_spot_shadow_map(
        const SpotLightFrameData& light,
        const tbx::uint32 texture_layer)
    {
        const auto direction = tbx::normalize(light.direction);
        const auto light_view = build_local_light_view(light.position, direction);
        const auto outer_angle_radians = std::acos(tbx::clamp(light.outer_cos, -1.0F, 1.0F));
        const auto vertical_fov =
            tbx::clamp(outer_angle_radians * 2.0F, tbx::to_radians(5.0F), tbx::to_radians(175.0F));
        const auto light_projection =
            tbx::perspective_projection(vertical_fov, 1.0F, ShadowNearPlane, light.range);
        return ProjectedShadowFrameData {
            .light_view_projection = light_projection * light_view,
            .near_plane = ShadowNearPlane,
            .far_plane = light.range,
            .normal_bias = LocalShadowNormalBias,
            .depth_bias = LocalShadowDepthBias,
            .texture_layer = texture_layer,
        };
    }

    static ProjectedShadowFrameData build_area_shadow_map(
        const AreaLightFrameData& light,
        const tbx::uint32 texture_layer)
    {
        const auto direction = tbx::normalize(light.direction);
        const auto emitter_radius = tbx::sqrt(
            (light.half_width * light.half_width) + (light.half_height * light.half_height));
        const auto shadow_depth =
            tbx::max(light.range + emitter_radius + AreaShadowDepthPadding, 0.5F);
        const auto light_position = light.position - (direction * (shadow_depth * 0.5F));
        const auto light_view = build_local_light_view(light_position, direction);
        const auto projection_extent = tbx::max(light.range + emitter_radius, 0.5F);
        const auto light_projection = tbx::ortho_projection(
            -projection_extent,
            projection_extent,
            -projection_extent,
            projection_extent,
            ShadowNearPlane,
            shadow_depth);
        return ProjectedShadowFrameData {
            .light_view_projection = light_projection * light_view,
            .near_plane = ShadowNearPlane,
            .far_plane = shadow_depth,
            .normal_bias = LocalShadowNormalBias,
            .depth_bias = LocalShadowDepthBias,
            .texture_layer = texture_layer,
        };
    }

    static void render_magenta_failure_frame(OpenGlGBuffer& gbuffer)
    {
        auto gbuffer_scope = OpenGlResourceScope(gbuffer);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glClearColor(1.0F, 0.0F, 1.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    static void hydrate_material_instance_defaults(
        tbx::MaterialInstance& material_instance,
        tbx::AssetManager& asset_manager,
        std::unordered_map<tbx::Uuid, tbx::Material>& material_defaults_cache)
    {
        if (!material_instance.has_loaded_defaults)
        {
            const auto* defaults = try_get_material_defaults(
                material_instance.handle,
                asset_manager,
                material_defaults_cache);
            if (defaults == nullptr)
                return;

            for (const auto& [name, value] : defaults->parameters.values)
            {
                if (material_instance.parameters.get(name) != nullptr)
                    continue;
                material_instance.parameters.set(name, value);
            }

            for (const auto& [name, texture] : defaults->textures.values)
            {
                const auto* existing_texture = material_instance.textures.get(name);
                if (existing_texture != nullptr && existing_texture->texture.handle.is_valid())
                    continue;
                material_instance.textures.set(name, texture);
            }

            material_instance.has_loaded_defaults = true;
        }
    }

    OpenGlRenderer::OpenGlRenderer(
        const tbx::GraphicsProcAddress loader,
        tbx::EntityRegistry& entity_registry,
        tbx::AssetManager& asset_manager,
        tbx::JobSystem& job_system,
        OpenGlContext context)
        : _context(std::move(context))
        , _entity_registry(entity_registry)
        , _asset_manager(asset_manager)
        , _job_system(job_system)
        , _resource_manager(asset_manager)
        , _render_pipeline(
              std::make_unique<OpenGlRenderPipeline>(_resource_manager, _job_system, _gbuffer))
    {
        initialize(loader);
    }

    OpenGlRenderer::~OpenGlRenderer() noexcept
    {
        shutdown();
    }

    bool OpenGlRenderer::render()
    {
        // Step one: make OpenGL context current.
        if (const auto make_current_result = _context.make_current(); !make_current_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: failed to make context current: {}",
                make_current_result.get_report());
            return false;
        }

        if (_pending_render_resolution.has_value())
        {
            set_render_resolution(_pending_render_resolution.value());
            _pending_render_resolution = std::nullopt;
        }

        auto target_render_size = _render_resolution;
        if (target_render_size.width == 0U || target_render_size.height == 0U)
            target_render_size = _viewport_size;

        // Step two: set viewport.
        glViewport(
            0,
            0,
            static_cast<GLsizei>(target_render_size.width),
            static_cast<GLsizei>(target_render_size.height));
        _gbuffer.resize(target_render_size);

        // Step three: build frame camera state.
        OpenGlFrameContext frame_context = build_frame_context();

        if (!frame_context.has_camera)
        {
            render_magenta_failure_frame(_gbuffer);
            _gbuffer.present(frame_context.render_stage, _viewport_size);

            if (const auto present_result = _context.present(); !present_result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: present request failed: {}",
                    present_result.get_report());
            }

            return true;
        }

        // Step four: cache resources and build draw calls in one pass.
        build_draw_calls(frame_context);

        // Step five: execute render pipeline.
        _render_pipeline->execute(frame_context);
        _gbuffer.present(frame_context.render_stage, _viewport_size);

        // Step six: present rendered frame.
        if (const auto present_result = _context.present(); !present_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: present request failed: {}",
                present_result.get_report());
        }

        return true;
    }

    OpenGlFrameContext OpenGlRenderer::build_frame_context() const
    {
        auto frame_context = OpenGlFrameContext();
        frame_context.clear_color = DefaultClearColor;
        frame_context.render_stage = _render_stage;
        frame_context.render_size = _render_resolution;
        if (frame_context.render_size.width == 0U || frame_context.render_size.height == 0U)
            frame_context.render_size = _viewport_size;

        if (const auto cameras = _entity_registry.get_with<tbx::Camera, tbx::Transform>();
            !cameras.empty())
        {
            _has_reported_missing_camera = false;
            frame_context.has_camera = true;
            const auto& camera_entity = cameras.front();
            auto& camera = camera_entity.get_component<tbx::Camera>();
            const auto camera_transform = tbx::get_world_space_transform(camera_entity);
            if (_viewport_size.width > 0 && _viewport_size.height > 0)
            {
                const auto aspect = static_cast<float>(_viewport_size.width)
                                    / static_cast<float>(_viewport_size.height);
                camera.set_aspect(aspect);
            }
            frame_context.camera_position = camera_transform.position;
            frame_context.camera_near_plane = camera.get_z_near();
            frame_context.camera_far_plane = camera.get_z_far();
            frame_context.is_camera_perspective = camera.is_perspective();
            frame_context.camera_vertical_fov_degrees = camera.get_fov();
            frame_context.camera_aspect = camera.get_aspect();
            frame_context.view_matrix =
                camera.get_view_matrix(camera_transform.position, camera_transform.rotation);
            frame_context.projection_matrix = camera.get_projection_matrix();
            frame_context.view_projection =
                frame_context.projection_matrix * frame_context.view_matrix;
        }
        else
        {
            if (!_has_reported_missing_camera)
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: no camera with both Camera and Transform components was "
                    "found. Rendering will use magenta fallback until one is available.");
                _has_reported_missing_camera = true;
            }
        }

        frame_context.inverse_view_projection = tbx::inverse(frame_context.view_projection);
        build_light_data(frame_context);
        build_shadow_data(frame_context);

        return frame_context;
    }

    void OpenGlRenderer::build_light_data(OpenGlFrameContext& frame_context) const
    {
        const auto view_frustum = tbx::Frustum(frame_context.view_projection);
        const auto directional_light_entities = _entity_registry.get_with<tbx::DirectionalLight>();
        frame_context.directional_lights.reserve(directional_light_entities.size());
        for (const auto& entity : directional_light_entities)
        {
            const auto world_transform = tbx::get_world_space_transform(entity);
            const auto& light = entity.get_component<tbx::DirectionalLight>();
            if (!has_light_radiance(light) && tbx::max(light.ambient, 0.0F) <= 0.0001F)
                continue;

            auto frame_light = DirectionalLightFrameData();
            frame_light.direction = get_light_direction(world_transform);
            frame_light.ambient_intensity = tbx::max(light.ambient, 0.0F);
            frame_light.radiance = get_light_radiance(light);
            frame_light.casts_shadows = light_specifies_no_shadows(light) ? 0.0F : 1.0F;
            frame_context.directional_lights.push_back(frame_light);
        }

        const auto point_light_entities = _entity_registry.get_with<tbx::PointLight>();
        frame_context.point_lights.reserve(point_light_entities.size());
        for (const auto& entity : point_light_entities)
        {
            const auto world_transform = tbx::get_world_space_transform(entity);
            const auto& light = entity.get_component<tbx::PointLight>();
            if (!has_light_radiance(light))
                continue;
            if (!intersects_light_influence(view_frustum, world_transform.position, light.range))
                continue;

            auto frame_light = PointLightFrameData();
            frame_light.position = world_transform.position;
            frame_light.range = tbx::max(light.range, 0.001F);
            frame_light.radiance = get_light_radiance(light);
            frame_light.shadow_index = light_specifies_no_shadows(light) ? -1 : 0;
            frame_context.point_lights.push_back(frame_light);
        }

        const auto spot_light_entities = _entity_registry.get_with<tbx::SpotLight>();
        frame_context.spot_lights.reserve(spot_light_entities.size());
        for (const auto& entity : spot_light_entities)
        {
            const auto world_transform = tbx::get_world_space_transform(entity);
            const auto& light = entity.get_component<tbx::SpotLight>();
            if (!has_light_radiance(light))
                continue;
            if (!intersects_light_influence(view_frustum, world_transform.position, light.range))
                continue;

            const auto inner_angle = tbx::clamp(light.inner_angle, 0.0F, light.outer_angle);
            const auto outer_angle = tbx::max(light.outer_angle, inner_angle + 0.001F);

            auto frame_light = SpotLightFrameData();
            frame_light.position = world_transform.position;
            frame_light.range = tbx::max(light.range, 0.001F);
            frame_light.direction = get_light_direction(world_transform);
            frame_light.inner_cos = tbx::cos(tbx::to_radians(inner_angle));
            frame_light.outer_cos = tbx::cos(tbx::to_radians(outer_angle));
            frame_light.radiance = get_light_radiance(light);
            frame_light.shadow_index = light_specifies_no_shadows(light) ? -1 : 0;
            frame_context.spot_lights.push_back(frame_light);
        }

        const auto area_light_entities = _entity_registry.get_with<tbx::AreaLight>();
        frame_context.area_lights.reserve(area_light_entities.size());
        for (const auto& entity : area_light_entities)
        {
            const auto world_transform = tbx::get_world_space_transform(entity);
            const auto& light = entity.get_component<tbx::AreaLight>();
            if (!has_light_radiance(light))
                continue;
            if (!intersects_light_influence(
                    view_frustum,
                    world_transform.position,
                    get_area_light_culling_radius(light)))
                continue;

            auto frame_light = AreaLightFrameData();
            frame_light.position = world_transform.position;
            frame_light.range = tbx::max(light.range, 0.001F);
            frame_light.direction = get_light_direction(world_transform);
            frame_light.half_width = tbx::max(light.area_size.x * 0.5F, 0.001F);
            frame_light.half_height = tbx::max(light.area_size.y * 0.5F, 0.001F);
            frame_light.radiance = get_light_radiance(light);
            frame_light.right = get_light_right(world_transform);
            frame_light.up = get_light_up(world_transform);
            frame_light.shadow_index = light_specifies_no_shadows(light) ? -1 : 0;
            frame_context.area_lights.push_back(frame_light);
        }
    }

    void OpenGlRenderer::build_shadow_data(OpenGlFrameContext& frame_context) const
    {
        frame_context.shadows.directional_map_resolution = DirectionalShadowMapResolution;
        frame_context.shadows.local_map_resolution = LocalShadowMapResolution;
        frame_context.shadows.point_map_resolution = PointShadowMapResolution;
        frame_context.shadows.max_distance = ShadowRenderDistance;
        frame_context.shadows.softness = ShadowFilterSoftness;
        frame_context.shadows.directional_cascades.clear();
        frame_context.shadows.spot_maps.clear();
        frame_context.shadows.area_maps.clear();

        for (auto& directional_light : frame_context.directional_lights)
        {
            directional_light.shadow_cascade_offset = 0U;
            directional_light.shadow_cascade_count = 0U;
        }

        auto point_shadow_index = 0;
        for (auto& point_light : frame_context.point_lights)
        {
            if (point_light.shadow_index < 0)
                continue;

            point_light.shadow_index = point_shadow_index++;
        }

        auto spot_shadow_layer = tbx::uint32 {0U};
        for (auto& spot_light : frame_context.spot_lights)
        {
            if (spot_light.shadow_index < 0)
                continue;

            spot_light.shadow_index = static_cast<int>(frame_context.shadows.spot_maps.size());
            frame_context.shadows.spot_maps.push_back(
                build_spot_shadow_map(spot_light, spot_shadow_layer++));
        }

        auto area_shadow_layer = tbx::uint32 {0U};
        for (auto& area_light : frame_context.area_lights)
        {
            if (area_light.shadow_index < 0)
                continue;

            area_light.shadow_index = static_cast<int>(frame_context.shadows.area_maps.size());
            frame_context.shadows.area_maps.push_back(
                build_area_shadow_map(area_light, area_shadow_layer++));
        }

        if (frame_context.directional_lights.empty())
            return;
        if (frame_context.render_size.width == 0U || frame_context.render_size.height == 0U)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: skipped shadow setup because the render size is invalid.");
            return;
        }
        if (frame_context.camera_far_plane <= frame_context.camera_near_plane)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: skipped shadow setup because the active camera clip planes are "
                "invalid.");
            return;
        }

        const auto max_shadow_distance = tbx::clamp(
            ShadowRenderDistance,
            frame_context.camera_near_plane + 0.001F,
            frame_context.camera_far_plane);
        if (max_shadow_distance <= frame_context.camera_near_plane)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: skipped shadow setup because the configured shadow distance is "
                "invalid for the active camera.");
            return;
        }
        const auto split_depths =
            build_shadow_splits(frame_context.camera_near_plane, max_shadow_distance);
        frame_context.shadows.directional_cascades.reserve(
            frame_context.directional_lights.size() * ShadowCascadeCount);

        auto directional_shadow_layer = tbx::uint32 {0U};
        for (auto& directional_light : frame_context.directional_lights)
        {
            if (directional_light.casts_shadows <= 0.5F)
                continue;

            const auto light_direction = tbx::normalize(directional_light.direction);
            if (light_direction.x == 0.0F && light_direction.y == 0.0F && light_direction.z == 0.0F)
            {
                directional_light.casts_shadows = 0.0F;
                continue;
            }

            directional_light.shadow_cascade_offset =
                static_cast<tbx::uint32>(frame_context.shadows.directional_cascades.size());
            auto previous_split = frame_context.camera_near_plane;
            for (tbx::uint32 cascade_index = 0U; cascade_index < ShadowCascadeCount;
                 ++cascade_index)
            {
                const auto cascade_far = tbx::clamp(
                    split_depths[cascade_index],
                    previous_split + 0.001F,
                    max_shadow_distance);
                const auto corners =
                    get_world_frustum_corners(frame_context, previous_split, cascade_far);

                auto frustum_center = tbx::Vec3(0.0F);
                for (const auto& corner : corners)
                    frustum_center += corner;
                frustum_center /= static_cast<float>(corners.size());

                auto light_up = tbx::Vec3(0.0F, 1.0F, 0.0F);
                if (std::abs(tbx::dot(light_up, light_direction)) > 0.95F)
                    light_up = tbx::Vec3(1.0F, 0.0F, 0.0F);

                auto radius = 0.0F;
                for (const auto& corner : corners)
                    radius = tbx::max(radius, glm::length(corner - frustum_center));
                radius = std::ceil(radius * 16.0F) / 16.0F;

                const auto light_position =
                    frustum_center - (light_direction * (radius + ShadowStabilizationPadding));
                auto light_view = tbx::look_at(light_position, frustum_center, light_up);
                const auto texel_size =
                    (radius * 2.0F)
                    / static_cast<float>(frame_context.shadows.directional_map_resolution);
                auto shadow_center = light_view * tbx::Vec4(frustum_center, 1.0F);
                shadow_center.x = std::floor(shadow_center.x / texel_size) * texel_size;
                shadow_center.y = std::floor(shadow_center.y / texel_size) * texel_size;
                const auto snapped_center_world =
                    tbx::inverse(light_view)
                    * tbx::Vec4(shadow_center.x, shadow_center.y, shadow_center.z, 1.0F);
                light_view = tbx::look_at(
                    tbx::Vec3(snapped_center_world)
                        - (light_direction * (radius + ShadowStabilizationPadding)),
                    tbx::Vec3(snapped_center_world),
                    light_up);

                const auto light_projection = tbx::ortho_projection(
                    -radius,
                    radius,
                    -radius,
                    radius,
                    0.1F,
                    (radius * 2.0F) + (ShadowStabilizationPadding * 2.0F));
                frame_context.shadows.directional_cascades.push_back(
                    ShadowCascadeFrameData {
                        .light_view_projection = light_projection * light_view,
                        .split_depth = cascade_far,
                        .normal_bias =
                            0.0003F * (1.0F + (static_cast<float>(cascade_index) * 0.5F)),
                        .depth_bias =
                            0.00003F * (1.0F + (static_cast<float>(cascade_index) * 0.5F)),
                        .blend_distance = 5.0F + static_cast<float>(cascade_index),
                        .texture_layer = directional_shadow_layer++,
                    });
                previous_split = cascade_far;
                ++directional_light.shadow_cascade_count;
            }

            if (directional_light.shadow_cascade_count == 0U)
                directional_light.casts_shadows = 0.0F;
        }
    }

    void OpenGlRenderer::build_draw_calls(OpenGlFrameContext& frame_context)
    {
        const auto entities = _entity_registry.get_with<tbx::Renderer>();
        frame_context.draw_calls.reserve(entities.size());
        frame_context.shadow_draw_calls.reserve(entities.size());
        frame_context.transparent_draw_calls.reserve(entities.size());

        const auto view_frustum = tbx::Frustum(frame_context.view_projection);
        auto draw_call_lookup = std::unordered_map<std::uint64_t, std::size_t>();
        auto shadow_draw_call_lookup = std::unordered_map<std::uint64_t, std::size_t>();
        draw_call_lookup.reserve(entities.size());
        shadow_draw_call_lookup.reserve(entities.size());

        for (const auto& entity : entities)
        {
            auto& renderer = entity.get_component<tbx::Renderer>();
            const auto world_transform = entity.has_component<tbx::Transform>()
                                             ? tbx::get_world_space_transform(entity)
                                             : tbx::Transform();
            const auto can_cast_shadows = renderer.shadow_mode != tbx::ShadowMode::None;

            const auto bounds_radius = tbx::max(get_max_component(world_transform.scale), 0.001F);
            const auto camera_distance_squared =
                get_distance_squared(world_transform.position, frame_context.camera_position);
            const auto camera_distance = tbx::sqrt(camera_distance_squared);

            if (renderer.render_distance > 0.0F && camera_distance > renderer.render_distance)
                continue;

            const auto is_visible_to_camera =
                !renderer.is_cullable
                || view_frustum.intersects(
                    tbx::Sphere {.center = world_transform.position, .radius = bounds_radius});
            if (!is_visible_to_camera && !can_cast_shadows)
                continue;

            auto& material_instance = renderer.material;
            if (!material_instance.handle.is_valid())
                material_instance.handle = tbx::lit_material;
            hydrate_material_instance_defaults(
                material_instance,
                _asset_manager,
                _material_defaults_cache);
            const auto transparency_amount = get_material_float_parameter_or_default(
                material_instance.parameters,
                "transparency_amount",
                0.0F);

            // Add shader program
            auto use_fallback_material_params = false;
            tbx::Uuid shader_program_key;
            {
                shader_program_key = _resource_manager.add_material(material_instance);

                if (!shader_program_key.is_valid())
                {
                    TBX_TRACE_WARNING(
                        "Failed to cache shader program for material '{}'. Using "
                        "fallback magenta material.",
                        material_instance.handle.id.value);
                    shader_program_key = get_fallback_material(_resource_manager);
                    use_fallback_material_params = true;
                }
                if (!shader_program_key.is_valid())
                {
                    TBX_ASSERT(false, "Fallback magenta material program is unavailable.");
                    continue;
                }
            }

            // Add mesh
            tbx::Uuid mesh_key;
            if (entity.has_component<tbx::DynamicMesh>())
            {
                const auto& dynamic_mesh = entity.get_component<tbx::DynamicMesh>();
                mesh_key = _resource_manager.add_dynamic_mesh(dynamic_mesh);
            }
            else if (entity.has_component<tbx::StaticMesh>())
            {
                auto static_mesh = entity.get_component<tbx::StaticMesh>();
                const auto lod_mesh_handle =
                    resolve_renderer_mesh_handle(renderer, camera_distance);
                if (lod_mesh_handle.is_valid())
                    static_mesh.handle = lod_mesh_handle;
                mesh_key = _resource_manager.add_static_mesh(static_mesh);
            }
            if (!mesh_key.is_valid())
            {
                TBX_TRACE_WARNING(
                    "Failed to add mesh for entity with ID {}.",
                    tbx::to_string(entity.get_id()));
                continue;
            }

            // Add material params
            auto material_params = OpenGlMaterialParams();
            if (use_fallback_material_params)
            {
                material_params = create_magenta_fallback_material_params(material_instance.handle);
            }
            else
            {
                material_params.material_handle = material_instance.handle;
                material_params.parameters.reserve(material_instance.parameters.values.size());
                for (const auto& [name, value] : material_instance.parameters.values)
                    material_params.parameters.push_back(
                        tbx::MaterialParameter {.name = name, .data = value});
            }

            // Add textures
            material_params.textures.reserve(material_instance.textures.values.size());
            for (const auto& [name, texture] : material_instance.textures.values)
            {
                tbx::Uuid texture_id;
                if (texture.handle.is_valid())
                    texture_id = _resource_manager.add_texture(texture.handle);
                else
                    texture_id = get_default_texture_for_binding(name, _resource_manager);

                if (!texture_id.is_valid())
                {
                    TBX_TRACE_WARNING(
                        "Failed to cache texture '{}' for material '{}'. "
                        "Using fallback texture.",
                        texture.handle.id.value,
                        material_instance.handle.id.value);
                    texture_id = get_default_texture_for_binding(name, _resource_manager);
                }

                if (!texture_id.is_valid())
                {
                    continue;
                }

                auto texture_resource = std::shared_ptr<OpenGlTexture>();
                if (!_resource_manager.try_get<OpenGlTexture>(texture_id, texture_resource))
                {
                    TBX_TRACE_WARNING(
                        "Failed to fetch texture resource '{}' for material "
                        "'{}'. Using fallback texture.",
                        texture_id.value,
                        material_instance.handle.id.value);
                    continue;
                }

                material_params.textures.push_back(
                    OpenGlMaterialTexture {
                        .name = name,
                        .texture_id = texture_id,
                        .gl_texture_id = texture_resource->get_texture_id(),
                        .bindless_handle = texture_resource->get_bindless_handle(),
                    });
            }

            // Use white fallback texture
            if (material_params.textures.empty())
            {
                const auto fallback_texture_id = get_fallback_texture(_resource_manager);
                if (auto fallback_texture_resource = std::shared_ptr<OpenGlTexture>();
                    fallback_texture_id.is_valid()
                    && _resource_manager.try_get<OpenGlTexture>(
                        fallback_texture_id,
                        fallback_texture_resource))
                    material_params.textures.push_back(
                        OpenGlMaterialTexture {
                            .name = "diffuse_map",
                            .texture_id = fallback_texture_id,
                            .gl_texture_id = fallback_texture_resource->get_texture_id(),
                            .bindless_handle = fallback_texture_resource->get_bindless_handle(),
                        });
            }

            // Add transform matrix
            const auto transform_matrix = tbx::build_transform_matrix(world_transform);

            if (can_cast_shadows)
            {
                const auto shadow_draw_call_key = renderer.is_two_sided ? 1ULL : 0ULL;
                auto shadow_draw_call_it = shadow_draw_call_lookup.find(shadow_draw_call_key);
                if (shadow_draw_call_it == shadow_draw_call_lookup.end())
                {
                    frame_context.shadow_draw_calls.push_back(
                        ShadowDrawCall {.is_two_sided = renderer.is_two_sided});
                    shadow_draw_call_it = shadow_draw_call_lookup
                                              .emplace(
                                                  shadow_draw_call_key,
                                                  frame_context.shadow_draw_calls.size() - 1U)
                                              .first;
                }

                auto& shadow_draw_call =
                    frame_context.shadow_draw_calls[shadow_draw_call_it->second];
                shadow_draw_call.meshes.push_back(mesh_key);
                shadow_draw_call.transforms.push_back(transform_matrix);
                shadow_draw_call.bounds_centers.push_back(world_transform.position);
                shadow_draw_call.bounds_radii.push_back(bounds_radius);
            }

            if (!is_visible_to_camera)
                continue;

            if (transparency_amount > 0.0001F)
            {
                frame_context.transparent_draw_calls.push_back(
                    TransparentDrawCall {
                        .shader_program = shader_program_key,
                        .is_two_sided = renderer.is_two_sided,
                        .mesh = mesh_key,
                        .material = std::move(material_params),
                        .transform = transform_matrix,
                        .camera_distance_squared = camera_distance_squared,
                    });
                continue;
            }

            // Push back new draw call
            const auto draw_call_key = (static_cast<std::uint64_t>(shader_program_key.value) << 1U)
                                       | (renderer.is_two_sided ? 1ULL : 0ULL);
            auto draw_call_it = draw_call_lookup.find(draw_call_key);
            if (draw_call_it == draw_call_lookup.end())
            {
                frame_context.draw_calls.emplace_back(shader_program_key, renderer.is_two_sided);
                frame_context.draw_calls.back().meshes.reserve(8U);
                frame_context.draw_calls.back().materials.reserve(8U);
                frame_context.draw_calls.back().transforms.reserve(8U);
                draw_call_it =
                    draw_call_lookup.emplace(draw_call_key, frame_context.draw_calls.size() - 1U)
                        .first;
            }

            // Update draw call with new mesh
            auto& draw_call = frame_context.draw_calls[draw_call_it->second];
            draw_call.meshes.push_back(mesh_key);
            draw_call.materials.push_back(std::move(material_params));
            draw_call.transforms.push_back(transform_matrix);
        }

        std::sort(
            frame_context.transparent_draw_calls.begin(),
            frame_context.transparent_draw_calls.end(),
            [](const TransparentDrawCall& left, const TransparentDrawCall& right)
            {
                return left.camera_distance_squared > right.camera_distance_squared;
            });
    }

    void OpenGlRenderer::set_viewport_size(const tbx::Size& viewport_size)
    {
        if (viewport_size.width == 0 || viewport_size.height == 0)
            return;

        _viewport_size = viewport_size;
    }

    void OpenGlRenderer::set_pending_render_resolution(
        const std::optional<tbx::Size>& pending_render_resolution)
    {
        _pending_render_resolution = pending_render_resolution;
    }

    const OpenGlContext& OpenGlRenderer::get_context() const
    {
        return _context;
    }

    void OpenGlRenderer::set_render_stage(const tbx::RenderStage render_stage)
    {
        _render_stage = render_stage;
    }

    void OpenGlRenderer::initialize(const tbx::GraphicsProcAddress loader) const
    {
        // Only init gl once
        static bool initialized = false;
        if (initialized)
            return;
        initialized = true;

        auto* glad_loader = loader;
        TBX_ASSERT(glad_loader != nullptr, "Context-ready event provided null loader.");
        TBX_ASSERT(
            _context.get_window_id().is_valid(),
            "Renderer requires a valid context window id.");

        if (const auto make_current_result = _context.make_current(); !make_current_result)
        {
            TBX_ASSERT(
                make_current_result,
                "Failed to make context current before GLAD initialization: {}",
                make_current_result.get_report());
        }

        const auto load_result = gladLoadGLLoader(glad_loader);
        TBX_ASSERT(load_result != 0, "Failed to initialize GLAD.");
        set_bindless_proc_loader(loader);

        TBX_TRACE_INFO("Initializing window {} context.", to_string(_context.get_window_id()));

        const auto major_version = GLVersion.major;
        const auto minor_version = GLVersion.minor;
        TBX_ASSERT(
            major_version > 4 || (major_version == 4 && minor_version >= 5),
            "Requires OpenGL 4.5 or newer.");

#if defined(TBX_DEBUG)
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE);
        glDebugMessageCallback(gl_message_callback, nullptr);
#endif

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glDisable(GL_BLEND);
        glClearColor(0.07f, 0.08f, 0.11f, 1.0f);
    }

    void OpenGlRenderer::shutdown()
    {
        _render_pipeline.reset();
        _pending_render_resolution = std::nullopt;
        _viewport_size = {};
        _render_resolution = {};
    }

    void OpenGlRenderer::set_render_resolution(const tbx::Size& render_resolution)
    {
        _render_resolution = render_resolution;
    }
}
