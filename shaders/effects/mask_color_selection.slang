//@name Color Range Mask
//@category Masks and Compositing
//@param key_color color 0 1 0 1 "Selected Color"
//@param tolerance float 0.12 0 1 "Color Tolerance"
//@param softness float 0.08 0.001 1 "Selection Softness"
//@param metric enum 0 "RGB|Chroma" "Distance Metric"
//@param invert bool 0 "Invert Selection"
//@param respect_alpha bool 1 "Respect Source Alpha"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 p = LinearToSrgb(max(c.rgb, 0.0));
    float3 k = LinearToSrgb(max(key_color.rgb, 0.0));
    float d;
    if (metric != 0)
    {
        p /= max(p.r + p.g + p.b, 0.0001);
        k /= max(k.r + k.g + k.b, 0.0001);
        d = distance(p, k) * 0.70710678;
    }
    else d = distance(p, k) * 0.57735027;
    float m = 1.0 - smoothstep(tolerance, tolerance + max(softness, 0.001), d);
    m = invert != 0 ? 1.0 - m : m;
    if (respect_alpha != 0) m *= c.a;
    // Masks store linear coverage; downstream mask effects read the red channel directly.
    return float4(saturate(m).xxx, 1.0);
}
