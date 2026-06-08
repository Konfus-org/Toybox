#include "ShaderBase.glsl"

layout(location = 0) out vec2 v_tex_coord;

const vec2 FULLSCREEN_POSITIONS[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

const vec2 FULLSCREEN_TEX_COORDS[3] = vec2[3](vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));

void main()
{
    // A single oversized triangle avoids the diagonal seam and one extra vertex of a quad.
    v_tex_coord = FULLSCREEN_TEX_COORDS[gl_VertexID];
    gl_Position = vec4(FULLSCREEN_POSITIONS[gl_VertexID], 0.0, 1.0);
}
