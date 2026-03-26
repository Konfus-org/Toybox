#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec4 v_color[];
in vec3 v_world_normal[];

out vec4 g_color;
out vec3 g_world_normal;
out vec3 g_barycentric;

void main()
{
    for (int vertex_index = 0; vertex_index < 3; ++vertex_index)
    {
        g_color = v_color[vertex_index];
        g_world_normal = v_world_normal[vertex_index];
        if (vertex_index == 0)
            g_barycentric = vec3(1.0, 0.0, 0.0);
        else if (vertex_index == 1)
            g_barycentric = vec3(0.0, 1.0, 0.0);
        else
            g_barycentric = vec3(0.0, 0.0, 1.0);

        gl_Position = gl_in[vertex_index].gl_Position;
        EmitVertex();
    }

    EndPrimitive();
}
