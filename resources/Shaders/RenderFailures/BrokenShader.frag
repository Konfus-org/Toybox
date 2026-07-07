#version 450 core

// Intentionally broken fragment shader for the ExampleProject "failure wall" (see
// Engine/docs/RenderFailures.md). It references an undeclared identifier, so it fails to compile and
// the renderer can't build a pipeline for it — exercising the SHADER_COMPILE fallback (the solid
// magenta debug material). Do not "fix" this shader; it must not compile.
layout(location = 0) out vec4 o_color;

void main()
{
    // `broken_undeclared_symbol` is never declared, so compilation fails here.
    o_color = broken_undeclared_symbol;
}
