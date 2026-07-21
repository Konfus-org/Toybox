#include "tbx/assets/builtin.h"
#include "tbx/core/log.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/gpu.h"
#include "tbx/app.h"
#include "tbx/files/files.h"
#include <chrono>
#include <filesystem>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace tbx::gpu
{
    // Shader source is GLSL for now — when a second gfx backend lands, sources move behind the
    // backend seam alongside gpu.h's implementations. Vertex layout everywhere: position(3) +
    // normal(3) + uv(2).
    /// @brief
    /// Purpose: Lazily-built renderer resources plus per-asset GPU caches (RAII; released at
    /// process exit).
    struct RendererState
    {
        std::unique_ptr<Shader> depth_shader;
        std::unique_ptr<Shader> lit_shader;
        std::unique_ptr<Shader> sky_shader;
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
        std::unordered_map<Uuid, std::unique_ptr<Shader>> shaders_by_fragment;
        std::unordered_map<Uuid, std::unique_ptr<Shader>> post_shaders_by_asset;
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

    //// ASSET RESOLUTION ////

    static const Mesh& resolve_mesh(const MeshRenderer& renderer, Assets* assets)
    {
        if (assets && renderer.model.is_valid())
        {
            const auto cached = g_renderer.meshes_by_asset.find(renderer.model.id);
            if (cached != g_renderer.meshes_by_asset.end())
                return *cached->second;
            if (const auto model = assets->acquire(renderer.model))
            {
                auto uploaded = upload_mesh(model->get().vertices, std::array {3, 3, 2});
                const Mesh& result = *uploaded;
                g_renderer.meshes_by_asset[renderer.model.id] = std::move(uploaded);
                return result;
            }
        }
        if (renderer.mesh == builtin::PLANE)
            return *g_renderer.plane;
        if (renderer.mesh == builtin::SPHERE)
            return *g_renderer.sphere;
        return *g_renderer.cube;
    }

    static const Texture2d& resolve_texture_handle(
        const AssetHandle<Texture>& handle,
        Assets* assets)
    {
        if (assets && handle.is_valid())
        {
            const auto cached = g_renderer.textures_by_asset.find(handle.id);
            if (cached != g_renderer.textures_by_asset.end())
                return *cached->second;
            if (const auto texture = assets->acquire(handle))
            {
                auto uploaded = upload_texture(
                    texture->get().width, texture->get().height, texture->get().pixels);
                const Texture2d& result = *uploaded;
                g_renderer.textures_by_asset[handle.id] = std::move(uploaded);
                return result;
            }
        }
        return *g_renderer.white;
    }

    /// @brief
    /// Purpose: Everything one draw needs after material resolution.
    struct ResolvedSurface
    {
        std::reference_wrapper<const Shader> shader;
        std::reference_wrapper<const Texture2d> texture;
        Color tint = {};
        Json uniforms = {};
    };

    static ResolvedSurface resolve_surface(const MeshRenderer& renderer, Assets* assets)
    {
        auto surface = ResolvedSurface {
            .shader = *g_renderer.lit_shader,
            .texture = resolve_texture_handle(renderer.texture, assets),
            .tint = renderer.tint};
        if (!assets || !renderer.material.is_valid())
            return surface;
        const auto material = assets->acquire(renderer.material);
        if (!material)
        {
            log_warn("material unavailable: {}", material.error());
            return surface;
        }

        // Material tint multiplies the per-toy tint; its texture wins when set.
        const Material& resolved = material->get();
        surface.tint = Color {
            .r = resolved.tint.r * renderer.tint.r,
            .g = resolved.tint.g * renderer.tint.g,
            .b = resolved.tint.b * renderer.tint.b,
            .a = resolved.tint.a * renderer.tint.a};
        if (resolved.texture.is_valid())
            surface.texture = resolve_texture_handle(resolved.texture, assets);
        surface.uniforms = resolved.uniforms;

        // A custom fragment stage pairs with the builtin lit vertex stage, cached by asset.
        if (resolved.fragment.is_valid())
        {
            const auto cached = g_renderer.shaders_by_fragment.find(resolved.fragment.id);
            if (cached != g_renderer.shaders_by_fragment.end())
                surface.shader = *cached->second;
            else if (const auto source = assets->acquire(resolved.fragment))
            {
                auto compiled = compile_shader(
                    g_renderer.lit_vertex_text.c_str(), source->get().text.c_str());
                if (compiled)
                {
                    surface.shader = **compiled;
                    g_renderer.shaders_by_fragment[resolved.fragment.id] = std::move(*compiled);
                }
                else
                    log_warn("material shader failed: {}", compiled.error());
            }
        }
        return surface;
    }

    /// @brief
    /// Purpose: A PostProcessing entry compiled against the builtin post vertex stage, cached
    /// by asset id (failures cache too, so a broken shader logs once, not every frame).
    static std::optional<std::reference_wrapper<const Shader>> resolve_post_shader(
        const AssetHandle<ShaderSource>& handle,
        Assets* assets)
    {
        if (!assets || !handle.is_valid())
            return {};
        const auto cached = g_renderer.post_shaders_by_asset.find(handle.id);
        if (cached != g_renderer.post_shaders_by_asset.end())
        {
            if (!cached->second)
                return {};
            return *cached->second;
        }
        const auto source = assets->acquire(handle);
        if (!source)
        {
            log_warn("post shader unavailable: {}", source.error());
            g_renderer.post_shaders_by_asset[handle.id] = nullptr;
            return {};
        }
        auto compiled =
            compile_shader(g_renderer.post_vertex_text.c_str(), source->get().text.c_str());
        if (!compiled)
        {
            log_warn("post shader failed: {}", compiled.error());
            g_renderer.post_shaders_by_asset[handle.id] = nullptr;
            return {};
        }
        const Shader& result = **compiled;
        g_renderer.post_shaders_by_asset[handle.id] = std::move(*compiled);
        return result;
    }

    //// RENDER ////

    // The raw pointer stays internal: null means "builtin primitives only" (no asset system in
    // play); both public overloads below are the API.
    static void render_internal(Sandbox& sandbox, Assets* assets)
    {
        if (!ensure_renderer_ready())
            return;
        auto& registry = sandbox.get_registry();

        // The first camera wins; no camera, no picture.
        auto view_projection = Mat4(1.0f);
        auto camera_position = Vec3(0.0f, 0.0f, 0.0f);
        bool has_camera = false;
        for (const auto [entity, camera] : registry.view<Camera>().each())
        {
            const Mat4 world = sandbox.get_world_matrix(Toy(sandbox, entity));
            camera_position = Vec3(world * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const float aspect = get_viewport_height() > 0
                ? static_cast<float>(get_viewport_width()) / get_viewport_height()
                : 1.0f;
            const Mat4 projection = math::perspective(
                math::radians(camera.fov_degrees), aspect, camera.near_plane, camera.far_plane);
            view_projection = projection * math::inverse(world);
            has_camera = true;
            break;
        }
        if (!has_camera)
            return;

        // The first directional light is the sun; light looks along its -Z.
        auto light_direction = math::normalize(Vec3(-0.4f, -1.0f, -0.3f));
        auto light_color = Color {};
        float light_intensity = 1.0f;
        for (const auto [entity, light] : registry.view<DirectionalLight>().each())
        {
            const Mat4 world = sandbox.get_world_matrix(Toy(sandbox, entity));
            light_direction = math::normalize(Vec3(world * Vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            light_color = light.color;
            light_intensity = light.intensity;
            break;
        }
        const Mat4 light_view = math::look_at(
            -light_direction * 30.0f,
            Vec3(0.0f, 0.0f, 0.0f),
            Vec3(0.0f, 1.0f, 0.0f));
        const Mat4 light_view_projection =
            math::orthographic(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 100.0f) * light_view;

        // Pass 1: depth from the light.
        begin_depth_pass(*g_renderer.shadow_target);
        set_uniform(*g_renderer.depth_shader, "u_light_view_projection", light_view_projection);
        for (const auto [entity, renderer] : registry.view<MeshRenderer>().each())
        {
            if (!registry.get<ToyHandle>(entity).is_enabled)
                continue;
            set_uniform(
                *g_renderer.depth_shader,
                "u_model",
                sandbox.get_world_matrix(Toy(sandbox, entity)));
            draw(*g_renderer.depth_shader, resolve_mesh(renderer, assets));
        }
        end_depth_pass();

        // When a PostProcessing block lists shaders, the scene renders into an offscreen
        // target and the chain fullscreen-passes it back onto the window at the end.
        auto post_chain = std::vector<std::reference_wrapper<const Shader>>();
        for (const auto [entity, post] : registry.view<PostProcessing>().each())
        {
            for (const AssetHandle<ShaderSource>& handle : post.shaders)
                if (const auto shader = resolve_post_shader(handle, assets))
                    post_chain.push_back(*shader);
            break; // the first PostProcessing toy wins
        }
        const bool has_post = !post_chain.empty();
        if (has_post)
        {
            const int width = get_viewport_width();
            const int height = get_viewport_height();
            if (!g_renderer.post_source || g_renderer.post_source->get_width() != width
                || g_renderer.post_source->get_height() != height)
            {
                g_renderer.post_source = make_render_target(width, height);
                g_renderer.post_swap = make_render_target(width, height);
            }
            begin_render_target(*g_renderer.post_source);
            clear(get_clear_color());
        }

        // Sky: the first Sky block paints the background along the view ray (depth writes
        // off, so the scene draws over it).
        for (const auto [entity, sky] : registry.view<Sky>().each())
        {
            const Shader& sky_shader = *g_renderer.sky_shader;
            set_uniform(sky_shader, "u_inverse_view_projection", math::inverse(view_projection));
            set_uniform(sky_shader, "u_camera_position", camera_position);
            set_uniform(sky_shader, "u_tint", sky.tint);
            set_uniform(sky_shader, "u_sky", 0);
            bind_texture(resolve_texture_handle(sky.texture, assets), 0);
            set_depth_write(false);
            draw(sky_shader, *g_renderer.fullscreen);
            set_depth_write(true);
            break;
        }

        // Pass 2: lit + shadowed + textured, material-driven per draw.
        bind_depth_texture(*g_renderer.shadow_target, 0);
        for (const auto [entity, renderer] : registry.view<MeshRenderer>().each())
        {
            if (!registry.get<ToyHandle>(entity).is_enabled)
                continue;
            const ResolvedSurface surface = resolve_surface(renderer, assets);
            const Shader& shader = surface.shader;
            set_uniform(shader, "u_view_projection", view_projection);
            set_uniform(shader, "u_light_view_projection", light_view_projection);
            set_uniform(shader, "u_light_direction", light_direction);
            set_uniform(shader, "u_light_color", light_color);
            set_uniform(shader, "u_light_intensity", light_intensity);
            set_uniform(shader, "u_camera_position", camera_position);
            set_uniform(shader, "u_shadow_map", 0);
            set_uniform(shader, "u_albedo", 1);
            set_uniform(shader, "u_model", sandbox.get_world_matrix(Toy(sandbox, entity)));
            set_uniform(shader, "u_tint", surface.tint);
            set_uniform(shader, "u_uv_scale", 1.0f);
            apply_uniforms(shader, surface.uniforms); // reflection-typed material extras
            bind_texture(surface.texture, 1);
            draw(shader, resolve_mesh(renderer, assets));
        }

        if (has_post)
        {
            // Ping-pong through the chain; the last pass lands on the window framebuffer.
            const float time_seconds =
                std::chrono::duration<float>(
                    std::chrono::steady_clock::now() - g_renderer.start_time)
                    .count();
            const auto resolution = Vec2(
                static_cast<float>(get_viewport_width()),
                static_cast<float>(get_viewport_height()));
            auto source = std::ref(*g_renderer.post_source);
            auto swap = std::ref(*g_renderer.post_swap);
            set_depth_test(false);
            for (size index = 0; index < post_chain.size(); ++index)
            {
                const bool is_last = index + 1 == post_chain.size();
                if (is_last)
                    end_render_target();
                else
                    begin_render_target(swap);
                const Shader& post_shader = post_chain[index];
                set_uniform(post_shader, "u_scene", 0);
                set_uniform(post_shader, "u_resolution", resolution);
                set_uniform(post_shader, "u_time", time_seconds);
                bind_render_target_texture(source, 0);
                draw(post_shader, *g_renderer.fullscreen);
                if (!is_last)
                    std::swap(source, swap);
            }
            set_depth_test(true);
        }
    }

    void render(Sandbox& sandbox)
    {
        render_internal(sandbox, nullptr);
    }

    void render(Sandbox& sandbox, Assets& assets)
    {
        render_internal(sandbox, &assets);
    }
}
