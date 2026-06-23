#version 460 core

// Gizmo overlay vertex stage. Geometry is supplied in WORLD space (CPU-built line/triangle lists),
// so a single camera view-projection projects it correctly for any view. Standalone (no ShaderBase):
// it pulls its own vertices from an SSBO and reads only a view-projection UBO.

struct GizmoVertex
{
    vec4 position; // xyz used (world space)
    vec4 color;    // rgba
};

layout(std430, binding = 0) readonly buffer GizmoVertices
{
    GizmoVertex gizmos[];
};

layout(std140, binding = 0) uniform GizmoView
{
    mat4 viewProjection;
};

layout(location = 0) out vec4 v_color;

void main()
{
    GizmoVertex vertex = gizmos[gl_VertexID];
    v_color = vertex.color;
    gl_Position = viewProjection * vec4(vertex.position.xyz, 1.0);
}
