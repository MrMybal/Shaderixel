//@name Scan Colorize Ink
//@category Scan and Line Art
//@input Mask
//@param color color 0.01 0.01 0.01 1 "Ink Color"
//@param opacity float 1 0 1 "Ink Opacity"
float4 Effect(EffectContext ctx)
{
    float m = saturate(Input(0, ctx.px).r);
    return float4(color.rgb, m * saturate(color.a * opacity));
}
