// 3D viewport: PBR preview of the document's material channels on the mesh.
// Channels (t0..t5): base color, roughness, metallic, normal (tangent space,
// DirectX/Unreal convention), emissive, ambient occlusion. Data channels are
// read as their visual (sRGB-encoded) value so a 50% gray means 0.5.
#include "common.slang"

cbuffer View : register(b1)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gLightDir;
    uint gFlags;       // bit i: channel i present
    float3 _pad;
};

Texture2D<float4> gBase : register(t0);
Texture2D<float4> gRough : register(t1);
Texture2D<float4> gMetal : register(t2);
Texture2D<float4> gNormal : register(t3);
Texture2D<float4> gEmissive : register(t4);
Texture2D<float4> gAO : register(t5);
Texture2D<float4> gHeight : register(t6);

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
    col += Shade(n, v, normalize(gLightDir.xyz), float3(1.0, 0.97, 0.92) * 1.6, diffColor, f0, rough);
    col += Shade(n, v, normalize(float3(-0.6, 0.3, -0.4)), float3(0.45, 0.5, 0.6) * 0.5, diffColor, f0, rough);
    // Ambient: hemisphere diffuse + rough-aware reflection.
    float NdotV = saturate(dot(n, v));
    float3 Fenv = f0 + (max(1.0 - rough, f0) - f0) * pow(1.0 - NdotV, 5.0);
    float3 r = reflect(-v, n);
    col += diffColor * Sky(n) * 0.6 * ao;
    col += Fenv * lerp(Sky(r), Sky(n) * 0.6, rough) * ao;
    col += emissive;
    col = col / (1.0 + col * 0.15);  // gentle highlight compression
    return float4(LinearToSrgb(saturate(col)), 1.0);
}
