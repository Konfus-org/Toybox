#include "MaterialShaderBase.glsl"

struct DrawIndexedIndirectCommand
{
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseVertex;
    uint baseInstance;
};

layout(std430, binding = TBX_SHADER_BINDING_DRAW_COMMAND_LOOKUP) readonly buffer DrawCommandLookupBuffer
{
    uint drawCommandIndices[];
};

layout(std430, binding = TBX_SHADER_BINDING_INDIRECT_COMMANDS) buffer IndirectCommandBuffer
{
    DrawIndexedIndirectCommand commands[];
};
