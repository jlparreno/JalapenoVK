// Prefilters the environment cube for specular IBL.
// Run once at startup, thirty times: one draw per (mip, face) pair, each targeting a
// single-level single-layer 2D view of the prefiltered cube image.

#include "include/ibl_sampling.slang"

// Samples taken per texel. High enough to keep the sampling pattern out of the rough mips; 
// this runs once at startup, so the cost is paid there and never again.
static const uint k_sampleCount = 1024;

// Side of one environment cube face. Must match k_environmentSize in EnvironmentMap.h:
// it converts a sample's solid angle into a mip level of that cube.
static const float k_environmentSize = 1024.0;

// The environment cube projected from the equirectangular source, mip chain included:
// each sample picks the level it reads.
[[vk::binding(0, 0)]] SamplerCube environmentTex;

// float4 rather than float3: push constant members are 16-byte aligned, so the
// padding would exist either way, and this keeps the C++ struct a plain copy.
struct CubeFaceOrientation
{
    float4 right;
    float4 up;
    float4 forward;
};

struct PrefilterParams
{
    CubeFaceOrientation face;       // Orientation of the face this draw is painting.
    float               roughness;  // Roughness this mip stands for: mip / (mips - 1).
};
[[vk::push_constant]]
ConstantBuffer<PrefilterParams> params;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 ndc;                     // Position of this fragment in normalized device coordinates, [-1, 1].
};

[shader("vertex")]
VSOutput vertMain(uint vertexID : SV_VertexID)
{
    // One oversized triangle covering the whole viewport: (-1,-1), (3,-1), (-1,3).
    float2 ndc = float2((vertexID << 1) & 2, vertexID & 2) * 2.0 - 1.0;

    VSOutput output;
    output.pos = float4(ndc, 0.0, 1.0);
    output.ndc = ndc;

    return output;
}

// GGX normal distribution: the same D term pbr.slang evaluates for direct lighting,
// needed here to recover the probability density each sample was drawn with.
float DistributionGGX(float NdotH, float roughness)
{
    float a     = roughness * roughness;
    float a2    = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;

    return a2 / (PI * denom * denom);
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET
{
    float3 N = normalize(params.face.forward.xyz + input.ndc.x * params.face.right.xyz + input.ndc.y * params.face.up.xyz);

    // Split-sum assumption: the view direction is unknown at prefilter time, so it is
    // taken to be the reflected direction, which in turn is taken to be the normal.
    // That is what reduces shading to a single cube lookup later, and also why this
    // approximation stretches reflections at grazing angles.
    float3 V = N;

    float3 prefiltered = 0;
    float  totalWeight = 0.0;

    for (uint i = 0; i < k_sampleCount; i++)
    {
        float2 Xi = Hammersley(i, k_sampleCount);
        float3 H  = ImportanceSampleGGX(Xi, N, params.roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = dot(N, L);
        if (NdotL > 0.0)
        {
            // Read an area of the environment rather than a point. pdf is how densely the GGX
            // sampling packs samples around L: dense means each sample stands for a small patch of
            // sky (saSample), sparse a large one. Compared with a texel's patch (saTexel), that picks
            // the mip; 0.5 * log2 because each level has 4x the texel area of the one above.
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf   = DistributionGGX(NdotH, params.roughness) * NdotH / (4.0 * HdotV) + 0.0001;

            float saTexel  = 4.0 * PI / (6.0 * k_environmentSize * k_environmentSize);
            float saSample = 1.0 / (float(k_sampleCount) * pdf + 0.0001);

            // Mip 0 is a plain copy of the environment: at zero roughness every sample
            // reads the same direction, so there is no solid angle to widen.
            float mipLevel = params.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

            // Weighting by NdotL is not part of the integral being solved: it biases
            // the average toward the centre of the lobe and further cuts the variance
            // in the rough mips.
            prefiltered += environmentTex.SampleLevel(L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    // Guard the division: a lobe that contributed nothing would otherwise produce NaNs,
    // which spread through the mip chain the moment anything filters them.
    return float4(prefiltered / max(totalWeight, 1e-4), 1.0);
}
