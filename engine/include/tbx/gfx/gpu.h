#pragma once
#include "tbx/core/math.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include <span>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: GPU shader program handle (opaque backend id).
    struct Shader
    {
        uint32 id = 0;
    };

    /// @brief
    /// Purpose: GPU mesh handle: interleaved vertex data uploaded and laid out for drawing.
    struct Mesh
    {
        uint32 vertex_array = 0;
        uint32 vertex_buffer = 0;
        int vertex_count = 0;
    };

    // The concrete GPU boundary (see cmake/tbx_backend.cmake). The selected gfx backend folder
    // (gfx/gl/) implements these; its library types/calls never escape that folder. Everything
    // above this header is backend-agnostic. M1 surface: enough to clear and draw raw meshes —
    // the renderer port (M6) grows this into buffers/textures/pipelines/passes.

    /// @brief
    /// Purpose: Clears the current framebuffer's color and depth.
    void clear(const Color& color);

    /// @brief
    /// Purpose: Uploads interleaved vertex data; attribute_sizes lists float counts per
    /// attribute (e.g. {3, 4} = position vec3 + color vec4).
    Mesh create_mesh(std::span<const float> vertices, std::span<const int> attribute_sizes);

    /// @brief
    /// Purpose: Compiles and links a shader program from backend-native source (GLSL for gl/).
    Result<Shader> create_shader(const char* vertex_source, const char* fragment_source);

    /// @brief
    /// Purpose: Releases a mesh's GPU buffers.
    void destroy_mesh(Mesh mesh);

    /// @brief
    /// Purpose: Releases a shader program.
    void destroy_shader(Shader shader);

    /// @brief
    /// Purpose: Draws a mesh as triangles with the given shader.
    void draw(Shader shader, const Mesh& mesh);

    /// @brief
    /// Purpose: Initializes the selected backend's GPU access; must run once after window
    /// creation. Each backend loads its functions its own way — no platform types leak here.
    void initialize();

    /// @brief
    /// Purpose: Reads back one pixel from the current framebuffer — verification/tooling.
    Color read_pixel(int x, int y);

    /// @brief
    /// Purpose: Sets the drawable region in pixels.
    void set_viewport(int width, int height);
}
