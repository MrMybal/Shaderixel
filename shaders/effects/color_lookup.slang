//@name Correspondance de couleur
//@category Réglages
//@param lut lut "LUT 3D (.cube)"
//@param amount float 1 0 1 "Intensité"
// Applies a 3D LUT (.cube, sRGB-encoded input/output as produced by grading tools).
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    if (lut.x < 2.0) return c;
    float3 s = LinearToSrgb(saturate(c.rgb));
    float3 g = lut_sample(s);
    c.rgb = lerp(c.rgb, SrgbToLinear(saturate(g)), amount);
    return c;
}
