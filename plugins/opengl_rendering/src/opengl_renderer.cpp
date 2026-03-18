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
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/matrices.h"
#include <algorithm>
#include <glad/glad.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace opengl_rendering
{
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

    static std::shared_ptr<tbx::Material> try_get_material_defaults(
        const tbx::Handle& material_handle,
        tbx::AssetManager& asset_manager)
    {
        auto material_asset = asset_manager.load<tbx::Material>(material_handle);
        return material_asset;
    }

    static void hydrate_material_instance_defaults(
        tbx::MaterialInstance& material_instance,
        tbx::AssetManager& asset_manager)
    {
        if (!material_instance.has_loaded_defaults)
        {
            const auto defaults =
                try_get_material_defaults(material_instance.handle, asset_manager);

            for (const auto& [name, value] : defaults->parameters.values)
            {
                if (material_instance.parameters.get(name) != nullptr)
                    continue;
                material_instance.parameters.set(name, value);
            }

            for (const auto& [name, texture] : defaults->textures.values)
            {
                if (material_instance.textures.get(name) != nullptr)
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
        OpenGlContext context)
        : _context(std::move(context))
        , _entity_registry(entity_registry)
        , _asset_manager(asset_manager)
        , _resource_manager(asset_manager)
        , _render_pipeline(std::make_unique<OpenGlRenderPipeline>(_resource_manager))
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

        // Step four: cache resources and build draw calls in one pass.
        build_draw_calls(frame_context);

        // Step five: execute render pipeline.
        {
            auto gbuffer_scope = OpenGlResourceScope(_gbuffer);
            _render_pipeline->execute(frame_context);
        }
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
        frame_context.clear_color = tbx::Color::BLACK;
        frame_context.view_projection = tbx::Mat4(1.0F);
        frame_context.render_stage = _render_stage;

        if (const auto cameras = _entity_registry.get_with<tbx::Camera, tbx::Transform>();
            !cameras.empty())
        {
            const auto& camera_entity = cameras.front();
            auto& camera = camera_entity.get_component<tbx::Camera>();
            const auto camera_transform = tbx::get_world_space_transform(camera_entity);
            if (_viewport_size.width > 0 && _viewport_size.height > 0)
            {
                const auto aspect = static_cast<float>(_viewport_size.width)
                                    / static_cast<float>(_viewport_size.height);
                camera.set_aspect(aspect);
            }
            frame_context.view_projection = camera.get_view_projection_matrix(
                camera_transform.position,
                camera_transform.rotation);
        }
        return frame_context;
    }

    void OpenGlRenderer::build_draw_calls(OpenGlFrameContext& frame_context)
    {
        const auto entities = _entity_registry.get_with<tbx::Renderer>();
        frame_context.draw_calls.reserve(entities.size());

        auto draw_call_lookup = std::unordered_map<tbx::Uuid, std::size_t>();
        draw_call_lookup.reserve(entities.size());

        for (const auto& entity : entities)
        {
            auto& renderer = entity.get_component<tbx::Renderer>();
            auto& material_instance = renderer.material;
            if (!material_instance.handle.is_valid())
                material_instance.handle = tbx::default_material;
            hydrate_material_instance_defaults(material_instance, _asset_manager);

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
                const auto& static_mesh = entity.get_component<tbx::StaticMesh>();
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
                    texture_id = get_fallback_texture(_resource_manager);

                if (!texture_id.is_valid())
                {
                    TBX_TRACE_WARNING(
                        "Failed to cache texture '{}' for material '{}'. "
                        "Using fallback texture.",
                        texture.handle.id.value,
                        material_instance.handle.id.value);
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
                            .name = "diffuse",
                            .texture_id = fallback_texture_id,
                            .gl_texture_id = fallback_texture_resource->get_texture_id(),
                            .bindless_handle = fallback_texture_resource->get_bindless_handle(),
                        });
            }

            // Add transform matrix
            auto transform_matrix = tbx::Mat4(1.0F);
            if (entity.has_component<tbx::Transform>())
            {
                const auto world_transform = tbx::get_world_space_transform(entity);
                transform_matrix = tbx::build_transform_matrix(world_transform);
            }

            // Push back new draw call
            auto draw_call_it = draw_call_lookup.find(shader_program_key);
            if (draw_call_it == draw_call_lookup.end())
            {
                frame_context.draw_calls.emplace_back(shader_program_key);
                frame_context.draw_calls.back().meshes.reserve(8U);
                frame_context.draw_calls.back().materials.reserve(8U);
                frame_context.draw_calls.back().transforms.reserve(8U);
                draw_call_it =
                    draw_call_lookup
                        .emplace(shader_program_key, frame_context.draw_calls.size() - 1U)
                        .first;
            }

            // Update draw call with new mesh
            auto& draw_call = frame_context.draw_calls[draw_call_it->second];
            draw_call.meshes.push_back(mesh_key);
            draw_call.materials.push_back(std::move(material_params));
            draw_call.transforms.push_back(transform_matrix);
        }
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
        glEnable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
