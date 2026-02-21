#include "opengl_renderer.h"
#include "opengl_post_processing.h"
#include "opengl_render_pipeline.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
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

    static void destroy_shadow_map_textures(std::vector<uint32>& texture_ids)
    {
        if (texture_ids.empty())
            return;

        glDeleteTextures(
            static_cast<GLsizei>(texture_ids.size()),
            reinterpret_cast<const GLuint*>(texture_ids.data()));
        texture_ids.clear();
    }

    static uint32 create_shadow_map_texture()
    {
        uint32 texture_id = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, reinterpret_cast<GLuint*>(&texture_id));
        if (texture_id == 0)
            return 0;

        glTextureStorage2D(
            texture_id,
            1,
            GL_DEPTH_COMPONENT32F,
            SHADOW_MAP_RESOLUTION,
            SHADOW_MAP_RESOLUTION);
        glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        constexpr float BORDER_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTextureParameterfv(texture_id, GL_TEXTURE_BORDER_COLOR, BORDER_COLOR);
        return texture_id;
    }

    static void ensure_shadow_map_textures(std::vector<uint32>& texture_ids, size_t desired_count)
    {
        if (texture_ids.size() == desired_count)
            return;

        destroy_shadow_map_textures(texture_ids);
        texture_ids.reserve(desired_count);
        for (size_t shadow_index = 0; shadow_index < desired_count; ++shadow_index)
        {
            auto texture_id = create_shadow_map_texture();
            if (texture_id == 0)
                break;

            texture_ids.push_back(texture_id);
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

    static void apply_runtime_material_overrides(
        const MaterialInstance& runtime_material,
        Material& in_out_material)
    {
        for (const auto& texture_binding : runtime_material.textures)
            append_or_override_texture(
                in_out_material.textures,
                texture_binding.name,
                texture_binding.texture);

        for (const auto& parameter_binding : runtime_material.parameters)
            append_or_override_material_parameter(
                in_out_material.parameters,
                parameter_binding.name,
                parameter_binding.value);
    }

    static bool has_sky_texture(const Material& material)
    {
        bool has_any_texture = false;
        const auto* diffuse_texture_binding = material.textures.get("diffuse");
        bool has_diffuse_texture =
            diffuse_texture_binding && diffuse_texture_binding->texture.handle.is_valid();

        for (const auto& texture_binding : material.textures)
        {
            auto normalized_name = normalize_uniform_name(texture_binding.name);
            const auto& texture_handle = texture_binding.texture.handle;
            if (!texture_handle.is_valid())
                continue;

            has_any_texture = true;
            if (normalized_name == "u_diffuse")
                continue;

            TBX_ASSERT(
                false,
                "OpenGL rendering: Sky only supports panoramic textures bound to 'diffuse'.");
            return false;
        }

        if (has_any_texture && !has_diffuse_texture)
        {
            TBX_ASSERT(
                false,
                "OpenGL rendering: Sky requires a panoramic texture bound to 'diffuse'.");
            return false;
        }

        return has_diffuse_texture;
    }

    static bool try_get_sky_color(const Material& material, RgbaColor& out_color)
    {
        const auto* parameter_binding = material.parameters.get("color");
        if (parameter_binding == nullptr)
            return false;

        if (std::holds_alternative<RgbaColor>(parameter_binding->value))
        {
            out_color = std::get<RgbaColor>(parameter_binding->value);
            return true;
        }

        if (std::holds_alternative<Vec4>(parameter_binding->value))
        {
            auto value = std::get<Vec4>(parameter_binding->value);
            out_color = RgbaColor(value.x, value.y, value.z, value.w);
            return true;
        }

        return false;
    }

    static bool is_valid_sky_shader_program(const ShaderProgram& shader_program)
    {
        if (shader_program.compute.is_valid())
            return false;

        if (!shader_program.vertex.is_valid() || !shader_program.fragment.is_valid())
            return false;

        return true;
    }

    static Vec3 get_camera_world_position(const OpenGlCameraView& camera_view)
    {
        if (!camera_view.camera_entity.has_component<Transform>())
            return Vec3(0.0f);

        const auto& camera_transform = camera_view.camera_entity.get_component<Transform>();
        return camera_transform.position;
    }

    static Quat get_camera_world_rotation(const OpenGlCameraView& camera_view)
    {
        if (!camera_view.camera_entity.has_component<Transform>())
            return Quat(1.0f, 0.0f, 0.0f, 0.0f);

        const auto& camera_transform = camera_view.camera_entity.get_component<Transform>();
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
        if (!entity.has_component<Transform>())
            return Vec3(0.0f, -1.0f, 0.0f);

        const auto& transform = entity.get_component<Transform>();
        return normalize(transform.rotation * Vec3(0.0f, 0.0f, -1.0f));
    }

    static Vec3 get_entity_position(const Entity& entity)
    {
        if (!entity.has_component<Transform>())
            return Vec3(0.0f);

        const auto& transform = entity.get_component<Transform>();
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
        MakeCurrentSender make_current_sender,
        PresentSender present_sender)
        : _entity_registry(&entity_registry)
        , _asset_manager(&asset_manager)
        , _make_current_sender(std::move(make_current_sender))
        , _present_sender(std::move(present_sender))
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
            _make_current_sender != nullptr,
            "OpenGL rendering: renderer requires a make-current sender.");
        TBX_ASSERT(
            _present_sender != nullptr,
            "OpenGL rendering: renderer requires a present sender.");

        auto load_result = gladLoadGLLoader(glad_loader);
        TBX_ASSERT(load_result != 0, "OpenGL rendering: failed to initialize GLAD.");
        initialize();
        _render_pipeline = std::make_unique<OpenGlRenderPipeline>(asset_manager);
    }

    OpenGlRenderer::~OpenGlRenderer() noexcept
    {
        shutdown();
    }

    const OpenGlRendererInfo& OpenGlRenderer::get_info() const
    {
        return _info;
    }

    bool OpenGlRenderer::render(const Uuid& target_window_id)
    {
        if (!_render_pipeline)
            return false;
        if (!target_window_id.is_valid())
            return false;

        auto make_current_result = _make_current_sender(target_window_id);
        if (!make_current_result)
            return false;

        if (_pending_render_resolution.has_value())
        {
            Size pending_resolution = _pending_render_resolution.value();
            set_render_resolution(pending_resolution);
            if (_render_resolution.width == pending_resolution.width
                && _render_resolution.height == pending_resolution.height)
                _pending_render_resolution = std::nullopt;
        }

        auto camera_view = OpenGlCameraView {};
        bool did_find_camera = false;
        _entity_registry->for_each_with<Camera>(
            [&camera_view, &did_find_camera](Entity& entity)
            {
                if (did_find_camera)
                    return;

                camera_view.camera_entity = entity;
                did_find_camera = true;
            });

        if (!did_find_camera)
            return false;

        _entity_registry->for_each_with<Renderer, StaticMesh>(
            [&camera_view](Entity& entity)
            {
                camera_view.in_view_static_entities.push_back(entity);
            });

        _entity_registry->for_each_with<Renderer, DynamicMesh>(
            [&camera_view](Entity& entity)
            {
                camera_view.in_view_dynamic_entities.push_back(entity);
            });

        auto frame_directional_lights = std::vector<OpenGlDirectionalLightData> {};
        auto frame_point_lights = std::vector<OpenGlPointLightData> {};
        auto frame_spot_lights = std::vector<OpenGlSpotLightData> {};
        auto frame_shadow_light_view_projections = std::vector<Mat4> {};
        auto frame_shadow_cascade_splits = std::vector<float> {};
        _entity_registry->for_each_with<DirectionalLight>(
            [&frame_directional_lights](Entity& entity)
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
            });

        auto directional_shadow_count = std::min(
            frame_directional_lights.size(),
            static_cast<size_t>(MAX_DIRECTIONAL_SHADOW_MAPS));
        ensure_shadow_map_textures(_shadow_map_texture_ids, directional_shadow_count);

        auto shadow_center_position = get_camera_world_position(camera_view);
        for (size_t shadow_index = 0; shadow_index < directional_shadow_count; ++shadow_index)
        {
            frame_shadow_light_view_projections.push_back(build_directional_shadow_view_projection(
                shadow_center_position,
                frame_directional_lights[shadow_index].direction));
        }

        _entity_registry->for_each_with<PointLight>(
            [&frame_point_lights](Entity& entity)
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
            });

        _entity_registry->for_each_with<SpotLight>(
            [&frame_spot_lights](Entity& entity)
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
            });

        _sky = {};
        auto sky_material = MaterialInstance {};
        auto frame_clear_color = RgbaColor::black;
        bool did_find_sky = false;
        bool did_warn_multiple_skies = false;
        _entity_registry->for_each_with<Sky>(
            [&sky_material, &did_find_sky, &did_warn_multiple_skies](Entity& entity)
            {
                if (did_find_sky)
                {
                    if (!did_warn_multiple_skies)
                    {
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: multiple Sky components found; using the first "
                            "one.");
                        did_warn_multiple_skies = true;
                    }
                    return;
                }

                did_find_sky = true;
                sky_material = entity.get_component<Sky>().material;
            });

        if (did_find_sky)
        {
            TBX_ASSERT(
                sky_material.handle.is_valid(),
                "OpenGL rendering: Sky component requires a valid material handle.");
            if (sky_material.handle.is_valid())
            {
                auto source_material = _asset_manager->load<Material>(sky_material.handle);
                TBX_ASSERT(
                    source_material != nullptr,
                    "OpenGL rendering: Sky material could not be loaded.");
                if (source_material)
                {
                    auto resolved_sky_material = *source_material;
                    apply_runtime_material_overrides(sky_material, resolved_sky_material);

                    TBX_ASSERT(
                        is_valid_sky_shader_program(resolved_sky_material.program),
                        "OpenGL rendering: Sky material must use a graphics shader program with "
                        "vertex+fragment stages and no compute stage.");
                    if (is_valid_sky_shader_program(resolved_sky_material.program))
                    {
                        if (has_sky_texture(resolved_sky_material))
                            _sky.sky_material = sky_material;
                        else
                        {
                            RgbaColor material_color = RgbaColor::black;
                            if (try_get_sky_color(resolved_sky_material, material_color))
                                frame_clear_color = material_color;
                        }
                    }
                }
            }
        }

        bool did_find_post_processing = false;
        bool did_warn_multiple_post_processing = false;
        bool is_post_processing_enabled = false;
        auto resolved_post_processing = OpenGlPostProcessing {};
        _entity_registry->for_each_with<PostProcessing>(
            [&resolved_post_processing,
             &did_find_post_processing,
             &did_warn_multiple_post_processing,
             &is_post_processing_enabled](Entity& entity)
            {
                if (did_find_post_processing)
                {
                    if (!did_warn_multiple_post_processing)
                    {
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: multiple PostProcessing components found; using "
                            "the first one.");
                        did_warn_multiple_post_processing = true;
                    }
                    return;
                }

                did_find_post_processing = true;
                const auto& post_processing = entity.get_component<PostProcessing>();
                is_post_processing_enabled = post_processing.is_enabled;

                resolved_post_processing.effects.reserve(post_processing.effects.size());
                for (const auto& effect : post_processing.effects)
                {
                    resolved_post_processing.effects.push_back(
                        OpenGlPostProcessEffect {
                            .is_enabled = effect.is_enabled,
                            .material = effect.material,
                            .blend = effect.blend,
                        });
                }
            });

        auto post_process_settings = OpenGlPostProcessSettings {
            .is_enabled = did_find_post_processing && is_post_processing_enabled,
            .effects = std::span<const OpenGlPostProcessEffect>(resolved_post_processing.effects),
        };
        auto view_projection = get_camera_view_projection(camera_view, _render_resolution);
        auto camera_world_position = get_camera_world_position(camera_view);

        auto frame_context = OpenGlRenderFrameContext {
            .camera_view = camera_view,
            .render_resolution = _render_resolution,
            .viewport_size = _viewport_size,
            .clear_color = frame_clear_color,
            .sky_material = _sky.sky_material,
            .post_process = post_process_settings,
            .gbuffer = &_gbuffer,
            .lighting_target = &_lighting_framebuffer,
            .post_process_ping_target = &_post_process_ping_framebuffer,
            .post_process_pong_target = &_post_process_pong_framebuffer,
            .camera_world_position = camera_world_position,
            .view_projection = view_projection,
            .inverse_view_projection = inverse(view_projection),
            .directional_lights =
                std::span<const OpenGlDirectionalLightData>(frame_directional_lights),
            .point_lights = std::span<const OpenGlPointLightData>(frame_point_lights),
            .spot_lights = std::span<const OpenGlSpotLightData>(frame_spot_lights),
            .shadow_data =
                OpenGlShadowFrameData {
                    .map_texture_ids = std::span<const uint32>(_shadow_map_texture_ids),
                    .light_view_projections =
                        std::span<const Mat4>(frame_shadow_light_view_projections),
                    .cascade_splits = std::span<const float>(frame_shadow_cascade_splits),
                },
            .present_mode = OpenGlFrameBufferPresentMode::ASPECT_FIT,
            .present_target_framebuffer_id = 0,
            .scene_color_texture_id = _lighting_framebuffer.get_color_texture_id(),
        };

        _render_pipeline->execute(frame_context);

        auto present_result = _present_sender(target_window_id);
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
        const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        _info.vendor = vendor ? vendor : "Unknown";
        _info.renderer = renderer ? renderer : "Unknown";
        _info.version = version ? version : "Unknown";
        _info.shading_language_version = glsl ? glsl : "Unknown";
        _info.major_version = GLVersion.major;
        _info.minor_version = GLVersion.minor;

        TBX_TRACE_INFO("OpenGL rendering: initializing OpenGL.");
        TBX_TRACE_INFO("OpenGL rendering: OpenGL info:");
        TBX_TRACE_INFO("  Vendor: {}", _info.vendor);
        TBX_TRACE_INFO("  Renderer: {}", _info.renderer);
        TBX_TRACE_INFO("  Version: {}", _info.version);
        TBX_TRACE_INFO("  GLSL: {}", _info.shading_language_version);

        TBX_ASSERT(
            _info.major_version > 4 || (_info.major_version == 4 && _info.minor_version >= 5),
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
        destroy_shadow_map_textures(_shadow_map_texture_ids);
        _shadow_map_texture_ids.clear();
        _sky = {};
        _render_pipeline.reset();
        _pending_render_resolution = std::nullopt;
        _viewport_size = {};
        _render_resolution = {};
        _info = {};
    }

    void OpenGlRenderer::set_render_resolution(const Size& render_resolution)
    {
        if (render_resolution.width == 0 || render_resolution.height == 0)
            return;
        if (_render_resolution.width == render_resolution.width
            && _render_resolution.height == render_resolution.height)
            return;

        _render_resolution = render_resolution;
        _gbuffer.set_resolution(render_resolution);
        _lighting_framebuffer.set_resolution(render_resolution);
        _post_process_ping_framebuffer.set_resolution(render_resolution);
        _post_process_pong_framebuffer.set_resolution(render_resolution);
    }
}
