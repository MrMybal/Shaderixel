//@name Scan Ink Coverage
//@category Scan and Line Art
//@input Scan
//@input Blurred Paper Reference
//@param threshold float 0.5 0 1 "Global Threshold"
//@param adaptive float 0 0 1 "Adaptive Strength"
//@param bias float 0.08 0 0.5 "Local Paper Bias"
//@param softness float 0.02 0 0.25 "Threshold Softness"
//@param invert bool 0 "Invert Coverage"
// Opaque grayscale output is linear coverage, ready for mask processing.
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = Luma(saturate(LinearToSrgb(max(c.rgb, 0.0))));
    float4 ref = HasInput(1) ? Input(1, ctx.px) : float4(1, 1, 1, 1);
    float bg = Luma(lerp(float3(1, 1, 1), saturate(LinearToSrgb(max(ref.rgb, 0.0))), saturate(ref.a)));
    float t = lerp(threshold, saturate(bg - bias), saturate(adaptive));
    float s = max(softness, 0.00001);
    float m = 1.0 - smoothstep(t - s, t + s, y);
    if (invert != 0) m = 1.0 - m;
    return float4((m * saturate(c.a)).xxx, 1.0);
}
