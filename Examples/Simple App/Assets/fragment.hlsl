// Output
struct PSOutput
{
    float4 outColor : SV_TARGET;
};

// Inputs
struct PSInput
{
    float4 color : COLOR0;
    float4 vertColor : COLOR1;
    float3 normal : NORMAL;
    float2 textureCoord : TEXCOORD0;
};

// Bindings matching GLSL layout(set=2,binding=0) and (set=3,binding=0)
Texture2D textureUniform : register(t0, space2);     // set 2, binding 0
SamplerState textureUniformSampler : register(s0, space2);

cbuffer UniformBlock : register(b0, space3)          // set 3, binding 0
{
    float time;
};

PSOutput main(PSInput input)
{
    PSOutput output;
    float4 texColor = textureUniform.Sample(textureUniformSampler, input.textureCoord);
    output.outColor = texColor;
    return output;
}
