// 3D viewport: the document composite as base color on the mesh, simple studio lighting.
#include "common.slang"

cbuffer View : register(b1)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gLightDir;
    uint gFlags;       // 1 = has texture, 2 = wireframe-ish UV checker
    float3 _pad;
};

Texture2D<float4> gAlbedo : register(t0);

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

float4 psMain(VSOut i, bool front : SV_IsFrontFace) : SV_Target0
{
    float3 n = normalize(i.nrm);
    float3 v = normalize(gCamPos.xyz - i.world);
    if (dot(n, v) < 0) n = -n;
    float3 base = float3(0.55, 0.55, 0.58);
    if (gFlags & 1)
    {
        float4 c = gAlbedo.SampleLevel(sLinearWrap, i.uv, 0);
        float4 s = Unpremultiply(c);
        base = lerp(base, s.rgb, saturate(c.a));
    }
    float3 l = normalize(gLightDir.xyz);
    float diff = saturate(dot(n, l));
    float3 h = normalize(l + v);
    float spec = pow(saturate(dot(n, h)), 48.0) * 0.25;
    float fill = saturate(dot(n, normalize(float3(-0.6, 0.3, -0.4)))) * 0.25;
    float rim = pow(1.0 - saturate(dot(n, v)), 3.0) * 0.15;
    float3 col = base * (0.18 + diff * 0.85 + fill) + spec + rim;
    return float4(LinearToSrgb(col), 1.0);
}
