// Inputs
struct VSInput
{
    float3 inPosition    : POSITION;     // location 0
    float4 inVertColor   : COLOR0;       // location 1
    float3 inNormal      : NORMAL;       // location 2
    float2 inTextureCoord: TEXCOORD0;    // location 3
};

// Outputs
struct VSOutput
{
    float4 position      : SV_POSITION;  // gl_Position
    float4 color         : COLOR0;       // location 0
    float4 vertColor     : COLOR1;       // location 1
    float3 normal        : NORMAL;       // location 2
    float2 textureCoord  : TEXCOORD0;    // location 3
};

// Uniform buffer at set = 1, binding = 0
cbuffer UniformBlock : register(b0, space1)
{
    float4x4 viewProjectionUni;
    // float4x4 transformUni;  // Uncomment and add if needed
    // float4 colorUni;        // Uncomment and add if needed
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Position transform
    output.position = mul(viewProjectionUni, float4(input.inPosition, 1.0));

    // Pass through vertex colors
    output.color = input.inVertColor;

    // For matching GLSL outputs (some are commented out)
    output.vertColor = input.inVertColor;  // assuming you want to output this too
    output.normal = input.inNormal;
    output.textureCoord = input.inTextureCoord;

    return output;
}
