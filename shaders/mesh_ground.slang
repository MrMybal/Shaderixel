// Ground shadow of the 3D view (raster): background pixels whose camera ray meets the
// plane under the model get the key light's shadow (shadow map) and a soft contact
// shadow. Plain background (alpha 0): the amount goes in alpha (< 0.5, post_final
// darkens its gray); HDRI background: its color is darkened directly.
#include "common.slang"

cbuffer Ground : register(b1)
{
    float4 gCamPos;
    float4 gRight;     // scaled by the frustum half width
    float4 gUp;        // scaled by the frustum half height
    float4 gFwd;
    float4 gPlane;     // y of the ground, fade radius, model center x, z
    float4 gFoot;      // footprint half extents x, z, contact strength, shadow strength (0: no shadow map)
    row_major float4x4 gLightVP;
    float4 gShadow;    // bias, 1 / map size
    uint4 gSize;
};

Texture2D<float> gDepth : register(t0);
Texture2D<float> gShadowMap : register(t1);
RWTexture2D<float4> gColor : register(u0);

float Lit(float3 p)
{
    float4 c = mul(gLightVP, float4(p, 1.0));
    float2 uv = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || c.z > 1.0) return 1.0;
    uint sw, sh;
    gShadowMap.GetDimensions(sw, sh);
    float lit = 0.0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
        {
            int2 q = clamp(int2(uv * float2(sw, sh)) + int2(x, y) * 2, int2(0, 0), int2(sw, sh) - 1);
            lit += c.z - gShadow.x <= gShadowMap.Load(int3(q, 0)) ? 1.0 : 0.0;
        }
    return lit / 25.0;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize.xy)) return;
    int2 p = int2(id.xy);
    if (gDepth.Load(int3(p, 0)) < 0.99999) return;  // the model is there
    float nx = (p.x + 0.5) / gSize.x * 2.0 - 1.0, ny = 1.0 - (p.y + 0.5) / gSize.y * 2.0;
    // Perspective: rays from the eye; orthographic (gSize.z): parallel rays from the view plane.
    float3 o = gSize.z ? gCamPos.xyz + gRight.xyz * nx + gUp.xyz * ny - gFwd.xyz * 100.0 : gCamPos.xyz;
    float3 d = gSize.z ? normalize(gFwd.xyz) : normalize(gFwd.xyz + gRight.xyz * nx + gUp.xyz * ny);
    if (d.y > -1e-4) return;
    float t = (gPlane.x - o.y) / d.y;
    if (t <= 0.0) return;
    float3 g = o + d * t;
    float2 rel = g.xz - gPlane.zw;
    float fade = 1.0 - smoothstep(gPlane.y * 0.5, gPlane.y, length(rel));
    if (fade <= 0.0) return;
    // Contact: a soft dark area under the footprint of the model.
    float k = length(rel / max(gFoot.xy, 1e-3));
    float contact = (1.0 - smoothstep(0.5, 1.5, k)) * gFoot.z;
    float shadow = gFoot.w > 0.0 ? (1.0 - Lit(g)) * gFoot.w : 0.0;
    float s = saturate(max(contact, shadow) + contact * shadow * 0.3) * fade;
    float4 c = gColor[p];
    if (c.a < 0.5) gColor[p] = float4(c.rgb, s * 0.45);
    else gColor[p] = float4(c.rgb * (1.0 - s * 0.85), c.a);
}
