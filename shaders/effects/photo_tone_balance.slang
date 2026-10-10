//@name Photo Tone Balance
//@category Photo Essentials
//@param exposure float 0 -5 5 "Exposure (Stops)"
//@param shadows float 0 -1 1 "Shadows"
//@param highlights float 0 -1 1 "Highlights"
//@param contrast float 0 -0.8 1 "Contrast"
//@param vibrance float 0 -1 1 "Vibrance"
//@param saturation float 1 0 2 "Saturation"
// Tonal adjustments cannot reconstruct detail that was clipped in the source.
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 p = LinearToSrgb(max(c.rgb * exp2(exposure), 0.0));
    float y = Luma(p);
    float sh = 1.0 - smoothstep(0.0, 0.6, y);
    float hi = smoothstep(0.45, 1.0, y);
    p = max(p + 0.25 * (shadows * sh + highlights * hi), 0.0);
    p = max((p - 0.5) * max(0.2, 1.0 + contrast) + 0.5, 0.0);
    float sat = (max(p.r, max(p.g, p.b)) - min(p.r, min(p.g, p.b))) / max(max(p.r, max(p.g, p.b)), 0.00001);
    float scale = max(0.0, saturation + vibrance * (1.0 - saturate(sat)));
    p = max(lerp(Luma(p).xxx, p, scale), 0.0);
    return float4(SrgbToLinear(p), c.a);
}
