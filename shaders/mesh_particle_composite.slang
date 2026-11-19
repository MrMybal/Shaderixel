// Resolve the same impact and trail coverage into up to four extra material channels.
cbuffer Root : register(b0) {
    uint2 origin;
    uint2 size;
    uint channelCount;
    uint hasSelection;
    float opacity;
};
struct Deposit {
    float4 trail;
    float4 impact;
    float4 strength;
};
cbuffer Channels : register(b1) { Deposit material[4]; };

Texture2D<float> trailCoverage : register(t0);
Texture2D<float> impactCoverage : register(t1);
Texture2D<float> selection : register(t2);
Texture2D<float4> before0 : register(t3);
Texture2D<float4> before1 : register(t4);
Texture2D<float4> before2 : register(t5);
Texture2D<float4> before3 : register(t6);
RWTexture2D<float4> layer0 : register(u0);
RWTexture2D<float4> layer1 : register(u1);
RWTexture2D<float4> layer2 : register(u2);
RWTexture2D<float4> layer3 : register(u3);

float4 ApplyDeposit(float4 before, Deposit m, float mw, float impact, float sel) {
    const float trailAlpha = saturate(mw * opacity * m.strength.x) * sel * m.trail.a;
    const float impactAlpha = saturate(impact * opacity * m.strength.y) * sel * m.impact.a;
    const float4 trail = float4(m.trail.rgb * trailAlpha, trailAlpha);
    const float4 splat = float4(m.impact.rgb * impactAlpha, impactAlpha);
    return splat + (trail + before * (1.0 - trailAlpha)) * (1.0 - impactAlpha);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= size)) return;
    const uint2 px = origin + tid.xy;
    const int3 p = int3(px, 0);
    const float mw = trailCoverage.Load(p);
    const float impact = impactCoverage.Load(p);
    if (max(mw, impact) <= 0.0) return;
    const float sel = hasSelection != 0 ? selection.Load(p) : 1.0;
    layer0[px] = ApplyDeposit(before0.Load(p), material[0], mw, impact, sel);
    if (channelCount > 1) layer1[px] = ApplyDeposit(before1.Load(p), material[1], mw, impact, sel);
    if (channelCount > 2) layer2[px] = ApplyDeposit(before2.Load(p), material[2], mw, impact, sel);
    if (channelCount > 3) layer3[px] = ApplyDeposit(before3.Load(p), material[3], mw, impact, sel);
}
