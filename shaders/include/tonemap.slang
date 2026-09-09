
static const float k_exposure = 0.5;

// Narkowicz 2015, analytic adjustment of ACES curve
float3 TonemapACES(float3 color)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}