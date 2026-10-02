//@name Négatif
//@category Réglages
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    return float4(SrgbToLinear(1.0 - LinearToSrgb(saturate(c.rgb))), c.a);
}
