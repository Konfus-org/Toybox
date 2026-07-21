#include "tbx/assets/builtin.h"
#include "tbx/core/log.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/render_graph.h"
#include "tbx/ui/ui.h"
#include "tbx/app.h"
#include "tbx/files/files.h"
#include <chrono>
#include <filesystem>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::gpu
{
    // Shader source is GLSL for now — when a second gfx backend lands, sources move behind the
    // backend seam alongside gpu.h's implementations. Vertex layout everywhere: position(3) +
    // normal(3) + uv(2).
    /// @brief
    /// Purpose: Lazily-built renderer resources plus per-asset GPU caches (RAII; released at
    /// process exit).
    /// @brief
    /// Purpose: A shader plus the pipeline-state object it draws with.
    struct CompiledPipeline
    {
        std::unique_ptr<Shader> shader;
        std::unique_ptr<Pipeline> pipeline;
    };

    struct RendererState
    {
        std::unique_ptr<Shader> depth_shader;
        std::unique_ptr<Shader> lit_shader;
        std::unique_ptr<Shader> sky_shader;
        std::unique_ptr<Pipeline> depth_pipeline;
        std::unique_ptr<Pipeline> lit_pipeline;
        std::unique_ptr<Pipeline> sky_pipeline;
        std::unique_ptr<Mesh> cube;
        std::unique_ptr<Mesh> plane;
        std::unique_ptr<Mesh> sphere;
        std::unique_ptr<Mesh> fullscreen;
        std::unique_ptr<Texture2d> white;
        std::unique_ptr<DepthTarget> shadow_target;
        std::unique_ptr<RenderTarget> post_source;
        std::unique_ptr<RenderTarget> post_swap;
        std::unordered_map<Uuid, std::unique_ptr<Mesh>> meshes_by_asset;
        std::unordered_map<Uuid, std::unique_ptr<Texture2d>> textures_by_asset;
        std::unordered_map<Uuid, CompiledPipeline> shaders_by_fragment;
        std::unordered_map<Uuid, CompiledPipeline> post_shaders_by_asset;
        std::unordered_map<Uuid, uint64> ui_documents_by_asset;
        std::unordered_set<Uuid> warned_assets;
        std::string lit_vertex_text;
        std::string post_vertex_text;
        std::chrono::steady_clock::time_point start_time;
    };

    static RendererState g_renderer = {};

    //// PRIMITIVES (position + normal + uv) ////

    static void push_vertex(
        std::vector<float>& vertices,
        const Vec3& position,
        const Vec3& normal,
        const Vec2& uv)
    {
        for (const float value :
             {position.x, position.y, position.z, normal.x, normal.y, normal.z, uv.x, uv.y})
            vertices.push_back(value);
    }

    static std::vector<float> build_plane_vertices()
    {
        auto vertices = std::vector<float>();
        const Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
        const Vec3 corners[4] = {
            {-0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, -0.5f}, {-0.5f, 0.0f, -0.5f}};
        const Vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (const int index : {0, 1, 2, 0, 2, 3})
            push_vertex(vertices, corners[index], up, uvs[index]);
        return vertices;
    }

    static std::vector<float> build_cube_vertices()
    {
        auto vertices = std::vector<float>();
        const Vec3 normals[6] = {
            {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (const Vec3& normal : normals)
        {
            // Build a face basis from the normal; corners wind counter-clockwise.
            const Vec3 up = std::abs(normal.y) > 0.5f ? Vec3(0, 0, -normal.y) : Vec3(0, 1, 0);
            const Vec3 right = Vec3(
                up.y * normal.z - up.z * normal.y,
                up.z * normal.x - up.x * normal.z,
                up.x * normal.y - up.y * normal.x);
            const Vec3 center = normal * 0.5f;
            const Vec3 corners[4] = {
                center - right * 0.5f - up * 0.5f,
                center + right * 0.5f - up * 0.5f,
                center + right * 0.5f + up * 0.5f,
                center - right * 0.5f + up * 0.5f};
            const Vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (const int index : {0, 1, 2, 0, 2, 3})
                push_vertex(vertices, corners[index], normal, uvs[index]);
        }
        return vertices;
    }

    static std::vector<float> build_fullscreen_vertices()
    {
        // One triangle covering NDC; uv doubles past 1 so v_uv spans [0,1] over the viewport.
        auto vertices = std::vector<float>();
        const Vec3 forward = Vec3(0.0f, 0.0f, 1.0f);
        push_vertex(vertices, Vec3(-1.0f, -1.0f, 0.0f), forward, Vec2(0.0f, 0.0f));
        push_vertex(vertices, Vec3(3.0f, -1.0f, 0.0f), forward, Vec2(2.0f, 0.0f));
        push_vertex(vertices, Vec3(-1.0f, 3.0f, 0.0f), forward, Vec2(0.0f, 2.0f));
        return vertices;
    }

    static std::vector<float> build_sphere_vertices(const int rings, const int segments)
    {
        auto vertices = std::vector<float>();
        auto point = [&](const int ring, const int segment)
        {
            const float phi = 3.14159265f * static_cast<float>(ring) / rings;
            const float theta = 2.0f * 3.14159265f * static_cast<float>(segment) / segments;
            const Vec3 normal = Vec3(
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta));
            const Vec2 uv =
                Vec2(static_cast<float>(segment) / segments, static_cast<float>(ring) / rings);
            push_vertex(vertices, normal * 0.5f, normal, uv);
        };
        for (int ring = 0; ring < rings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                point(ring, segment);
                point(ring + 1, segment + 1);
                point(ring + 1, segment);
                point(ring, segment);
                point(ring, segment + 1);
                point(ring + 1, segment + 1);
            }
        }
        return vertices;
    }

    //// SETUP ////

    /// @brief
    /// Purpose: Reads one builtin shader stage from the engine resources
    /// (resources/Shaders/Tbx) — shaders are files, not string literals.
    static Result<std::string> read_builtin_shader(const char* file_name)
    {
        const auto path =
            std::filesystem::path(TBX_RESOURCES_PATH) / "Shaders" / "Tbx" / file_name;
        return files::read_text(path);
    }

    static bool ensure_renderer_ready()
    {
        if (g_renderer.lit_shader)
            return true;
        register_builtin_blocks();
        const auto lit_vertex = read_builtin_shader("lit.vert");
        const auto lit_fragment = read_builtin_shader("lit.frag");
        const auto depth_vertex = read_builtin_shader("depth.vert");
        const auto depth_fragment = read_builtin_shader("depth.frag");
        const auto sky_vertex = read_builtin_shader("sky.vert");
        const auto sky_fragment = read_builtin_shader("sky.frag");
        const auto post_vertex = read_builtin_shader("post.vert");
        if (!lit_vertex || !lit_fragment || !depth_vertex || !depth_fragment || !sky_vertex
            || !sky_fragment || !post_vertex)
        {
            log_error("builtin shaders missing under resources/Shaders/Tbx");
            return false;
        }
        auto depth = compile_shader(depth_vertex->c_str(), depth_fragment->c_str());
        auto lit = compile_shader(lit_vertex->c_str(), lit_fragment->c_str());
        auto sky = compile_shader(sky_vertex->c_str(), sky_fragment->c_str());
        if (!depth || !lit || !sky)
        {
            log_error(
                "renderer shaders failed: {}",
                !depth ? depth.error() : (!lit ? lit.error() : sky.error()));
            return false;
        }
        g_renderer.lit_vertex_text = *lit_vertex;
        g_renderer.post_vertex_text = *post_vertex;
        g_renderer.depth_shader = std::move(*depth);
        g_renderer.lit_shader = std::move(*lit);
        g_renderer.sky_shader = std::move(*sky);
        // Front-face culling in the shadow pass reduces acne on closed meshes; the sky skips
        // depth entirely (drawn first, the scene covers it).
        g_renderer.depth_pipeline =
            make_pipeline({.shader = *g_renderer.depth_shader, .cull = CullMode::FRONT});
        g_renderer.lit_pipeline = make_pipeline({.shader = *g_renderer.lit_shader});
        g_renderer.sky_pipeline = make_pipeline(
            {.shader = *g_renderer.sky_shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE});
        g_renderer.cube = upload_mesh(build_cube_vertices(), std::array {3, 3, 2});
        g_renderer.plane = upload_mesh(build_plane_vertices(), std::array {3, 3, 2});
        g_renderer.sphere = upload_mesh(build_sphere_vertices(16, 24), std::array {3, 3, 2});
        g_renderer.fullscreen = upload_mesh(build_fullscreen_vertices(), std::array {3, 3, 2});
        g_renderer.start_time = std::chrono::steady_clock::now();
        constexpr std::byte WHITE[4] = {
            std::byte {255}, std::byte {255}, std::byte {255}, std::byte {255}};
        g_renderer.white = upload_texture(1, 1, WHITE);
        g_renderer.shadow_target = make_depth_target(2048);
        return true;
    }

    //// FAILURE STATES ////
    // Old-Toybox style: a broken reference never crashes and never hides — the draw flashes a
    // color naming the failure, and the log says why exactly once per asset.

    static constexpr Color FAILURE_MODEL = Color {.r = 1.0f, .g = 0.0f, .b = 1.0f};    // magenta
    static constexpr Color FAILURE_TEXTURE = Color {.r = 1.0f, .g = 1.0f, .b = 0.0f};  // yellow
    static constexpr Color FAILURE_MATERIAL = Color {.r = 1.0f, .g = 0.0f, .b = 0.0f}; // red
    static constexpr Color FAILURE_SHADER = Color {.r = 0.0f, .g = 1.0f, .b = 1.0f};   // cyan

    static void warn_once(const Uuid& id, const std::string& message)
    {
        if (g_renderer.warned_assets.insert(id).second)
            log_warn("{}", message);
    }

    static float get_failure_flash()
    {
        const float seconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - g_renderer.start_time)
                .count();
        return 0.55f + 0.45f * std::sin(seconds * 8.0f);
    }

    //// ASSET RESOLUTION ////

    /// @brief
    /// Purpose: A mesh choice plus whether it is a failure stand-in.
    struct ResolvedMesh
    {
        std::reference_wrapper<const Mesh> mesh;
        bool is_failed = false;
    };

    static ResolvedMesh resolve_mesh(const Renderer& renderer, Assets& assets)
    {
        if (renderer.model.is_set())
        {
            const auto cached = g_renderer.meshes_by_asset.find(renderer.model.id);
            if (cached != g_renderer.meshes_by_asset.end())
                return {.mesh = *cached->second};
            if (const auto model = assets.load_now(renderer.model))
            {
                auto uploaded = upload_mesh(model->get().vertices, std::array {3, 3, 2});
                const Mesh& result = *uploaded;
                g_renderer.meshes_by_asset[renderer.model.id] = std::move(uploaded);
                return {.mesh = result};
            }
            else
            {
                warn_once(renderer.model.id, "model unavailable: " + model.error());
                return {.mesh = *g_renderer.cube, .is_failed = true};
            }
        }
        if (renderer.mesh == builtin::PLANE)
            return {.mesh = *g_renderer.plane};
        if (renderer.mesh == builtin::SPHERE)
            return {.mesh = *g_renderer.sphere};
        return {.mesh = *g_renderer.cube};
    }

    /// @brief
    /// Purpose: A texture choice plus whether it is a failure stand-in.
    struct ResolvedTexture
    {
        std::reference_wrapper<const Texture2d> texture;
        bool is_failed = false;
    };

    static ResolvedTexture resolve_texture_handle(
        const AssetHandle<Texture>& handle,
        Assets& assets)
    {
        if (!handle.is_set())
            return {.texture = *g_renderer.white};
        const auto cached = g_renderer.textures_by_asset.find(handle.id);
        if (cached != g_renderer.textures_by_asset.end())
            return {.texture = *cached->second};
        if (const auto texture = assets.load_now(handle))
        {
            auto uploaded = upload_texture(
                texture->get().width, texture->get().height, texture->get().pixels);
            const Texture2d& result = *uploaded;
            g_renderer.textures_by_asset[handle.id] = std::move(uploaded);
            return {.texture = result};
        }
        else
        {
            warn_once(handle.id, "texture unavailable: " + texture.error());
            return {.texture = *g_renderer.white, .is_failed = true};
        }
    }

    /// @brief
    /// Purpose: Everything one draw needs after material resolution; a set failure paints the
    /// draw with that flashing color instead of its look.
    struct ResolvedSurface
    {
        std::reference_wrapper<const Shader> shader;
        std::reference_wrapper<const Pipeline> pipeline;
        std::reference_wrapper<const Texture2d> texture;
        Color tint = {};
        Json uniforms = {};
        std::optional<Color> failure = {};
    };

    static ResolvedSurface resolve_surface(const Renderer& renderer, Assets& assets)
    {
        const auto base_texture = resolve_texture_handle(renderer.texture, assets);
        auto surface = ResolvedSurface {
            .shader = *g_renderer.lit_shader,
            .pipeline = *g_renderer.lit_pipeline,
            .texture = base_texture.texture,
            .tint = renderer.tint};
        if (base_texture.is_failed)
            surface.failure = FAILURE_TEXTURE;
        if (!renderer.material.is_set())
            return surface;
        const auto material = assets.load_now(renderer.material);
        if (!material)
        {
            warn_once(renderer.material.id, "material unavailable: " + material.error());
            surface.failure = FAILURE_MATERIAL;
            return surface;
        }

        // Material tint multiplies the per-toy tint; its texture wins when set.
        const Material& resolved = material->get();
        surface.tint = Color {
            .r = resolved.tint.r * renderer.tint.r,
            .g = resolved.tint.g * renderer.tint.g,
            .b = resolved.tint.b * renderer.tint.b,
            .a = resolved.tint.a * renderer.tint.a};
        if (resolved.texture.is_set())
        {
            const auto material_texture = resolve_texture_handle(resolved.texture, assets);
            surface.texture = material_texture.texture;
            if (material_texture.is_failed)
                surface.failure = FAILURE_TEXTURE;
        }
        surface.uniforms = resolved.uniforms;

        // A custom fragment stage pairs with the builtin lit vertex stage, cached by asset.
        if (resolved.fragment.is_set())
        {
            const auto cached = g_renderer.shaders_by_fragment.find(resolved.fragment.id);
            if (cached != g_renderer.shaders_by_fragment.end())
            {
                surface.shader = *cached->second.shader;
                surface.pipeline = *cached->second.pipeline;
            }
            else if (const auto source = assets.load_now(resolved.fragment))
            {
                auto compiled = compile_shader(
                    g_renderer.lit_vertex_text.c_str(), source->get().text.c_str());
                if (compiled)
                {
                    auto& entry = g_renderer.shaders_by_fragment[resolved.fragment.id];
                    entry.shader = std::move(*compiled);
                    entry.pipeline = make_pipeline({.shader = *entry.shader});
                    surface.shader = *entry.shader;
                    surface.pipeline = *entry.pipeline;
                }
                else
                {
                    warn_once(
                        resolved.fragment.id, "material shader failed: " + compiled.error());
                    surface.failure = FAILURE_SHADER;
                }
            }
            else
            {
                warn_once(resolved.fragment.id, "material shader unavailable: " + source.error());
                surface.failure = FAILURE_SHADER;
            }
        }
        return surface;
    }

    /// @brief
    /// Purpose: A PostProcessing entry compiled against the builtin post vertex stage, cached
    /// by asset id (failures cache too, so a broken shader warns once and is skipped).
    static std::optional<std::reference_wrapper<const CompiledPipeline>> resolve_post_shader(
        const AssetHandle<ShaderSource>& handle,
        Assets& assets)
    {
        if (!handle.is_set())
            return {};
        const auto cached = g_renderer.post_shaders_by_asset.find(handle.id);
        if (cached != g_renderer.post_shaders_by_asset.end())
        {
            if (!cached->second.shader)
                return {};
            return cached->second;
        }
        const auto source = assets.load_now(handle);
        if (!source)
        {
            warn_once(handle.id, "post shader unavailable: " + source.error());
            g_renderer.post_shaders_by_asset[handle.id] = {};
            return {};
        }
        auto compiled =
            compile_shader(g_renderer.post_vertex_text.c_str(), source->get().text.c_str());
        if (!compiled)
        {
            warn_once(handle.id, "post shader failed: " + compiled.error());
            g_renderer.post_shaders_by_asset[handle.id] = {};
            return {};
        }
        auto& entry = g_renderer.post_shaders_by_asset[handle.id];
        entry.shader = std::move(*compiled);
        entry.pipeline = make_pipeline(
            {.shader = *entry.shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE});
        return entry;
    }

    //// FRAME CONTEXT (shared between the builtin passes of one frame) ////

    struct FrameContext
    {
        Mat4 view_projection = Mat4(1.0f);
        Vec3 camera_position = Vec3(0.0f, 0.0f, 0.0f);
        bool has_camera = false;
        Vec3 light_direction = Vec3(0.0f, -1.0f, 0.0f);
        Color light_color = {};
        float light_intensity = 1.0f;
        Mat4 light_view_projection = Mat4(1.0f);
        bool is_post_active = false;
    };

    static FrameContext g_frame = {};

    static void refresh_camera(Sandbox& sandbox)
    {
        auto& registry = sandbox.get_registry();
        g_frame.has_camera = false;
        for (const auto [entity, camera] : registry.view<Camera>().each())
        {
            const Mat4 world = sandbox.get_world_matrix(Toy(sandbox, entity));
            g_frame.camera_position = Vec3(world * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const float aspect = get_viewport_height() > 0
                ? static_cast<float>(get_viewport_width()) / get_viewport_height()
                : 1.0f;
            const Mat4 projection = math::perspective(
                math::radians(camera.fov_degrees), aspect, camera.near_plane, camera.far_plane);
            g_frame.view_projection = projection * math::inverse(world);
            g_frame.has_camera = true;
            break;
        }
    }

    static void refresh_lighting(Sandbox& sandbox)
    {
        auto& registry = sandbox.get_registry();
        g_frame.light_direction = math::normalize(Vec3(-0.4f, -1.0f, -0.3f));
        g_frame.light_color = Color {};
        g_frame.light_intensity = 1.0f;
        for (const auto [entity, light] : registry.view<DirectionalLight>().each())
        {
            const Mat4 world = sandbox.get_world_matrix(Toy(sandbox, entity));
            g_frame.light_direction =
                math::normalize(Vec3(world * Vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            g_frame.light_color = light.color;
            g_frame.light_intensity = light.intensity;
            break;
        }
        const Mat4 light_view = math::look_at(
            -g_frame.light_direction * 30.0f,
            Vec3(0.0f, 0.0f, 0.0f),
            Vec3(0.0f, 1.0f, 0.0f));
        g_frame.light_view_projection =
            math::orthographic(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 100.0f) * light_view;
    }

    //// BUILTIN PASSES ////

    static void render_shadow_pass(Sandbox& sandbox, Assets& assets)
    {
        if (!ensure_renderer_ready())
            return;
        auto& registry = sandbox.get_registry();
        refresh_lighting(sandbox);

        begin_render_pass({.depth_target = *g_renderer.shadow_target});
        set_pipeline(*g_renderer.depth_pipeline);
        set_uniform(
            *g_renderer.depth_shader, "u_light_view_projection", g_frame.light_view_projection);
        for (const auto [entity, renderer] : registry.view<Renderer>().each())
        {
            if (!registry.get<ToyHandle>(entity).is_enabled)
                continue;
            set_uniform(
                *g_renderer.depth_shader,
                "u_model",
                sandbox.get_world_matrix(Toy(sandbox, entity)));
            draw(resolve_mesh(renderer, assets).mesh);
        }
        end_render_pass();
    }

    static void render_geometry_pass(Sandbox& sandbox, Assets& assets)
    {
        if (!ensure_renderer_ready())
            return;
        auto& registry = sandbox.get_registry();
        refresh_camera(sandbox);
        refresh_lighting(sandbox);
        g_frame.is_post_active = false;
        if (!g_frame.has_camera)
            return; // no camera, no picture

        // When a PostProcessing block lists shaders, the scene renders into an offscreen
        // target; the post pass chains it back onto the swapchain (the two passes pair).
        auto post_chain_probe = std::vector<std::reference_wrapper<const CompiledPipeline>>();
        for (const auto [entity, post] : registry.view<PostProcessing>().each())
        {
            for (const AssetHandle<ShaderSource>& handle : post.shaders)
                if (const auto stage = resolve_post_shader(handle, assets))
                    post_chain_probe.push_back(*stage);
            break; // the first PostProcessing toy wins
        }
        g_frame.is_post_active = !post_chain_probe.empty();
        if (g_frame.is_post_active)
        {
            const int width = get_viewport_width();
            const int height = get_viewport_height();
            if (!g_renderer.post_source || g_renderer.post_source->get_width() != width
                || g_renderer.post_source->get_height() != height)
            {
                g_renderer.post_source = make_render_target(width, height);
                g_renderer.post_swap = make_render_target(width, height);
            }
            begin_render_pass(
                {.color_target = *g_renderer.post_source,
                 .load = LoadOperation::CLEAR,
                 .clear_color = get_clear_color()});
        }

        // Sky: the first Sky block paints the background along the view ray.
        for (const auto [entity, sky] : registry.view<Sky>().each())
        {
            const Shader& sky_shader = *g_renderer.sky_shader;
            set_pipeline(*g_renderer.sky_pipeline);
            set_uniform(
                sky_shader, "u_inverse_view_projection", math::inverse(g_frame.view_projection));
            set_uniform(sky_shader, "u_camera_position", g_frame.camera_position);
            set_uniform(sky_shader, "u_tint", sky.tint);
            set_uniform(sky_shader, "u_sky", 0);
            bind_texture(resolve_texture_handle(sky.texture, assets).texture, 0);
            draw(*g_renderer.fullscreen);
            break;
        }

        // Lit + shadowed + textured, material-driven per draw; failures flash their color.
        const float flash = get_failure_flash();
        bind_depth_texture(*g_renderer.shadow_target, 0);
        for (const auto [entity, renderer] : registry.view<Renderer>().each())
        {
            if (!registry.get<ToyHandle>(entity).is_enabled)
                continue;
            const ResolvedMesh mesh = resolve_mesh(renderer, assets);
            auto surface = resolve_surface(renderer, assets);
            if (mesh.is_failed)
                surface.failure = FAILURE_MODEL;
            if (surface.failure)
            {
                surface.tint = Color {
                    .r = surface.failure->r * flash,
                    .g = surface.failure->g * flash,
                    .b = surface.failure->b * flash};
                surface.texture = *g_renderer.white;
                surface.uniforms = {};
            }
            const Shader& shader = surface.shader;
            set_pipeline(surface.pipeline);
            set_uniform(shader, "u_view_projection", g_frame.view_projection);
            set_uniform(shader, "u_light_view_projection", g_frame.light_view_projection);
            set_uniform(shader, "u_light_direction", g_frame.light_direction);
            set_uniform(shader, "u_light_color", g_frame.light_color);
            set_uniform(shader, "u_light_intensity", g_frame.light_intensity);
            set_uniform(shader, "u_camera_position", g_frame.camera_position);
            set_uniform(shader, "u_shadow_map", 0);
            set_uniform(shader, "u_albedo", 1);
            set_uniform(shader, "u_model", sandbox.get_world_matrix(Toy(sandbox, entity)));
            set_uniform(shader, "u_tint", surface.tint);
            set_uniform(shader, "u_uv_scale", 1.0f);
            apply_uniforms(shader, surface.uniforms); // reflection-typed material extras
            bind_texture(surface.texture, 1);
            draw(mesh.mesh);
        }
    }

    static void render_post_pass(Sandbox& sandbox, Assets& assets)
    {
        if (!ensure_renderer_ready() || !g_frame.is_post_active)
            return;
        auto& registry = sandbox.get_registry();
        auto post_chain = std::vector<std::reference_wrapper<const CompiledPipeline>>();
        for (const auto [entity, post] : registry.view<PostProcessing>().each())
        {
            for (const AssetHandle<ShaderSource>& handle : post.shaders)
                if (const auto stage = resolve_post_shader(handle, assets))
                    post_chain.push_back(*stage);
            break;
        }
        g_frame.is_post_active = false;
        if (post_chain.empty())
            return;

        // Ping-pong through the chain; the last stage lands on the swapchain.
        const float time_seconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - g_renderer.start_time)
                .count();
        const auto resolution = Vec2(
            static_cast<float>(get_viewport_width()),
            static_cast<float>(get_viewport_height()));
        auto source = std::ref(*g_renderer.post_source);
        auto swap = std::ref(*g_renderer.post_swap);
        end_render_pass(); // close the geometry pass's offscreen target
        for (size index = 0; index < post_chain.size(); ++index)
        {
            const bool is_last = index + 1 == post_chain.size();
            if (is_last)
                begin_render_pass({}); // the swapchain; the fullscreen draw covers it
            else
                begin_render_pass({.color_target = swap});
            const CompiledPipeline& stage = post_chain[index];
            set_pipeline(*stage.pipeline);
            set_uniform(*stage.shader, "u_scene", 0);
            set_uniform(*stage.shader, "u_resolution", resolution);
            set_uniform(*stage.shader, "u_time", time_seconds);
            bind_render_target_texture(source, 0);
            draw(*g_renderer.fullscreen);
            end_render_pass();
            if (!is_last)
                std::swap(source, swap);
        }
    }

    static void render_ui_pass(Sandbox& sandbox, Assets& assets)
    {
        // Ui blocks own documents: load on first sight, show/hide with the block, unload when
        // the toy goes away. Documents loaded directly through tbx::ui (debug view, tools)
        // are untouched.
        auto& registry = sandbox.get_registry();
        auto seen = std::unordered_set<Uuid>();
        for (const auto [entity, ui_block] : registry.view<Ui>().each())
        {
            if (!ui_block.document.is_set())
                continue;
            const Uuid key = ui_block.document.is_valid()
                ? ui_block.document.id
                : Uuid {.hi = hash(ui_block.document.path), .lo = ~hash(ui_block.document.path)};
            auto found = g_renderer.ui_documents_by_asset.find(key);
            if (found == g_renderer.ui_documents_by_asset.end())
            {
                auto loaded_id = uint64(0);
                if (const auto document = assets.load_now(ui_block.document))
                {
                    if (const auto shown = ui::load_document(document->get().text))
                        loaded_id = *shown;
                    else
                        warn_once(key, "ui document failed: " + shown.error());
                }
                else
                    warn_once(key, "ui document unavailable: " + document.error());
                found = g_renderer.ui_documents_by_asset.emplace(key, loaded_id).first;
            }
            seen.insert(key);
            if (found->second != 0)
                ui::set_document_visible(
                    found->second,
                    ui_block.is_visible && registry.get<ToyHandle>(entity).is_enabled);
        }
        for (auto it = g_renderer.ui_documents_by_asset.begin();
             it != g_renderer.ui_documents_by_asset.end();)
        {
            if (!seen.contains(it->first))
            {
                if (it->second != 0)
                    ui::unload_document(it->second);
                it = g_renderer.ui_documents_by_asset.erase(it);
            }
            else
                ++it;
        }
        ui::render();
    }
}

namespace tbx
{
    //// RENDER GRAPH ////

    RenderGraph::RenderGraph()
    {
        _passes.push_back(make_shadow_pass());
        _passes.push_back(make_geometry_pass());
        _passes.push_back(make_post_pass());
        _passes.push_back(make_ui_pass());
    }

    void RenderGraph::add_pass(RenderPass pass)
    {
        _passes.push_back(std::move(pass));
    }

    const std::vector<RenderPass>& RenderGraph::get_passes() const
    {
        return _passes;
    }

    void RenderGraph::remove_pass(const std::string_view name)
    {
        for (auto it = _passes.begin(); it != _passes.end(); ++it)
        {
            if (it->name == name)
            {
                _passes.erase(it);
                return;
            }
        }
    }

    void RenderGraph::render(Sandbox& sandbox, Assets& assets)
    {
        for (const RenderPass& pass : _passes)
            if (pass.render)
                pass.render(sandbox, assets);
    }

    void RenderGraph::set_passes(std::vector<RenderPass> passes)
    {
        _passes = std::move(passes);
    }

    RenderPass make_shadow_pass()
    {
        return {.name = "shadow", .render = &gpu::render_shadow_pass};
    }

    RenderPass make_geometry_pass()
    {
        return {.name = "geometry", .render = &gpu::render_geometry_pass};
    }

    RenderPass make_post_pass()
    {
        return {.name = "post", .render = &gpu::render_post_pass};
    }

    RenderPass make_ui_pass()
    {
        return {.name = "ui", .render = &gpu::render_ui_pass};
    }
}
