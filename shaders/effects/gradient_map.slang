//@name Courbe de transfert de dégradé
//@category Réglages
//@param shadows color 0.02 0.0 0.1 1 "Ombres"
//@param highlights color 1 0.85 0.5 1 "Lumières"
//@param reverse bool 0 "Inverser"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float t = saturate(LinearToSrgb1(Luma(c.rgb)));
    if (reverse != 0) t = 1.0 - t;
    return float4(lerp(shadows.rgb, highlights.rgb, t), c.a);
}
