#include "Toybox/ShaderBase.glsl"

layout(binding = TBX_BINDING_POST_SOURCE_COLOR)
uniform sampler2D u_source_color;

layout(binding = TBX_BINDING_POST_SOURCE_DEPTH)
uniform sampler2D u_source_depth;
