#pragma once
#include "tbx/serialization/json.h"
#include "tbx/core/color.h"
#include "tbx/core/math.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/assets/assets.h"
#include "tbx/ecs/sandbox.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: GPU shader program — RAII: the destructor (defined by the selected gfx
    /// backend) releases the program. Obtain via compile_shader().
    class Shader final
    {
      public:
        explicit Shader(uint32 id)
            : _id(id)
        {
        }
        ~Shader();

      public:
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native program id (used by the backend's draw path).
        uint32 get_id() const
        {
            return _id;
        }

      private:
        uint32 _id = 0;
    };

    /// @brief
    /// Purpose: GPU mesh — RAII: the destructor (defined by the selected gfx backend) releases
    /// the buffers. Obtain via upload_mesh().
    class Mesh final
    {
      public:
        Mesh(uint32 vertex_array, uint32 vertex_buffer, int vertex_count)
            : _vertex_array(vertex_array)
            , _vertex_buffer(vertex_buffer)
            , _vertex_count(vertex_count)
        {
        }
        ~Mesh();

      public:
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native vertex array id.
        uint32 get_vertex_array() const
        {
            return _vertex_array;
        }

        /// @brief
        /// Purpose: Backend-native vertex buffer id.
        uint32 get_vertex_buffer() const
        {
            return _vertex_buffer;
        }

        /// @brief
        /// Purpose: Number of vertices to draw.
        int get_vertex_count() const
        {
            return _vertex_count;
        }

      private:
        uint32 _vertex_array = 0;
        uint32 _vertex_buffer = 0;
        int _vertex_count = 0;
    };

    /// @brief
    /// Purpose: GPU 2D texture (RGBA8) — RAII: the backend-defined destructor releases it.
    /// Obtain via upload_texture().
    class Texture2d final
    {
      public:
        explicit Texture2d(uint32 id)
            : _id(id)
        {
        }
        ~Texture2d();

      public:
        Texture2d(const Texture2d&) = delete;
        Texture2d& operator=(const Texture2d&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native texture id.
        uint32 get_id() const
        {
            return _id;
        }

      private:
        uint32 _id = 0;
    };

    /// @brief
    /// Purpose: Depth-only render target for shadow passes — RAII via the backend.
    /// Obtain via make_depth_target().
    class DepthTarget final
    {
      public:
        DepthTarget(uint32 framebuffer, uint32 depth_texture, int resolution)
            : _framebuffer(framebuffer)
            , _depth_texture(depth_texture)
            , _resolution(resolution)
        {
        }
        ~DepthTarget();

      public:
        DepthTarget(const DepthTarget&) = delete;
        DepthTarget& operator=(const DepthTarget&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native depth texture id.
        uint32 get_depth_texture() const
        {
            return _depth_texture;
        }

        /// @brief
        /// Purpose: Backend-native framebuffer id.
        uint32 get_framebuffer() const
        {
            return _framebuffer;
        }

        /// @brief
        /// Purpose: Square resolution in pixels.
        int get_resolution() const
        {
            return _resolution;
        }

      private:
        uint32 _framebuffer = 0;
        uint32 _depth_texture = 0;
        int _resolution = 0;
    };

    /// @brief
    /// Purpose: Offscreen color+depth render target (post-processing, editor viewports) —
    /// RAII via the backend. Obtain via make_render_target().
    class RenderTarget final
    {
      public:
        RenderTarget(
            uint32 framebuffer,
            uint32 color_texture,
            uint32 depth_buffer,
            int width,
            int height)
            : _framebuffer(framebuffer)
            , _color_texture(color_texture)
            , _depth_buffer(depth_buffer)
            , _width(width)
            , _height(height)
        {
        }
        ~RenderTarget();

      public:
        RenderTarget(const RenderTarget&) = delete;
        RenderTarget& operator=(const RenderTarget&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native color texture id.
        uint32 get_color_texture() const
        {
            return _color_texture;
        }

        /// @brief
        /// Purpose: Backend-native depth buffer id.
        uint32 get_depth_buffer() const
        {
            return _depth_buffer;
        }

        /// @brief
        /// Purpose: Backend-native framebuffer id.
        uint32 get_framebuffer() const
        {
            return _framebuffer;
        }

        /// @brief
        /// Purpose: Height in pixels.
        int get_height() const
        {
            return _height;
        }

        /// @brief
        /// Purpose: Width in pixels.
        int get_width() const
        {
            return _width;
        }

      private:
        uint32 _framebuffer = 0;
        uint32 _color_texture = 0;
        uint32 _depth_buffer = 0;
        int _width = 0;
        int _height = 0;
    };

    /// @brief
    /// Purpose: The type of one reflected shader uniform.
    enum class UniformKind : uint8
    {
        FLOAT,
        INT,
        BOOL,
        VEC2,
        VEC3,
        VEC4,
        MAT4,
        TEXTURE,
        UNKNOWN
    };

    /// @brief
    /// Purpose: One uniform a shader exposes, discovered by reflect().
    struct UniformInfo
    {
        std::string name = {};
        UniformKind kind = UniformKind::UNKNOWN;
    };

    /// @brief
    /// Purpose: Everything a shader exposes — the schema materials program against, so any
    /// arbitrary shader "just works" without per-shader engine code.
    struct ShaderInfo
    {
        std::vector<UniformInfo> uniforms = {};
    };

    /// @brief
    /// Purpose: One 2D UI vertex: screen-space position, packed RGBA (premultiplied), uv.
    struct UiVertex
    {
        Vec2 position = Vec2(0.0f, 0.0f);
        uint32 color = 0xFFFFFFFF;
        Vec2 uv = Vec2(0.0f, 0.0f);
    };

    // The concrete GPU boundary (see cmake/tbx_backend.cmake). The selected gfx backend folder
    // (gfx/gl/) implements these; its library types/calls never escape that folder. Everything
    // above this header is backend-agnostic. M1 surface: enough to clear and draw raw meshes —
    // the renderer port (M6) grows this into buffers/textures/pipelines/passes.

    /// @brief
    /// Purpose: What a frame starts as; grows a render-target member with the renderer port.
    struct FrameDescription
    {
        Color clear = Color {.r = 0.08f, .g = 0.08f, .b = 0.10f, .a = 1.0f};
        int width = 0; // 0 = keep the current viewport
        int height = 0;
    };

    /// @brief
    /// Purpose: Starts a frame: applies the viewport and clears color+depth. ALL rendering
    /// lives in tbx::gpu — hosts call begin_frame, then draw, then tbx::run presents.
    void begin_frame(const FrameDescription& description = {});

    /// @brief
    /// Purpose: Clears the current framebuffer's color and depth.
    void clear(const Color& color);

    /// @brief
    /// Purpose: Compiles and links a shader program from backend-native source (GLSL for gl/).
    Result<std::unique_ptr<Shader>> compile_shader(
        const char* vertex_source,
        const char* fragment_source);

    /// @brief
    /// Purpose: Draws a mesh as triangles with the given shader.
    void draw(const Shader& shader, const Mesh& mesh);

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
    /// Purpose: Draws indexed 2D UI geometry in screen space (y-down, origin top-left) with
    /// premultiplied-alpha blending, optionally textured, offset by translation. Depth testing
    /// is suspended for the draw.
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
    /// Purpose: Starts rendering into a depth target (shadow pass); end_depth_pass() returns
    /// to the window framebuffer and viewport.
    void begin_depth_pass(const DepthTarget& target);

    /// @brief
    /// Purpose: Binds a depth target's texture to a sampler slot.
    void bind_depth_texture(const DepthTarget& target, int slot);

    /// @brief
    /// Purpose: Binds a texture to a sampler slot.
    void bind_texture(const Texture2d& texture, int slot);

    /// @brief
    /// Purpose: Ends the depth pass started by begin_depth_pass().
    void end_depth_pass();

    /// @brief
    /// Purpose: Creates a square depth-only render target for shadow maps.
    std::unique_ptr<DepthTarget> make_depth_target(int resolution);

    /// @brief
    /// Purpose: Starts rendering into an offscreen color+depth target; end_render_target()
    /// returns to the window framebuffer and viewport.
    void begin_render_target(const RenderTarget& target);

    /// @brief
    /// Purpose: Binds a render target's color texture to a sampler slot.
    void bind_render_target_texture(const RenderTarget& target, int slot);

    /// @brief
    /// Purpose: Ends the pass started by begin_render_target().
    void end_render_target();

    /// @brief
    /// Purpose: The clear color of the most recent clear()/begin_frame() — offscreen passes
    /// that re-render the scene reuse it.
    Color get_clear_color();

    /// @brief
    /// Purpose: Creates an offscreen color+depth render target.
    std::unique_ptr<RenderTarget> make_render_target(int width, int height);

    /// @brief
    /// Purpose: Enables/disables depth testing (fullscreen passes disable it).
    void set_depth_test(bool is_enabled);

    /// @brief
    /// Purpose: Enables/disables depth writes (sky and fullscreen passes disable them).
    void set_depth_write(bool is_enabled);

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
    /// Purpose: Sets a shader uniform by name (overloads per type).
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
