
// Scale applied to the linear HDR radiance before tonemapping. 
// Shared by the skybox and the PBR shader, so the background and the lit geometry are exposed alike.
// Value is tied to the HDRI used: a different environment may need a different value.
static const float k_exposure = 0.5f;

// Narkowicz 2015, analytic adjustment of ACES curve
float3 TonemapACES(float3 color)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}