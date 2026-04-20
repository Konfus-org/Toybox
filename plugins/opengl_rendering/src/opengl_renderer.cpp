#include "opengl_renderer.h"
#include "opengl_fallbacks.h"
#include "opengl_resources/opengl_bindless.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_shader.h"
#include "opengl_resources/opengl_texture.h"
#include "passes/geometry_pass.h"
#include "passes/lighting_pass.h"
#include "passes/open_gl_draw_calls.h"
#include "passes/post_processing_pass.h"
#include "passes/render_pipeline_failure.h"
#include "passes/shadow_pass.h"
#include "passes/transparent_pass.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <cstdint>
#include <type_traits>
#include <unordered_map>

namespace opengl_rendering
{
    namespace
    {
        void gl_message_callback(
            GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            const GLsizei length,
            const GLchar* message,
            const void*)
        {
            switch (severity)
            {
                case GL_DEBUG_SEVERITY_HIGH:
                case GL_DEBUG_SEVERITY_MEDIUM:
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

        bool should_warn_about_integrated_or_software_gpu(
            const std::string_view vendor_text,
            const std::string_view renderer_text)
        {
            const auto is_software_renderer =
                tbx::contains_case_insensitive(renderer_text, "llvmpipe")
                || tbx::contains_case_insensitive(renderer_text, "softpipe")
                || tbx::contains_case_insensitive(renderer_text, "software rasterizer");
            if (is_software_renderer)
                return true;

            const auto is_intel_vendor = tbx::contains_case_insensitive(vendor_text, "intel");
            const auto is_discrete_arc = tbx::contains_case_insensitive(renderer_text, "arc");
            if (is_intel_vendor && !is_discrete_arc)
                return true;

            return tbx::contains_case_insensitive(renderer_text, "integrated");
        }

        void maybe_warn_about_gpu_selection(
            const std::string_view vendor_text,
            const std::string_view renderer_text)
        {
            if (!should_warn_about_integrated_or_software_gpu(vendor_text, renderer_text))
                return;

            TBX_TRACE_WARNING(
                "OpenGL rendering appears to be running on an integrated/software GPU "
                "(vendor='{}', renderer='{}'). If a discrete GPU is available, prefer launching "
                "this app on the high-performance GPU. This can reduce performance and may affect "
                "shadow quality or availability on some drivers.",
                vendor_text,
                renderer_text);
        }

        tbx::Uuid resolve_shader_program_for_material(
            const tbx::Uuid& material_resource,
            OpenGlResources& resources,
            bool& use_fallback_material_params)
        {
            use_fallback_material_params = false;
            auto shader_program_key = material_resource;
            if (shader_program_key.is_valid())
                return shader_program_key;

            shader_program_key = get_fallback_material(resources);
            use_fallback_material_params = true;
            if (shader_program_key.is_valid())
                return shader_program_key;

            TBX_ASSERT(false, "Fallback magenta material program is unavailable.");
            return {};
        }

        tbx::Uuid get_default_texture_for_binding(
            std::string_view binding_name,
            OpenGlResources& resources)
        {
            if (binding_name == "u_normal_map")
                return get_flat_normal_texture(resources);
            if (binding_name == "u_specular_map")
                return get_fallback_texture(resources);
            if (binding_name == "u_shininess_map")
                return get_fallback_texture(resources);
            if (binding_name == "u_emissive_map")
                return get_fallback_texture(resources);
            return get_fallback_texture(resources);
        }

        bool try_append_material_texture(
            OpenGlMaterialParams& material_params,
            const std::string& binding_name,
            const tbx::TextureInstance& texture_instance,
            OpenGlResources& resources)
        {
            auto texture_id = tbx::Uuid {};
            if (texture_instance.handle.is_valid())
                texture_id = texture_instance.handle.get_id();
            else
                texture_id = get_default_texture_for_binding(binding_name, resources);

            if (!texture_id.is_valid())
                texture_id = get_default_texture_for_binding(binding_name, resources);

            if (!texture_id.is_valid())
                return false;

            auto texture_resource = std::shared_ptr<OpenGlTexture> {};
            if (!resources.try_get<OpenGlTexture>(texture_id, texture_resource))
            {
                TBX_TRACE_WARNING(
                    "Failed to fetch texture resource '{}'. Using fallback texture.",
                    texture_id.value);
                return false;
            }

            material_params.textures.push_back(
                OpenGlMaterialTexture {
                    .name = binding_name,
                    .texture_id = texture_id,
                    .gl_texture_id = texture_resource->get_texture_id(),
                    .bindless_handle = texture_resource->get_bindless_handle(),
                });
            return true;
        }

        void append_default_material_texture_if_needed(
            OpenGlMaterialParams& material_params,
            OpenGlResources& resources)
        {
            if (!material_params.textures.empty())
                return;

            const auto fallback_texture_id = get_fallback_texture(resources);
            auto fallback_texture_resource = std::shared_ptr<OpenGlTexture> {};
            if (!fallback_texture_id.is_valid()
                || !resources.try_get<OpenGlTexture>(fallback_texture_id, fallback_texture_resource))
            {
                return;
            }

            material_params.textures.push_back(
                OpenGlMaterialTexture {
                    .name = "diffuse_map",
                    .texture_id = fallback_texture_id,
                    .gl_texture_id = fallback_texture_resource->get_texture_id(),
                    .bindless_handle = fallback_texture_resource->get_bindless_handle(),
                });
        }

        OpenGlMaterialParams build_material_params(
            const tbx::Uuid& material_resource,
            const tbx::MaterialConfig& material_config,
            const tbx::ParamBindings& material_parameters,
            const tbx::TextureBindings& material_textures,
            const bool use_fallback_material_params,
            OpenGlResources& resources)
        {
            auto material_params = OpenGlMaterialParams {};
            if (use_fallback_material_params)
            {
                material_params = create_magenta_fallback_material_params(tbx::Handle(material_resource));
            }
            else
            {
                material_params.material_handle = tbx::Handle(material_resource);
                material_params.parameters.reserve(material_parameters.values.size());
                for (const auto& [name, value] : material_parameters.values)
                    material_params.parameters.push_back(tbx::MaterialParameter(name, value));
            }

            material_params.render_config = tbx::MaterialRenderConfig {
                .depth = material_config.depth,
                .transparency = material_config.transparency,
            };

            material_params.textures.reserve(material_textures.values.size());
            for (const auto& binding : material_textures.values)
            {
                try_append_material_texture(
                    material_params,
                    binding.name,
                    binding.texture,
                    resources);
            }

            append_default_material_texture_if_needed(material_params, resources);
            return material_params;
        }

        void append_shadow_draw_call(
            std::vector<ShadowDrawCall>& shadow_draw_calls,
            std::unordered_map<std::uint64_t, std::size_t>& shadow_draw_call_lookup,
            const bool is_two_sided,
            const tbx::Uuid& mesh_key,
            const tbx::Mat4& transform_matrix,
            const tbx::Vec3& bounds_center,
            const float bounds_radius)
        {
            const auto shadow_draw_call_key = is_two_sided ? 1ULL : 0ULL;
            auto shadow_draw_call_it = shadow_draw_call_lookup.find(shadow_draw_call_key);
            if (shadow_draw_call_it == shadow_draw_call_lookup.end())
            {
                shadow_draw_calls.push_back(ShadowDrawCall {.is_two_sided = is_two_sided});
                shadow_draw_call_it =
                    shadow_draw_call_lookup.emplace(shadow_draw_call_key, shadow_draw_calls.size() - 1U)
                        .first;
            }

            auto& shadow_draw_call = shadow_draw_calls[shadow_draw_call_it->second];
            shadow_draw_call.meshes.push_back(mesh_key);
            shadow_draw_call.transforms.push_back(transform_matrix);
            shadow_draw_call.bounds_centers.push_back(bounds_center);
            shadow_draw_call.bounds_radii.push_back(bounds_radius);
        }

        void append_visible_draw_call(
            std::vector<DrawCall>& draw_calls,
            std::vector<TransparentDrawCall>& transparent_draw_calls,
            std::unordered_map<std::uint64_t, std::size_t>& draw_call_lookup,
            const tbx::Uuid& shader_program_key,
            const bool is_two_sided,
            const tbx::MaterialBlendMode blend_mode,
            const tbx::Uuid& mesh_key,
            OpenGlMaterialParams&& material_params,
            const tbx::Mat4& transform_matrix,
            const float camera_distance_squared)
        {
            if (blend_mode == tbx::MaterialBlendMode::AlphaBlend)
            {
                transparent_draw_calls.push_back(
                    TransparentDrawCall {
                        .shader_program = shader_program_key,
                        .is_two_sided = is_two_sided,
                        .mesh = mesh_key,
                        .material = std::move(material_params),
                        .transform = transform_matrix,
                        .camera_distance_squared = camera_distance_squared,
                    });
                return;
            }

            const auto draw_call_key =
                (static_cast<std::uint64_t>(shader_program_key.value) << 1U)
                | (is_two_sided ? 1ULL : 0ULL);
            auto draw_call_it = draw_call_lookup.find(draw_call_key);
            if (draw_call_it == draw_call_lookup.end())
            {
                draw_calls.emplace_back(shader_program_key, is_two_sided);
                draw_calls.back().meshes.reserve(8U);
                draw_calls.back().materials.reserve(8U);
                draw_calls.back().transforms.reserve(8U);
                draw_call_it = draw_call_lookup.emplace(draw_call_key, draw_calls.size() - 1U).first;
            }

            auto& draw_call = draw_calls[draw_call_it->second];
            draw_call.meshes.push_back(mesh_key);
            draw_call.materials.push_back(std::move(material_params));
            draw_call.transforms.push_back(transform_matrix);
        }

        void render_magenta_failure_frame(OpenGlGBuffer& gbuffer)
        {
            auto gbuffer_scope = OpenGlResourceScope(gbuffer);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glClearColor(1.0F, 0.0F, 1.0F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    struct OpenGlWindowRendererState
    {
        OpenGlWindowRendererState(
            OpenGlResources& resources,
            tbx::JobSystem& job_system,
            const tbx::Size& initial_render_size)
            : shadow_pass(std::make_unique<ShadowPass>(resources))
            , geometry_pass(std::make_unique<GeometryPass>(resources))
            , lighting_pass(
                  std::make_unique<LightingPass>(resources, job_system, gbuffer, *shadow_pass))
            , transparent_pass(std::make_unique<TransparentPass>(resources, gbuffer))
            , post_processing_pass(std::make_unique<PostProcessingPass>(resources, gbuffer))
        {
            render_size = initial_render_size;
        }

        OpenGlGBuffer gbuffer = {};
        std::unique_ptr<ShadowPass> shadow_pass = nullptr;
        std::unique_ptr<GeometryPass> geometry_pass = nullptr;
        std::unique_ptr<LightingPass> lighting_pass = nullptr;
        std::unique_ptr<TransparentPass> transparent_pass = nullptr;
        std::unique_ptr<PostProcessingPass> post_processing_pass = nullptr;
        tbx::Color clear_color = tbx::Color::BLACK;
        tbx::Size render_size = {0U, 0U};
        tbx::RenderStage render_stage = tbx::RenderStage::FINAL_COLOR;
        bool has_camera = false;
        std::vector<DrawCall> draw_calls = {};
        std::vector<ShadowDrawCall> shadow_draw_calls = {};
        std::vector<TransparentDrawCall> transparent_draw_calls = {};
        bool has_reported_pipeline_failure = false;
    };

    OpenGlRenderer::OpenGlRenderer(tbx::AssetManager& asset_manager, tbx::JobSystem& job_system)
        : _asset_manager(asset_manager)
        , _job_system(job_system)
        , _resources(_asset_manager)
    {
    }

    OpenGlRenderer::~OpenGlRenderer() noexcept = default;

    tbx::GraphicsApi OpenGlRenderer::get_api() const
    {
        return tbx::GraphicsApi::OPEN_GL;
    }

    tbx::Result OpenGlRenderer::initialize(const tbx::GraphicsProcAddress loader)
    {
        initialize_runtime(loader);
        return {};
    }

    tbx::Result OpenGlRenderer::upload(const tbx::Mesh& mesh, tbx::Uuid& out_resource_uuid)
    {
        auto resource_uuid = out_resource_uuid;
        if (!resource_uuid.is_valid())
            resource_uuid = tbx::Uuid::generate();
        const auto uploaded_resource_uuid = _resources.upload_mesh(mesh, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload mesh resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::upload(const tbx::Material& material, tbx::Uuid& out_resource_uuid)
    {
        auto shader_resources = std::vector<std::shared_ptr<OpenGlShader>> {};
        auto try_append_shader = [this, &shader_resources](const tbx::Handle& shader_handle)
        {
            if (!shader_handle.is_valid())
                return true;

            const auto shader = _asset_manager.load<tbx::Shader>(shader_handle);
            if (!shader)
                return false;

            for (const auto& source : shader->sources)
            {
                auto shader_resource = std::make_shared<OpenGlShader>(source);
                if (!shader_resource->compile())
                    return false;
                shader_resources.emplace_back(std::move(shader_resource));
            }

            return !shader->sources.empty();
        };

        const auto has_compute = material.program.compute.is_valid();
        const auto appended_compute = try_append_shader(material.program.compute);
        const auto appended_vertex = has_compute ? true : try_append_shader(material.program.vertex);
        const auto appended_fragment = has_compute ? true : try_append_shader(material.program.fragment);
        const auto appended_tesselation =
            has_compute ? true : try_append_shader(material.program.tesselation);
        const auto appended_geometry =
            has_compute ? true : try_append_shader(material.program.geometry);
        const auto has_valid_shader_set =
            (has_compute && appended_compute)
            || (!has_compute && appended_vertex && appended_fragment && appended_tesselation
                && appended_geometry);
        if (!has_valid_shader_set)
            return tbx::Result(false, "OpenGL rendering: failed to compile material shader set.");

        const auto shader_program = std::make_shared<OpenGlShaderProgram>(shader_resources);
        if (shader_program->get_program_id() == 0)
            return tbx::Result(false, "OpenGL rendering: failed to link shader program.");

        auto resource_uuid = out_resource_uuid;
        if (!resource_uuid.is_valid())
            resource_uuid = tbx::Uuid::generate();
        const auto uploaded_resource_uuid =
            _resources.upload_material(shader_program, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload material resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::upload(const tbx::Texture& texture, tbx::Uuid& out_resource_uuid)
    {
        const auto resource_uuid = out_resource_uuid.is_valid() ? out_resource_uuid : tbx::Uuid::generate();
        const auto uploaded_resource_uuid = _resources.upload_texture(texture, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload texture resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::upload(
        const tbx::TextureSettings& texture_settings,
        tbx::Uuid& out_resource_uuid)
    {
        const auto resource_uuid = out_resource_uuid.is_valid() ? out_resource_uuid : tbx::Uuid::generate();
        auto texture = tbx::Texture {};
        static_cast<tbx::TextureSettings&>(texture) = texture_settings;
        const auto uploaded_resource_uuid = _resources.upload_texture(texture, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload render texture resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::unload(const tbx::Uuid& resource_uuid)
    {
        _resources.unload(resource_uuid);
        return {};
    }

    tbx::Result OpenGlRenderer::begin_draw(
        const tbx::Window& window,
        const tbx::Camera& view,
        const tbx::Size& resolution,
        const tbx::Color& clear_color)
    {
        static_cast<void>(window);
        static_cast<void>(view);
        auto& state = ensure_state();
        state.clear_color = clear_color;
        state.render_size = resolution;
        state.render_stage = tbx::RenderStage::FINAL_COLOR;
        state.has_camera = false;
        state.draw_calls.clear();
        state.shadow_draw_calls.clear();
        state.transparent_draw_calls.clear();

        glViewport(
            0,
            0,
            static_cast<GLsizei>(resolution.width),
            static_cast<GLsizei>(resolution.height));
        state.gbuffer.resize(resolution);
        clear_render_pipeline_failure();
        return {};
    }

    tbx::Result OpenGlRenderer::draw_shadows(const tbx::ShadowRenderInfo& shadows)
    {
        auto* state = try_get_active_state();
        if (!state)
            return tbx::Result(false, "OpenGL rendering: missing renderer state.");

        build_shadow_draw_calls(*state, shadows.draw_items);
        state->shadow_pass->draw(shadows, state->shadow_draw_calls);
        return {};
    }

    tbx::Result OpenGlRenderer::draw_geometry(const tbx::GeometryRenderInfo& geo)
    {
        auto* state = try_get_active_state();
        if (!state)
            return tbx::Result(false, "OpenGL rendering: missing renderer state.");

        build_geometry_draw_calls(*state, geo.draw_items);
        state->gbuffer.prepare_geometry_pass();
        state->geometry_pass->draw(state->clear_color, geo.view_projection, state->draw_calls);
        return {};
    }

    tbx::Result OpenGlRenderer::draw_lighting(const tbx::LightingRenderInfo& lighting)
    {
        auto* state = try_get_active_state();
        if (!state)
            return tbx::Result(false, "OpenGL rendering: missing renderer state.");

        state->has_camera = lighting.has_camera;
        state->render_stage = lighting.render_stage;
        state->lighting_pass->draw(state->clear_color, state->render_size, lighting);
        return {};
    }

    tbx::Result OpenGlRenderer::draw_transparent(const tbx::TransparentRenderInfo& transparency)
    {
        auto* state = try_get_active_state();
        if (!state)
            return tbx::Result(false, "OpenGL rendering: missing renderer state.");

        build_transparent_draw_calls(*state, transparency.draw_items);
        state->transparent_pass->draw(
            transparency.view_projection,
            state->transparent_draw_calls);
        return {};
    }

    tbx::Result OpenGlRenderer::apply_post_processing(const tbx::PostProcessingPass& post)
    {
        auto* state = try_get_active_state();
        if (!state)
            return tbx::Result(false, "OpenGL rendering: missing renderer state.");

        state->post_processing_pass->draw(state->render_size, post.post_processing);
        return {};
    }

    tbx::Result OpenGlRenderer::end_draw()
    {
        auto* state = try_get_active_state();
        if (!state)
            return tbx::Result(false, "OpenGL rendering: missing renderer state.");

        if (!state->has_camera)
        {
            render_magenta_failure_frame(state->gbuffer);
        }
        else if (has_render_pipeline_failure())
        {
            if (!state->has_reported_pipeline_failure)
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: one or more render passes failed without producing a usable "
                    "frame. Rendering magenta fallback frame.");
                state->has_reported_pipeline_failure = true;
            }
            render_magenta_failure_frame(state->gbuffer);
        }
        else
        {
            state->has_reported_pipeline_failure = false;
        }

        state->gbuffer.present(state->render_stage, state->render_size);
        state->draw_calls.clear();
        state->shadow_draw_calls.clear();
        state->transparent_draw_calls.clear();
        return {};
    }

    OpenGlWindowRendererState& OpenGlRenderer::ensure_state()
    {
        if (_state)
            return *_state;

        _state = std::make_shared<OpenGlWindowRendererState>(
            _resources,
            _job_system,
            tbx::Size {0U, 0U});
        return *_state;
    }

    OpenGlWindowRendererState* OpenGlRenderer::try_get_active_state()
    {
        return _state.get();
    }

    void OpenGlRenderer::build_geometry_draw_calls(
        OpenGlWindowRendererState& state,
        const std::vector<tbx::RenderDrawItem>& draw_items)
    {
        state.draw_calls.clear();
        state.draw_calls.reserve(draw_items.size());

        auto draw_call_lookup = std::unordered_map<std::uint64_t, std::size_t> {};
        draw_call_lookup.reserve(draw_items.size());

        for (const auto& draw_item : draw_items)
        {
            auto use_fallback_material_params = false;
            const auto shader_program_key = resolve_shader_program_for_material(
                draw_item.material_resource,
                _resources,
                use_fallback_material_params);
            if (!shader_program_key.is_valid())
                continue;

            const auto mesh_key = draw_item.mesh_resource;
            if (!mesh_key.is_valid())
                continue;

            auto material_params = build_material_params(
                draw_item.material_resource,
                draw_item.material_config,
                draw_item.material_parameters,
                draw_item.material_textures,
                use_fallback_material_params,
                _resources);

            append_visible_draw_call(
                state.draw_calls,
                state.transparent_draw_calls,
                draw_call_lookup,
                shader_program_key,
                draw_item.material_config.is_two_sided,
                draw_item.material_config.transparency.blend_mode,
                mesh_key,
                std::move(material_params),
                draw_item.transform,
                draw_item.camera_distance_squared);
        }
    }

    void OpenGlRenderer::build_shadow_draw_calls(
        OpenGlWindowRendererState& state,
        const std::vector<tbx::RenderShadowItem>& draw_items)
    {
        state.shadow_draw_calls.clear();
        state.shadow_draw_calls.reserve(draw_items.size());

        auto shadow_draw_call_lookup = std::unordered_map<std::uint64_t, std::size_t> {};
        shadow_draw_call_lookup.reserve(draw_items.size());

        for (const auto& draw_item : draw_items)
        {
            const auto mesh_key = draw_item.mesh_resource;
            if (!mesh_key.is_valid())
                continue;

            append_shadow_draw_call(
                state.shadow_draw_calls,
                shadow_draw_call_lookup,
                draw_item.is_two_sided,
                mesh_key,
                draw_item.transform,
                tbx::Vec3(draw_item.transform[3]),
                draw_item.bounds_radius);
        }
    }

    void OpenGlRenderer::build_transparent_draw_calls(
        OpenGlWindowRendererState& state,
        const std::vector<tbx::RenderDrawItem>& draw_items)
    {
        state.transparent_draw_calls.clear();
        state.transparent_draw_calls.reserve(draw_items.size());

        auto draw_call_lookup = std::unordered_map<std::uint64_t, std::size_t> {};
        for (const auto& draw_item : draw_items)
        {
            auto use_fallback_material_params = false;
            const auto shader_program_key = resolve_shader_program_for_material(
                draw_item.material_resource,
                _resources,
                use_fallback_material_params);
            if (!shader_program_key.is_valid())
                continue;

            const auto mesh_key = draw_item.mesh_resource;
            if (!mesh_key.is_valid())
                continue;

            auto material_params = build_material_params(
                draw_item.material_resource,
                draw_item.material_config,
                draw_item.material_parameters,
                draw_item.material_textures,
                use_fallback_material_params,
                _resources);

            append_visible_draw_call(
                state.draw_calls,
                state.transparent_draw_calls,
                draw_call_lookup,
                shader_program_key,
                draw_item.material_config.is_two_sided,
                tbx::MaterialBlendMode::AlphaBlend,
                mesh_key,
                std::move(material_params),
                draw_item.transform,
                draw_item.camera_distance_squared);
        }
    }

    void OpenGlRenderer::initialize_runtime(const tbx::GraphicsProcAddress loader)
    {
        if (_is_runtime_initialized)
            return;

        auto* glad_loader = loader;
        TBX_ASSERT(glad_loader != nullptr, "Context manager provided null loader.");

        const auto load_result = gladLoadGLLoader(glad_loader);
        TBX_ASSERT(load_result != 0, "Failed to initialize GLAD.");
        set_bindless_proc_loader(loader);

        const auto major_version = GLVersion.major;
        const auto minor_version = GLVersion.minor;
        TBX_ASSERT(
            major_version > 4 || (major_version == 4 && minor_version >= 5),
            "Requires OpenGL 4.5 or newer.");

        const auto* vendor_string = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* renderer_string = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* version_string = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glsl_version_string =
            reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
        const auto* vendor_text = vendor_string != nullptr ? vendor_string : "unknown";
        const auto* renderer_text = renderer_string != nullptr ? renderer_string : "unknown";
        TBX_TRACE_INFO(
            "OpenGL runtime info: vendor='{}', renderer='{}', version='{}', GLSL='{}'.",
            vendor_text,
            renderer_text,
            version_string != nullptr ? version_string : "unknown",
            glsl_version_string != nullptr ? glsl_version_string : "unknown");
        maybe_warn_about_gpu_selection(vendor_text, renderer_text);

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
        glClearColor(0.07F, 0.08F, 0.11F, 1.0F);

        _is_runtime_initialized = true;
    }
}
