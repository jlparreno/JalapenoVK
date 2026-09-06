struct VSInput 
{
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    float3 inNormal;
    float4 inTangent;
};

struct UniformBuffer
{
    float4x4 view;
    float4x4 proj;
    float4   lightDirection;
    float4   lightColorIntensity;
    float4   cameraPosition;
};
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;

struct PushConstants
{
    float4x4 model;
};
[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

struct PBRFactors
{
    float4 baseColor;
    float4 metallicRoughness;
    float4 emissive;
};
[[vk::binding(0, 1)]] ConstantBuffer<PBRFactors> factors;
[[vk::binding(1, 1)]] Sampler2D albedoTex;
[[vk::binding(2, 1)]] Sampler2D normalTex;
[[vk::binding(3, 1)]] Sampler2D metallicRoughnessTex;
[[vk::binding(4, 1)]] Sampler2D occlusionTex;
[[vk::binding(5, 1)]] Sampler2D emissiveTex;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(pushConstants.model, float4(input.inPosition, 1.0))));
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    return albedoTex.Sample(vertIn.fragTexCoord) * factors.baseColor;
}