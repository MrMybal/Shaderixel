//@name Reoriented Normal Blend
//@category Blends
//@blend
//@input Base
//@input Detail
//@input Mask
//@param force float 1 0 1 "Detail Strength"
float3 DecodeNormal(float3 rgb)
{
    float3 n = LinearToSrgb(max(rgb, 0.0)) * 2.0 - 1.0;
    return dot(n, n) > 0.000001 ? normalize(n) : float3(0, 0, 1);
}
float4 Effect(EffectContext ctx)
{
    float4 a = Input(0, ctx.px), b = Input(1, ctx.px);
    float3 base = HasInput(0) && a.a > 0.00001 ? DecodeNormal(a.rgb) : float3(0, 0, 1);
    float3 detail = HasInput(1) && b.a > 0.00001 ? DecodeNormal(b.rgb) : float3(0, 0, 1);
    float mask = HasInput(2) ? LinearToSrgb1(Input(2, ctx.px).r) * Input(2, ctx.px).a : 1.0;
    float3 t = base + float3(0, 0, 1);
    float3 u = detail * float3(-1, -1, 1);
    float3 rotated = t * dot(t, u) / max(t.z, 0.0001) - u;
    float3 n = lerp(base, rotated, saturate(force * mask * b.a));
    n = dot(n, n) > 0.000001 ? normalize(n) : base;
    return float4(SrgbToLinear(n * 0.5 + 0.5), 1.0);
}
