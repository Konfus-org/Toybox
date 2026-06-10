#include "ShaderBase.glsl"

// Depth-only: the fixed-function depth write is the entire output, so main does nothing. (A complete
// program still requires a fragment stage; ShaderBase is included only to match the shared preamble.)
void main()
{
}
