// Quasi-random sampling shared by the two IBL precompute shaders: the specular
// prefilter and the BRDF lookup table. Both solve an integral over the GGX lobe by
// Monte Carlo, so both need the same sequence and the same importance distribution.

static const float PI = 3.14159265359;

// Van der Corput sequence: mirrors the bits of i around the binary point, which places
// consecutive indices as far apart as possible within [0, 1).
float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);

    return float(bits) * 2.3283064365386963e-10;    // 1 / 2^32
}

// Hammersley point set: a low-discrepancy sequence in two dimensions. It covers the
// domain far more evenly than random numbers would for the same sample count, which is
// what keeps these integrals smooth without paying for many more samples.
float2 Hammersley(uint i, uint count)
{
    return float2(float(i) / float(count), RadicalInverseVdC(i));
}

// Draws a halfway vector from the GGX distribution for this roughness, so samples
// concentrate where the lobe actually carries energy instead of spreading uniformly
// over the hemisphere.
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Spherical to cartesian, in the tangent space where N is +Z.
    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    // Tangent space to world. The guard avoids the degenerate cross product when N is
    // parallel to the reference axis, the same problem the irradiance convolution has
    // on the +Y and -Y faces.
    float3 up        = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent   = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}
