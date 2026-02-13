#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/renderer.h"
#include "tbx/messages/observable.h"
#include <glad/glad.h>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace tbx::plugins
{
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

    static uint64 hash_runtime_material(const MaterialInstance& runtime_material)
    {
        constexpr uint64 fnv_offset = 1469598103934665603ULL;
        constexpr uint64 fnv_prime = 1099511628211ULL;

        auto hash_value = fnv_offset;
        auto hash_bytes = [&hash_value](const void* data, size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                hash_value ^= static_cast<uint64>(bytes[i]);
                hash_value *= fnv_prime;
            }
        };

        hash_bytes(&runtime_material.handle.id, sizeof(runtime_material.handle.id));

        for (const auto& parameter_binding : runtime_material.parameters)
        {
            hash_bytes(parameter_binding.name.data(), parameter_binding.name.size());
            const auto variant_index = static_cast<uint64>(parameter_binding.value.index());
            hash_bytes(&variant_index, sizeof(variant_index));
            std::visit(
                [&hash_bytes](const auto& value)
                {
                    hash_bytes(&value, sizeof(value));
                },
                parameter_binding.value);
        }

        for (const auto& texture_binding : runtime_material.textures)
        {
            hash_bytes(texture_binding.name.data(), texture_binding.name.size());
            hash_bytes(&texture_binding.texture.handle.id, sizeof(texture_binding.texture.handle.id));
            const bool has_settings_override = texture_binding.texture.settings.has_value();
            hash_bytes(&has_settings_override, sizeof(has_settings_override));
            if (has_settings_override)
            {
                const TextureSettings& texture_settings = texture_binding.texture.settings.value();
                hash_bytes(
                    &texture_settings.filter,
                    sizeof(texture_settings.filter));
                hash_bytes(
                    &texture_settings.wrap,
                    sizeof(texture_settings.wrap));
                hash_bytes(
                    &texture_settings.format,
                    sizeof(texture_settings.format));
                hash_bytes(
                    &texture_settings.mipmaps,
                    sizeof(texture_settings.mipmaps));
                hash_bytes(
                    &texture_settings.compression,
                    sizeof(texture_settings.compression));
            }
        }

        return hash_value;
    }

    static bool has_sky_texture(const Material& material)
    {
        bool has_any_texture = false;
        const auto* diffuse_texture_binding = material.textures.get("diffuse");
        const bool has_diffuse_texture = diffuse_texture_binding
            && diffuse_texture_binding->texture.handle.is_valid();

        for (const auto& texture_binding : material.textures)
        {
            const auto normalized_name = normalize_uniform_name(texture_binding.name);
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
            const auto value = std::get<Vec4>(parameter_binding->value);
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

    void OpenGlRenderingPlugin::on_attach(IPluginHost& host)
    {
        _render_pipeline = std::make_unique<OpenGlRenderPipeline>(host.get_asset_manager());
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        _is_context_ready = false;
        _render_pipeline.reset();
        _is_sky_cache_valid = false;
        _cached_has_sky_component = false;
        _cached_sky_source_material_hash = 0U;
        _cached_sky_material = nullptr;
        _cached_resolved_sky = {};
        _cached_resolved_post_processing.effects.clear();
    }

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        if (!_is_context_ready || !_render_pipeline)
            return;

        auto make_current_result = send_message<WindowMakeCurrentRequest>(_window_id);
        if (!make_current_result)
            return;

        auto camera_view = OpenGlCameraView();
        bool did_find_camera = false;
        auto& registry = get_host().get_entity_registry();
        registry.for_each_with<Camera>(
            [&camera_view, &did_find_camera](Entity& entity)
            {
                if (did_find_camera)
                    return;

                camera_view.camera_entity = entity;
                did_find_camera = true;
            });

        if (!did_find_camera)
            return;

        registry.for_each_with<Renderer, StaticMesh>(
            [&camera_view](Entity& entity)
            {
                camera_view.in_view_static_entities.push_back(entity);
            });

        registry.for_each_with<Renderer, DynamicMesh>(
            [&camera_view](Entity& entity)
            {
                camera_view.in_view_dynamic_entities.push_back(entity);
            });

        auto sky_material = MaterialInstance {};
        bool did_find_sky = false;
        bool did_warn_multiple_skies = false;
        registry.for_each_with<Sky>(
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

        if (!did_find_sky)
        {
            _is_sky_cache_valid = true;
            _cached_has_sky_component = false;
            _cached_sky_source_material_hash = 0U;
            _cached_sky_material = nullptr;
            _cached_resolved_sky = {};
        }
        else
        {
            const auto sky_material_hash = hash_runtime_material(sky_material);
            if (
                !_is_sky_cache_valid || !_cached_has_sky_component
                || _cached_sky_source_material_hash != sky_material_hash)
            {
                _cached_has_sky_component = true;
                _cached_sky_source_material_hash = sky_material_hash;
                _cached_sky_material = nullptr;
                _cached_resolved_sky = {};

                TBX_ASSERT(
                    sky_material.handle.is_valid(),
                    "OpenGL rendering: Sky component requires a valid material handle.");
                if (sky_material.handle.is_valid())
                {
                    auto& asset_manager = get_host().get_asset_manager();
                    _cached_sky_material = asset_manager.load<Material>(sky_material.handle);
                    TBX_ASSERT(
                        _cached_sky_material != nullptr,
                        "OpenGL rendering: Sky material could not be loaded.");
                    if (_cached_sky_material)
                    {
                        auto resolved_sky_material = *_cached_sky_material;
                        apply_runtime_material_overrides(sky_material, resolved_sky_material);

                        TBX_ASSERT(
                            is_valid_sky_shader_program(resolved_sky_material.program),
                            "OpenGL rendering: Sky material must use a graphics shader program "
                            "with vertex+fragment stages and no compute stage.");

                        if (is_valid_sky_shader_program(resolved_sky_material.program))
                        {
                            if (has_sky_texture(resolved_sky_material))
                            {
                                _cached_resolved_sky.sky_material = sky_material;
                            }
                            else
                            {
                                RgbaColor material_color = RgbaColor::black;
                                if (try_get_sky_color(resolved_sky_material, material_color))
                                    _cached_resolved_sky.clear_color = material_color;
                            }
                        }
                    }
                }

                _is_sky_cache_valid = true;
            }
        }

        bool did_find_post_processing = false;
        bool did_warn_multiple_post_processing = false;
        bool is_post_processing_enabled = false;
        _cached_resolved_post_processing.effects.clear();
        registry.for_each_with<PostProcessing>(
            [this,
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

                _cached_resolved_post_processing.effects.reserve(post_processing.effects.size());
                for (const auto& effect : post_processing.effects)
                {
                    _cached_resolved_post_processing.effects.push_back(
                        OpenGlPostProcessEffect {
                            .is_enabled = effect.is_enabled,
                            .material = effect.material,
                            .blend = effect.blend,
                        });
                }
            });

        const auto post_process_settings = OpenGlPostProcessSettings {
            .is_enabled = did_find_post_processing && is_post_processing_enabled,
            .effects =
                std::span<const OpenGlPostProcessEffect>(_cached_resolved_post_processing.effects),
        };

        auto frame_context = OpenGlRenderFrameContext {
            .camera_view = camera_view,
            .render_resolution = _render_resolution,
            .viewport_size = _viewport_size,
            .clear_color = _cached_resolved_sky.clear_color,
            .sky_material = _cached_resolved_sky.sky_material,
            .post_process = post_process_settings,
            .gbuffer_target = &_gbuffer_framebuffer,
            .lighting_target = &_lighting_framebuffer,
            .post_process_ping_target = &_post_process_ping_framebuffer,
            .post_process_pong_target = &_post_process_pong_framebuffer,
            .present_mode = OpenGlFrameBufferPresentMode::ASPECT_FIT,
            .present_target_framebuffer_id = 0,
        };

        _render_pipeline->execute(frame_context);
        auto present_result = send_message<WindowPresentRequest>(_window_id);
        if (!present_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: present request failed: {}",
                present_result.get_report());
        }
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            _window_id = ready_event->window;
            if (!_render_pipeline)
                _render_pipeline =
                    std::make_unique<OpenGlRenderPipeline>(get_host().get_asset_manager());

            auto* loader = reinterpret_cast<GLADloadproc>(ready_event->get_proc_address);
            TBX_ASSERT(
                loader != nullptr,
                "OpenGL rendering: context-ready event provided null loader.");
            const auto load_result = gladLoadGLLoader(loader);
            TBX_ASSERT(load_result != 0, "OpenGL rendering: failed to initialize GLAD.");

            initialize_opengl();
            set_viewport_size(ready_event->size);
            set_render_resolution(ready_event->size);
            _is_context_ready = true;
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            if (size_event->owner && size_event->owner->id == _window_id)
            {
                set_viewport_size(size_event->current);
                set_render_resolution(size_event->current);
            }
            return;
        }
    }

    void OpenGlRenderingPlugin::initialize_opengl() const
    {
        TBX_TRACE_INFO("OpenGL rendering: initializing OpenGL.");
        TBX_TRACE_INFO("OpenGL rendering: OpenGL info:");

        const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        TBX_TRACE_INFO("  Vendor: {}", vendor ? vendor : "Unknown");
        TBX_TRACE_INFO("  Renderer: {}", renderer ? renderer : "Unknown");
        TBX_TRACE_INFO("  Version: {}", version ? version : "Unknown");
        TBX_TRACE_INFO("  GLSL: {}", glsl ? glsl : "Unknown");

        TBX_ASSERT(
            GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5),
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

    void OpenGlRenderingPlugin::set_viewport_size(const Size& viewport_size)
    {
        if (viewport_size.width == 0 || viewport_size.height == 0)
            return;

        _viewport_size = viewport_size;
    }

    void OpenGlRenderingPlugin::set_render_resolution(const Size& render_resolution)
    {
        if (render_resolution.width == 0 || render_resolution.height == 0)
            return;

        _render_resolution = render_resolution;
        _gbuffer_framebuffer.set_resolution(render_resolution);
        _lighting_framebuffer.set_resolution(render_resolution);
        _post_process_ping_framebuffer.set_resolution(render_resolution);
        _post_process_pong_framebuffer.set_resolution(render_resolution);
    }
}
