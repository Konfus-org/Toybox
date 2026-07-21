#version 460 core
// Toybox builtin UI composite stage: lays the premultiplied UI texture over the frame.
in vec2 v_uv;

uniform sampler2D u_ui;

out vec4 out_color;

void main()
{
    out_color = texture(u_ui, v_uv);
}
