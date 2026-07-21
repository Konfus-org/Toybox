#version 460 core
// Toybox builtin UI fragment stage: textured, vertex-colored (premultiplied alpha).
in vec4 v_color;
in vec2 v_uv;

uniform sampler2D u_texture;

out vec4 out_color;

void main()
{
    out_color = texture(u_texture, v_uv) * v_color;
}
