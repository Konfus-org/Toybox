
// Uniform buffer at set = 1, binding = 0
cbuffer UniformBlock : register(b0, space1)
{
    float4x4 viewProjection;
    float4x4 transform;
    float4 color;
};

// Inputs
struct VSInput
{
    float3 inPosition      : POSITION;     // location 0
    float4 inVertColor     : COLOR0;       // location 1
    float3 inNormal        : NORMAL;       // location 2
    float2 inTextureCoord  : TEXCOORD0;    // location 3
};

// Outputs
struct VSOutput
{
    float4 outPosition      : SV_POSITION;  // gl_Position
    float4 outColor         : COLOR0;       // location 0
    float4 outVertColor     : COLOR1;       // location 1
    float3 outNormal        : NORMAL;       // location 2
    float2 outTextureCoord  : TEXCOORD0;    // location 3
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Position transform
    output.outPosition = mul(transform * viewProjection, float4(input.inPosition, 1.0));

    // Pass through vertex colors
    output.outColor = color;

    // For matching GLSL outputs (some are commented out)
    output.outVertColor = input.inVertColor;
    output.outNormal = input.inNormal;
    output.outTextureCoord = input.inTextureCoord;

    return output;
}
