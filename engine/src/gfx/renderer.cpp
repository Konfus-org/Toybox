#include "tbx/app.h"
#include "tbx/assets/builtin.h"
#include "tbx/debug/debug_view.h"
#include "tbx/debug/log.h"
#include "tbx/reflection/reflection.h"
#include "tbx/ecs/block.h"
#include "tbx/files/files.h"
#include "tbx/gfx/camera.h"
#include "tbx/gfx/directional_light.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/post_processing.h"
#include "tbx/gfx/render_graph.h"
#include "tbx/gfx/renderer_state.h"
#include "tbx/gfx/sky.h"
#include "tbx/math/transform.h"
#include "tbx/ui/ui.h"
#include "tbx/ui/ui_block.h"
#include "tbx/utils/hash.h"
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::gfx
{
    // Shader source is GLSL for now — when a second gfx backend lands, sources move behind the
    // backend seam alongside gpu.h's implementations. Vertex layout everywhere: position(3) +
    // normal(3) + uv(2). The state is runtime.renderer (gfx/renderer_state.h).

    //// FAILURE STATES ////
    // docs/RenderFailures.md: a broken reference never crashes and never hides — the draw
    // shows its mode's loud, unlit, full-strength fallback, and the log says why exactly
    // once per asset.

    /// @brief
    /// Purpose: The five render failure modes, each with a distinct visual.
    enum class RenderFailure : uint8
    {
        NONE = 0,
        SHADER_COMPILE, // magenta
        MISSING_TEXTURE, // cyan over the debug checkerboard
        INVALID_MATERIAL_DATA, // yellow
        MISSING_MATERIAL, // red
        MISSING_MESH // red question-mark mesh
    };

    static constexpr Color FAILURE_COLORS[] = {
        Color {}, // NONE — never drawn
        Color {.r = 1.0f, .g = 0.0f, .b = 1.0f}, // SHADER_COMPILE
        Color {.r = 0.0f, .g = 1.0f, .b = 1.0f}, // MISSING_TEXTURE
        Color {.r = 1.0f, .g = 1.0f, .b = 0.0f}, // INVALID_MATERIAL_DATA
        Color {.r = 1.0f, .g = 0.0f, .b = 0.0f}, // MISSING_MATERIAL
        Color {.r = 1.0f, .g = 0.0f, .b = 0.0f}}; // MISSING_MESH

    /// @brief
    /// Purpose: A mesh choice plus whether it is a failure stand-in.
    struct ResolvedMesh
    {
        std::reference_wrapper<const Mesh> mesh;
        bool is_failed = false;
    };

    /// @brief
    /// Purpose: A texture choice plus whether it is a failure stand-in.
    struct ResolvedTexture
    {
        std::reference_wrapper<const Texture2d> texture;
        bool is_failed = false;
    };

    /// @brief
    /// Purpose: Everything one draw needs after material resolution; a set failure draws the
    /// unlit fallback for that mode instead of the surface's look.
    struct ResolvedSurface
    {
        std::reference_wrapper<const Shader> shader;
        std::reference_wrapper<const Pipeline> pipeline;
        std::reference_wrapper<const Texture2d> albedo;
        std::optional<std::reference_wrapper<const Texture2d>> normal_map = {};
        std::optional<std::reference_wrapper<const Texture2d>> metallic_map = {};
        std::optional<std::reference_wrapper<const Texture2d>> roughness_map = {};
        Color tint = {};
        float metallic = 0.0f;
        float roughness = 0.8f;
        float uv_scale = 1.0f;
        Color emissive = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f};
        RenderFailure failure = RenderFailure::NONE;
    };

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

    static void push_box(std::vector<float>& vertices, const Vec3& center, const Vec3& half)
    {
        const Vec3 normals[6] =
            {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (const Vec3& normal : normals)
        {
            // Build a face basis from the normal; corners wind counter-clockwise. The face
            // axes are axis-aligned, so scaling them component-wise by the half extents
            // shapes the box.
            const Vec3 up = std::abs(normal.y) > 0.5f ? Vec3(0, 0, -normal.y) : Vec3(0, 1, 0);
            const Vec3 right = Vec3(
                up.y * normal.z - up.z * normal.y,
                up.z * normal.x - up.x * normal.z,
                up.x * normal.y - up.y * normal.x);
            const Vec3 face =
                center + Vec3(normal.x * half.x, normal.y * half.y, normal.z * half.z);
            const Vec3 r = Vec3(right.x * half.x, right.y * half.y, right.z * half.z);
            const Vec3 u = Vec3(up.x * half.x, up.y * half.y, up.z * half.z);
            const Vec3 corners[4] = {face - r - u, face + r - u, face + r + u, face - r + u};
            const Vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (const int index : {0, 1, 2, 0, 2, 3})
                push_vertex(vertices, corners[index], normal, uvs[index]);
        }
    }

    static std::vector<float> build_cube_vertices()
    {
        auto vertices = std::vector<float>();
        push_box(vertices, Vec3(0.0f), Vec3(0.5f, 0.5f, 0.5f));
        return vertices;
    }

    static std::vector<float> build_question_mark_vertices()
    {
        // A blocky question mark built from boxes — the unmistakable missing-mesh stand-in
        // (docs/RenderFailures.md), roughly filling the unit-cube footprint.
        auto vertices = std::vector<float>();
        constexpr float DEPTH = 0.07f;
        push_box(vertices, Vec3(0.0f, 0.42f, 0.0f), Vec3(0.22f, 0.07f, DEPTH)); // top of the arc
        push_box(vertices, Vec3(-0.22f, 0.31f, 0.0f), Vec3(0.07f, 0.09f, DEPTH)); // arc start
        push_box(vertices, Vec3(0.22f, 0.25f, 0.0f), Vec3(0.07f, 0.13f, DEPTH)); // arc right side
        push_box(vertices, Vec3(0.07f, 0.12f, 0.0f), Vec3(0.16f, 0.06f, DEPTH)); // hook inward
        push_box(vertices, Vec3(0.0f, -0.06f, 0.0f), Vec3(0.07f, 0.13f, DEPTH)); // stem
        push_box(vertices, Vec3(0.0f, -0.40f, 0.0f), Vec3(0.09f, 0.09f, DEPTH)); // the dot
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

    static std::optional<std::reference_wrapper<RendererState>> ensure_renderer_ready(
        RendererState& renderer)
    {
        if (renderer.lit_shader)
            return renderer;
        reflection::initialize();
        const auto lit_vertex = read_builtin_shader("pbr.vert");
        const auto lit_fragment = read_builtin_shader("pbr.frag");
        const auto depth_vertex = read_builtin_shader("depth.vert");
        const auto depth_fragment = read_builtin_shader("depth.frag");
        const auto sky_vertex = read_builtin_shader("sky.vert");
        const auto sky_fragment = read_builtin_shader("sky.frag");
        const auto post_vertex = read_builtin_shader("post.vert");
        const auto fallback_vertex = read_builtin_shader("fallback.vert");
        const auto fallback_fragment = read_builtin_shader("fallback.frag");
        if (!lit_vertex || !lit_fragment || !depth_vertex || !depth_fragment || !sky_vertex
            || !sky_fragment || !post_vertex || !fallback_vertex || !fallback_fragment)
        {
            TBX_ERROR("builtin shaders missing under resources/Shaders/Tbx");
            return {};
        }
        auto depth = compile_shader(*depth_vertex, *depth_fragment);
        auto lit = compile_shader(*lit_vertex, *lit_fragment);
        auto sky = compile_shader(*sky_vertex, *sky_fragment);
        auto fallback = compile_shader(*fallback_vertex, *fallback_fragment);
        if (!depth || !lit || !sky || !fallback)
        {
            TBX_ERROR(
                "renderer shaders failed: {}",
                !depth ? depth.error()
                       : (!lit ? lit.error() : (!sky ? sky.error() : fallback.error())));
            return {};
        }
        renderer.pbr_vertex_text = *lit_vertex;
        renderer.pbr_fragment_text = *lit_fragment;
        renderer.post_vertex_text = *post_vertex;
        renderer.depth_shader = std::move(*depth);
        renderer.lit_shader = std::move(*lit);
        renderer.sky_shader = std::move(*sky);
        renderer.fallback_shader = std::move(*fallback);
        // Front-face culling in the shadow pass reduces acne on closed meshes; the sky skips
        // depth entirely (drawn first, the scene covers it).
        renderer.depth_pipeline =
            make_pipeline({.shader = *renderer.depth_shader, .cull = CullMode::FRONT});
        renderer.lit_pipeline = make_pipeline({.shader = *renderer.lit_shader});
        renderer.sky_pipeline = make_pipeline(
            {.shader = *renderer.sky_shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE});
        renderer.fallback_pipeline = make_pipeline({.shader = *renderer.fallback_shader});
        renderer.cube = upload_mesh(build_cube_vertices(), std::array {3, 3, 2});
        renderer.plane = upload_mesh(build_plane_vertices(), std::array {3, 3, 2});
        renderer.sphere = upload_mesh(build_sphere_vertices(16, 24), std::array {3, 3, 2});
        renderer.fullscreen = upload_mesh(build_fullscreen_vertices(), std::array {3, 3, 2});
        renderer.question_mark = upload_mesh(build_question_mark_vertices(), std::array {3, 3, 2});
        const auto ui_composite = read_builtin_shader("ui_composite.frag");
        if (!ui_composite)
            return {};
        renderer.ui_composite_fragment_text = *ui_composite;
        renderer.start_time = std::chrono::steady_clock::now();
        constexpr std::byte WHITE[4] =
            {std::byte {255}, std::byte {255}, std::byte {255}, std::byte {255}};
        renderer.white = upload_texture(1, 1, WHITE);
        // 64x64 black/white checkerboard (8px cells) — the missing-texture fallback pattern.
        auto checker_pixels = std::vector<std::byte>(static_cast<size>(64) * 64 * 4);
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 64; ++x)
            {
                const bool is_dark = ((x / 8) + (y / 8)) % 2 == 0;
                const auto value = static_cast<std::byte>(is_dark ? 40 : 255);
                const size at = (static_cast<size>(y) * 64 + x) * 4;
                checker_pixels[at] = value;
                checker_pixels[at + 1] = value;
                checker_pixels[at + 2] = value;
                checker_pixels[at + 3] = std::byte {255};
            }
        }
        renderer.checker = upload_texture(64, 64, checker_pixels);
        renderer.shadow_target = make_depth_target(renderer.shadow_resolution);
        return renderer;
    }

    //// FAILURE FEEDBACK ////

    static void warn_once(RendererState& renderer, const Uuid& id, const std::string& message)
    {
        if (renderer.warned_assets.insert(id).second)
            TBX_WARN("{}", message);
    }

    //// ASSET RESOLUTION ////

    static ResolvedMesh resolve_mesh(
        RenderContext& context,
        RendererState& state,
        const Renderer& renderer)
    {
        if (!renderer.model.is_set() || renderer.model.id == builtin::CUBE.id)
            return {.mesh = *state.cube};
        if (renderer.model.id == builtin::PLANE.id)
            return {.mesh = *state.plane};
        if (renderer.model.id == builtin::SPHERE.id)
            return {.mesh = *state.sphere};

        // Ask the asset system every frame — the reference keeps the asset resident; the GPU
        // upload is only a cache over it (dropped via forget_asset when the asset goes).
        const auto model = assets::load_now(context.assets, context.events, renderer.model);
        if (!model)
        {
            warn_once(state, renderer.model.id, "model unavailable: " + model.error());
            return {.mesh = *state.cube, .is_failed = true};
        }
        const auto cached = state.meshes_by_asset.find(renderer.model.id);
        if (cached != state.meshes_by_asset.end())
            return {.mesh = *cached->second};
        auto uploaded = upload_mesh(model->get().vertices, std::array {3, 3, 2});
        const Mesh& result = *uploaded;
        state.meshes_by_asset[renderer.model.id] = std::move(uploaded);
        return {.mesh = result};
    }

    static ResolvedTexture resolve_texture_handle(
        RenderContext& context,
        RendererState& state,
        const assets::AssetHandle<Texture>& handle)
    {
        if (!handle.is_set())
            return {.texture = *state.white};
        const auto cached = state.textures_by_asset.find(handle.id);
        if (cached != state.textures_by_asset.end())
            return {.texture = *cached->second};
        if (const auto texture = assets::load_now(context.assets, context.events, handle))
        {
            auto uploaded =
                upload_texture(texture->get().width, texture->get().height, texture->get().pixels);
            const Texture2d& result = *uploaded;
            state.textures_by_asset[handle.id] = std::move(uploaded);
            return {.texture = result};
        }
        else
        {
            warn_once(state, handle.id, "texture unavailable: " + texture.error());
            return {.texture = *state.white, .is_failed = true};
        }
    }

    /// @brief
    /// Purpose: The pipeline for a material's shader stages: either stage may be a custom
    /// ShaderSource, the other falls back to the builtin pbr stage; pairs cache together.
    static void resolve_material_shaders(
        RenderContext& context,
        RendererState& state,
        const Material& material,
        ResolvedSurface& surface)
    {
        if (!material.vertex.is_set() && !material.fragment.is_set())
            return;
        const uint64 pair_key = material.vertex.id.lo * 0x9E3779B97F4A7C15ull
                                ^ material.vertex.id.hi ^ ~material.fragment.id.lo
                                ^ material.fragment.id.hi * 3ull;
        const auto cached = state.pipelines_by_shader_pair.find(pair_key);
        if (cached != state.pipelines_by_shader_pair.end())
        {
            if (!cached->second.shader)
            {
                surface.failure = RenderFailure::SHADER_COMPILE;
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
            const auto source = assets::load_now(context.assets, context.events, material.vertex);
            if (!source)
            {
                warn_once(state, material.vertex.id, "material vertex shader: " + source.error());
                surface.failure = RenderFailure::SHADER_COMPILE;
                state.pipelines_by_shader_pair[pair_key] = {};
                return;
            }
            vertex_text = source->get().text;
        }
        else
            vertex_text = state.pbr_vertex_text;
        if (material.fragment.is_set())
        {
            const auto source = assets::load_now(context.assets, context.events, material.fragment);
            if (!source)
            {
                warn_once(
                    state,
                    material.fragment.id,
                    "material fragment shader: " + source.error());
                surface.failure = RenderFailure::SHADER_COMPILE;
                state.pipelines_by_shader_pair[pair_key] = {};
                return;
            }
            fragment_text = source->get().text;
        }
        else
            fragment_text = state.pbr_fragment_text;

        auto compiled = compile_shader(vertex_text, fragment_text);
        if (!compiled)
        {
            warn_once(
                state,
                material.fragment.is_set() ? material.fragment.id : material.vertex.id,
                "material shader failed: " + compiled.error());
            surface.failure = RenderFailure::SHADER_COMPILE;
            state.pipelines_by_shader_pair[pair_key] = {};
            return;
        }
        auto& entry = state.pipelines_by_shader_pair[pair_key];
        entry.shader = std::move(*compiled);
        entry.pipeline = make_pipeline({.shader = *entry.shader});
        surface.shader = *entry.shader;
        surface.pipeline = *entry.pipeline;
    }

    static ResolvedSurface resolve_surface(
        RenderContext& context,
        RendererState& state,
        const Renderer& renderer)
    {
        auto surface = ResolvedSurface {
            .shader = *state.lit_shader,
            .pipeline = *state.lit_pipeline,
            .albedo = *state.white};
        if (!renderer.material.is_set())
            return surface; // the builtin white PBR surface

        const auto material = assets::load_now(context.assets, context.events, renderer.material);
        if (!material)
        {
            warn_once(state, renderer.material.id, "material unavailable: " + material.error());
            surface.failure = RenderFailure::MISSING_MATERIAL;
            return surface;
        }

        const Material& resolved = material->get();
        surface.tint = resolved.albedo;
        surface.metallic = resolved.metallic;
        surface.roughness = resolved.roughness;
        surface.uv_scale = resolved.uv_scale;
        surface.emissive = resolved.emissive;
        // Invalid material data — factors outside [0,1] cannot drive the BRDF. Checked
        // before the textures so a missing texture takes precedence (docs/RenderFailures.md).
        if (resolved.metallic < 0.0f || resolved.metallic > 1.0f || resolved.roughness < 0.0f
            || resolved.roughness > 1.0f || resolved.uv_scale <= 0.0f)
        {
            warn_once(
                state,
                renderer.material.id,
                "material factors out of range: " + resolved.path);
            surface.failure = RenderFailure::INVALID_MATERIAL_DATA;
        }
        if (resolved.albedo_map.is_set())
        {
            const auto albedo_map = resolve_texture_handle(context, state, resolved.albedo_map);
            surface.albedo = albedo_map.texture;
            if (albedo_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
        }
        if (resolved.normal_map.is_set())
        {
            const auto normal_map = resolve_texture_handle(context, state, resolved.normal_map);
            if (normal_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
            else
                surface.normal_map = normal_map.texture;
        }
        if (resolved.metallic_map.is_set())
        {
            const auto metallic_map = resolve_texture_handle(context, state, resolved.metallic_map);
            if (metallic_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
            else
                surface.metallic_map = metallic_map.texture;
        }
        if (resolved.roughness_map.is_set())
        {
            const auto roughness_map =
                resolve_texture_handle(context, state, resolved.roughness_map);
            if (roughness_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
            else
                surface.roughness_map = roughness_map.texture;
        }
        resolve_material_shaders(context, state, resolved, surface);
        return surface;
    }

    /// @brief
    /// Purpose: A PostProcessing entry compiled against the builtin post vertex stage, cached
    /// by asset id (failures cache too, so a broken shader warns once and is skipped).
    static std::optional<std::reference_wrapper<const CompiledPipeline>> resolve_post_shader(
        RenderContext& context,
        RendererState& state,
        const assets::AssetHandle<ShaderSource>& handle)
    {
        if (!handle.is_set())
            return {};
        const auto cached = state.post_shaders_by_asset.find(handle.id);
        if (cached != state.post_shaders_by_asset.end())
        {
            if (!cached->second.shader)
                return {};
            return cached->second;
        }
        const auto source = assets::load_now(context.assets, context.events, handle);
        if (!source)
        {
            warn_once(state, handle.id, "post shader unavailable: " + source.error());
            state.post_shaders_by_asset[handle.id] = {};
            return {};
        }
        auto compiled = compile_shader(state.post_vertex_text, source->get().text);
        if (!compiled)
        {
            warn_once(state, handle.id, "post shader failed: " + compiled.error());
            state.post_shaders_by_asset[handle.id] = {};
            return {};
        }
        auto& entry = state.post_shaders_by_asset[handle.id];
        entry.shader = std::move(*compiled);
        entry.pipeline = make_pipeline(
            {.shader = *entry.shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE});
        return entry;
    }

    //// FRAME CONTEXT (shared between the builtin passes of one frame) ////

    static void refresh_lighting(RendererState& state, ecs::Sandbox& sandbox)
    {
        FrameContext& frame = state.frame;
        auto& registry = sandbox.get_registry();
        frame.light_direction = math::normalize(Vec3(-0.4f, -1.0f, -0.3f));
        frame.light_color = Color {};
        frame.light_intensity = 1.0f;
        for (const auto [entity, light] : registry.view<DirectionalLight>().each())
        {
            const Mat4 world = sandbox.get_world_matrix(ecs::Toy(sandbox, entity));
            frame.light_direction = math::normalize(Vec3(world * Vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            frame.light_color = light.color;
            frame.light_intensity = light.intensity;
            break;
        }
        const Mat4 light_view = math::look_at(
            -frame.light_direction * 30.0f,
            Vec3(0.0f, 0.0f, 0.0f),
            Vec3(0.0f, 1.0f, 0.0f));
        frame.light_view_projection =
            math::orthographic(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 100.0f) * light_view;
    }

    //// BUILTIN PASSES ////

    static void render_shadow_pass(RenderContext& context)
    {
        // The shadow map is view-independent: rendered once per frame with the main window's
        // graph run, reused by every window after it.
        if (!context.is_main)
            return;
        const auto ready = ensure_renderer_ready(context.renderer);
        if (!ready)
            return;
        RendererState& state = ready->get();
        ecs::Sandbox& sandbox = context.sandbox;
        auto& registry = sandbox.get_registry();
        refresh_lighting(state, sandbox);

        if (!state.shadow_target
            || state.shadow_target->get_resolution() != state.shadow_resolution)
            state.shadow_target = make_depth_target(state.shadow_resolution);
        begin_render_pass({.depth_target = *state.shadow_target});
        set_pipeline(*state.depth_pipeline);
        set_uniform(
            *state.depth_shader,
            "u_light_view_projection",
            state.frame.light_view_projection);
        for (const auto [entity, renderer] : registry.view<Renderer>().each())
        {
            if (!registry.get<ecs::ToyHandle>(entity).is_enabled)
                continue;
            set_uniform(
                *state.depth_shader,
                "u_model",
                sandbox.get_world_matrix(ecs::Toy(sandbox, entity)));
            draw(resolve_mesh(context, state, renderer).mesh);
        }
        end_render_pass();
    }

    /// @brief
    /// Purpose: Draws the scene (sky + every enabled Renderer toy) with the frame context's
    /// current camera — called once per camera by the geometry pass.
    static void draw_scene(RenderContext& context, RendererState& state)
    {
        FrameContext& frame = state.frame;
        ecs::Sandbox& sandbox = context.sandbox;
        auto& registry = sandbox.get_registry();

        // Sky: the first Sky block paints the background along the view ray.
        for (const auto [entity, sky] : registry.view<Sky>().each())
        {
            const Shader& sky_shader = *state.sky_shader;
            set_pipeline(*state.sky_pipeline);
            set_uniform(
                sky_shader,
                "u_inverse_view_projection",
                math::inverse(frame.view_projection));
            set_uniform(sky_shader, "u_camera_position", frame.camera_position);
            set_uniform(sky_shader, "u_tint", sky.tint);
            set_uniform(sky_shader, "u_sky", 0);
            const auto sky_bindings = std::array {TextureBinding {
                .slot = 0,
                .texture =
                    std::cref(resolve_texture_handle(context, state, sky.texture).texture.get())}};
            draw(*state.fullscreen, sky_bindings);
            break;
        }

        // Lit + shadowed + textured, material-driven per draw; a broken reference draws its
        // failure mode's loud unlit fallback instead (docs/RenderFailures.md).
        for (const auto [entity, renderer] : registry.view<Renderer>().each())
        {
            if (!registry.get<ecs::ToyHandle>(entity).is_enabled)
                continue;
            const ResolvedMesh mesh = resolve_mesh(context, state, renderer);
            auto surface = resolve_surface(context, state, renderer);
            if (mesh.is_failed)
                surface.failure = RenderFailure::MISSING_MESH;
            if (surface.failure != RenderFailure::NONE)
            {
                // Full strength and unlit so the failure shows regardless of scene lighting;
                // a missing mesh becomes the question mark, a missing texture shows its
                // color over the debug checkerboard.
                const Shader& fallback = *state.fallback_shader;
                set_pipeline(*state.fallback_pipeline);
                set_uniform(fallback, "u_view_projection", frame.view_projection);
                set_uniform(fallback, "u_model", sandbox.get_world_matrix(ecs::Toy(sandbox, entity)));
                set_uniform(fallback, "u_tint", FAILURE_COLORS[static_cast<size>(surface.failure)]);
                set_uniform(fallback, "u_albedo", 0);
                const Mesh& fallback_mesh = surface.failure == RenderFailure::MISSING_MESH
                                                ? *state.question_mark
                                                : mesh.mesh.get();
                const Texture2d& fallback_albedo = surface.failure == RenderFailure::MISSING_TEXTURE
                                                       ? *state.checker
                                                       : *state.white;
                const auto fallback_bindings =
                    std::array {TextureBinding {.slot = 0, .texture = std::cref(fallback_albedo)}};
                draw(fallback_mesh, fallback_bindings);
                continue;
            }
            const Shader& shader = surface.shader;
            set_pipeline(surface.pipeline);
            set_uniform(shader, "u_view_projection", frame.view_projection);
            set_uniform(shader, "u_light_view_projection", frame.light_view_projection);
            set_uniform(shader, "u_light_direction", frame.light_direction);
            set_uniform(shader, "u_light_color", frame.light_color);
            set_uniform(shader, "u_light_intensity", frame.light_intensity);
            set_uniform(shader, "u_camera_position", frame.camera_position);
            set_uniform(shader, "u_shadow_map", 0);
            set_uniform(shader, "u_albedo", 1);
            set_uniform(shader, "u_normal_map", 2);
            set_uniform(shader, "u_metallic_map", 3);
            set_uniform(shader, "u_roughness_map", 4);
            set_uniform(shader, "u_model", sandbox.get_world_matrix(ecs::Toy(sandbox, entity)));
            set_uniform(shader, "u_tint", surface.tint);
            set_uniform(shader, "u_metallic", surface.metallic);
            set_uniform(shader, "u_roughness", surface.roughness);
            set_uniform(shader, "u_emissive", surface.emissive);
            set_uniform(shader, "u_has_normal_map", surface.normal_map ? 1 : 0);
            set_uniform(shader, "u_has_metallic_map", surface.metallic_map ? 1 : 0);
            set_uniform(shader, "u_has_roughness_map", surface.roughness_map ? 1 : 0);
            set_uniform(shader, "u_uv_scale", surface.uv_scale);
            const auto surface_bindings = std::array {
                TextureBinding {.slot = 0, .texture = std::cref(*state.shadow_target)},
                TextureBinding {.slot = 1, .texture = std::cref(surface.albedo.get())},
                TextureBinding {
                    .slot = 2,
                    .texture =
                        std::cref(surface.normal_map ? surface.normal_map->get() : *state.white)},
                TextureBinding {
                    .slot = 3,
                    .texture = std::cref(
                        surface.metallic_map ? surface.metallic_map->get() : *state.white)},
                TextureBinding {
                    .slot = 4,
                    .texture = std::cref(
                        surface.roughness_map ? surface.roughness_map->get() : *state.white)}};
            draw(mesh.mesh, surface_bindings);
        }
    }

    static void render_geometry_pass(RenderContext& context)
    {
        const auto ready = ensure_renderer_ready(context.renderer);
        if (!ready)
            return;
        RendererState& state = ready->get();
        FrameContext& frame = state.frame;
        ecs::Sandbox& sandbox = context.sandbox;
        auto& registry = sandbox.get_registry();
        refresh_lighting(state, sandbox);
        frame.post_chain.clear();
        frame.has_camera = false;

        // A camera aims at a window by name; an empty name means the main window.
        const auto camera_matches_window = [&context](const Camera& camera)
        {
            return camera.window.empty() ? context.is_main : camera.window == context.window.name;
        };
        bool has_any_camera = false;
        for (const auto [entity, camera] : registry.view<Camera>().each())
        {
            if (registry.get<ecs::ToyHandle>(entity).is_enabled && camera_matches_window(camera))
            {
                has_any_camera = true;
                break;
            }
        }
        if (!has_any_camera)
            return; // no camera, no picture

        // When a PostProcessing block lists shaders, the scene renders into an offscreen
        // target and the resolved chain rides the frame context to the post pass (resolved
        // once per frame; the two passes pair). The chain — like the UI — belongs to the
        // main window: its targets are sized to exactly one drawable.
        if (context.is_main)
        {
            for (const auto [entity, post] : registry.view<PostProcessing>().each())
            {
                for (const assets::AssetHandle<ShaderSource>& handle : post.shaders)
                    if (const auto stage = resolve_post_shader(context, state, handle))
                        frame.post_chain.push_back(*stage);
                break; // the first PostProcessing toy wins
            }
        }
        if (!frame.post_chain.empty())
        {
            const int width = context.window.width;
            const int height = context.window.height;
            if (!state.post_source || state.post_source->get_width() != width
                || state.post_source->get_height() != height)
            {
                state.post_source = make_render_target(width, height);
                state.post_swap = make_render_target(width, height);
            }
            begin_render_pass(
                {.color_target = *state.post_source,
                 .load = LoadOperation::CLEAR,
                 .clear_color = get_clear_color()});
        }

        // Every matching camera renders the scene into its normalized viewport rect.
        for (const auto [entity, camera] : registry.view<Camera>().each())
        {
            if (!registry.get<ecs::ToyHandle>(entity).is_enabled || !camera_matches_window(camera))
                continue;
            const int x = static_cast<int>(camera.viewport.x * context.window.width);
            const int y = static_cast<int>(camera.viewport.y * context.window.height);
            const int width = static_cast<int>(camera.viewport.z * context.window.width);
            const int height = static_cast<int>(camera.viewport.w * context.window.height);
            if (width <= 0 || height <= 0)
                continue;
            set_viewport(x, y, width, height);

            const Mat4 world = sandbox.get_world_matrix(ecs::Toy(sandbox, entity));
            frame.camera_position = Vec3(world * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const Mat4 projection = math::perspective(
                math::radians(camera.fov_degrees),
                static_cast<float>(width) / height,
                camera.near_plane,
                camera.far_plane);
            frame.view_projection = projection * math::inverse(world);
            frame.has_camera = true;
            draw_scene(context, state);
        }
        // Sub-rect viewports are per camera; the passes after draw the full window.
        set_viewport(0, 0, context.window.width, context.window.height);
    }

    static void render_post_pass(RenderContext& context)
    {
        const auto ready = ensure_renderer_ready(context.renderer);
        if (!ready)
            return;
        RendererState& state = ready->get();
        // The geometry pass resolved the chain into the frame context; consume it.
        const auto post_chain = std::move(state.frame.post_chain);
        state.frame.post_chain.clear();
        if (post_chain.empty())
            return;

        // Ping-pong through the chain; the last stage lands on the swapchain.
        const float time_seconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - state.start_time)
                .count();
        const auto resolution = Vec2(
            static_cast<float>(context.window.width),
            static_cast<float>(context.window.height));
        auto source = std::ref(*state.post_source);
        auto swap = std::ref(*state.post_swap);
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
            draw(*state.fullscreen, post_bindings);
            end_render_pass();
            if (!is_last)
                std::swap(source, swap);
        }
    }

    /// @brief
    /// Purpose: The composite pipeline for one Ui block's custom stages (vertex vs the
    /// builtin fullscreen stage, fragment vs the builtin ui composite), cached by pair.
    static std::optional<std::reference_wrapper<const CompiledPipeline>> resolve_ui_composite(
        RendererState& state,
        const std::string& vertex_source,
        const std::string& fragment_source)
    {
        const uint64 key =
            hash(std::string_view(vertex_source)) ^ ~hash(std::string_view(fragment_source));
        const auto cached = state.ui_composites_by_pair.find(key);
        if (cached != state.ui_composites_by_pair.end())
        {
            if (!cached->second.shader)
                return {};
            return cached->second;
        }
        const std::string& vertex = vertex_source.empty() ? state.post_vertex_text : vertex_source;
        const std::string& fragment =
            fragment_source.empty() ? state.ui_composite_fragment_text : fragment_source;
        auto compiled = compile_shader(vertex, fragment);
        if (!compiled)
        {
            TBX_ERROR("ui composite shader failed: {}", compiled.error());
            state.ui_composites_by_pair[key] = {};
            return {};
        }
        auto& entry = state.ui_composites_by_pair[key];
        entry.shader = std::move(*compiled);
        entry.pipeline = make_pipeline(
            {.shader = *entry.shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE,
             .blend = BlendMode::PREMULTIPLIED});
        return entry;
    }

    static void composite_ui_texture(
        RendererState& state,
        const RenderTarget& target,
        const CompiledPipeline& composite)
    {
        const float time_seconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - state.start_time)
                .count();
        set_pipeline(*composite.pipeline);
        set_uniform(*composite.shader, "u_ui", 0);
        set_uniform(*composite.shader, "u_time", time_seconds);
        const auto bindings = std::array {TextureBinding {.slot = 0, .texture = std::cref(target)}};
        draw(*state.fullscreen, bindings);
    }

    static void render_ui_pass(RenderContext& context)
    {
        // The UI (and the debug overlay riding it) belongs to the main window.
        if (!context.is_main)
            return;
        const auto ready = ensure_renderer_ready(context.renderer);
        if (!ready)
            return;
        RendererState& state = ready->get();
        ecs::Sandbox& sandbox = context.sandbox;
        const int width = context.window.width;
        const int height = context.window.height;
        if (!state.ui_layer_targets.empty())
        {
            const auto& first = state.ui_layer_targets.begin()->second;
            if (first->get_width() != width || first->get_height() != height)
                state.ui_layer_targets.clear();
        }

        // No queues: each layer (every enabled Ui block, plus the debug overlay) draws to
        // its own target and composites through its own gpu pipeline — custom stages when
        // the block names them, the builtin ui composite otherwise.
        const auto composite_layer = [&](const uint32 layer_key,
                                         const UiDocument& document,
                                         const std::string& vertex_source,
                                         const std::string& fragment_source)
        {
            auto& target = state.ui_layer_targets[layer_key];
            if (!target)
                target = make_render_target(width, height);
            ui::draw(context.ui, document, *target);
            if (const auto composite = resolve_ui_composite(state, vertex_source, fragment_source))
                composite_ui_texture(state, *target, composite->get());
        };

        auto& registry = sandbox.get_registry();
        for (const auto [entity, ui_block] : registry.view<Ui>().each())
        {
            if (!ui_block.document.is_set() || !registry.get<ecs::ToyHandle>(entity).is_enabled)
                continue;
            const auto document =
                assets::load_now(context.assets, context.events, ui_block.document);
            if (!document)
            {
                const Uuid key = ui_block.document.is_valid()
                                     ? ui_block.document.id
                                     : Uuid {
                                           .hi = hash(ui_block.document.path),
                                           .lo = ~hash(ui_block.document.path)};
                warn_once(state, key, "ui document unavailable: " + document.error());
                continue;
            }
            if (ui_block.is_world_anchored && state.frame.has_camera)
            {
                // Project the toy (nudged toward the floor) into screen space and feed its
                // label's anchor slot; behind the camera the label hides.
                auto world_position = Vec3(
                    sandbox.get_world_matrix(ecs::Toy(sandbox, entity)) * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
                world_position.y -= 1.2f;
                const Vec4 clip = state.frame.view_projection * Vec4(world_position, 1.0f);
                // Both states spell out display: RmlUi's style attribute only SETS the
                // properties it parses — a property from an earlier style string (the
                // display: none while behind the camera) is never removed, so coming back
                // into view must explicitly set it visible again.
                auto style = std::string("display: none;");
                if (clip.w > 0.05f)
                {
                    const float screen_x = (clip.x / clip.w * 0.5f + 0.5f) * width;
                    const float screen_y = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * height;
                    style = std::format(
                        "left: {}px; top: {}px; display: block;",
                        static_cast<int>(screen_x) - 80,
                        static_cast<int>(screen_y));
                }
                context.ui.bindings["anchor_" + registry.get<ecs::ToyHandle>(entity).name] = style;
            }

            auto vertex_source = std::string();
            auto fragment_source = std::string();
            if (ui_block.vertex.is_set())
            {
                if (const auto source =
                        assets::load_now(context.assets, context.events, ui_block.vertex))
                    vertex_source = source->get().text;
                else
                    warn_once(state, ui_block.vertex.id, "ui vertex shader: " + source.error());
            }
            if (ui_block.fragment.is_set())
            {
                if (const auto source =
                        assets::load_now(context.assets, context.events, ui_block.fragment))
                    fragment_source = source->get().text;
                else
                    warn_once(state, ui_block.fragment.id, "ui fragment shader: " + source.error());
            }
            composite_layer(
                static_cast<uint32>(entity),
                document->get(),
                vertex_source,
                fragment_source);
        }

        // The engine overlay is just one more layer with the builtin composite.
        const debug::view::DebugState& overlay = context.debug;
        if (overlay.is_open && !overlay.document.source.empty())
            composite_layer(0xFFFFFFFFu, overlay.document, {}, {});
    }

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

    void RenderGraph::render(RenderContext& context)
    {
        gfx::begin_frame({.clear = Color {.r = 0.05f, .g = 0.05f, .b = 0.08f}});
        for (const RenderPass& pass : _passes)
            if (pass.render)
                pass.render(context);
    }

    void RenderGraph::set_passes(std::vector<RenderPass> passes)
    {
        _passes = std::move(passes);
    }

    RenderPass make_shadow_pass()
    {
        return {.name = "shadow", .render = &gfx::render_shadow_pass};
    }

    RenderPass make_geometry_pass()
    {
        return {.name = "geometry", .render = &gfx::render_geometry_pass};
    }

    RenderPass make_post_pass()
    {
        return {.name = "post", .render = &gfx::render_post_pass};
    }

    RenderPass make_ui_pass()
    {
        return {.name = "ui", .render = &gfx::render_ui_pass};
    }

    void forget_asset(gfx::RendererState& state, const Uuid& asset_id)
    {
        state.meshes_by_asset.erase(asset_id);
        state.textures_by_asset.erase(asset_id);
        state.post_shaders_by_asset.erase(asset_id);
        // Material pipelines key on shader pairs; the whole cache rebuilds lazily. UI
        // documents cache by content behind the ui boundary and refresh on their own.
        state.pipelines_by_shader_pair.clear();
        state.warned_assets.erase(asset_id); // a fresh copy earns a fresh warning
    }
}
