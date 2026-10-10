//@name Photo Local Contrast
//@category Photo Essentials
//@input Image
//@input Blurred Image
//@param amount float 0.3 -1 2 "Local Contrast"
//@param threshold float 0.01 0 0.2 "Detail Threshold"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    if (!HasInput(1)) return c;
    float3 p = LinearToSrgb(max(c.rgb, 0.0));
    float3 b = LinearToSrgb(max(Input(1, ctx.px).rgb, 0.0));
    float y = Luma(p), d = y - Luma(b);
    float mid = 4.0 * saturate(y) * (1.0 - saturate(y));
    float gate = smoothstep(threshold, threshold + 0.02, abs(d));
    p = max(p + d * amount * mid * gate, 0.0);
    return float4(SrgbToLinear(p), c.a);
}
