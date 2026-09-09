
#include "include/tonemap.slang"

static const float PI = 3.14159265359;

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
    float3 fragWorldPos;
    float3 fragNormal;
    float4 fragTangent; //xyz + w = bitangent sign
};


[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(pushConstants.model, float4(input.inPosition, 1.0));

    output.fragWorldPos = worldPos.xyz;
    output.pos = mul(ubo.proj, mul(ubo.view, worldPos));
    output.fragTexCoord = input.inTexCoord;
    output.fragNormal = mul((float3x3)pushConstants.model, input.inNormal);
    output.fragTangent = float4(mul((float3x3)pushConstants.model, input.inTangent.xyz), input.inTangent.w);

    return output;
}

// --- PBR FUNCTIONS ---

// Normal Distribution Function (NDF) - Trowbridge-Reitz GGX.
// Estimates the fraction of microfacets aligned with the half-vector H.
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Geometry Function - Smith's Schlick-GGX.
// Models the probability that microfacets are not self-shadowed/masked, for both V and L.
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0; // direct lighting
    float gV = NdotV / (NdotV * (1.0 - k) + k);
    float gL = NdotL / (NdotL * (1.0 - k) + k);
    return gV * gL;
}

// Fresnel - Schlick approximation.
// How much light reflects vs refracts, depending on viewing angle.
float3 FresnelSchlick(float VdotH, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    // Sample textures and apply factors
    float3 albedo = albedoTex.Sample(vertIn.fragTexCoord).rgb * factors.baseColor.rgb;
    float4 mrSample = metallicRoughnessTex.Sample(vertIn.fragTexCoord);
    float metallic = mrSample.b * factors.metallicRoughness.x;  // glTF: B = metallic
    float roughness = mrSample.g * factors.metallicRoughness.y; // glTF: G = roughness
    float ao = occlusionTex.Sample(vertIn.fragTexCoord).r;
    float3 emissive = emissiveTex.Sample(vertIn.fragTexCoord).rgb * factors.emissive.rgb;

    // Normal
    float3 N = normalize(vertIn.fragNormal);
    float3 T = normalize(vertIn.fragTangent.xyz - N * dot(N, vertIn.fragTangent.xyz)); // Gram-Schmidt
    float3 B = cross(N, T) * vertIn.fragTangent.w;
    float3x3 TBN = float3x3(T, B, N);

    float3 tangentNormal = normalTex.Sample(vertIn.fragTexCoord).rgb * 2.0 - 1.0; // [0,1] -> [-1,1]
    N = normalize(mul(tangentNormal, TBN));

    // View and light vectors
    float3 V = normalize(ubo.cameraPosition.xyz - vertIn.fragWorldPos);
    float3 L = normalize(-ubo.lightDirection.xyz);
    float3 H = normalize(V + L);
    float3 radiance = ubo.lightColorIntensity.rgb * ubo.lightColorIntensity.w;

    // Variables for the BRDF functions
    float NdotV = max(dot(N, V), 0.0001);
    float NdotL = max(dot(N, L), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // Cook-Torrance BRDF: GGX distribution + Smith geometry + Fresnel-Schlick
    float  D = DistributionGGX(NdotH, roughness);
    float  G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(VdotH, F0);

    float3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001); // +epsilon avoids division by zero

    // Energy split: 
    // kS = fraction of light reflected specularly: already F (the Fresnel term above)
    // kD = the rest, which would scatter as diffuse: except metals absorb that "leftover" energy instead of scattering it (at metallic=1, kD collapses to 0 entirely).
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    // Outgoing radiance: the light leaving towards the camera from this point, for this light:
    // Diffuse + specular, scaled by the incoming radiance and the cosine of incidence (NdotL).
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // Ambient placeholder (pre-IBL), with AO + emissive
    float3 ambient = 0.03 * albedo * ao;
    float3 color = ambient + Lo + emissive;

    return float4(TonemapACES(color * k_exposure), 1.0);
}