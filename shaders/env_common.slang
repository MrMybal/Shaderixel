// HDRI environment atlas: an equirectangular map prefiltered into 6 blur levels
// stacked vertically (level k is (W>>k) x (H>>k), H = W/2). Shared by the 3D
// view's shading (mesh_view) and its background (mesh_sky).
#include "common.slang"

static const float kEnvPI = 3.14159265;

float3 EnvLevel(Texture2D<float4> env, float3 d, int k, float rotation)
{
    uint aw, ah;
    env.GetDimensions(aw, ah);
    float W = aw, H = aw * 0.5;
    float s = sin(rotation), c = cos(rotation);
    d = float3(c * d.x + s * d.z, d.y, -s * d.x + c * d.z);
    float2 uv = float2(atan2(d.x, -d.z) / (2.0 * kEnvPI) + 0.5, acos(clamp(d.y, -1.0, 1.0)) / kEnvPI);
    float w = W / exp2(k), h = H / exp2(k);
    float y0 = 2.0 * H - 2.0 * h;
    float2 px = float2(frac(uv.x) * w, clamp(uv.y * h, 0.5, h - 0.5));
    px.x = clamp(px.x, 0.5, w - 0.5);
    return env.SampleLevel(sLinearClamp, (px + float2(0, y0)) / float2(aw, ah), 0).rgb;
}

// Environment radiance in direction d, lod in [0, 5] (0 sharp, 5 most blurred).
float3 EnvSample(Texture2D<float4> env, float3 d, float lod, float rotation)
{
    lod = clamp(lod, 0.0, 5.0);
    int k0 = (int)floor(lod);
    int k1 = min(k0 + 1, 5);
    return lerp(EnvLevel(env, d, k0, rotation), EnvLevel(env, d, k1, rotation), lod - k0);
}
