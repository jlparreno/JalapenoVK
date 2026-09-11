
#include "include/tonemap.slang"

static const float PI = 3.14159265359;

// Highest mip index of the prefiltered cube. Must equal k_prefilterMips - 1 in
// ImageBasedLighting.h: roughness 1 has to land on the last level.
static const float k_prefilterMaxMip = 4.0;

[[vk::binding(0, 2)]] SamplerCube irradianceTex;
[[vk::binding(1, 2)]] SamplerCube prefilterTex;
[[vk::binding(2, 2)]] Sampler2D   brdfLutTex;

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

// Fresnel for IBL: there is no single half-vector here, since the light arrives from
// the whole hemisphere at once, so NdotV stands in for VdotH. The roughness term keeps
// rough surfaces from picking up the full grazing-angle boost, which they should not.
float3 FresnelSchlickRoughness(float NdotV, float3 F0, float roughness)
{
    float3 maxF = max(float3(1.0 - roughness), F0);
    return F0 + (maxF - F0) * pow(saturate(1.0 - NdotV), 5.0);
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

    // Useful variables for the BRDF functions
    float NdotV = max(dot(N, V), 0.0001); // ensure min value to avoid division by zero
    float NdotL = max(dot(N, L), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // --- Direct Lighting ---

    // Cook-Torrance BRDF: GGX distribution + Smith geometry + Fresnel-Schlick
    float  D = DistributionGGX(NdotH, roughness);
    float  G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(VdotH, F0);

    float3 specular = (D * G * F) / (4.0 * NdotV * NdotL);

    // Energy split: 
    // kS = fraction of light reflected specularly: already F (the Fresnel term above)
    // kD = the rest, which would scatter as diffuse: except metals absorb that "leftover" energy instead of scattering it (at metallic=1, kD collapses to 0 entirely).
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    // Outgoing radiance: the light leaving towards the camera from this point, for this light:
    // Diffuse + specular, scaled by the incoming radiance and the cosine of incidence (NdotL).
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // --- IBL Ambient Lighting ---

    // Energy split for the ambient light, as kS/kD above. There is no single light direction,
    // so the roughness-aware Fresnel uses NdotV in place of VdotH.
    float3 ambientKS = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 ambientKD = (1.0 - ambientKS) * (1.0 - metallic);

    // Diffuse: the irradiance cube already holds the cosine-weighted light arriving around N,
    // so one lookup stands in for the whole hemisphere integral. No division by PI here, unlike
    // the direct term: the convolution stored irradiance / PI already.
    float3 irradiance = irradianceTex.Sample(N).rgb;
    float3 diffuseIBL = ambientKD * irradiance * albedo;

    // Specular, split-sum approximation (Karis 2013): the product of two precomputed halves.
    //  - prefiltered: the environment around R, blurred for this roughness (one roughness per mip).
    //  - brdf: scale (x) and bias (y) applied to F0.
    float3 R = reflect(-V, N);
    float3 prefiltered = prefilterTex.SampleLevel(R, roughness * k_prefilterMaxMip).rgb;
    float2 brdf = brdfLutTex.Sample(float2(NdotV, roughness)).rg;
    float3 specularIBL = prefiltered * (F0 * brdf.x + brdf.y);

    // Ambient occlusion only darkens the ambient light.
    float3 ambient = (diffuseIBL + specularIBL) * ao;

    // Emissive is light the surface gives off itself, so it is added after the occlusion.
    float3 color = ambient + Lo + emissive;

    // Linear output: the swapchain is sRGB, so the hardware applies the gamma encoding on write.
    return float4(TonemapACES(color * k_exposure), 1.0);
}