#include "render_primitives.h"
#include "render_resolve.h"
#include "tbx/app.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/debugging.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/block.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/files/files.h"
#include "tbx/gfx/camera.h"
#include "tbx/gfx/directional_light.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/post_processing.h"
#include "tbx/gfx/render_graph.h"
#include "tbx/gfx/sky.h"
#include "tbx/gfx/state.h"
#include "tbx/math/transform.h"
#include "tbx/platform/window.h"
#include "tbx/reflection/reflection.h"
#include "tbx/serialization/serializers.h"
#include "tbx/ui/ui.h"
#include "tbx/ui/ui_block.h"
#include "tbx/utils/hash.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <vector>

namespace tbx
{
    // Shader source is GLSL for now — when a second gpu backend lands, sources move behind the
    // backend seam alongside gpu.h's implementations. Vertex layout everywhere: position(3) +
    // normal(3) + uv(2). The state is runtime.renderer (gpu/state.h). Asset resolution + the
    // render-failure policy live in render_resolve.{h,cpp}; the builtin primitive meshes in
    // render_primitives.{h,cpp}.

    //// SETUP ////

    /// @brief
    /// Purpose: Reads one builtin shader stage from the engine resources
    /// (resources/Shaders/Tbx) — shaders are files, not string literals.
    static Result<std::string> read_builtin_shader(const char* file_name)
    {
        const auto path = std::filesystem::path(TBX_RESOURCES_PATH) / "Shaders" / "Tbx" / file_name;
        return read_text(path);
    }

    static std::optional<std::reference_wrapper<RenderState>> ensure_renderer_ready(
        RenderState& renderer)
    {
        if (renderer.lit_shader)
            return renderer;
        TBX_ASSERT(
            is_reflection_ready() && is_serialization_ready(),
            "reflection not initialized — initialize_assets() must run before the renderer");
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
            make_render_pipeline({.shader = *renderer.depth_shader, .cull = CullMode::FRONT});
        renderer.lit_pipeline = make_render_pipeline({.shader = *renderer.lit_shader});
        renderer.sky_pipeline = make_render_pipeline(
            {.shader = *renderer.sky_shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE});
        renderer.fallback_pipeline = make_render_pipeline({.shader = *renderer.fallback_shader});
        renderer.cube = upload_mesh_to_gpu(build_cube_vertices(), std::array {3, 3, 2});
        renderer.plane = upload_mesh_to_gpu(build_plane_vertices(), std::array {3, 3, 2});
        renderer.sphere = upload_mesh_to_gpu(build_sphere_vertices(16, 24), std::array {3, 3, 2});
        renderer.fullscreen = upload_mesh_to_gpu(build_fullscreen_vertices(), std::array {3, 3, 2});
        renderer.question_mark =
            upload_mesh_to_gpu(build_question_mark_vertices(), std::array {3, 3, 2});
        const auto ui_composite = read_builtin_shader("ui_composite.frag");
        if (!ui_composite)
            return {};
        renderer.ui_composite_fragment_text = *ui_composite;
        renderer.start_time = std::chrono::steady_clock::now();
        constexpr std::byte WHITE[4] =
            {std::byte {255}, std::byte {255}, std::byte {255}, std::byte {255}};
        renderer.white = upload_texture_to_gpu(1, 1, WHITE);
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
        renderer.checker = upload_texture_to_gpu(64, 64, checker_pixels);
        renderer.shadow_target = make_depth_render_target(renderer.shadow_resolution);
        return renderer;
    }

    //// FRAME CONTEXT (shared between the builtin passes of one frame) ////

    /// @brief
    /// Purpose: Seconds since the renderer started — the shared wall-clock the flashing failure
    /// fallback, the post chain, and the UI composite all animate against.
    static float elapsed_seconds(const RenderState& state)
    {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - state.start_time)
            .count();
    }

    static void refresh_lighting(RenderState& state, Sandbox& sandbox)
    {
        FrameContext& frame = state.frame;
        frame.light_direction = normalize(Vec3(-0.4f, -1.0f, -0.3f));
        frame.light_color = Color {};
        frame.light_intensity = 1.0f;
        bool has_light = false;
        sandbox.for_each_with<DirectionalLight>(
            [&](Toy toy, DirectionalLight& light)
            {
                if (has_light)
                    return; // the first directional light wins
                has_light = true;
                const Mat4 world = toy.get_world_transform();
                frame.light_direction = normalize(Vec3(world * Vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                frame.light_color = light.color;
                frame.light_intensity = light.intensity;
            });
        const Mat4 light_view =
            look_at(-frame.light_direction * 30.0f, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        frame.light_view_projection =
            orthographic(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 100.0f) * light_view;
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
        RenderState& state = ready->get();
        Sandbox& sandbox = context.sandbox;

        if (!state.shadow_target
            || state.shadow_target->get_resolution() != state.shadow_resolution)
            state.shadow_target = make_depth_render_target(state.shadow_resolution);
        begin_render_pass({.depth_target = *state.shadow_target});
        set_render_pipeline(*state.depth_pipeline);
        set_shader_uniform(
            *state.depth_shader,
            "u_light_view_projection",
            state.frame.light_view_projection);
        sandbox.for_each_with<Renderer>(
            [&](Toy toy, Renderer& renderer)
            {
                if (!toy.is_enabled())
                    return;
                set_shader_uniform(*state.depth_shader, "u_model", toy.get_world_transform());
                draw(resolve_mesh(context, state, renderer).mesh);
            });
        end_render_pass();
    }

    /// @brief
    /// Purpose: Draws the scene (sky + every enabled Renderer toy) with the frame context's
    /// current camera — called once per camera by the geometry pass.
    static void draw_scene(RenderContext& context, RenderState& state)
    {
        FrameContext& frame = state.frame;
        Sandbox& sandbox = context.sandbox;

        // Sky: the first Sky block paints the background along the view ray.
        bool has_sky = false;
        sandbox.for_each_with<Sky>(
            [&](Toy, Sky& sky)
            {
                if (has_sky)
                    return;
                has_sky = true;
                const Shader& sky_shader = *state.sky_shader;
                set_render_pipeline(*state.sky_pipeline);
                set_shader_uniform(
                    sky_shader,
                    "u_inverse_view_projection",
                    inverse(frame.view_projection));
                set_shader_uniform(sky_shader, "u_camera_position", frame.camera_position);
                set_shader_uniform(sky_shader, "u_tint", sky.tint);
                set_shader_uniform(sky_shader, "u_sky", 0);
                const auto sky_bindings = std::array {TextureBinding {
                    .slot = 0,
                    .texture = std::cref(
                        resolve_texture_handle(context, state, sky.texture).texture.get())}};
                draw(*state.fullscreen, sky_bindings);
            });

        // Lit + shadowed + textured, material-driven per draw; a broken reference draws its
        // failure mode's loud unlit fallback instead (docs/RenderFailures.md).
        sandbox.for_each_with<Renderer>(
            [&](Toy toy, Renderer& renderer)
            {
                if (!toy.is_enabled())
                    return;
                const ResolvedMesh mesh = resolve_mesh(context, state, renderer);
                auto surface = resolve_surface(context, state, renderer);
                if (mesh.is_failed)
                    surface.failure = RenderFailure::MISSING_MESH;
                if (surface.failure != RenderFailure::NONE)
                {
                    // Emissive, unlit, and flashing so the failure shows at full strength
                    // regardless of scene lighting and visibly blinks for attention; a missing mesh
                    // becomes the question mark, a missing texture shows its color over the debug
                    // checkerboard.
                    const Shader& fallback = *state.fallback_shader;
                    set_render_pipeline(*state.fallback_pipeline);
                    set_shader_uniform(fallback, "u_view_projection", frame.view_projection);
                    set_shader_uniform(fallback, "u_model", toy.get_world_transform());
                    set_shader_uniform(
                        fallback,
                        "u_tint",
                        FAILURE_COLORS[static_cast<size>(surface.failure)]);
                    set_shader_uniform(fallback, "u_albedo", 0);
                    // Same wall-clock source the post/UI passes use; drives the flash in
                    // fallback.frag.
                    const float time_seconds = elapsed_seconds(state);
                    set_shader_uniform(fallback, "u_time", time_seconds);
                    const Mesh& fallback_mesh = surface.failure == RenderFailure::MISSING_MESH
                                                    ? *state.question_mark
                                                    : mesh.mesh.get();
                    const Texture2d& fallback_albedo =
                        surface.failure == RenderFailure::MISSING_TEXTURE ? *state.checker
                                                                          : *state.white;
                    const auto fallback_bindings = std::array {
                        TextureBinding {.slot = 0, .texture = std::cref(fallback_albedo)}};
                    draw(fallback_mesh, fallback_bindings);
                    return;
                }
                const Shader& shader = surface.shader;
                set_render_pipeline(surface.pipeline);
                set_shader_uniform(shader, "u_view_projection", frame.view_projection);
                set_shader_uniform(shader, "u_light_view_projection", frame.light_view_projection);
                set_shader_uniform(shader, "u_light_direction", frame.light_direction);
                set_shader_uniform(shader, "u_light_color", frame.light_color);
                set_shader_uniform(shader, "u_light_intensity", frame.light_intensity);
                set_shader_uniform(shader, "u_camera_position", frame.camera_position);
                set_shader_uniform(shader, "u_shadow_map", 0);
                set_shader_uniform(shader, "u_albedo", 1);
                set_shader_uniform(shader, "u_normal_map", 2);
                set_shader_uniform(shader, "u_metallic_map", 3);
                set_shader_uniform(shader, "u_roughness_map", 4);
                set_shader_uniform(shader, "u_model", toy.get_world_transform());
                set_shader_uniform(shader, "u_tint", surface.tint);
                set_shader_uniform(shader, "u_metallic", surface.metallic);
                set_shader_uniform(shader, "u_roughness", surface.roughness);
                set_shader_uniform(shader, "u_emissive", surface.emissive);
                set_shader_uniform(shader, "u_has_normal_map", surface.normal_map ? 1 : 0);
                set_shader_uniform(shader, "u_has_metallic_map", surface.metallic_map ? 1 : 0);
                set_shader_uniform(shader, "u_has_roughness_map", surface.roughness_map ? 1 : 0);
                set_shader_uniform(shader, "u_uv_scale", surface.uv_scale);
                const auto surface_bindings = std::array {
                    TextureBinding {.slot = 0, .texture = std::cref(*state.shadow_target)},
                    TextureBinding {.slot = 1, .texture = std::cref(surface.albedo.get())},
                    TextureBinding {
                        .slot = 2,
                        .texture = std::cref(
                            surface.normal_map ? surface.normal_map->get() : *state.white)},
                    TextureBinding {
                        .slot = 3,
                        .texture = std::cref(
                            surface.metallic_map ? surface.metallic_map->get() : *state.white)},
                    TextureBinding {
                        .slot = 4,
                        .texture = std::cref(
                            surface.roughness_map ? surface.roughness_map->get() : *state.white)}};
                draw(mesh.mesh, surface_bindings);
            });
    }

    static void render_geometry_pass(RenderContext& context)
    {
        const auto ready = ensure_renderer_ready(context.renderer);
        if (!ready)
            return;
        RenderState& state = ready->get();
        FrameContext& frame = state.frame;
        Sandbox& sandbox = context.sandbox;
        frame.post_chain.clear();
        frame.has_camera = false;

        // A camera aims at a window by name; an empty name means the main window.
        const auto camera_matches_window = [&context](const Camera& camera)
        {
            return camera.window.empty() ? context.is_main : camera.window == context.window.name;
        };
        bool has_any_camera = false;
        sandbox.for_each_with<Camera>(
            [&](Toy toy, Camera& camera)
            {
                has_any_camera =
                    has_any_camera || (toy.is_enabled() && camera_matches_window(camera));
            });
        if (!has_any_camera)
            return; // no camera, no picture

        // When a PostProcessing block lists shaders, the scene renders into an offscreen
        // target and the resolved chain rides the frame context to the post pass (resolved
        // once per frame; the two passes pair). The chain — like the UI — belongs to the
        // main window: its targets are sized to exactly one drawable.
        if (context.is_main)
        {
            bool has_post = false;
            sandbox.for_each_with<PostProcessing>(
                [&](Toy, PostProcessing& post)
                {
                    if (has_post)
                        return; // the first PostProcessing toy wins
                    has_post = true;
                    for (const AssetHandle<ShaderSource>& handle : post.shaders)
                        if (const auto stage = resolve_post_shader(context, state, handle))
                            frame.post_chain.push_back(*stage);
                });
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
                 .clear_color = get_render_clear_color()});
        }

        // Every matching camera renders the scene into its normalized viewport rect.
        sandbox.for_each_with<Camera>(
            [&](Toy toy, Camera& camera)
            {
                if (!toy.is_enabled() || !camera_matches_window(camera))
                    return;
                const int x = static_cast<int>(camera.viewport.x * context.window.width);
                const int y = static_cast<int>(camera.viewport.y * context.window.height);
                const int width = static_cast<int>(camera.viewport.z * context.window.width);
                const int height = static_cast<int>(camera.viewport.w * context.window.height);
                if (width <= 0 || height <= 0)
                    return;
                set_render_viewport(x, y, width, height);

                const Mat4 world = toy.get_world_transform();
                frame.camera_position = Vec3(world * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
                frame.view_projection =
                    get_view_projection(camera, world, static_cast<float>(width) / height);
                frame.has_camera = true;
                draw_scene(context, state);
            });
        // Sub-rect viewports are per camera; the passes after draw the full window.
        set_render_viewport(0, 0, context.window.width, context.window.height);
    }

    static void render_post_pass(RenderContext& context)
    {
        const auto ready = ensure_renderer_ready(context.renderer);
        if (!ready)
            return;
        RenderState& state = ready->get();
        // The geometry pass resolved the chain into the frame context; consume it.
        const auto post_chain = std::move(state.frame.post_chain);
        state.frame.post_chain.clear();
        if (post_chain.empty())
            return;

        // Ping-pong through the chain; the last stage lands on the swapchain.
        const float time_seconds = elapsed_seconds(state);
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
            set_render_pipeline(*stage.pipeline);
            set_shader_uniform(*stage.shader, "u_scene", 0);
            set_shader_uniform(*stage.shader, "u_resolution", resolution);
            set_shader_uniform(*stage.shader, "u_time", time_seconds);
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
        RenderState& state,
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
        entry.pipeline = make_render_pipeline(
            {.shader = *entry.shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE,
             .blend = BlendMode::PREMULTIPLIED});
        return entry;
    }

    static void composite_ui_texture(
        RenderState& state,
        const RenderTarget& target,
        const CompiledPipeline& composite)
    {
        const float time_seconds = elapsed_seconds(state);
        set_render_pipeline(*composite.pipeline);
        set_shader_uniform(*composite.shader, "u_ui", 0);
        set_shader_uniform(*composite.shader, "u_time", time_seconds);
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
        RenderState& state = ready->get();
        Sandbox& sandbox = context.sandbox;
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
                                         const Document& document,
                                         const std::string& vertex_source,
                                         const std::string& fragment_source,
                                         const UI* owner)
        {
            auto& target = state.ui_layer_targets[layer_key];
            if (!target)
                target = make_render_target(width, height);
            draw_ui(context.ui, document, *target, owner);
            if (const auto composite = resolve_ui_composite(state, vertex_source, fragment_source))
                composite_ui_texture(state, *target, composite->get());
        };

        sandbox.for_each_with<UI>(
            [&](Toy toy, UI& ui_block)
            {
                if (!ui_block.document.is_set() || !toy.is_enabled())
                    return;
                const auto document =
                    load_asset_now(context.assets, context.events, ui_block.document);
                if (!document)
                {
                    const Uuid key = ui_block.document.is_valid()
                                         ? ui_block.document.id
                                         : Uuid {
                                               .hi = hash(ui_block.document.path),
                                               .lo = ~hash(ui_block.document.path)};
                    warn_once(state, key, "ui document unavailable: " + document.error());
                    return;
                }
                if (ui_block.mode == UIMode::WORLDSPACE && state.frame.has_camera)
                {
                    // Project the toy (nudged toward the floor) into screen space and feed its
                    // label's anchor slot; behind the camera the label hides.
                    auto world_position =
                        Vec3(toy.get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
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
                    context.ui.bindings["anchor_" + toy.get_name()] = style;
                }

                auto vertex_source = std::string();
                auto fragment_source = std::string();
                if (ui_block.vertex.is_set())
                {
                    if (const auto source =
                            load_asset_now(context.assets, context.events, ui_block.vertex))
                        vertex_source = source->get().text;
                    else
                        warn_once(state, ui_block.vertex.id, "ui vertex shader: " + source.error());
                }
                if (ui_block.fragment.is_set())
                {
                    if (const auto source =
                            load_asset_now(context.assets, context.events, ui_block.fragment))
                        fragment_source = source->get().text;
                    else
                        warn_once(
                            state,
                            ui_block.fragment.id,
                            "ui fragment shader: " + source.error());
                }
                composite_layer(
                    static_cast<uint32>(toy.get_id()),
                    document->get(),
                    vertex_source,
                    fragment_source,
                    &ui_block);
            });

        // The engine overlay is just one more layer with the builtin composite; it has no
        // owning block, so it reads only the global bindings (debug slots).
        const DebuggingState& overlay = context.debug;
        if (overlay.is_open && !overlay.document.text.empty())
            composite_layer(0xFFFFFFFFu, overlay.document, {}, {}, nullptr);
    }

    //// RENDER GRAPH ////

    RenderGraph make_default_render_graph()
    {
        RenderGraph graph;
        graph.passes.push_back(make_shadow_render_pass());
        graph.passes.push_back(make_geometry_shadow_pass());
        graph.passes.push_back(make_post_render_pass());
        graph.passes.push_back(make_ui_render_pass());
        return graph;
    }

    void render(RenderContext& context)
    {
        // render owns the per-window render concerns: bind the window + viewport, open the frame,
        // refresh the frame's view-independent lighting once, then run the render state's graph
        // pass by pass. Lighting is hoisted here (not into a pass) so every graph — even a custom
        // one that drops the shadow or geometry pass — still has correct light data.
        make_current(context.window);
        set_render_viewport(context.window.width, context.window.height);
        begin_render_frame({.clear = Color {.r = 0.05f, .g = 0.05f, .b = 0.08f}});
        refresh_lighting(context.renderer, context.sandbox);
        for (const RenderPass& pass : context.renderer.render_graph.passes)
            if (pass.render)
                pass.render(context);
    }

    RenderPass make_shadow_render_pass()
    {
        return {.name = "shadow", .render = &render_shadow_pass};
    }

    RenderPass make_geometry_shadow_pass()
    {
        return {.name = "geometry", .render = &render_geometry_pass};
    }

    RenderPass make_post_render_pass()
    {
        return {.name = "post", .render = &render_post_pass};
    }

    RenderPass make_ui_render_pass()
    {
        return {.name = "ui", .render = &render_ui_pass};
    }

    void gpu_purge(RenderState& state, const Uuid& asset_id)
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
