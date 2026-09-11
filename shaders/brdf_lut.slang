// Integrates the second half of the split-sum approximation into a lookup table: the
// environment BRDF, which depends only on the view angle and the roughness and so is
// independent of the environment itself.

#include "include/ibl_sampling.slang"

// Samples per texel. Paid once at startup.
static const uint k_sampleCount = 1024;

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

// Smith geometry term, Schlick-GGX form.
//
// The k here is roughness^2 / 2, which is the IBL remapping. pbr.slang uses
// (roughness + 1)^2 / 8 for direct lighting, and the two are NOT interchangeable:
// substituting the direct-lighting one produces a table that is wrong by a smooth
// factor, so it reads as reflections at the wrong intensity.
float GeometrySchlickGGX(float NdotX, float roughness)
{
    float a = roughness * roughness;
    float k = a / 2.0;

    return NdotX / (NdotX * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Solves the environment BRDF for one (NdotV, roughness) pair. The two outputs are the
// scale and the bias later applied to the surface's F0: specular = F0 * A + B.
float2 IntegrateBRDF(float NdotV, float roughness)
{
    // Work in the tangent space where the normal is +Z. Only the angle between V and N
    // matters, so V can be placed in the XZ plane without loss of generality.
    float3 N = float3(0, 0, 1);
    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    float A = 0.0;
    float B = 0.0;

    for (uint i = 0; i < k_sampleCount; i++)
    {
        float2 Xi = Hammersley(i, k_sampleCount);
        float3 H  = ImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        // N is +Z, so the cosines against it are just the z components.
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G     = GeometrySmith(NdotV, NdotL, roughness);

            // Each sample's BRDF * NdotL divided by its pdf, with F left out: D cancels out
            // because H was drawn from D itself, leaving only G and these cosines.
            float GVis  = (G * VdotH) / (NdotH * NdotV);

            // Fresnel with F0 factored out, so the result can be applied to any F0.
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * GVis;
            B += Fc * GVis;
        }
    }

    return float2(A, B) / float(k_sampleCount);
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET
{
    // u = NdotV, v = roughness, with no vertical flip: pbr.slang reads the table back with
    // float2(NdotV, roughness), so changing the layout here means changing it there too.
    float2 uv = input.ndc * 0.5 + 0.5;

    // Guard NdotV against zero: it divides GVis, and the leftmost texel column would
    // otherwise come out as NaN and bleed into its neighbour under linear filtering.
    float NdotV     = max(uv.x, 1e-4);
    float roughness = uv.y;

    // Only two channels are stored; the format is eR16G16Sfloat.
    return float4(IntegrateBRDF(NdotV, roughness), 0.0, 1.0);
}
