#pragma once
#include "tbx/assets/assets.h"
#include "tbx/core/color.h"
#include "tbx/core/math.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gfx/depth_target.h"
#include "tbx/gfx/mesh.h"
#include "tbx/gfx/pipeline.h"
#include "tbx/gfx/render_pass.h"
#include "tbx/gfx/render_target.h"
#include "tbx/gfx/shader.h"
#include "tbx/gfx/texture2d.h"
#include "tbx/gfx/ui_vertex.h"
#include "tbx/serialization/json.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>

// The concrete GPU boundary (see cmake/tbx_backend.cmake). The selected gfx backend folder
// (gfx/gl/) implements these; its library types/calls never escape that folder. The surface
// is modern-API shaped so backends like Vulkan, Metal, DirectX 12, WebGPU, or SDL_GPU map
// directly: explicit render passes (attachments + load operations), immutable pipeline-state
// objects (no loose state toggles), and per-draw uniforms as a name-keyed push layer the
// backend routes through shader reflection.
namespace tbx::gpu
{
    /// @brief
    /// Purpose: What a frame starts as: the swapchain pass's clear and an optional viewport
    /// resize.
    struct FrameDescription
    {
        Color clear = Color {.r = 0.08f, .g = 0.08f, .b = 0.10f, .a = 1.0f};
        int width = 0; // 0 = keep the current viewport
        int height = 0;
    };

    /// @brief
    /// Purpose: Acquires the frame and begins the swapchain render pass, cleared. Hosts call
    /// begin_frame, then draw (directly or via render()), then tbx::run presents.
    void begin_frame(const FrameDescription& description = {});

    /// @brief
    /// Purpose: Begins a render pass: binds its attachments and applies the load operation.
    /// Passes never nest — end the current one first.
    void begin_render_pass(const RenderPassDescription& description);

    /// @brief
    /// Purpose: Ends the current render pass and returns to the swapchain.
    void end_render_pass();

    /// @brief
    /// Purpose: Compiles and links a shader module from backend-native source (GLSL for gl/).
    Result<std::unique_ptr<Shader>> compile_shader(
        const char* vertex_source,
        const char* fragment_source);

    /// @brief
    /// Purpose: Bakes a pipeline-state object (shader + depth/cull/blend).
    std::unique_ptr<Pipeline> make_pipeline(const PipelineDescription& description);

    /// @brief
    /// Purpose: Binds a pipeline; subsequent draw() calls use it.
    void set_pipeline(const Pipeline& pipeline);

    /// @brief
    /// Purpose: Draws a mesh as triangles with the bound pipeline.
    void draw(const Mesh& mesh);

    /// @brief
    /// Purpose: Current viewport height in pixels.
    int get_viewport_height();

    /// @brief
    /// Purpose: Current viewport width in pixels.
    int get_viewport_width();

    /// @brief
    /// Purpose: Initializes the selected backend's GPU access; must run once after window
    /// creation. Each backend loads its functions its own way — no platform types leak here.
    void initialize();

    /// @brief
    /// Purpose: Reads back one pixel from the current framebuffer — verification/tooling.
    Color read_pixel(int x, int y);

    /// @brief
    /// Purpose: The clear color of the most recent begin_frame()/CLEAR pass — offscreen
    /// passes that re-render the scene reuse it.
    Color get_clear_color();

    /// @brief
    /// Purpose: Draws indexed 2D UI geometry in screen space (y-down, origin top-left) with
    /// premultiplied-alpha blending, optionally textured, offset by translation. Runs its own
    /// internal pipeline; depth testing is suspended for the draw.
    void draw_ui(
        std::span<const UiVertex> vertices,
        std::span<const int> indices,
        std::optional<std::reference_wrapper<const Texture2d>> texture,
        const Vec2& translation);

    /// @brief
    /// Purpose: Enables/positions the scissor rectangle for UI clipping (y-down coordinates).
    void set_scissor(bool is_enabled, int x, int y, int width, int height);

    /// @brief
    /// Purpose: Sets the drawable region in pixels.
    void set_viewport(int width, int height);

    /// @brief
    /// Purpose: Binds a depth target's texture to a sampler slot.
    void bind_depth_texture(const DepthTarget& target, int slot);

    /// @brief
    /// Purpose: Binds a render target's color texture to a sampler slot.
    void bind_render_target_texture(const RenderTarget& target, int slot);

    /// @brief
    /// Purpose: Binds a texture to a sampler slot.
    void bind_texture(const Texture2d& texture, int slot);

    /// @brief
    /// Purpose: Creates a square depth-only render target for shadow maps.
    std::unique_ptr<DepthTarget> make_depth_target(int resolution);

    /// @brief
    /// Purpose: Creates an offscreen color+depth render target.
    std::unique_ptr<RenderTarget> make_render_target(int width, int height);

    /// @brief
    /// Purpose: Renders the sandbox: every MeshRenderer toy, lit by the DirectionalLight,
    /// shadowed, seen from the first Camera. ALL rendering lives in tbx::gpu. Builtin
    /// primitives only — use the Assets overload to resolve model/texture handles.
    void render(Sandbox& sandbox);

    /// @brief
    /// Purpose: Renders the sandbox with asset resolution: MeshRenderer model/texture handles
    /// upload on first sight and cache by asset id.
    void render(Sandbox& sandbox, Assets& assets);

    /// @brief
    /// Purpose: Applies a bag of named values ({"u_tint": [1,0,0,1], "u_shine": 0.5, ...}) to
    /// a shader, typed by its reflection — the material system's engine: values the shader
    /// does not declare are skipped, declared kinds drive the parse. Backend-agnostic.
    void apply_uniforms(const Shader& shader, const Json& values);

    /// @brief
    /// Purpose: Reflects a compiled shader's uniform schema (implemented per backend).
    ShaderInfo reflect(const Shader& shader);

    /// @brief
    /// Purpose: Sets a per-draw uniform by name (overloads per type) — the push-constant
    /// layer: name-keyed here, routed by each backend through its reflection (GL uniforms
    /// today; a uniform-buffer write in bind-group APIs).
    void set_uniform(const Shader& shader, const char* name, const Mat4& value);
    void set_uniform(const Shader& shader, const char* name, const Vec2& value);
    void set_uniform(const Shader& shader, const char* name, const Vec3& value);
    void set_uniform(const Shader& shader, const char* name, const Vec4& value);
    void set_uniform(const Shader& shader, const char* name, const Color& value);
    void set_uniform(const Shader& shader, const char* name, float value);
    void set_uniform(const Shader& shader, const char* name, int value);

    /// @brief
    /// Purpose: Uploads interleaved vertex data; attribute_sizes lists float counts per
    /// attribute (e.g. {3, 4} = position vec3 + color vec4).
    std::unique_ptr<Mesh> upload_mesh(
        std::span<const float> vertices,
        std::span<const int> attribute_sizes);

    /// @brief
    /// Purpose: Uploads an RGBA8 texture.
    std::unique_ptr<Texture2d> upload_texture(
        int width,
        int height,
        std::span<const std::byte> rgba_pixels);
}
