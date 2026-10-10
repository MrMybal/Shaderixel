//@name Print Ink Plate
//@category Print and Halftone
//@halo full
//@param channel enum 0 "Darkness|Red Absorption|Green Absorption|Blue Absorption" "Tone Source"
//@param offset float2 0 0 -64 64 "Registration Offset (px)"
//@param ink color 0.02 0.12 0.5 1 "Ink Color"
//@param gamma float 1 0.1 4 "Coverage Curve"
//@param density float 1 0 2 "Ink Density"
// Transparent pigment layer. Outside the document is transparent.
float4 Effect(EffectContext ctx)
{
    float2 p = float2(ctx.px) + 0.5 - offset;
    if (any(p < 0.5) || any(p > ctx.size - 0.5)) return 0.0;
    float4 c = InputUV(0, p * ctx.texel);
    float3 rgb = saturate(LinearToSrgb(max(c.rgb, 0.0)));
    float v = channel == 1 ? rgb.r : (channel == 2 ? rgb.g : (channel == 3 ? rgb.b : Luma(rgb)));
    float coverage = saturate(pow(1.0 - v, max(gamma, 0.1)) * density) * saturate(c.a * ink.a);
    return float4(ink.rgb, coverage);
}
