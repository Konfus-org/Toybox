#include "ShaderBase.glsl"

// Selection mask: every fragment of a tagged entity writes a flat white silhouette into the mask
// target. Paired with Shaders/Material/Fallback.vert (SSBO vertex pull + camera viewProjection); the
// selection-outline post effect edge-detects this mask. Presence is all the mask carries — no shading.
layout(location = 0) out vec4 o_mask;

void main()
{
    o_mask = vec4(1.0);
}
