#include "opengl_renderer.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_gbuffer.h"
#include "opengl_resources/opengl_runtime_resource_descriptor.h"
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
#include <cmath>
#include <glad/glad.h>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace tbx::plugins
{
    static constexpr int MAX_DIRECTIONAL_SHADOW_MAPS = 1;
    static constexpr int SHADOW_MAP_RESOLUTION = 2048;

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
        OpenGlResourceManager& resource_manager)
    {
        if (shadow_map_resources.size() < desired_count)
            shadow_map_resources.reserve(desired_count);

        while (shadow_map_resources.size() < desired_count)
        {
            auto shadow_map_resource = Uuid::generate();
            const auto did_register = resource_manager.add(
                shadow_map_resource,
                OpenGlRuntimeResourceDescriptor {
                    .kind = OpenGlRuntimeResourceKind::SHADOW_MAP,
                    .shadow_map_resolution =
                        Size {
                            static_cast<uint32>(SHADOW_MAP_RESOLUTION),
                            static_cast<uint32>(SHADOW_MAP_RESOLUTION),
                        },
                });
            TBX_ASSERT(did_register, "OpenGL rendering: failed to register shadow map resource.");
            if (!did_register)
                continue;

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

    static Mat4 build_directional_shadow_view_projection(
        const Vec3& camera_position,
        const Vec3& directional_light_direction)
    {
        auto direction_to_light = normalize(directional_light_direction);
        auto shadow_center = camera_position;
        auto light_position = shadow_center + (direction_to_light * 40.0f);

        auto up_axis = Vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(dot(direction_to_light, up_axis)) > 0.95f)
            up_axis = Vec3(1.0f, 0.0f, 0.0f);

        auto light_view = look_at(light_position, shadow_center, up_axis);
        auto light_projection = ortho_projection(-50.0f, 50.0f, -50.0f, 50.0f, 1.0f, 120.0f);
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
        OpenGlContext context)
        : _entity_registry(&entity_registry)
        , _asset_manager(&asset_manager)
        , _context(std::move(context))
    {
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

        _gbuffer_resource = Uuid::generate();
        auto did_register_gbuffer = _resource_manager->add(
            _gbuffer_resource,
            OpenGlRuntimeResourceDescriptor {
                .kind = OpenGlRuntimeResourceKind::GBUFFER,
            });
        TBX_ASSERT(did_register_gbuffer, "OpenGL rendering: failed to register gbuffer resource.");
        _resource_manager->pin(_gbuffer_resource);

        _lighting_framebuffer_resource = Uuid::generate();
        auto did_register_lighting = _resource_manager->add(
            _lighting_framebuffer_resource,
            OpenGlRuntimeResourceDescriptor {
                .kind = OpenGlRuntimeResourceKind::FRAMEBUFFER,
            });
        TBX_ASSERT(
            did_register_lighting,
            "OpenGL rendering: failed to register lighting framebuffer resource.");
        _resource_manager->pin(_lighting_framebuffer_resource);

        _post_process_ping_framebuffer_resource = Uuid::generate();
        auto did_register_post_process_ping = _resource_manager->add(
            _post_process_ping_framebuffer_resource,
            OpenGlRuntimeResourceDescriptor {
                .kind = OpenGlRuntimeResourceKind::FRAMEBUFFER,
            });
        TBX_ASSERT(
            did_register_post_process_ping,
            "OpenGL rendering: failed to register ping post-process framebuffer resource.");
        _resource_manager->pin(_post_process_ping_framebuffer_resource);

        _post_process_pong_framebuffer_resource = Uuid::generate();
        auto did_register_post_process_pong = _resource_manager->add(
            _post_process_pong_framebuffer_resource,
            OpenGlRuntimeResourceDescriptor {
                .kind = OpenGlRuntimeResourceKind::FRAMEBUFFER,
            });
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

        // Step 6: Gather directional lights and the corresponding shadow-map metadata.
        auto frame_directional_lights = std::vector<OpenGlDirectionalLightData> {};
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
                });
        }

        auto directional_shadow_count = std::min(
            frame_directional_lights.size(),
            static_cast<size_t>(MAX_DIRECTIONAL_SHADOW_MAPS));
        ensure_shadow_map_resources(
            _shadow_map_resources,
            directional_shadow_count,
            resource_manager);

        auto camera_world_position = get_camera_world_position(camera_view);
        auto frame_shadow_light_view_projections = std::vector<Mat4> {};
        frame_shadow_light_view_projections.reserve(directional_shadow_count);
        auto frame_shadow_map_resources = std::vector<Uuid> {};
        frame_shadow_map_resources.reserve(directional_shadow_count);
        for (size_t shadow_index = 0; shadow_index < directional_shadow_count; ++shadow_index)
        {
            frame_shadow_light_view_projections.push_back(build_directional_shadow_view_projection(
                camera_world_position,
                frame_directional_lights[shadow_index].direction));
            frame_shadow_map_resources.push_back(_shadow_map_resources[shadow_index]);
        }

        // Step 7: Gather local (point/spot) lights for the deferred lighting pass.
        auto frame_point_lights = std::vector<OpenGlPointLightData> {};
        for (auto& entity : _entity_registry->get_with<PointLight>())
        {
            auto color = Vec3(1.0f);
            auto intensity = 1.0f;
            const auto& light = entity.get_component<PointLight>();
            resolve_light_color(light.color, light.intensity, color, intensity);
            frame_point_lights.push_back(
                OpenGlPointLightData {
                    .position = get_entity_position(entity),
                    .range = std::max(light.range, 0.001f),
                    .color = color,
                    .intensity = intensity,
                });
        }

        auto frame_spot_lights = std::vector<OpenGlSpotLightData> {};
        for (auto& entity : _entity_registry->get_with<SpotLight>())
        {
            auto color = Vec3(1.0f);
            auto intensity = 1.0f;
            const auto& light = entity.get_component<SpotLight>();
            resolve_light_color(light.color, light.intensity, color, intensity);
            float inner_radians = to_radians(std::max(light.inner_angle, 0.0f));
            float outer_radians = to_radians(std::max(light.outer_angle, light.inner_angle));
            frame_spot_lights.push_back(
                OpenGlSpotLightData {
                    .position = get_entity_position(entity),
                    .range = std::max(light.range, 0.001f),
                    .direction = get_entity_forward_direction(entity),
                    .inner_cos = cos(inner_radians),
                    .color = color,
                    .outer_cos = cos(outer_radians),
                    .intensity = intensity,
                });
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
                    .cascade_splits = std::span<const float>(),
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
        _shadow_map_resources.clear();
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
