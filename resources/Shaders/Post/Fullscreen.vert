#include "ShaderBase.glsl"

// Shared vertex stage for every post-processing effect: emits one oversized triangle covering the
// screen (no diagonal seam, one fewer vertex than a quad) and the matching [0,1] tex coords. The
// effect's fragment shader does all the work; no vertex buffers are bound (gl_VertexID picks corners).
layout(location = 0) out vec2 v_tex_coord;

const vec2 TBX_FULLSCREEN_POSITIONS[3] =
    vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

const vec2 TBX_FULLSCREEN_TEX_COORDS[3] = vec2[3](vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));

void main()
{
    v_tex_coord = TBX_FULLSCREEN_TEX_COORDS[gl_VertexID];
    gl_Position = vec4(TBX_FULLSCREEN_POSITIONS[gl_VertexID], 0.0, 1.0);
}
