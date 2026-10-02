//@name Masque de luminance
//@category Composition
//@param low float 0 0 1 "Bas"
//@param high float 1 0 1 "Haut"
//@param invert bool 0 "Inverser"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = smoothstep(low, max(high, low + 1e-4), LinearToSrgb1(Luma(c.rgb)));
    if (invert != 0) y = 1.0 - y;
    return float4(y.xxx, 1.0);
}
