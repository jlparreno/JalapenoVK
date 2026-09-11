
static const float PI = 3.14159265359;

// Source mip to integrate from. The grid below takes one sample per small patch of sky.
// At mip 0 a patch holds hundreds of texels but only one is read, so a small bright spot like the sun 
// is either hit or missed entirely, showing up as fireflies.
// At mip 5 one texel already covers a patch, so each sample reads the average of the sky it stands for.
// Must stay below k_environmentMips in EnvironmentMap.h.
static const float k_sourceMip = 5.0;

[[vk::binding(0, 0)]] SamplerCube environmentTex;

// float4 rather than float3: push constant members are 16-byte aligned, so the
// padding would exist either way, and this keeps the C++ struct a plain copy.
struct CubeFaceOrientation
{
    float4 right;
    float4 up;
    float4 forward;
};
[[vk::push_constant]]
ConstantBuffer<CubeFaceOrientation> face;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 ndc;                     // Position of this fragment in normalized device coordinates, [-1, 1].
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
    // Direction this output texel stands for: the normal of a surface facing that way.
    float3 N = normalize(face.forward.xyz + input.ndc.x * face.right.xyz + input.ndc.y * face.up.xyz);

    // Tangent frame around N, to turn hemisphere angles into world directions.
    // The helper up vector switches near the poles, where it would be parallel to N.
    float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(0, 0, 1);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float3 irradiance = 0;
    int samples = 0;

    // Angular step of the sampling grid, in radians.
    const float delta = 0.025;

    // Walk the hemisphere around N: phi goes around it, theta from N down to the horizon.
    for (float phi = 0.0; phi < 2.0 * PI; phi += delta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += delta)
        {
            // Spherical to cartesian in tangent space (N is +Z), then into world space.
            float3 tangent = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleDir = tangent.x * right + tangent.y * up + tangent.z * N;

            // cos(theta): light arriving at a grazing angle counts less (Lambert).
            // sin(theta): rows near N cover less sky, so their samples weigh less.
            irradiance += environmentTex.SampleLevel(sampleDir, k_sourceMip).rgb * cos(theta) * sin(theta);
            samples++;
        }
    }

    // Average over the grid. The PI factor makes this irradiance / PI, which pbr.slang
    // multiplies by the albedo directly, without dividing by PI again.
    irradiance = PI * irradiance / float(samples);

    return float4(irradiance, 1.0);
}