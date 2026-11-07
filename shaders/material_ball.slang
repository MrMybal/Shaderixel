// Brush material preview (Substance-like): a sphere shaded with the brush's PBR values (base color, roughness,
// metal, emissive) under a studio light (key, rim and a sky / floor environment), GGX specular. Output: sRGB with
// alpha (the sphere's silhouette, antialiased).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    float gBaseR, gBaseG;     // sRGB base color
    float gBaseB, gRough, gMetal, gEmiR;
    float gEmiG, gEmiB;       // sRGB emissive (black: none)
    float gHeight;            // 0.5 flat; a bump pattern shows relief / hollows
    float gHeightOn;
};

RWTexture2D<float4> gOut : register(u0);

float D_GGX(float nh, float a)
{
    const float a2 = a * a;
    const float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * d * d, 1e-6);
}

float G_Smith(float nv, float nl, float a)
{
    const float k = a * 0.5;
    return (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
}

// Studio environment: bright sky above, a soft horizon, a dark floor; blurrier with the roughness.
float3 Env(float3 d, float rough)
{
    const float y = d.y;
    const float3 sky = float3(0.95, 0.97, 1.0) * 1.25, horizon = float3(0.55, 0.55, 0.58), floorC = float3(0.06, 0.06, 0.07);
    const float s = lerp(0.12, 0.9, rough);
    float3 c = y > 0.0 ? lerp(horizon, sky, smoothstep(0.0, s, y)) : lerp(horizon, floorC, smoothstep(0.0, s, -y));
    // A softbox on the left, sharp on glossy materials.
    const float box = smoothstep(0.80 - rough * 0.5, 0.97 - rough * 0.3, dot(d, normalize(float3(-0.7, 0.45, 0.55))));
    return c + box * (2.5 * (1.0 - rough * 0.7));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    const float R = min(gSize.x, gSize.y) * 0.46;
    const float2 c = float2(gSize) * 0.5;
    const float2 q = (float2(id.xy) + 0.5 - c) / R;
    const float r2 = dot(q, q);
    const float edge = saturate((1.0 - sqrt(r2)) * R + 0.5);
    if (edge <= 0.0) { gOut[id.xy] = float4(0, 0, 0, 0); return; }
    float3 n = normalize(float3(q.x, -q.y, sqrt(saturate(1.0 - r2))));
    if (gHeightOn > 0.5 && abs(gHeight - 0.5) > 0.01)
    {
        // Relief (height above 0.5) or hollows (below): rings of bumps on the sphere.
        const float3 p = n * 9.0;
        const float k = (gHeight - 0.5) * 0.9;
        const float3 g = float3(cos(p.x) * sin(p.y) * sin(p.z), sin(p.x) * cos(p.y) * sin(p.z), sin(p.x) * sin(p.y) * cos(p.z));
        n = normalize(n - (g - n * dot(g, n)) * k);
    }
    const float3 v = float3(0, 0, 1);
    const float3 base = SrgbToLinear(saturate(float3(gBaseR, gBaseG, gBaseB)));
    const float rough = clamp(gRough, 0.03, 1.0), a = rough * rough, metal = saturate(gMetal);
    const float3 f0 = lerp(float3(0.04, 0.04, 0.04), base, metal);
    const float3 diffC = base * (1.0 - metal);
    const float nv = saturate(dot(n, v)) + 1e-4;
    float3 col = 0;
    // Lights: key (upper left), rim (behind right), fill (right).
    const float3 L[3] = {normalize(float3(-0.55, 0.6, 0.6)), normalize(float3(0.8, 0.35, -0.45)), normalize(float3(0.9, -0.1, 0.4))};
    const float I[3] = {3.0, 2.0, 0.6};
    for (int i = 0; i < 3; ++i)
    {
        const float3 l = L[i], hv = normalize(l + v);
        const float nl = saturate(dot(n, l));
        if (nl <= 0.0) continue;
        const float nh = saturate(dot(n, hv)), vh = saturate(dot(v, hv));
        const float3 F = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
        const float3 spec = D_GGX(nh, a) * G_Smith(nv, nl, a) * F / max(4.0 * nv * nl, 1e-4);
        col += (diffC / 3.14159265 * (1.0 - F) + spec) * nl * I[i];
    }
    // Environment: diffuse from the hemisphere, specular from the reflected direction.
    const float3 rdir = reflect(-v, n);
    const float3 Fe = f0 + (max(1.0 - rough, f0) - f0) * pow(saturate(1.0 - nv), 5.0);
    col += diffC * Env(n, 1.0) * 0.35 + Env(rdir, rough) * Fe * 0.8;
    col += SrgbToLinear(saturate(float3(gEmiR, gEmiG, gEmiB))) * 2.0;
    // Filmic-ish curve, then sRGB.
    col = col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14);
    gOut[id.xy] = float4(LinearToSrgb(saturate(col)), edge);
}
