#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gfx/depth_target.h"
#include "tbx/gfx/mesh.h"
#include "tbx/gfx/pipeline.h"
#include "tbx/gfx/render_pass.h"
#include "tbx/gfx/render_target.h"
#include "tbx/gfx/shader.h"
#include "tbx/gfx/texture.h"
#include "tbx/gfx/texture2d.h"
#include "tbx/gfx/texture_binding.h"
#include "tbx/math/math.h"
#include "tbx/serialization/json.h"
#include "tbx/utils/color.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// The concrete GPU boundary (see cmake/tbx_backend.cmake). The selected gpu backend folder
// (gpu/gl/) implements these; its library types/calls never escape that folder. The surface
// is modern-API shaped so backends like Vulkan, Metal, DirectX 12, WebGPU, or SDL_GPU map
// directly: explicit render passes (attachments + load operations), immutable pipeline-state
// objects (no loose state toggles), and per-draw uniforms as a name-keyed push layer the
// backend routes through shader reflection.
namespace tbx
{
    /// @brief
    /// Purpose: What a frame starts as: the swapchain pass's clear and an optional viewport
    /// resize.
    struct TBX_API FrameDescription
    {
        Color clear = Color {.r = 0.08f, .g = 0.08f, .b = 0.10f, .a = 1.0f};
        int width = 0; // 0 = keep the current viewport
        int height = 0;
    };

    /// @brief
    /// Purpose: Initializes the selected backend's GPU access; must run once after window
    /// creation. Each backend loads its functions its own way — no platform types leak here.
    TBX_API void initialize_rendering();

    /// @brief
    /// Purpose: Acquires the frame and begins the swapchain render pass, cleared. Hosts call
    /// begin_frame, then gpu_draw (directly or via render()), then tbx::run presents.
    TBX_API void begin_render_frame(const FrameDescription& description = {});

    /// @brief
    /// Purpose: Begins a render pass: binds its attachments and applies the load operation.
    /// Passes never nest — end the current one first.
    TBX_API void begin_render_pass(const RenderPassDescription& description);

    /// @brief
    /// Purpose: Ends the current render pass and returns to the swapchain.
    TBX_API void end_render_pass();

    /// @brief
    /// Purpose: Compiles and links a shader module from backend-native source (GLSL for gl/).
    TBX_API Result<std::unique_ptr<Shader>> compile_shader(
        std::string_view vertex_source,
        std::string_view fragment_source);

    /// @brief
    /// Purpose: Bakes a pipeline-state object (shader + depth/cull/blend).
    TBX_API std::unique_ptr<Pipeline> make_render_pipeline(const PipelineDescription& description);

    /// @brief
    /// Purpose: Binds a pipeline; subsequent gpu_draw() calls use it.
    TBX_API void set_render_pipeline(const Pipeline& pipeline);

    /// @brief
    /// Purpose: Draws a mesh as triangles with the bound pipeline; the given textures bind
    /// for exactly this gpu_draw (modern-API shape — no loose slot state).
    TBX_API void draw(const Mesh& mesh, std::span<const TextureBinding> textures = {});

    /// @brief
    /// Purpose: Current viewport height in pixels.
    TBX_API int get_render_viewport_height();

    /// @brief
    /// Purpose: Current viewport width in pixels.
    TBX_API int get_render_viewport_width();

    /// @brief
    /// Purpose: Whether presents wait for vertical sync — the gpu-side desired state; the
    /// platform backend applies it to every window surface.
    TBX_API bool is_vsync_enabled();

    /// @brief
    /// Purpose: Requests vsync on or off. Applied to each window surface by the next windows
    /// update; fed from GraphicsSettings at boot and on .tapp reload.
    TBX_API void set_vsync(bool is_enabled);

    /// @brief
    /// Purpose: Reads back one pixel from the current framebuffer — verification/tooling.
    TBX_API Color read_pixel_from_frame_buffer(int x, int y);

    /// @brief
    /// Purpose: Reads the current framebuffer into an RGBA8 Texture (top-down rows) at the
    /// current viewport size — capture right before the frame presents.
    TBX_API Result<void> render_screenshot(Texture& result);

    /// @brief
    /// Purpose: The clear color of the most recent gpu_begin_frame()/CLEAR pass — offscreen
    /// passes that re-render the scene reuse it.
    TBX_API Color get_render_clear_color();

    /// @brief
    /// Purpose: Enables/positions the scissor rectangle for UI clipping (y-down coordinates).
    TBX_API void set_render_scissor(bool is_enabled, int x, int y, int width, int height);

    /// @brief
    /// Purpose: Sets the drawable region in pixels.
    TBX_API void set_render_viewport(int width, int height);

    /// @brief
    /// Purpose: Sets a sub-rectangle viewport for subsequent draws within the current pass
    /// (camera viewports; origin bottom-left). Pass boundaries reset to the full drawable.
    TBX_API void set_render_viewport(int x, int y, int width, int height);

    /// @brief
    /// Purpose: Creates a square depth-only render target for shadow maps.
    TBX_API std::unique_ptr<DepthTarget> make_depth_render_target(int resolution);

    /// @brief
    /// Purpose: Creates an offscreen color+depth render target.
    TBX_API std::unique_ptr<RenderTarget> make_render_target(int width, int height);

    /// @brief
    /// Purpose: Applies a bag of named values ({"u_tint": [1,0,0,1], "u_shine": 0.5, ...}) to
    /// a shader, typed by its reflection — the material system's engine: values the shader
    /// does not declare are skipped, declared kinds drive the parse. Backend-agnostic.
    TBX_API void apply_shader_uniforms(const Shader& shader, const Json& values);

    /// @brief
    /// Purpose: Reflects a compiled shader's uniform schema (implemented per backend).
    TBX_API ShaderInfo reflect_shader(const Shader& shader);

    /// @brief
    /// Purpose: Sets a per-draw uniform by name (overloads per type) — the push-constant
    /// layer: name-keyed here, routed by each backend through its reflection (GL uniforms
    /// today; a uniform-buffer write in bind-group APIs).
    /// C boundary: uniform names go null-terminated straight into the graphics API on the
    /// per-draw hot path — const char* is deliberate.
    TBX_API void set_shader_uniform(const Shader& shader, const char* name, const Mat4& value);
    TBX_API void set_shader_uniform(const Shader& shader, const char* name, const Vec2& value);
    TBX_API void set_shader_uniform(const Shader& shader, const char* name, const Vec3& value);
    TBX_API void set_shader_uniform(const Shader& shader, const char* name, const Vec4& value);
    TBX_API void set_shader_uniform(const Shader& shader, const char* name, const Color& value);
    TBX_API void set_shader_uniform(const Shader& shader, const char* name, float value);
    TBX_API void set_shader_uniform(const Shader& shader, const char* name, int value);

    /// @brief
    /// Purpose: Uploads interleaved vertex data; attribute_sizes lists float counts per
    /// attribute (e.g. {3, 4} = position vec3 + color vec4).
    TBX_API std::unique_ptr<Mesh> upload_mesh_to_gpu(
        std::span<const float> vertices,
        std::span<const int> attribute_sizes);

    /// @brief
    /// Purpose: Uploads an RGBA8 texture.
    TBX_API std::unique_ptr<Texture2d> upload_texture_to_gpu(
        int width,
        int height,
        std::span<const std::byte> rgba_pixels);
}
