#include "ShaderBase.glsl"

// Shell-extruded grass. Takes each ground triangle from Pbr.vert and emits it again as a stack of
// thin shells pushed up along the surface normal. Grass.frag then carves soft round tufts out of each
// shell (discarding the gaps), so the stacked layers read as real, fuzzy 3D clumps when seen at a
// grazing angle. Shell count is fixed here; tuft height is the 4th lane of params[1].
//   params[1] = (blade_scale, fuzz, roughness, height)
// Only the varyings Grass.frag actually consumes are emitted (world position/normal, material id,
// shell height) — emitting the full vertex set blows past the driver's geometry-output component cap.
layout(triangles) in;
layout(triangle_strip, max_vertices = 48) out;

// Core profile requires the gl_PerVertex block to be redeclared when a geometry stage both reads the
// vertex-stage gl_Position and writes its own; without this the program fails to link.
in gl_PerVertex
{
    vec4 gl_Position;
} gl_in[];

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 2) in vec3 v_world_position[];
layout(location = 3) in vec3 v_world_normal[];
layout(location = 5) in flat uint v_material_id[];

layout(location = 0) out vec3 g_world_position;
layout(location = 1) out vec3 g_world_normal;
layout(location = 2) out flat uint g_material_id;
layout(location = 3) out float g_shell; // 0 at the ground, 1 at the tallest blade tip

const int TBX_GRASS_SHELLS = 16;

void main()
{
    uint mid = v_material_id[0];
    float height = max(tbx_material_param(mid, 1u).w, 0.01);

    for (int s = 0; s < TBX_GRASS_SHELLS; ++s)
    {
        float shell = float(s) / float(TBX_GRASS_SHELLS - 1);
        for (int v = 0; v < 3; ++v)
        {
            vec3 world_position = v_world_position[v] + v_world_normal[v] * (shell * height);
            g_world_position = world_position;
            g_world_normal = v_world_normal[v];
            g_material_id = mid;
            g_shell = shell;
            gl_Position = viewProjection * vec4(world_position, 1.0);
            EmitVertex();
        }
        EndPrimitive();
    }
}
