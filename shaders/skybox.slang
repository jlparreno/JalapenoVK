// Draws the environment cubemap as the scene background.
// No cube mesh and no vertex buffer: one fullscreen triangle whose fragments recover
// their view ray from the inverse view-projection matrix, and sample the cube with it.

#include "include/tonemap.slang"

[[vk::binding(0, 0)]] SamplerCube environmentTex;

struct PushConstants 
{ 
    float4x4 invViewProj; 
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 ndc;                 // Position of this fragment in normalized device coordinates, [-1, 1].
};

[shader("vertex")]
VSOutput vertMain(uint vertexID: SV_VertexID)
{
    // One oversized triangle covering the whole viewport: (-1,-1), (3,-1), (-1,3).
    float2 ndc = float2((vertexID << 1) & 2, vertexID & 2) * 2.0 - 1.0;

    VSOutput output;
    output.pos = float4(ndc, 0.0, 1.0);
    output.ndc = ndc;

    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET
{
    float4 farPoint = mul(pushConstants.invViewProj, float4(input.ndc, 1.0, 1.0));
    float3 dir = normalize(farPoint.xyz / farPoint.w);

    return float4(TonemapACES(environmentTex.Sample(dir).rgb * k_exposure), 1.0);
}