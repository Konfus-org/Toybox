#include "tbx/app.h"
#include "tbx/debug/debug_view.h"
#include "tbx/assets/builtin.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/block.h"
#include "tbx/files/files.h"
#include "tbx/gfx/camera.h"
#include "tbx/gfx/directional_light.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/post_processing.h"
#include "tbx/gfx/render_graph.h"
#include "tbx/gfx/sky.h"
#include "tbx/math/transform.h"
#include "tbx/ui/ui.h"
#include "tbx/ui/ui_block.h"
#include <array>
#include <chrono>
#include <format>
#include <cmath>
#include <filesystem>
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
        std::unique_ptr<RenderTarget> ui_target;
        CompiledPipeline ui_composite;
        std::unordered_map<Uuid, std::unique_ptr<Mesh>> meshes_by_asset;
        std::unordered_map<Uuid, std::unique_ptr<Texture2d>> textures_by_asset;
        std::unordered_map<uint64, CompiledPipeline> pipelines_by_shader_pair;
        std::unordered_map<Uuid, CompiledPipeline> post_shaders_by_asset;
        std::unordered_set<Uuid> warned_assets;
        int shadow_resolution = 2048;
        std::string pbr_vertex_text;
        std::string pbr_fragment_text;
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
        const Vec3 corners[4] =
            {{-0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, -0.5f}, {-0.5f, 0.0f, -0.5f}};
        const Vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (const int index : {0, 1, 2, 0, 2, 3})
            push_vertex(vertices, corners[index], up, uvs[index]);
        return vertices;
    }

    static std::vector<float> build_cube_vertices()
    {
        auto vertices = std::vector<float>();
        const Vec3 normals[6] =
            {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
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
        const auto path = std::filesystem::path(TBX_RESOURCES_PATH) / "Shaders" / "Tbx" / file_name;
        return files::read_text(path);
    }

    static bool ensure_renderer_ready()
    {
        if (g_renderer.lit_shader)
            return true;
        register_builtin_blocks();
        const auto lit_vertex = read_builtin_shader("pbr.vert");
        const auto lit_fragment = read_builtin_shader("pbr.frag");
        const auto depth_vertex = read_builtin_shader("depth.vert");
        const auto depth_fragment = read_builtin_shader("depth.frag");
        const auto sky_vertex = read_builtin_shader("sky.vert");
        const auto sky_fragment = read_builtin_shader("sky.frag");
        const auto post_vertex = read_builtin_shader("post.vert");
        if (!lit_vertex || !lit_fragment || !depth_vertex || !depth_fragment || !sky_vertex
            || !sky_fragment || !post_vertex)
        {
            TBX_ERROR("builtin shaders missing under resources/Shaders/Tbx");
            return false;
        }
        auto depth = compile_shader(depth_vertex->c_str(), depth_fragment->c_str());
        auto lit = compile_shader(lit_vertex->c_str(), lit_fragment->c_str());
        auto sky = compile_shader(sky_vertex->c_str(), sky_fragment->c_str());
        if (!depth || !lit || !sky)
        {
            TBX_ERROR(
                "renderer shaders failed: {}",
                !depth ? depth.error() : (!lit ? lit.error() : sky.error()));
            return false;
        }
        g_renderer.pbr_vertex_text = *lit_vertex;
        g_renderer.pbr_fragment_text = *lit_fragment;
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
        constexpr std::byte WHITE[4] =
            {std::byte {255}, std::byte {255}, std::byte {255}, std::byte {255}};
        g_renderer.white = upload_texture(1, 1, WHITE);
        g_renderer.shadow_target = make_depth_target(g_renderer.shadow_resolution);
        return true;
    }

    //// FAILURE STATES ////
    // Old-Toybox style: a broken reference never crashes and never hides — the draw flashes a
    // color naming the failure, and the log says why exactly once per asset.

    static constexpr Color FAILURE_MODEL = Color {.r = 1.0f, .g = 0.0f, .b = 1.0f}; // magenta
    static constexpr Color FAILURE_TEXTURE = Color {.r = 1.0f, .g = 1.0f, .b = 0.0f}; // yellow
    static constexpr Color FAILURE_MATERIAL = Color {.r = 1.0f, .g = 0.0f, .b = 0.0f}; // red
    static constexpr Color FAILURE_SHADER = Color {.r = 0.0f, .g = 1.0f, .b = 1.0f}; // cyan

    static void warn_once(const Uuid& id, const std::string& message)
    {
        if (g_renderer.warned_assets.insert(id).second)
            TBX_WARN("{}", message);
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
        if (!renderer.model.is_set() || renderer.model.id == builtin::CUBE.id)
            return {.mesh = *g_renderer.cube};
        if (renderer.model.id == builtin::PLANE.id)
            return {.mesh = *g_renderer.plane};
        if (renderer.model.id == builtin::SPHERE.id)
            return {.mesh = *g_renderer.sphere};

        // Ask the asset system every frame — the reference keeps the asset resident; the GPU
        // upload is only a cache over it (dropped via forget_asset when the asset goes).
        const auto model = assets.load_now(renderer.model);
        if (!model)
        {
            warn_once(renderer.model.id, "model unavailable: " + model.error());
            return {.mesh = *g_renderer.cube, .is_failed = true};
        }
        const auto cached = g_renderer.meshes_by_asset.find(renderer.model.id);
        if (cached != g_renderer.meshes_by_asset.end())
            return {.mesh = *cached->second};
        auto uploaded = upload_mesh(model->get().vertices, std::array {3, 3, 2});
        const Mesh& result = *uploaded;
        g_renderer.meshes_by_asset[renderer.model.id] = std::move(uploaded);
        return {.mesh = result};
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
            auto uploaded =
                upload_texture(texture->get().width, texture->get().height, texture->get().pixels);
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
        std::reference_wrapper<const Texture2d> albedo;
        std::optional<std::reference_wrapper<const Texture2d>> normal_map = {};
        std::optional<std::reference_wrapper<const Texture2d>> metallic_roughness_map = {};
        Color tint = {};
        float metallic = 0.0f;
        float roughness = 0.8f;
        Color emissive = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f};
        Json uniforms = {};
        std::optional<Color> failure = {};
    };

    /// @brief
    /// Purpose: The pipeline for a material's shader stages: either stage may be a custom
    /// ShaderSource, the other falls back to the builtin pbr stage; pairs cache together.
    static void resolve_material_shaders(
        const Material& material,
        ResolvedSurface& surface,
        Assets& assets)
    {
        if (!material.vertex.is_set() && !material.fragment.is_set())
            return;
        const uint64 pair_key = material.vertex.id.lo * 0x9E3779B97F4A7C15ull
            ^ material.vertex.id.hi ^ ~material.fragment.id.lo ^ material.fragment.id.hi * 3ull;
        const auto cached = g_renderer.pipelines_by_shader_pair.find(pair_key);
        if (cached != g_renderer.pipelines_by_shader_pair.end())
        {
            if (!cached->second.shader)
            {
                surface.failure = FAILURE_SHADER;
                return;
            }
            surface.shader = *cached->second.shader;
            surface.pipeline = *cached->second.pipeline;
            return;
        }

        auto vertex_text = std::string();
        auto fragment_text = std::string();
        if (material.vertex.is_set())
        {
            const auto source = assets.load_now(material.vertex);
            if (!source)
            {
                warn_once(material.vertex.id, "material vertex shader: " + source.error());
                surface.failure = FAILURE_SHADER;
                g_renderer.pipelines_by_shader_pair[pair_key] = {};
                return;
            }
            vertex_text = source->get().text;
        }
        else
            vertex_text = g_renderer.pbr_vertex_text;
        if (material.fragment.is_set())
        {
            const auto source = assets.load_now(material.fragment);
            if (!source)
            {
                warn_once(material.fragment.id, "material fragment shader: " + source.error());
                surface.failure = FAILURE_SHADER;
                g_renderer.pipelines_by_shader_pair[pair_key] = {};
                return;
            }
            fragment_text = source->get().text;
        }
        else
            fragment_text = g_renderer.pbr_fragment_text;

        auto compiled = compile_shader(vertex_text.c_str(), fragment_text.c_str());
        if (!compiled)
        {
            warn_once(
                material.fragment.is_set() ? material.fragment.id : material.vertex.id,
                "material shader failed: " + compiled.error());
            surface.failure = FAILURE_SHADER;
            g_renderer.pipelines_by_shader_pair[pair_key] = {};
            return;
        }
        auto& entry = g_renderer.pipelines_by_shader_pair[pair_key];
        entry.shader = std::move(*compiled);
        entry.pipeline = make_pipeline({.shader = *entry.shader});
        surface.shader = *entry.shader;
        surface.pipeline = *entry.pipeline;
    }

    static ResolvedSurface resolve_surface(const Renderer& renderer, Assets& assets)
    {
        auto surface = ResolvedSurface {
            .shader = *g_renderer.lit_shader,
            .pipeline = *g_renderer.lit_pipeline,
            .albedo = *g_renderer.white};
        if (!renderer.material.is_set())
            return surface; // the builtin white PBR surface

        const auto material = assets.load_now(renderer.material);
        if (!material)
        {
            warn_once(renderer.material.id, "material unavailable: " + material.error());
            surface.failure = FAILURE_MATERIAL;
            return surface;
        }

        const Material& resolved = material->get();
        surface.tint = resolved.albedo;
        surface.metallic = resolved.metallic;
        surface.roughness = resolved.roughness;
        surface.emissive = resolved.emissive;
        surface.uniforms = resolved.uniforms;
        if (resolved.albedo_map.is_set())
        {
            const auto albedo_map = resolve_texture_handle(resolved.albedo_map, assets);
            surface.albedo = albedo_map.texture;
            if (albedo_map.is_failed)
                surface.failure = FAILURE_TEXTURE;
        }
        if (resolved.normal_map.is_set())
        {
            const auto normal_map = resolve_texture_handle(resolved.normal_map, assets);
            if (normal_map.is_failed)
                surface.failure = FAILURE_TEXTURE;
            else
                surface.normal_map = normal_map.texture;
        }
        if (resolved.metallic_roughness_map.is_set())
        {
            const auto mr_map = resolve_texture_handle(resolved.metallic_roughness_map, assets);
            if (mr_map.is_failed)
                surface.failure = FAILURE_TEXTURE;
            else
                surface.metallic_roughness_map = mr_map.texture;
        }
        resolve_material_shaders(resolved, surface, assets);
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
            const float aspect =
                get_viewport_height() > 0
                    ? static_cast<float>(get_viewport_width()) / get_viewport_height()
                    : 1.0f;
            const Mat4 projection = math::perspective(
                math::radians(camera.fov_degrees),
                aspect,
                camera.near_plane,
                camera.far_plane);
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
            g_frame.light_direction = math::normalize(Vec3(world * Vec4(0.0f, 0.0f, -1.0f, 0.0f)));
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
            *g_renderer.depth_shader,
            "u_light_view_projection",
            g_frame.light_view_projection);
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
                sky_shader,
                "u_inverse_view_projection",
                math::inverse(g_frame.view_projection));
            set_uniform(sky_shader, "u_camera_position", g_frame.camera_position);
            set_uniform(sky_shader, "u_tint", sky.tint);
            set_uniform(sky_shader, "u_sky", 0);
            const auto sky_bindings = std::array {TextureBinding {
                .slot = 0,
                .texture = std::cref(resolve_texture_handle(sky.texture, assets).texture.get())}};
            draw(*g_renderer.fullscreen, sky_bindings);
            break;
        }

        // Lit + shadowed + textured, material-driven per draw; failures flash their color.
        const float flash = get_failure_flash();
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
                surface.albedo = *g_renderer.white;
                surface.normal_map = {};
                surface.metallic_roughness_map = {};
                surface.emissive = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f};
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
            set_uniform(shader, "u_normal_map", 2);
            set_uniform(shader, "u_metallic_roughness_map", 3);
            set_uniform(shader, "u_model", sandbox.get_world_matrix(Toy(sandbox, entity)));
            set_uniform(shader, "u_tint", surface.tint);
            set_uniform(shader, "u_metallic", surface.metallic);
            set_uniform(shader, "u_roughness", surface.roughness);
            set_uniform(shader, "u_emissive", surface.emissive);
            set_uniform(shader, "u_has_normal_map", surface.normal_map ? 1 : 0);
            set_uniform(
                shader,
                "u_has_metallic_roughness_map",
                surface.metallic_roughness_map ? 1 : 0);
            set_uniform(shader, "u_uv_scale", 1.0f);
            apply_uniforms(shader, surface.uniforms); // reflection-typed material extras
            const auto surface_bindings = std::array {
                TextureBinding {.slot = 0, .texture = std::cref(*g_renderer.shadow_target)},
                TextureBinding {.slot = 1, .texture = std::cref(surface.albedo.get())},
                TextureBinding {
                    .slot = 2,
                    .texture = std::cref(
                        surface.normal_map ? surface.normal_map->get() : *g_renderer.white)},
                TextureBinding {
                    .slot = 3,
                    .texture = std::cref(
                        surface.metallic_roughness_map ? surface.metallic_roughness_map->get()
                                                       : *g_renderer.white)}};
            draw(mesh.mesh, surface_bindings);
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
            const auto post_bindings =
                std::array {TextureBinding {.slot = 0, .texture = std::cref(source.get())}};
            draw(*g_renderer.fullscreen, post_bindings);
            end_render_pass();
            if (!is_last)
                std::swap(source, swap);
        }
    }

    static void render_ui_pass(Sandbox& sandbox, Assets& assets)
    {
        if (!ensure_renderer_ready())
            return;

        // Several small steps, pass-composable: queue every enabled Ui block's document (and
        // the debug overlay), render the queue into the UI texture, then composite that
        // texture over the frame with the engine ui shaders. A pass could just as well post-
        // process the texture or map it into the world instead.
        auto& registry = sandbox.get_registry();
        for (const auto [entity, ui_block] : registry.view<Ui>().each())
        {
            if (!ui_block.document.is_set() || !ui_block.is_visible
                || !registry.get<ToyHandle>(entity).is_enabled)
                continue;
            if (const auto document = assets.load_now(ui_block.document))
            {
                auto vertex_source = std::string();
                auto fragment_source = std::string();
                if (ui_block.vertex.is_set())
                {
                    if (const auto source = assets.load_now(ui_block.vertex))
                        vertex_source = source->get().text;
                    else
                        warn_once(ui_block.vertex.id, "ui vertex shader: " + source.error());
                }
                if (ui_block.fragment.is_set())
                {
                    if (const auto source = assets.load_now(ui_block.fragment))
                        fragment_source = source->get().text;
                    else
                        warn_once(
                            ui_block.fragment.id, "ui fragment shader: " + source.error());
                }
                if (ui_block.is_world_anchored && g_frame.has_camera)
                {
                    // Project the toy (nudged toward the floor) into screen space and feed
                    // its label's anchor slot; behind the camera the label hides.
                    auto world_position =
                        Vec3(sandbox.get_world_matrix(Toy(sandbox, entity))
                             * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
                    world_position.y -= 1.2f;
                    const Vec4 clip =
                        g_frame.view_projection * Vec4(world_position, 1.0f);
                    auto style = std::string("display: none;");
                    if (clip.w > 0.05f)
                    {
                        const float screen_x =
                            (clip.x / clip.w * 0.5f + 0.5f) * get_viewport_width();
                        const float screen_y =
                            (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * get_viewport_height();
                        style = std::format(
                            "left: {}px; top: {}px;",
                            static_cast<int>(screen_x) - 80,
                            static_cast<int>(screen_y));
                    }
                    ui::set_string(
                        "anchor_" + registry.get<ToyHandle>(entity).name, style);
                }
                ui::draw(document->get(), vertex_source, fragment_source);
            }
            else
            {
                const Uuid key = ui_block.document.is_valid()
                    ? ui_block.document.id
                    : Uuid {
                          .hi = hash(ui_block.document.path),
                          .lo = ~hash(ui_block.document.path)};
                warn_once(key, "ui document unavailable: " + document.error());
            }
        }
        debug::draw(); // the engine overlay rides the same pass

        const int width = get_viewport_width();
        const int height = get_viewport_height();
        if (!g_renderer.ui_target || g_renderer.ui_target->get_width() != width
            || g_renderer.ui_target->get_height() != height)
            g_renderer.ui_target = make_render_target(width, height);
        ui::draw_to(*g_renderer.ui_target);

        if (!g_renderer.ui_composite.shader)
        {
            const auto composite = read_builtin_shader("ui_composite.frag");
            if (!composite)
            {
                TBX_ERROR("ui composite shader missing under resources/Shaders/Tbx");
                return;
            }
            auto compiled = compile_shader(
                g_renderer.post_vertex_text.c_str(), composite->c_str());
            if (!compiled)
            {
                TBX_ERROR("ui composite shader failed: {}", compiled.error());
                return;
            }
            g_renderer.ui_composite.shader = std::move(*compiled);
            g_renderer.ui_composite.pipeline = make_pipeline(
                {.shader = *g_renderer.ui_composite.shader,
                 .is_depth_test_enabled = false,
                 .is_depth_write_enabled = false,
                 .cull = CullMode::NONE,
                 .blend = BlendMode::PREMULTIPLIED});
        }
        set_pipeline(*g_renderer.ui_composite.pipeline);
        set_uniform(*g_renderer.ui_composite.shader, "u_ui", 0);
        const auto ui_bindings = std::array {
            TextureBinding {.slot = 0, .texture = std::cref(*g_renderer.ui_target)}};
        draw(*g_renderer.fullscreen, ui_bindings);
    }

    void render(Sandbox& sandbox)
    {
        if (!is_app_running())
            return;
        begin_frame();
        get_render_graph().render(sandbox, get_assets());
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
        gpu::begin_frame({.clear = Color {.r = 0.05f, .g = 0.05f, .b = 0.08f}});
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

    void set_shadow_resolution(const int resolution)
    {
        if (resolution <= 0 || resolution == gpu::g_renderer.shadow_resolution)
            return;
        gpu::g_renderer.shadow_resolution = resolution;
        if (gpu::g_renderer.shadow_target)
            gpu::g_renderer.shadow_target = gpu::make_depth_target(resolution);
    }

    void forget_asset(const Uuid& asset_id)
    {
        gpu::g_renderer.meshes_by_asset.erase(asset_id);
        gpu::g_renderer.textures_by_asset.erase(asset_id);
        gpu::g_renderer.post_shaders_by_asset.erase(asset_id);
        // Material pipelines key on shader pairs; the whole cache rebuilds lazily. UI
        // documents cache by content behind the ui boundary and refresh on their own.
        gpu::g_renderer.pipelines_by_shader_pair.clear();
        gpu::g_renderer.warned_assets.erase(asset_id); // a fresh copy earns a fresh warning
    }
}
