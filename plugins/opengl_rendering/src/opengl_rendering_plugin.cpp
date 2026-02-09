#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/light.h"
#include "tbx/math/matrices.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include "tbx/math/vectors.h"
#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <utility>
#include <vector>

namespace tbx::plugins
{
    static constexpr int MAX_LIGHTS = 16;
    static constexpr float MIN_LIGHT_RANGE = 0.01f;
    static constexpr float MAX_SPOT_ANGLE = 179.0f;

    struct MeshDrawRequest
    {
        Uuid mesh_id = {};
        const OpenGlMaterial* material = nullptr;
        Mat4 model_matrix = Mat4(1.0f);
    };

    struct ModelMeshInstance
    {
        Uuid mesh_id = {};
        Uuid material_id = {};
        Mat4 model_matrix = Mat4(1.0f);
    };

    static void append_model_mesh_instances(
        const OpenGlModel& model,
        const Mat4& entity_matrix,
        std::vector<ModelMeshInstance>& instances)
    {
        if (model.parts.empty())
        {
            for (const auto& mesh_id : model.meshes)
            {
                if (!mesh_id.is_valid())
                    continue;

                ModelMeshInstance instance = {
                    .mesh_id = mesh_id,
                    .model_matrix = entity_matrix,
                };
                instances.push_back(instance);
            }
            return;
        }

        size_t part_count = model.parts.size();
        std::vector<bool> is_child = std::vector<bool>(part_count, false);

        for (const auto& part : model.parts)
        {
            for (const auto& child_index : part.children)
            {
                if (child_index < part_count)
                    is_child[child_index] = true;
            }
        }

        std::vector<uint32> root_indices = {};
        root_indices.reserve(part_count);

        for (uint32 part_index = 0U; part_index < part_count; ++part_index)
        {
            if (!is_child[part_index])
                root_indices.push_back(part_index);
        }

        if (root_indices.empty())
        {
            for (uint32 part_index = 0U; part_index < part_count; ++part_index)
                root_indices.push_back(part_index);
        }

        std::vector<std::pair<uint32, Mat4>> stack = {};
        stack.reserve(part_count);

        for (const auto& root_index : root_indices)
        {
            stack.emplace_back(root_index, entity_matrix);
        }

        while (!stack.empty())
        {
            auto [part_index, parent_matrix] = stack.back();
            stack.pop_back();

            if (part_index >= part_count)
                continue;

            const OpenGlModelPart& part = model.parts[part_index];
            Mat4 part_matrix = parent_matrix * part.transform;

            if (part.mesh_id.is_valid())
            {
                ModelMeshInstance instance = {
                    .mesh_id = part.mesh_id,
                    .material_id = part.material_id,
                    .model_matrix = part_matrix,
                };
                instances.push_back(instance);
            }

            for (const auto& child_index : part.children)
            {
                stack.emplace_back(child_index, part_matrix);
            }
        }
    }

    static void append_static_model_draw_requests(
        OpenGlRenderCache& cache,
        GlIdProvider& id_provider,
        AssetManager& asset_manager,
        const StaticRenderData& static_data,
        const Mat4& entity_matrix,
        const std::shared_ptr<Material>& fallback_material,
        std::vector<MeshDrawRequest>& draw_requests)
    {
        auto model_asset = cache.get_cached_model(asset_manager, static_data.model, id_provider);
        if (!model_asset)
            return;

        auto fallback_gl_material =
            cache.get_cached_fallback_material(asset_manager, fallback_material, id_provider);
        if (!fallback_gl_material)
            return;

        OpenGlMaterial* override_material = nullptr;
        if (static_data.material.is_valid())
        {
            override_material = cache.get_cached_material(
                asset_manager,
                static_data.material,
                fallback_material,
                id_provider);
        }

        std::vector<ModelMeshInstance> instances = {};
        instances.reserve(model_asset->meshes.size() + model_asset->parts.size());
        append_model_mesh_instances(*model_asset, entity_matrix, instances);

        for (const auto& instance : instances)
        {
            const OpenGlMaterial* material = override_material;
            if (!material && instance.material_id.is_valid())
                material = cache.get_cached_material(instance.material_id);
            if (!material)
                material = fallback_gl_material;

            MeshDrawRequest request = {
                .mesh_id = instance.mesh_id,
                .material = material,
                .model_matrix = instance.model_matrix,
            };
            draw_requests.push_back(request);
        }
    }

    static void append_procedural_draw_requests(
        OpenGlRenderCache& cache,
        GlIdProvider& id_provider,
        AssetManager& asset_manager,
        const ProceduralData& procedural_data,
        const Mat4& entity_matrix,
        const std::shared_ptr<Material>& fallback_material,
        std::vector<MeshDrawRequest>& draw_requests)
    {
        size_t mesh_count = procedural_data.meshes.size();
        for (size_t mesh_index = 0U; mesh_index < mesh_count; ++mesh_index)
        {
            const Mesh& mesh = procedural_data.meshes[mesh_index];
            if (mesh.vertices.empty() || mesh.indices.empty())
                continue;

            Handle material_handle = {};
            if (mesh_index < procedural_data.materials.size())
                material_handle = procedural_data.materials[mesh_index];

            auto material_asset = cache.get_cached_material(
                asset_manager,
                material_handle,
                fallback_material,
                id_provider);
            if (!material_asset)
                continue;

            Uuid mesh_key =
                id_provider.provide(procedural_data.id, static_cast<uint32>(mesh_index));
            auto mesh_resource = cache.get_mesh(mesh, mesh_key);
            if (!mesh_resource)
                continue;

            MeshDrawRequest request = {
                .mesh_id = mesh_key,
                .material = material_asset,
                .model_matrix = entity_matrix,
            };
            draw_requests.push_back(request);
        }
    }

    static void GLAPIENTRY gl_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* user_param)
    {
        (void)source;
        (void)type;
        (void)id;
        (void)length;
        (void)user_param;

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
        _render_resolution = host.get_settings().resolution.value;
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        for (auto& entry : _render_targets)
        {
            Uuid window_id = entry.first;
            auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (result)
            {
                entry.second.destroy();
                entry.second.destroy_present_pipeline();
            }
        }

        _is_gl_ready = false;
        _window_sizes.clear();
        _render_targets.clear();
        _cache.clear();
    }

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        if (!_is_gl_ready || _window_sizes.empty())
            return;

        for (const auto& entry : _window_sizes)
        {
            Uuid window_id = entry.first;
            const Size& window_size = entry.second;
            Size render_resolution = get_effective_resolution(window_size);

            // Bind the window context before issuing any GL commands.
            auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (!result)
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to make window current: {}",
                    result.get_report());
                continue;
            }

            auto& target = _render_targets[window_id];
            if (!target.try_resize(render_resolution))
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: Failed to create render target {}x{} for window {}.",
                    render_resolution.width,
                    render_resolution.height,
                    window_id.value);
                continue;
            }

            auto& pipeline = target.get_present_pipeline();
            if (!pipeline.is_ready() && !pipeline.try_initialize())
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: Failed to initialize present pipeline for window {}.",
                    window_id.value);
                continue;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(target.get_framebuffer()));
            glViewport(
                0,
                0,
                static_cast<GLsizei>(render_resolution.width),
                static_cast<GLsizei>(render_resolution.height));
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Fixed 255 to 1.0f
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Draw all scene cameras to the offscreen target at the logical resolution.
            draw_models_for_cameras(render_resolution);

            // Present the scaled image to the window backbuffer via a shader, avoiding
            // glBlitFramebuffer scaling (which can be invalid with multisampled defaults).
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(
                0,
                0,
                static_cast<GLsizei>(window_size.width),
                static_cast<GLsizei>(window_size.height));
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            float x_scale =
                static_cast<float>(window_size.width) / static_cast<float>(render_resolution.width);
            float y_scale = static_cast<float>(window_size.height)
                            / static_cast<float>(render_resolution.height);
            float scale = std::min(x_scale, y_scale);

            int scaled_width =
                std::max(1, static_cast<int>(static_cast<float>(render_resolution.width) * scale));
            int scaled_height =
                std::max(1, static_cast<int>(static_cast<float>(render_resolution.height) * scale));

            int x_offset = std::max(0, (static_cast<int>(window_size.width) - scaled_width) / 2);
            int y_offset = std::max(0, (static_cast<int>(window_size.height) - scaled_height) / 2);

            bool was_depth_test_enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
            bool was_blend_enabled = glIsEnabled(GL_BLEND) == GL_TRUE;

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            GlResourceScope program_scope(*pipeline.program);
            glBindVertexArray(static_cast<GLuint>(pipeline.vertex_array));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(target.get_color_texture()));
            pipeline.program->try_upload({
                .name = "u_texture",
                .data = 0,
            });

            glViewport(x_offset, y_offset, scaled_width, scaled_height);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glBindTexture(GL_TEXTURE_2D, 0);
            glBindVertexArray(0);

            if (was_blend_enabled)
                glEnable(GL_BLEND);
            if (was_depth_test_enabled)
                glEnable(GL_DEPTH_TEST);

            glViewport(
                0,
                0,
                static_cast<GLsizei>(window_size.width),
                static_cast<GLsizei>(window_size.height));

            // Present the rendered frame to the window.
            glFlush();
            auto present_result = send_message<WindowPresentRequest>(window_id);
            if (!present_result)
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to present window: {}",
                    present_result.get_report());
            }
        }
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            handle_window_ready(*ready_event);
            return;
        }

        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            handle_window_open_changed(*open_event);
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            handle_window_resized(*size_event);
            return;
        }

        if (auto* resolution_event = handle_property_changed<&AppSettings::resolution>(msg))
        {
            handle_resolution_changed(*resolution_event);
            return;
        }
    }

    void OpenGlRenderingPlugin::handle_window_ready(WindowContextReadyEvent& event)
    {
        bool loaded = false;

        if (event.get_proc_address)
            loaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(event.get_proc_address));

        if (loaded)
        {
            initialize_opengl();
            _is_gl_ready = true;
        }
        else
            TBX_TRACE_WARNING("Failed to load glad!");

        if (!_is_gl_ready)
        {
            event.state = MessageState::ERROR;
            event.result.flag_failure("OpenGL rendering: GL loader not initialized.");
            return;
        }

        _window_sizes[event.window] = event.size;

        event.state = MessageState::HANDLED;
        event.result.flag_success();
    }

    void OpenGlRenderingPlugin::handle_window_open_changed(
        PropertyChangedEvent<Window, bool>& event)
    {
        if (!event.owner)
        {
            return;
        }

        if (event.current)
        {
            return;
        }

        remove_window_state(event.owner->id, true);
    }

    void OpenGlRenderingPlugin::handle_window_resized(PropertyChangedEvent<Window, Size>& event)
    {
        if (!event.owner)
        {
            return;
        }

        auto window_id = event.owner->id;
        if (_window_sizes.contains(window_id))
        {
            _window_sizes[window_id] = event.current;
        }
    }

    void OpenGlRenderingPlugin::handle_resolution_changed(
        PropertyChangedEvent<AppSettings, Size>& event)
    {
        _render_resolution = event.current;
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
    }

    Size OpenGlRenderingPlugin::get_effective_resolution(const Size& window_size) const
    {
        Size resolution = _render_resolution;
        if (resolution.width == 0U || resolution.height == 0U)
        {
            resolution = window_size;
        }

        if (resolution.width == 0U)
        {
            resolution.width = 1U;
        }

        if (resolution.height == 0U)
        {
            resolution.height = 1U;
        }

        return resolution;
    }

    void OpenGlRenderingPlugin::update_frame_lighting()
    {
        _frame_light_uniforms.clear();
        _frame_light_count = 0;

        auto& ecs = get_host().get_entity_registry();
        auto lights = ecs.get_with<Light>();
        if (lights.empty())
            return;

        size_t light_count = std::min(lights.size(), static_cast<size_t>(MAX_LIGHTS));
        _frame_light_uniforms.reserve(light_count * 9U);

        for (size_t light_index = 0U; light_index < light_count; ++light_index)
        {
            Entity entity = lights[light_index];
            const Light& light = entity.get_component<Light>();
            Transform* light_tran = nullptr;
            if (entity.has_component<Transform>())
            {
                light_tran = &entity.get_component<Transform>();
            }

            Vec3 direction = light_tran ? normalize(light_tran->rotation * Vec3(0.0f, 0.0f, -1.0f))
                                        : Vec3(0, -1, 0);
            float intensity = std::max(0.0f, light.intensity);
            float range = std::max(light.range, MIN_LIGHT_RANGE);

            float inner_angle = std::clamp(light.inner_angle, 0.0f, MAX_SPOT_ANGLE);
            float outer_angle = std::clamp(light.outer_angle, inner_angle, MAX_SPOT_ANGLE);

            float inner_cos = std::cos(degrees_to_radians(inner_angle));
            float outer_cos = std::cos(degrees_to_radians(outer_angle));

            Vec2 area_size = light.area_size;
            area_size.x = std::max(area_size.x, 0.0f);
            area_size.y = std::max(area_size.y, 0.0f);

            Vec3 color = Vec3(light.color.r, light.color.g, light.color.b);

            std::string base = "u_lights[" + std::to_string(light_index) + "].";
            _frame_light_uniforms.push_back({base + "mode", static_cast<int>(light.mode)});
            _frame_light_uniforms.push_back(
                {base + "position", light_tran ? light_tran->position : Vec3(0)});
            _frame_light_uniforms.push_back({base + "direction", direction});
            _frame_light_uniforms.push_back({base + "color", color});
            _frame_light_uniforms.push_back({base + "intensity", intensity});
            _frame_light_uniforms.push_back({base + "range", range});
            _frame_light_uniforms.push_back({base + "inner_cos", inner_cos});
            _frame_light_uniforms.push_back({base + "outer_cos", outer_cos});
            _frame_light_uniforms.push_back({base + "area_size", area_size});
        }

        _frame_light_count = static_cast<int>(light_count);
    }

    void OpenGlRenderingPlugin::remove_window_state(const Uuid& window_id, bool try_release)
    {
        _window_sizes.erase(window_id);

        auto it = _render_targets.find(window_id);
        if (it == _render_targets.end())
        {
            return;
        }

        if (try_release)
        {
            auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (result)
            {
                it->second.destroy();
                it->second.destroy_present_pipeline();
            }
        }

        _render_targets.erase(it);
    }

    void OpenGlRenderingPlugin::draw_models(const Mat4& view_projection)
    {
        auto& ecs = get_host().get_entity_registry();
        auto& asset_manager = get_host().get_asset_manager();

        static const auto FALLBACK_MATERIAL = std::make_shared<Material>();

        ecs.for_each_with<Renderer>(
            [&](Entity entity)
            {
                const Renderer& renderer = entity.get_component<Renderer>();
                if (!renderer.data)
                    return;

                // Step 2: Compute the entity model matrix (identity if no transform).
                Mat4 entity_matrix = Mat4(1.0f);
                if (entity.has_component<Transform>())
                    entity_matrix = build_transform_matrix(entity.get_component<Transform>());

                std::vector<MeshDrawRequest> draw_requests = {};

                if (auto* static_data = dynamic_cast<const StaticRenderData*>(renderer.data.get()))
                {
                    append_static_model_draw_requests(
                        _cache,
                        _id_provider,
                        asset_manager,
                        *static_data,
                        entity_matrix,
                        FALLBACK_MATERIAL,
                        draw_requests);
                }
                else if (
                    auto* procedural_data =
                        dynamic_cast<const ProceduralData*>(renderer.data.get()))
                {
                    append_procedural_draw_requests(
                        _cache,
                        _id_provider,
                        asset_manager,
                        *procedural_data,
                        entity_matrix,
                        FALLBACK_MATERIAL,
                        draw_requests);
                }

                for (const auto& request : draw_requests)
                {
                    if (!request.material)
                        continue;

                    draw_mesh_with_material(
                        request.mesh_id,
                        *request.material,
                        request.model_matrix,
                        view_projection);
                }
            });
    }

    void OpenGlRenderingPlugin::draw_mesh_with_material(
        const Uuid& mesh_key,
        const OpenGlMaterial& material,
        const Mat4& model_matrix,
        const Mat4& view_projection)
    {
        // Step 1: Fetch the mesh resource for this cache key.
        auto mesh_resource = _cache.get_cached_mesh(mesh_key);
        if (!mesh_resource)
            return;

        // Step 2: Determine which shader programs to render with.
        std::vector<Uuid> shader_programs = material.shader_programs;
        if (shader_programs.empty())
            shader_programs.push_back(default_shader.id);

        for (const auto& shader_id : shader_programs)
        {
            auto program = _cache.get_cached_shader_program(shader_id);
            if (!program)
                continue;

            // Step 3: Bind the shader program and upload view/model matrices.
            GlResourceScope program_scope(*program);

            program->try_upload({
                .name = "u_view_proj",
                .data = view_projection,
            });
            program->try_upload({
                .name = "u_model",
                .data = model_matrix,
            });
            program->try_upload({
                .name = "u_light_count",
                .data = _frame_light_count,
            });
            for (const auto& light_uniform : _frame_light_uniforms)
            {
                program->try_upload(light_uniform);
            }

            // Step 4: Upload scalar/vector material parameters.
            for (const auto& parameter : material.parameters)
                program->upload(parameter);

            // Step 5: Bind textures and upload sampler slots.
            std::vector<GlResourceScope> texture_scopes = {};
            texture_scopes.reserve(material.textures.size());
            uint32 texture_slot = 0U;

            for (const auto& texture_binding : material.textures)
            {
                auto texture_resource = _cache.get_default_texture();
                if (texture_binding.texture_id.is_valid())
                {
                    auto cached_texture = _cache.get_cached_texture(texture_binding.texture_id);
                    if (cached_texture)
                    {
                        texture_resource = cached_texture;
                    }
                }

                if (texture_resource)
                {
                    texture_resource->set_slot(static_cast<int>(texture_slot));
                    texture_scopes.emplace_back(*texture_resource);
                }

                program->upload({
                    .name = texture_binding.name,
                    .data = static_cast<int>(texture_slot),
                });
                ++texture_slot;
            }

            // Step 6: Bind the mesh and issue the draw call.
            GlResourceScope mesh_scope(*mesh_resource);
            mesh_resource->draw();
        }
    }

    void OpenGlRenderingPlugin::draw_models_for_cameras(const Size& window_size)
    {
        auto& ecs = get_host().get_entity_registry();
        auto cameras = ecs.get_with<Camera, Transform>();

        update_frame_lighting();

        if (cameras.begin() == cameras.end())
        {
            float aspect = window_size.get_aspect_ratio();
            Mat4 default_view =
                look_at(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
            Mat4 default_projection =
                perspective_projection(degrees_to_radians(60.0f), aspect, 0.1f, 1000.0f);
            Mat4 view_projection = default_projection * default_view;
            draw_models(view_projection);
            return;
        }

        float aspect = window_size.get_aspect_ratio();
        for (const auto& camera_ent : cameras)
        {
            auto& camera = camera_ent.get_component<Camera>();
            camera.set_aspect(aspect);

            Transform& transform = camera_ent.get_component<Transform>();
            Mat4 view_projection =
                camera.get_view_projection_matrix(transform.position, transform.rotation);

            draw_models(view_projection);
        }
    }
}
