#include "opengl_renderer.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_gbuffer.h"
#include "opengl_resources/opengl_shadow_map.h"
#include "pipeline/opengl_post_processing.h"
#include "pipeline/opengl_render_pipeline.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/trig.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glad/glad.h>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace tbx::plugins
{
    static constexpr size_t MAX_SHADOWED_DIRECTIONAL_LIGHTS = 1U;
    static constexpr size_t DIRECTIONAL_SHADOW_CASCADE_COUNT = 2U;
    static constexpr size_t MAX_SHADOWED_SPOT_LIGHTS = 4U;
    static constexpr size_t MAX_SHADOWED_POINT_LIGHTS = 2U;
    static constexpr size_t POINT_SHADOW_FACE_COUNT = 6U;
    static constexpr float DIRECTIONAL_SHADOW_SPLIT_LAMBDA = 0.7F;
    static constexpr float SHADOW_NEAR_PLANE = 0.1F;

    static std::shared_ptr<OpenGlGBuffer> try_load_gbuffer(
        OpenGlResourceManager& resource_manager,
        const Uuid& resource_uuid)
    {
        auto base_resource = std::shared_ptr<IOpenGlResource> {};
        if (!resource_manager.try_get(resource_uuid, base_resource))
            return nullptr;

        return std::dynamic_pointer_cast<OpenGlGBuffer>(base_resource);
    }

    static std::shared_ptr<OpenGlFrameBuffer> try_load_framebuffer(
        OpenGlResourceManager& resource_manager,
        const Uuid& resource_uuid)
    {
        auto base_resource = std::shared_ptr<IOpenGlResource> {};
        if (!resource_manager.try_get(resource_uuid, base_resource))
            return nullptr;

        return std::dynamic_pointer_cast<OpenGlFrameBuffer>(base_resource);
    }

    static void ensure_shadow_map_resources(
        std::vector<Uuid>& shadow_map_resources,
        size_t desired_count,
        const OpenGlShadowSettings& shadow_settings,
        OpenGlResourceManager& resource_manager)
    {
        while (shadow_map_resources.size() > desired_count)
        {
            resource_manager.unpin(shadow_map_resources.back());
            shadow_map_resources.pop_back();
        }

        if (shadow_map_resources.size() < desired_count)
            shadow_map_resources.reserve(desired_count);

        while (shadow_map_resources.size() < desired_count)
        {
            auto shadow_map_resource = resource_manager.add<OpenGlShadowMap>(
                Size(shadow_settings.shadow_map_resolution, shadow_settings.shadow_map_resolution));
            if (!shadow_map_resource.is_valid())
            {
                TBX_ASSERT(false, "OpenGL rendering: failed to register shadow map resource.");
                continue;
            }

            resource_manager.pin(shadow_map_resource);
            shadow_map_resources.push_back(shadow_map_resource);
        }
    }

    static std::string normalize_uniform_name(std::string_view name)
    {
        if (name.size() >= 2U && name[0] == 'u' && name[1] == '_')
            return std::string(name);

        std::string normalized = "u_";
        normalized.append(name.begin(), name.end());
        return normalized;
    }

    static void append_or_override_material_parameter(
        MaterialParameterBindings& parameters,
        std::string_view name,
        MaterialParameterData data)
    {
        parameters.set(name, std::move(data));
    }

    static void append_or_override_texture(
        MaterialTextureBindings& textures,
        std::string_view name,
        const TextureInstance& runtime_texture)
    {
        textures.set(name, runtime_texture);
    }

    static Vec3 get_camera_world_position(const OpenGlCameraView& camera_view)
    {
        const auto camera_transform = get_world_space_transform(camera_view.camera_entity);
        return camera_transform.position;
    }

    static Quat get_camera_world_rotation(const OpenGlCameraView& camera_view)
    {
        const auto camera_transform = get_world_space_transform(camera_view.camera_entity);
        return camera_transform.rotation;
    }

    static Mat4 get_camera_view_projection(
        const OpenGlCameraView& camera_view,
        const Size& render_resolution)
    {
        if (!camera_view.camera_entity.has_component<Camera>())
            return Mat4(1.0f);

        auto& camera = camera_view.camera_entity.get_component<Camera>();
        camera.set_aspect(render_resolution.get_aspect_ratio());
        auto camera_position = get_camera_world_position(camera_view);
        auto camera_rotation = get_camera_world_rotation(camera_view);
        return camera.get_view_projection_matrix(camera_position, camera_rotation);
    }

    static Vec3 get_entity_forward_direction(const Entity& entity)
    {
        const auto transform = get_world_space_transform(entity);
        return normalize(transform.rotation * Vec3(0.0f, 0.0f, -1.0f));
    }

    static Vec3 get_entity_position(const Entity& entity)
    {
        const auto transform = get_world_space_transform(entity);
        return transform.position;
    }

    static void resolve_light_color(
        const RgbaColor& raw_color,
        float raw_intensity,
        Vec3& out_color,
        float& out_intensity)
    {
        out_color = Vec3(raw_color.r, raw_color.g, raw_color.b);
        out_intensity = std::max(raw_intensity, 0.0f);

        float max_channel = std::max(out_color.x, std::max(out_color.y, out_color.z));
        if (max_channel <= std::numeric_limits<float>::epsilon())
        {
            out_color = Vec3(1.0f);
            out_intensity = 0.0f;
            return;
        }

        out_color /= max_channel;
        out_intensity *= max_channel;
    }

    static float calculate_local_light_shadow_importance(
        float intensity,
        const Vec3& light_position,
        const Vec3& camera_world_position,
        float range)
    {
        const float safe_range = std::max(range, 0.001F);
        const float distance_to_camera = length(light_position - camera_world_position);
        const float normalized_distance = distance_to_camera / safe_range;
        return std::max(intensity, 0.0F) / (1.0F + (normalized_distance * normalized_distance));
    }

    static float calculate_directional_shadow_split_distance(
        float shadow_near_plane,
        float shadow_far_plane)
    {
        const float safe_near = std::max(0.001F, shadow_near_plane);
        const float safe_far = std::max(safe_near + 0.001F, shadow_far_plane);
        const float split_t = 0.5F;
        const float logarithmic_split = safe_near * std::pow(safe_far / safe_near, split_t);
        const float linear_split = safe_near + ((safe_far - safe_near) * split_t);
        const float blended_split = (DIRECTIONAL_SHADOW_SPLIT_LAMBDA * logarithmic_split)
                                    + ((1.0F - DIRECTIONAL_SHADOW_SPLIT_LAMBDA) * linear_split);
        return std::clamp(blended_split, safe_near + 0.001F, safe_far - 0.001F);
    }

    static Mat4 build_directional_shadow_view_projection(
        const OpenGlCameraView& camera_view,
        const Size& render_resolution,
        const Vec3& directional_light_direction,
        float shadow_near_plane,
        float shadow_far_plane,
        const OpenGlShadowSettings& shadow_settings)
    {
        const float safe_shadow_near_plane = std::max(0.001F, shadow_near_plane);
        const float safe_shadow_far_plane =
            std::max(safe_shadow_near_plane + 0.001F, shadow_far_plane);
        auto camera_position = get_camera_world_position(camera_view);
        auto camera_rotation = get_camera_world_rotation(camera_view);
        auto forward_axis = normalize(camera_rotation * Vec3(0.0F, 0.0F, -1.0F));
        auto right_axis = normalize(camera_rotation * Vec3(1.0F, 0.0F, 0.0F));
        auto up_axis = normalize(camera_rotation * Vec3(0.0F, 1.0F, 0.0F));

        float near_half_height = 0.5F;
        float near_half_width = 0.5F;
        float far_half_height = 0.5F;
        float far_half_width = 0.5F;
        if (camera_view.camera_entity.has_component<Camera>())
        {
            auto& camera = camera_view.camera_entity.get_component<Camera>();
            camera.set_aspect(render_resolution.get_aspect_ratio());

            if (camera.is_perspective())
            {
                const float tan_half_fov = std::tan(to_radians(camera.get_fov() * 0.5F));
                near_half_height = tan_half_fov * safe_shadow_near_plane;
                near_half_width = near_half_height * camera.get_aspect();
                far_half_height = tan_half_fov * safe_shadow_far_plane;
                far_half_width = far_half_height * camera.get_aspect();
            }
            else
            {
                const float ortho_half_height = std::max(0.001F, camera.get_fov() * 0.5F);
                const float ortho_half_width = ortho_half_height * camera.get_aspect();
                near_half_height = ortho_half_height;
                near_half_width = ortho_half_width;
                far_half_height = ortho_half_height;
                far_half_width = ortho_half_width;
            }
        }

        const Vec3 near_center = camera_position + (forward_axis * safe_shadow_near_plane);
        const Vec3 far_center = camera_position + (forward_axis * safe_shadow_far_plane);
        const auto near_up = up_axis * near_half_height;
        const auto near_right = right_axis * near_half_width;
        const auto far_up = up_axis * far_half_height;
        const auto far_right = right_axis * far_half_width;
        const auto frustum_corners = std::array<Vec3, 8> {
            near_center + near_up - near_right,
            near_center + near_up + near_right,
            near_center - near_up - near_right,
            near_center - near_up + near_right,
            far_center + far_up - far_right,
            far_center + far_up + far_right,
            far_center - far_up - far_right,
            far_center - far_up + far_right,
        };

        Vec3 frustum_center = Vec3(0.0F);
        for (const auto& corner : frustum_corners)
            frustum_center += corner;
        frustum_center /= static_cast<float>(frustum_corners.size());

        auto direction_to_light = normalize(directional_light_direction);
        auto light_position = frustum_center + (direction_to_light * safe_shadow_far_plane);

        auto light_up_axis = Vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(dot(direction_to_light, light_up_axis)) > 0.95f)
            light_up_axis = Vec3(1.0f, 0.0f, 0.0f);

        auto light_view = look_at(light_position, frustum_center, light_up_axis);
        float min_x = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float min_y = std::numeric_limits<float>::max();
        float max_y = std::numeric_limits<float>::lowest();
        float min_z = std::numeric_limits<float>::max();
        float max_z = std::numeric_limits<float>::lowest();

        for (const auto& corner : frustum_corners)
        {
            const auto light_space_corner = light_view * Vec4(corner, 1.0F);
            min_x = std::min(min_x, light_space_corner.x);
            max_x = std::max(max_x, light_space_corner.x);
            min_y = std::min(min_y, light_space_corner.y);
            max_y = std::max(max_y, light_space_corner.y);
            min_z = std::min(min_z, light_space_corner.z);
            max_z = std::max(max_z, light_space_corner.z);
        }

        const float width = std::max(max_x - min_x, 0.001F);
        const float height = std::max(max_y - min_y, 0.001F);
        const float map_resolution =
            std::max(1.0F, static_cast<float>(shadow_settings.shadow_map_resolution));
        const float texel_size_x = width / map_resolution;
        const float texel_size_y = height / map_resolution;
        float center_x = (min_x + max_x) * 0.5F;
        float center_y = (min_y + max_y) * 0.5F;
        center_x = std::floor(center_x / texel_size_x) * texel_size_x;
        center_y = std::floor(center_y / texel_size_y) * texel_size_y;
        min_x = center_x - (width * 0.5F);
        max_x = center_x + (width * 0.5F);
        min_y = center_y - (height * 0.5F);
        max_y = center_y + (height * 0.5F);

        const float z_padding = std::max(0.1F, safe_shadow_far_plane * 0.1F);
        const float near_plane = std::max(0.001F, -max_z - z_padding);
        const float far_plane = std::max(near_plane + 0.001F, -min_z + z_padding);
        auto light_projection = ortho_projection(min_x, max_x, min_y, max_y, near_plane, far_plane);
        return light_projection * light_view;
    }

    static Mat4 build_spot_shadow_view_projection(
        const Vec3& light_position,
        const Vec3& light_direction,
        float outer_angle_radians,
        float range)
    {
        auto safe_direction = normalize(light_direction);
        auto up_axis = Vec3(0.0F, 1.0F, 0.0F);
        if (std::abs(dot(safe_direction, up_axis)) > 0.95F)
            up_axis = Vec3(1.0F, 0.0F, 0.0F);

        const float near_plane = SHADOW_NEAR_PLANE;
        const float far_plane = std::max(near_plane + 0.001F, range);
        const float clamped_fov =
            std::clamp(outer_angle_radians * 2.0F, to_radians(1.0F), to_radians(175.0F));
        auto light_view = look_at(light_position, light_position + safe_direction, up_axis);
        auto light_projection = perspective_projection(clamped_fov, 1.0F, near_plane, far_plane);
        return light_projection * light_view;
    }

    static Mat4 build_point_shadow_face_view_projection(
        const Vec3& light_position,
        float range,
        size_t face_index)
    {
        static const auto face_directions = std::array<Vec3, POINT_SHADOW_FACE_COUNT> {
            Vec3(1.0F, 0.0F, 0.0F),
            Vec3(-1.0F, 0.0F, 0.0F),
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, 0.0F, 1.0F),
            Vec3(0.0F, 0.0F, -1.0F),
        };
        static const auto face_up_vectors = std::array<Vec3, POINT_SHADOW_FACE_COUNT> {
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, 0.0F, 1.0F),
            Vec3(0.0F, 0.0F, -1.0F),
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F),
        };

        const size_t safe_face_index = std::min(face_index, POINT_SHADOW_FACE_COUNT - 1U);
        const float near_plane = SHADOW_NEAR_PLANE;
        const float far_plane = std::max(near_plane + 0.001F, range);
        auto light_view = look_at(
            light_position,
            light_position + face_directions[safe_face_index],
            face_up_vectors[safe_face_index]);
        auto light_projection =
            perspective_projection(to_radians(90.0F), 1.0F, near_plane, far_plane);
        return light_projection * light_view;
    }

    static void GLAPIENTRY gl_message_callback(
        GLenum,
        GLenum,
        GLuint,
        GLenum severity,
        GLsizei,
        const GLchar* message,
        const void*)
    {
        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:
                TBX_ASSERT(false, "OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_MEDIUM:
                TBX_TRACE_ERROR("OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_LOW:
                TBX_TRACE_WARNING("OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_NOTIFICATION:
                TBX_TRACE_INFO("OpenGL callback: {}", message);
                break;
            default:
                TBX_TRACE_WARNING("OpenGL callback: {}", message);
                break;
        }
    }

    OpenGlRenderer::OpenGlRenderer(
        GraphicsProcAddress loader,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        OpenGlContext context,
        const OpenGlShadowSettings& shadow_settings)
        : _entity_registry(&entity_registry)
        , _asset_manager(&asset_manager)
        , _context(std::move(context))
    {
        set_shadow_settings(shadow_settings);

        auto* glad_loader = reinterpret_cast<GLADloadproc>(loader);
        TBX_ASSERT(
            glad_loader != nullptr,
            "OpenGL rendering: context-ready event provided null loader.");
        TBX_ASSERT(
            _entity_registry != nullptr,
            "OpenGL rendering: renderer requires a valid entity registry.");
        TBX_ASSERT(
            _asset_manager != nullptr,
            "OpenGL rendering: renderer requires a valid asset manager.");
        TBX_ASSERT(
            _context.get_window_id().is_valid(),
            "OpenGL rendering: renderer requires a valid context window id.");

        auto load_result = gladLoadGLLoader(glad_loader);
        TBX_ASSERT(load_result != 0, "OpenGL rendering: failed to initialize GLAD.");
        initialize();
        _deferred_lighting_resource = _entity_registry->add("OpenGlDeferredLighting");
        auto& deferred_post_processing =
            _entity_registry->add<PostProcessing>(_deferred_lighting_resource);
        deferred_post_processing.effects = {PostProcessingEffect {
            .material =
                MaterialInstance {
                    .handle = deferred_lighting_material,
                },
            .is_enabled = true,
            .blend = 1.0f,
        }};
        deferred_post_processing.is_enabled = true;
        _resource_manager = std::make_unique<OpenGlResourceManager>(asset_manager);
        _render_pipeline = std::make_unique<OpenGlRenderPipeline>(*_resource_manager);

        _gbuffer_resource = _resource_manager->add<OpenGlGBuffer>();
        auto did_register_gbuffer = _gbuffer_resource.is_valid();
        TBX_ASSERT(did_register_gbuffer, "OpenGL rendering: failed to register gbuffer resource.");
        _resource_manager->pin(_gbuffer_resource);

        _lighting_framebuffer_resource = _resource_manager->add<OpenGlFrameBuffer>();
        auto did_register_lighting = _lighting_framebuffer_resource.is_valid();
        TBX_ASSERT(
            did_register_lighting,
            "OpenGL rendering: failed to register lighting framebuffer resource.");
        _resource_manager->pin(_lighting_framebuffer_resource);

        _post_process_ping_framebuffer_resource = _resource_manager->add<OpenGlFrameBuffer>();
        auto did_register_post_process_ping = _post_process_ping_framebuffer_resource.is_valid();
        TBX_ASSERT(
            did_register_post_process_ping,
            "OpenGL rendering: failed to register ping post-process framebuffer resource.");
        _resource_manager->pin(_post_process_ping_framebuffer_resource);

        _post_process_pong_framebuffer_resource = _resource_manager->add<OpenGlFrameBuffer>();
        auto did_register_post_process_pong = _post_process_pong_framebuffer_resource.is_valid();
        TBX_ASSERT(
            did_register_post_process_pong,
            "OpenGL rendering: failed to register pong post-process framebuffer resource.");
        _resource_manager->pin(_post_process_pong_framebuffer_resource);
    }

    OpenGlRenderer::~OpenGlRenderer() noexcept
    {
        shutdown();
    }

    bool OpenGlRenderer::render()
    {
        // Step 1: Validate renderer state required to build and submit a frame.
        if (!_render_pipeline)
            return false;
        TBX_ASSERT(
            _resource_manager != nullptr,
            "OpenGL rendering: renderer requires a valid resource manager.");
        if (_resource_manager == nullptr)
            return false;
        auto& resource_manager = *_resource_manager;

        auto make_current_result = _context.make_current();
        if (!make_current_result)
            return false;

        // Step 2: Apply any queued render-resolution change once the context is current.
        if (_pending_render_resolution.has_value())
        {
            Size pending_resolution = _pending_render_resolution.value();
            set_render_resolution(pending_resolution);
            if (_render_resolution.width == pending_resolution.width
                && _render_resolution.height == pending_resolution.height)
                _pending_render_resolution = std::nullopt;
        }

        // Step 3: Resolve frame targets that the pipeline depends on.
        auto try_load_required_framebuffer = [&resource_manager](const Uuid& resource_uuid)
        {
            return try_load_framebuffer(resource_manager, resource_uuid);
        };
        auto gbuffer = try_load_gbuffer(resource_manager, _gbuffer_resource);
        if (!gbuffer)
            return false;
        auto lighting_framebuffer = try_load_required_framebuffer(_lighting_framebuffer_resource);
        if (!lighting_framebuffer)
            return false;
        auto post_process_ping_framebuffer =
            try_load_required_framebuffer(_post_process_ping_framebuffer_resource);
        if (!post_process_ping_framebuffer)
            return false;
        auto post_process_pong_framebuffer =
            try_load_required_framebuffer(_post_process_pong_framebuffer_resource);
        if (!post_process_pong_framebuffer)
            return false;

        // Step 4: Select the first available camera as the active frame camera.
        auto camera_view = OpenGlCameraView {};
        auto camera_entities = _entity_registry->get_with<Camera>();
        if (camera_entities.empty())
            return false;
        if (camera_entities.size() > 1)
            TBX_TRACE_WARNING(
                "OpenGL rendering: multiple Camera components found; using the first one.");
        camera_view.camera_entity = camera_entities.front();

        // Step 5: Collect visible renderables for static and dynamic passes.
        for (auto& entity : _entity_registry->get_with<Renderer, StaticMesh>())
            camera_view.in_view_static_entities.push_back(entity);
        for (auto& entity : _entity_registry->get_with<Renderer, DynamicMesh>())
            camera_view.in_view_dynamic_entities.push_back(entity);

        auto camera_world_position = get_camera_world_position(camera_view);

        // Step 6: Gather directional/local light inputs and select capped shadow casters.
        auto frame_directional_lights = std::vector<OpenGlDirectionalLightData> {};
        auto directional_shadow_light_indices = std::vector<size_t> {};
        for (auto& entity : _entity_registry->get_with<DirectionalLight>())
        {
            auto color = Vec3(1.0f);
            auto intensity = 1.0f;
            const auto& light = entity.get_component<DirectionalLight>();
            resolve_light_color(light.color, light.intensity, color, intensity);
            frame_directional_lights.push_back(
                OpenGlDirectionalLightData {
                    .direction = -get_entity_forward_direction(entity),
                    .intensity = intensity,
                    .color = color,
                    .ambient = std::max(light.ambient, 0.0f),
                    .shadow_map_index = -1,
                });
            directional_shadow_light_indices.push_back(frame_directional_lights.size() - 1U);
        }

        auto frame_point_lights = std::vector<OpenGlPointLightData> {};
        struct PointShadowCandidate final
        {
            size_t light_index = 0U;
            Vec3 position = Vec3(0.0F);
            float range = 1.0F;
            float importance = 0.0F;
        };
        auto point_shadow_candidates = std::vector<PointShadowCandidate> {};
        for (auto& entity : _entity_registry->get_with<PointLight>())
        {
            auto color = Vec3(1.0f);
            auto intensity = 1.0f;
            const auto& light = entity.get_component<PointLight>();
            const float safe_range = std::max(light.range, 0.001F);
            const auto light_position = get_entity_position(entity);
            resolve_light_color(light.color, light.intensity, color, intensity);
            frame_point_lights.push_back(
                OpenGlPointLightData {
                    .position = light_position,
                    .range = safe_range,
                    .color = color,
                    .intensity = intensity,
                    .shadow_map_index = -1,
                });
            point_shadow_candidates.push_back(
                PointShadowCandidate {
                    .light_index = frame_point_lights.size() - 1U,
                    .position = light_position,
                    .range = safe_range,
                    .importance = calculate_local_light_shadow_importance(
                        intensity,
                        light_position,
                        camera_world_position,
                        safe_range),
                });
        }

        auto frame_spot_lights = std::vector<OpenGlSpotLightData> {};
        struct SpotShadowCandidate final
        {
            size_t light_index = 0U;
            Vec3 position = Vec3(0.0F);
            Vec3 direction = Vec3(0.0F, 0.0F, -1.0F);
            float range = 1.0F;
            float outer_angle_radians = 0.0F;
            float importance = 0.0F;
        };
        auto spot_shadow_candidates = std::vector<SpotShadowCandidate> {};
        for (auto& entity : _entity_registry->get_with<SpotLight>())
        {
            auto color = Vec3(1.0f);
            auto intensity = 1.0f;
            const auto& light = entity.get_component<SpotLight>();
            const float safe_range = std::max(light.range, 0.001F);
            const auto light_position = get_entity_position(entity);
            const auto light_direction = get_entity_forward_direction(entity);
            resolve_light_color(light.color, light.intensity, color, intensity);
            float inner_radians = to_radians(std::max(light.inner_angle, 0.0f));
            float outer_radians = to_radians(std::max(light.outer_angle, light.inner_angle));
            frame_spot_lights.push_back(
                OpenGlSpotLightData {
                    .position = light_position,
                    .range = safe_range,
                    .direction = light_direction,
                    .inner_cos = cos(inner_radians),
                    .color = color,
                    .outer_cos = cos(outer_radians),
                    .intensity = intensity,
                    .shadow_map_index = -1,
                });
            spot_shadow_candidates.push_back(
                SpotShadowCandidate {
                    .light_index = frame_spot_lights.size() - 1U,
                    .position = light_position,
                    .direction = light_direction,
                    .range = safe_range,
                    .outer_angle_radians = outer_radians,
                    .importance = calculate_local_light_shadow_importance(
                        intensity,
                        light_position,
                        camera_world_position,
                        safe_range),
                });
        }

        std::sort(
            directional_shadow_light_indices.begin(),
            directional_shadow_light_indices.end(),
            [&frame_directional_lights](size_t lhs, size_t rhs)
            {
                return frame_directional_lights[lhs].intensity
                       > frame_directional_lights[rhs].intensity;
            });
        if (directional_shadow_light_indices.size() > MAX_SHADOWED_DIRECTIONAL_LIGHTS)
            directional_shadow_light_indices.resize(MAX_SHADOWED_DIRECTIONAL_LIGHTS);

        std::sort(
            point_shadow_candidates.begin(),
            point_shadow_candidates.end(),
            [](const PointShadowCandidate& lhs, const PointShadowCandidate& rhs)
            {
                return lhs.importance > rhs.importance;
            });
        if (point_shadow_candidates.size() > MAX_SHADOWED_POINT_LIGHTS)
            point_shadow_candidates.resize(MAX_SHADOWED_POINT_LIGHTS);

        std::sort(
            spot_shadow_candidates.begin(),
            spot_shadow_candidates.end(),
            [](const SpotShadowCandidate& lhs, const SpotShadowCandidate& rhs)
            {
                return lhs.importance > rhs.importance;
            });
        if (spot_shadow_candidates.size() > MAX_SHADOWED_SPOT_LIGHTS)
            spot_shadow_candidates.resize(MAX_SHADOWED_SPOT_LIGHTS);

        // Step 7: Build per-light shadow map projections for directional/spot/point casters.
        const size_t directional_shadow_map_count =
            directional_shadow_light_indices.size() * DIRECTIONAL_SHADOW_CASCADE_COUNT;
        const size_t point_shadow_map_count =
            point_shadow_candidates.size() * POINT_SHADOW_FACE_COUNT;
        const size_t spot_shadow_map_count = spot_shadow_candidates.size();
        const size_t total_shadow_map_count =
            directional_shadow_map_count + point_shadow_map_count + spot_shadow_map_count;
        ensure_shadow_map_resources(
            _shadow_map_resources,
            total_shadow_map_count,
            _shadow_settings,
            resource_manager);

        auto frame_shadow_light_view_projections = std::vector<Mat4> {};
        frame_shadow_light_view_projections.reserve(total_shadow_map_count);
        auto frame_shadow_map_resources = std::vector<Uuid> {};
        frame_shadow_map_resources.reserve(total_shadow_map_count);
        auto frame_cascade_splits = std::vector<float> {};
        frame_cascade_splits.reserve(directional_shadow_light_indices.size());
        size_t shadow_map_index = 0U;
        const float directional_shadow_far_plane =
            std::max(SHADOW_NEAR_PLANE + 0.001F, _shadow_settings.shadow_render_distance);
        const float directional_split_distance = calculate_directional_shadow_split_distance(
            SHADOW_NEAR_PLANE,
            directional_shadow_far_plane);

        auto append_shadow_projection = [&](const Mat4& light_view_projection) -> bool
        {
            if (shadow_map_index >= _shadow_map_resources.size())
                return false;

            frame_shadow_light_view_projections.push_back(light_view_projection);
            frame_shadow_map_resources.push_back(_shadow_map_resources[shadow_map_index]);
            shadow_map_index += 1U;
            return true;
        };

        for (const auto directional_light_index : directional_shadow_light_indices)
        {
            frame_directional_lights[directional_light_index].shadow_map_index =
                static_cast<int>(shadow_map_index);
            frame_cascade_splits.push_back(directional_split_distance);

            auto near_plane = SHADOW_NEAR_PLANE;
            for (size_t cascade_index = 0; cascade_index < DIRECTIONAL_SHADOW_CASCADE_COUNT;
                 ++cascade_index)
            {
                const float far_plane =
                    cascade_index == 0U ? directional_split_distance : directional_shadow_far_plane;
                const auto shadow_matrix = build_directional_shadow_view_projection(
                    camera_view,
                    _render_resolution,
                    frame_directional_lights[directional_light_index].direction,
                    near_plane,
                    far_plane,
                    _shadow_settings);
                if (!append_shadow_projection(shadow_matrix))
                    break;
                near_plane = far_plane;
            }
        }

        for (const auto& spot_shadow_candidate : spot_shadow_candidates)
        {
            frame_spot_lights[spot_shadow_candidate.light_index].shadow_map_index =
                static_cast<int>(shadow_map_index);
            const auto shadow_matrix = build_spot_shadow_view_projection(
                spot_shadow_candidate.position,
                spot_shadow_candidate.direction,
                spot_shadow_candidate.outer_angle_radians,
                spot_shadow_candidate.range);
            if (!append_shadow_projection(shadow_matrix))
                break;
        }

        for (const auto& point_shadow_candidate : point_shadow_candidates)
        {
            frame_point_lights[point_shadow_candidate.light_index].shadow_map_index =
                static_cast<int>(shadow_map_index);
            for (size_t face_index = 0; face_index < POINT_SHADOW_FACE_COUNT; ++face_index)
            {
                const auto shadow_matrix = build_point_shadow_face_view_projection(
                    point_shadow_candidate.position,
                    point_shadow_candidate.range,
                    face_index);
                if (!append_shadow_projection(shadow_matrix))
                    break;
            }
        }

        // Step 8: Resolve the active sky and maintain pinned sky-resource ownership.
        auto sky_material = MaterialInstance {};
        auto sky_entity = Entity {};
        auto frame_clear_color = RgbaColor::black;
        auto frame_sky_resource = Uuid::NONE;
        auto frame_sky_entity = Entity {};
        auto sky_entities = _entity_registry->get_with<Sky>();
        if (sky_entities.size() > 1)
            TBX_TRACE_WARNING(
                "OpenGL rendering: multiple Sky components found; using the first one.");
        if (!sky_entities.empty())
        {
            sky_entity = sky_entities.front();
            sky_material = sky_entity.get_component<Sky>().material;
            TBX_ASSERT(
                sky_material.handle.is_valid(),
                "OpenGL rendering: Sky component requires a valid material handle.");
            if (sky_material.handle.is_valid())
            {
                if (resource_manager.add(sky_entity, true))
                {
                    frame_sky_entity = sky_entity;
                    frame_sky_resource = sky_entity.get_id();
                }
            }
        }

        if (_pinned_sky_resource.is_valid() && _pinned_sky_resource != frame_sky_resource)
            resource_manager.unpin(_pinned_sky_resource);
        _pinned_sky_resource = frame_sky_resource;

        // Step 9: Resolve post-processing settings and flatten enabled effects.
        bool is_post_processing_enabled = false;
        auto post_process_owner_entity = Entity {};
        auto resolved_post_processing = OpenGlPostProcessing {};
        auto post_process_entities = _entity_registry->get_with<PostProcessing>();
        auto selected_post_process_entity = Entity {};
        size_t user_post_process_count = 0;
        for (const auto& candidate : post_process_entities)
        {
            if (candidate.get_id() == _deferred_lighting_resource)
                continue;

            if (!selected_post_process_entity.get_id().is_valid())
                selected_post_process_entity = candidate;
            ++user_post_process_count;
        }

        if (user_post_process_count > 1)
            TBX_TRACE_WARNING(
                "OpenGL rendering: multiple PostProcessing components found; using the first one.");
        if (selected_post_process_entity.get_id().is_valid())
        {
            post_process_owner_entity = selected_post_process_entity;
            const auto& post_processing = post_process_owner_entity.get_component<PostProcessing>();
            is_post_processing_enabled = post_processing.is_enabled;
            resource_manager.add(post_process_owner_entity);

            resolved_post_processing.effects.reserve(post_processing.effects.size());
            for (size_t effect_index = 0; effect_index < post_processing.effects.size();
                 ++effect_index)
            {
                const auto& effect = post_processing.effects[effect_index];
                resolved_post_processing.effects.push_back(
                    OpenGlPostProcessEffect {
                        .owner_entity = post_process_owner_entity,
                        .source_effect_index = effect_index,
                        .is_enabled = effect.is_enabled,
                        .material = effect.material,
                        .blend = effect.blend,
                    });
            }
        }

        // Step 10: Build a frame context payload and execute the render pipeline.
        auto post_process_settings = OpenGlPostProcessSettings {
            .is_enabled =
                selected_post_process_entity.get_id().is_valid() && is_post_processing_enabled,
            .owner_entity = post_process_owner_entity,
            .effects = std::span<const OpenGlPostProcessEffect>(resolved_post_processing.effects),
        };
        auto view_projection = get_camera_view_projection(camera_view, _render_resolution);
        auto inverse_view_projection = inverse(view_projection);

        auto frame_context = OpenGlRenderFrameContext {
            .camera_view = camera_view,
            .render_resolution = _render_resolution,
            .viewport_size = _viewport_size,
            .clear_color = frame_clear_color,
            .sky_entity = frame_sky_entity,
            .post_process = post_process_settings,
            .deferred_lighting_entity = _entity_registry->get(_deferred_lighting_resource),
            .gbuffer = gbuffer.get(),
            .lighting_target = lighting_framebuffer.get(),
            .post_process_ping_target = post_process_ping_framebuffer.get(),
            .post_process_pong_target = post_process_pong_framebuffer.get(),
            .camera_world_position = camera_world_position,
            .view_projection = view_projection,
            .inverse_view_projection = inverse_view_projection,
            .directional_lights =
                std::span<const OpenGlDirectionalLightData>(frame_directional_lights),
            .point_lights = std::span<const OpenGlPointLightData>(frame_point_lights),
            .spot_lights = std::span<const OpenGlSpotLightData>(frame_spot_lights),
            .shadow_data =
                OpenGlShadowFrameData {
                    .map_uuids = std::span<const Uuid>(frame_shadow_map_resources),
                    .light_view_projections =
                        std::span<const Mat4>(frame_shadow_light_view_projections),
                    .cascade_splits = std::span<const float>(frame_cascade_splits),
                    .shadow_map_resolution = _shadow_settings.shadow_map_resolution,
                    .shadow_softness = _shadow_settings.shadow_softness,
                },
            .present_mode = OpenGlFrameBufferPresentMode::ASPECT_FIT,
            .present_target_framebuffer_id = 0,
            .scene_color_texture_id = lighting_framebuffer->get_color_texture_id(),
        };

        _render_pipeline->execute(frame_context);

        // Step 11: Present the final image to the window backbuffer.
        auto present_result = _context.present();
        if (!present_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: present request failed: {}",
                present_result.get_report());
        }

        return true;
    }

    void OpenGlRenderer::set_viewport_size(const Size& viewport_size)
    {
        if (viewport_size.width == 0 || viewport_size.height == 0)
            return;

        _viewport_size = viewport_size;
    }

    void OpenGlRenderer::set_pending_render_resolution(
        const std::optional<Size>& pending_render_resolution)
    {
        _pending_render_resolution = pending_render_resolution;
    }

    void OpenGlRenderer::set_shadow_settings(const OpenGlShadowSettings& shadow_settings)
    {
        auto sanitized = shadow_settings;
        sanitized.shadow_map_resolution = std::max(1U, sanitized.shadow_map_resolution);
        sanitized.shadow_render_distance = std::max(0.001F, sanitized.shadow_render_distance);
        sanitized.shadow_softness = std::max(0.0F, sanitized.shadow_softness);

        const bool did_resolution_change =
            _shadow_settings.shadow_map_resolution != sanitized.shadow_map_resolution;
        _shadow_settings = sanitized;

        if (did_resolution_change)
            reset_shadow_maps();
    }

    void OpenGlRenderer::initialize()
    {
        auto major_version = GLVersion.major;
        auto minor_version = GLVersion.minor;

        TBX_TRACE_INFO(
            "OpenGL rendering: initializing window {} context.",
            to_string(_context.get_window_id()));

        TBX_ASSERT(
            major_version > 4 || (major_version == 4 && minor_version >= 5),
            "OpenGL rendering: requires OpenGL 4.5 or newer.");

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
        glEnable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.07f, 0.08f, 0.11f, 1.0f);
    }

    void OpenGlRenderer::shutdown()
    {
        if (_entity_registry && _deferred_lighting_resource.is_valid()
            && _entity_registry->has<PostProcessing>(_deferred_lighting_resource))
        {
            auto deferred_entity = _entity_registry->get(_deferred_lighting_resource);
            _entity_registry->remove(deferred_entity);
        }
        if (_resource_manager && _pinned_sky_resource.is_valid())
            _resource_manager->unpin(_pinned_sky_resource);
        _pinned_sky_resource = Uuid::NONE;
        _deferred_lighting_resource = Uuid::NONE;
        reset_shadow_maps();
        _gbuffer_resource = Uuid::NONE;
        _lighting_framebuffer_resource = Uuid::NONE;
        _post_process_ping_framebuffer_resource = Uuid::NONE;
        _post_process_pong_framebuffer_resource = Uuid::NONE;
        if (_resource_manager)
            _resource_manager->clear();
        _render_pipeline.reset();
        _resource_manager.reset();
        _pending_render_resolution = std::nullopt;
        _viewport_size = {};
        _render_resolution = {};
    }

    void OpenGlRenderer::reset_shadow_maps()
    {
        if (_resource_manager)
        {
            for (const auto& shadow_map_resource : _shadow_map_resources)
                _resource_manager->unpin(shadow_map_resource);
        }

        _shadow_map_resources.clear();
    }

    void OpenGlRenderer::set_render_resolution(const Size& render_resolution)
    {
        if (render_resolution.width == 0 || render_resolution.height == 0)
            return;
        if (_render_resolution.width == render_resolution.width
            && _render_resolution.height == render_resolution.height)
            return;

        _render_resolution = render_resolution;
        if (_render_pipeline == nullptr || _resource_manager == nullptr)
            return;

        auto& resource_manager = *_resource_manager;
        auto gbuffer = try_load_gbuffer(resource_manager, _gbuffer_resource);
        auto lighting_framebuffer =
            try_load_framebuffer(resource_manager, _lighting_framebuffer_resource);
        auto post_process_ping_framebuffer =
            try_load_framebuffer(resource_manager, _post_process_ping_framebuffer_resource);
        auto post_process_pong_framebuffer =
            try_load_framebuffer(resource_manager, _post_process_pong_framebuffer_resource);
        TBX_ASSERT(
            gbuffer && lighting_framebuffer && post_process_ping_framebuffer
                && post_process_pong_framebuffer,
            "OpenGL rendering: missing runtime render targets while resizing.");
        if (!gbuffer || !lighting_framebuffer || !post_process_ping_framebuffer
            || !post_process_pong_framebuffer)
            return;

        gbuffer->set_resolution(render_resolution);
        lighting_framebuffer->set_resolution(render_resolution);
        post_process_ping_framebuffer->set_resolution(render_resolution);
        post_process_pong_framebuffer->set_resolution(render_resolution);
    }
}
