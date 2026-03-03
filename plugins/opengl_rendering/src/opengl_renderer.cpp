#include "opengl_renderer.h"
#include "builtin_assets.generated.h"
#include "pipeline/OpenGlFrameContext.h"
#include "pipeline/opengl_render_pipeline.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/matrices.h"
#include <algorithm>
#include <glad/glad.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace opengl_rendering
{
    constexpr tbx::uint32 TextureSalt = 0x7EAE0001U;

    static tbx::Uuid make_typed_key(const tbx::Uuid& base_key, const tbx::uint32 salt)
    {
        if (!base_key.is_valid())
            return {};

        auto typed_key = tbx::Uuid(base_key);
        typed_key.combine(salt);
        if (!typed_key.is_valid())
            return tbx::Uuid(1U);
        return typed_key;
    }

    static tbx::Uuid make_texture_key(const tbx::Handle& texture_handle)
    {
        return make_typed_key(texture_handle.id, TextureSalt);
    }

    void gl_message_callback(
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

    OpenGlRenderer::OpenGlRenderer(
        const tbx::GraphicsProcAddress loader,
        tbx::EntityRegistry& entity_registry,
        tbx::AssetManager& asset_manager,
        OpenGlContext context)
        : _context(std::move(context))
        , _entity_registry(entity_registry)
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

        // Step two: set viewport
        glViewport(0, 0, _viewport_size.width, _viewport_size.height);

        // Step three: cache resources and build draw calls in one pass.
        const auto entities = _entity_registry.get_with<tbx::Renderer>();
        auto frame_context = OpenGlFrameContext();
        auto draw_call_lookup = std::unordered_map<tbx::Uuid, std::size_t>();
        auto cached_resource = std::shared_ptr<IOpenGlResource>();

        for (const auto& entity : entities)
        {
            const auto& renderer = entity.get_component<tbx::Renderer>();
            auto material = renderer.material;
            if (!material.handle.is_valid())
                material.handle = tbx::default_material;

            const auto shader_program_key = _resource_manager.add_material(material);
            if (!shader_program_key.is_valid())
                continue;
            if (!_resource_manager.try_get(shader_program_key, cached_resource))
                continue;

            auto mesh_key = tbx::Uuid {};
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
                continue;
            if (!_resource_manager.try_get(mesh_key, cached_resource))
                continue;

            auto material_params = OpenGlMaterialParams();
            material_params.parameters.reserve(renderer.material.parameters.values.size());
            for (const auto& [name, value] : renderer.material.parameters.values)
            {
                material_params.parameters.push_back(
                    tbx::MaterialParameter {.name = name, .data = value});
            }

            material_params.textures.reserve(renderer.material.textures.values.size());
            for (const auto& [name, texture] : renderer.material.textures.values)
            {
                const auto texture_id = make_texture_key(texture.handle);
                if (!texture_id.is_valid())
                    continue;
                if (!_resource_manager.try_get(texture_id, cached_resource))
                    continue;

                material_params.textures.push_back(
                    OpenGlMaterialTexture {.name = name, .texture_id = texture_id});
            }

            auto transform_matrix = tbx::Mat4(1.0F);
            if (entity.has_component<tbx::Transform>())
            {
                const auto world_transform = tbx::get_world_space_transform(entity);
                transform_matrix = tbx::build_transform_matrix(world_transform);
            }

            auto draw_call_it = draw_call_lookup.find(shader_program_key);
            if (draw_call_it == draw_call_lookup.end())
            {
                frame_context.draw_calls.push_back(DrawCall {.shader_program = shader_program_key});
                draw_call_it =
                    draw_call_lookup
                        .emplace(shader_program_key, frame_context.draw_calls.size() - 1U)
                        .first;
            }

            auto& draw_call = frame_context.draw_calls[draw_call_it->second];
            draw_call.meshes.push_back(mesh_key);
            draw_call.materials.push_back(std::move(material_params));
            draw_call.transforms.push_back(transform_matrix);
        }

        // Step four: execute render pipeline.
        _render_pipeline->execute(frame_context);

        // Step five: present rendered frame.
        if (const auto present_result = _context.present(); !present_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: present request failed: {}",
                present_result.get_report());
        }

        return true;
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
