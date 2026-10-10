//@name Luminance Range Mask
//@category Masks and Compositing
//@param low float 0.2 0 1 "Lower Luminance"
//@param high float 0.8 0 1 "Upper Luminance"
//@param softness float 0.05 0 0.5 "Selection Softness"
//@param invert bool 0 "Invert Selection"
//@param respect_alpha bool 1 "Respect Source Alpha"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = Luma(LinearToSrgb(max(c.rgb, 0.0)));
    float lo = min(low, high), hi = max(low, high);
    float lower = lo <= 0.0 ? 1.0 : softness <= 0.0 ? step(lo, y) : smoothstep(lo - softness, lo + softness, y);
    float upper = hi >= 1.0 ? 1.0 : softness <= 0.0 ? step(y, hi) : 1.0 - smoothstep(hi - softness, hi + softness, y);
    float m = saturate(lower * upper);
    m = invert != 0 ? 1.0 - m : m;
    if (respect_alpha != 0) m *= c.a;
    return float4(m.xxx, 1.0);
}
