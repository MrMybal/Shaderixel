//@name Print Ink Overprint
//@category Print and Halftone
//@blend
//@input Paper
//@input Ink
//@input Mask
//@param opacity float 1 0 1 "Ink Opacity"
//@param display_space bool 1 "Display-Space Pigment Mix"
// Alpha-correct multiply, with a straight-color fallback over transparency.
float4 Effect(EffectContext ctx)
{
    float4 a = Input(0, ctx.px), b = Input(1, ctx.px);
    float m = HasInput(2) ? saturate(Input(2, ctx.px).r) : 1.0;
    float ab = saturate(b.a * m * opacity), aa = saturate(a.a);
    float3 mixed = a.rgb * b.rgb;
    if (display_space != 0)
        mixed = SrgbToLinear(LinearToSrgb(max(a.rgb, 0.0)) * LinearToSrgb(max(b.rgb, 0.0)));
    float alpha = ab + aa * (1.0 - ab);
    float3 rgb = ab * (1.0 - aa) * b.rgb + aa * ab * mixed + aa * (1.0 - ab) * a.rgb;
    return float4(alpha > 0.000001 ? rgb / alpha : 0.0, alpha);
}
