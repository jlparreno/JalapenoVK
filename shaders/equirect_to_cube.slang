// Projects an equirectangular HDR image onto one face of a cubemap. 
// Run once at startup, six times, once per face, each draw targeting a single-layer 2D view of the cube image.

static const float PI = 3.14159265359;

// Equirectangular HDR texture sampler
[[vk::binding(0, 0)]] Sampler2D equirectTex;

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
    float2 ndc;                 // Position of this fragment in normalized device coordinates, [-1, 1].
};

[shader("vertex")]
VSOutput vertMain(uint vertexID : SV_VertexID)
{
    // One oversized triangle covering the whole viewport: (-1,-1), (3,-1), (-1,3).
    // ┌──────────┬─────────┬───────────────┬────────┬──────────────┐
    // │ vertexID │ binary  │ (id << 1) & 2 │ id & 2 │  * 2 - 1     │ 
    // ├──────────┼─────────┼───────────────┼────────┼──────────────┤
    // │ 0        │ 00      │ 0             │ 0      │ (-1, -1)     │
    // ├──────────┼─────────┼───────────────┼────────┼──────────────┤
    // │ 1        │ 01      │ 2             │ 0      │ (3, -1)      │
    // ├──────────┼─────────┼───────────────┼────────┼──────────────┤
    // │ 2        │ 10      │ 0             │ 2      │ (-1, 3)      │
    // └──────────┴─────────┴───────────────┴────────┴──────────────┘

    float2 ndc = float2((vertexID << 1) & 2, vertexID & 2) * 2.0 - 1.0;

    VSOutput output;
    output.pos = float4(ndc, 0.0, 1.0);
    output.ndc = ndc;

    return output;
}

// Maps a world-space direction to a texel in the equirectangular source.
float2 DirectionToEquirectUV(float3 dir)
{
    // atan2 gives longitude in [-PI, PI] and asin latitude in [-PI/2, PI/2].
    // Scaling and biasing puts both in [0, 1].
    float2 uv = float2(atan2(dir.z, dir.x), asin(dir.y));
    uv *= float2(1.0 / (2.0 * PI), 1.0 / PI);
    uv += 0.5;

    // Vertical flip required: Vulkan samples textures from a top-left origin.
    uv.y = 1.0 - uv.y;

    return uv;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET
{
    float3 dir = normalize(face.forward.xyz + input.ndc.x * face.right.xyz + input.ndc.y * face.up.xyz);

    // Alpha is unused
    return float4(equirectTex.Sample(DirectionToEquirectUV(dir)).rgb, 1.0);
}
