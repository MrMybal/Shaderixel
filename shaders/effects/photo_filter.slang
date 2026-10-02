//@name Filtre photo
//@category Réglages
//@param tint color 0.93 0.54 0.1 1 "Couleur"
//@param density float 0.25 0 1 "Densité"
//@param preserve bool 1 "Conserver la luminosité"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 filtered = c.rgb * lerp(1.0, tint.rgb * 2.0, density);
    if (preserve != 0)
        filtered *= Luma(c.rgb) / max(Luma(filtered), 1e-5);
    return float4(max(filtered, 0.0), c.a);
}
