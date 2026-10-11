// 3D viewport: PBR preview of the document's material channels on the mesh.
// Channels (t0..t5): base color, roughness, metallic, normal (tangent space,
// DirectX/Unreal convention), emissive, ambient occlusion. Data channels are
// read as their visual (sRGB-encoded) value so a 50% gray means 0.5.
#include "env_common.slang"

cbuffer View : register(b1)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gLightDir;
    uint gFlags;       // bit i: channel i present, bit 7: HDRI environment
    float gEnvIntensity;
    float gEnvRotation; // radians around the vertical axis
    float gViewMode;    // 0 PBR, k > 0: show channel k-1 alone (visual values), 8..11 baked mesh maps (in t5)
    float gExposure;    // EV
    uint gTonemap;      // 0 soft, 1 ACES (filmic)
    float2 _pad;
    row_major float4x4 gLightVP;  // shadow map
    float4 gLightCol;             // key light color x intensity
    float4 gShadow;               // x strength, y bias, z 1 / map size
    uint4 gOpts;                  // x: 1 shadows, 2 baked occlusion, 4 HDR output (post-process after); y: occlusion strength (x1000); z: extra lights
    float4 gXDir[4];              // extra lights (fill, rim...): direction toward the light
    float4 gXCol[4];              // color x intensity
};

float3 Aces(float3 x)
{
    // Narkowicz fit of the ACES filmic curve.
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

Texture2D<float4> gBase : register(t0);
Texture2D<float4> gRough : register(t1);
Texture2D<float4> gMetal : register(t2);
Texture2D<float4> gNormal : register(t3);
Texture2D<float4> gEmissive : register(t4);
Texture2D<float4> gAO : register(t5);
Texture2D<float4> gHeight : register(t6);

// HDRI: equirectangular environment prefiltered into 6 blur levels stacked
// vertically (level k is (W>>k) x (H>>k), H = W/2), sampled by roughness.
Texture2D<float4> gEnv : register(t7);
Texture2D<float4> gMaps : register(t8);      // baked mesh maps (R occlusion)
Texture2D<float> gShadowMap : register(t9);

float ShadowAt(float3 world, float3 n)
{
    // 3x3 PCF in the orthographic light view.
    float4 c = mul(gLightVP, float4(world + n * 0.004, 1.0));
    float2 uv = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    uint sw, sh;
    gShadowMap.GetDimensions(sw, sh);
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            int2 p = clamp(int2(uv * float2(sw, sh)) + int2(x, y), int2(0, 0), int2(sw, sh) - 1);
            lit += c.z - gShadow.y <= gShadowMap.Load(int3(p, 0)) ? 1.0 : 0.0;
        }
    return lerp(1.0, lit / 9.0, gShadow.x);
}

static const float kBumpStrength = 32.0;  // tangent tilt per unit of height change per texel (same as bake_normal)

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};
struct VSOut
{
    float4 clip : SV_Position;
    float3 world : TEXCOORD0;
    float3 nrm : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

VSOut vsMain(VSIn i)
{
    VSOut o;
    o.clip = mul(gViewProj, float4(i.pos, 1.0));
    o.world = i.pos;
    o.nrm = i.nrm;
    o.uv = i.uv;
    return o;
}

// Visual value of a data channel (straight sRGB) blended over a default by coverage.
float3 Data(Texture2D<float4> t, float2 uv, float3 def)
{
    float4 c = t.SampleLevel(sLinearWrap, uv, 0);
    float4 s = Unpremultiply(c);
    return lerp(def, LinearToSrgb(saturate(s.rgb)), saturate(c.a));
}

static const float PI = 3.14159265;

float3 Shade(float3 n, float3 v, float3 l, float3 lightCol, float3 diffColor, float3 f0, float rough)
{
    float NdotL = saturate(dot(n, l));
    if (NdotL <= 0) return 0;
    float3 h = normalize(l + v);
    float NdotV = max(dot(n, v), 1e-3), NdotH = saturate(dot(n, h)), VdotH = saturate(dot(v, h));
    float a = max(rough * rough, 0.002);
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d);
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float G = (NdotV / (NdotV * (1 - k) + k)) * (NdotL / (NdotL * (1 - k) + k));
    float3 F = f0 + (1.0 - f0) * pow(1.0 - VdotH, 5.0);
    float3 spec = D * G * F / (4.0 * NdotV * NdotL + 1e-4);
    float3 diff = (1.0 - F) * diffColor / PI;
    return (diff + spec) * lightCol * NdotL * PI;
}

float3 Sky(float3 d)
{
    float t = d.y * 0.5 + 0.5;
    return lerp(float3(0.20, 0.19, 0.18), float3(0.55, 0.60, 0.70), t);
}

float3 Env(float3 d, float lod)
{
    return EnvSample(gEnv, d, lod, gEnvRotation) * gEnvIntensity;
}

float4 psMain(VSOut i, bool front : SV_IsFrontFace) : SV_Target0
{
    float3 n = normalize(i.nrm);
    float3 v = normalize(gCamPos.xyz - i.world);
    if (dot(n, v) < 0) n = -n;

    float3 base = float3(0.55, 0.55, 0.58);
    if (gFlags & 1)
    {
        float4 c = gBase.SampleLevel(sLinearWrap, i.uv, 0);
        base = lerp(base, Unpremultiply(c).rgb, saturate(c.a));
    }
    float rough = (gFlags & 2) ? Data(gRough, i.uv, 0.5).r : 0.5;
    float metal = (gFlags & 4) ? Data(gMetal, i.uv, 0.0).r : 0.0;
    float ao = (gFlags & 32) ? Data(gAO, i.uv, 1.0).r : 1.0;
    float3 emissive = 0;
    if (gFlags & 16)
    {
        float4 c = gEmissive.SampleLevel(sLinearWrap, i.uv, 0);
        emissive = c.rgb;  // premultiplied linear = color * coverage
    }
    if (gFlags & (8 | 64))
    {
        // Tangent frame from screen-space derivatives (no vertex tangents needed).
        float3 tn = (gFlags & 8) ? Data(gNormal, i.uv, float3(0.5, 0.5, 1.0)) * 2.0 - 1.0 : float3(0, 0, 1);
        if (gFlags & 64)
        {
            // Height: central differences one texel apart, added to the normal's slope.
            uint hw, hh;
            gHeight.GetDimensions(hw, hh);
            float2 t = 1.0 / float2(hw, hh);
            float dhdu = (Data(gHeight, i.uv + float2(t.x, 0), 0.5).r - Data(gHeight, i.uv - float2(t.x, 0), 0.5).r) * 0.5;
            float dhdv = (Data(gHeight, i.uv + float2(0, t.y), 0.5).r - Data(gHeight, i.uv - float2(0, t.y), 0.5).r) * 0.5;
            tn = normalize(float3(tn.xy - float2(dhdu, dhdv) * kBumpStrength * tn.z, tn.z));
        }
        float3 dp1 = ddx(i.world), dp2 = ddy(i.world);
        float2 duv1 = ddx(i.uv), duv2 = ddy(i.uv);
        float3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
        float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
        float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
        float inv = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-20));
        n = normalize(T * (inv * tn.x) + B * (inv * tn.y) + n * tn.z);
    }
    rough = clamp(rough, 0.03, 1.0);
    float3 diffColor = base * (1.0 - metal);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), base, metal);

    float3 col = 0;
    if (gOpts.x & 2)
    {
        // Baked occlusion of the model (Cuisson / smart mask maps) on the ambient light.
        float bao = gMaps.SampleLevel(sLinearWrap, i.uv, 0).r;
        ao *= lerp(1.0, bao, gOpts.y / 1000.0);
    }
    const float shadow = (gOpts.x & 1) ? ShadowAt(i.world, normalize(i.nrm)) : 1.0;
    float NdotV = saturate(dot(n, v));
    float3 Fenv = f0 + (max(1.0 - rough, f0) - f0) * pow(1.0 - NdotV, 5.0);
    float3 r = reflect(-v, n);
    if (gFlags & 128)
    {
        // Image-based lighting: blurred environment for diffuse, roughness-selected level for specular.
        col += diffColor * Env(n, 5.0) * ao;
        col += Fenv * Env(r, sqrt(rough) * 5.0) * ao;
        // Sun (optional with an HDRI): the key light, with its shadow.
        if (any(gLightCol.rgb > 0.0)) col += Shade(n, v, normalize(gLightDir.xyz), gLightCol.rgb, diffColor, f0, rough) * shadow;
    }
    else
    {
        col += Shade(n, v, normalize(gLightDir.xyz), gLightCol.rgb, diffColor, f0, rough) * shadow;
        col += Shade(n, v, normalize(float3(-0.6, 0.3, -0.4)), float3(0.45, 0.5, 0.6) * 0.5, diffColor, f0, rough);
        // Ambient: hemisphere diffuse + rough-aware reflection.
        col += diffColor * Sky(n) * 0.6 * ao;
        col += Fenv * lerp(Sky(r), Sky(n) * 0.6, rough) * ao;
    }
    for (uint xi = 0; xi < min(gOpts.z, 4u); ++xi)
        col += Shade(n, v, normalize(gXDir[xi].xyz), gXCol[xi].rgb, diffColor, f0, rough);
    col += emissive;
    int vmode = (int)(gViewMode + 0.5);
    if ((gOpts.x & 4) && vmode == 0) return float4(col, 1.0);  // linear HDR: exposure, tone curve... in the post-process
    col *= exp2(gExposure);
    col = gTonemap == 1 ? Aces(col * 0.8) : col / (1.0 + col * 0.15);  // filmic, or gentle highlight compression
    int vm = (int)(gViewMode + 0.5);
    if (vm >= 8)
    {
        // Baked mesh maps (smart masks): AO, convexity, concavity, thickness.
        float4 m = gAO.SampleLevel(sLinearWrap, i.uv, 0);
        float x = vm == 8 ? m.r : vm == 9 ? m.g : vm == 10 ? m.b : m.a;
        return float4(x, x, x, 1.0);
    }
    if (vm > 0)
    {
        // Single-channel inspection (unlit, as painted).
        float3 o;
        if (vm == 1) o = LinearToSrgb(saturate(base));
        else if (vm == 2) o = float3(rough, rough, rough);
        else if (vm == 3) o = float3(metal, metal, metal);
        else if (vm == 4) o = (gFlags & 8) ? Data(gNormal, i.uv, float3(0.5, 0.5, 1.0)) : float3(0.5, 0.5, 1.0);
        else if (vm == 5) o = LinearToSrgb(saturate(emissive));
        else if (vm == 6) o = float3(ao, ao, ao);
        else o = (gFlags & 64) ? Data(gHeight, i.uv, 0.5).rrr : float3(0.5, 0.5, 0.5);
        return float4(o, 1.0);
    }
    return float4(LinearToSrgb(saturate(col)), 1.0);
}
