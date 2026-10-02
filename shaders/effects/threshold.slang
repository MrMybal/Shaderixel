//@name Seuil
//@category Réglages
//@param level float 0.5 0 1 "Seuil"
//@param softness float 0 0 0.5 "Douceur"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = LinearToSrgb1(Luma(c.rgb));
    float v = softness > 0.0 ? smoothstep(level - softness, level + softness, y) : step(level, y);
    return float4(v.xxx, c.a);
}
