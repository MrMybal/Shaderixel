//@name Noir et blanc
//@category Réglages
//@param weights color 0.30 0.59 0.11 1 "Pondération RVB"
//@param tint color 1 1 1 1 "Virage"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 w = weights.rgb / max(dot(weights.rgb, 1.0), 1e-4);
    float y = dot(c.rgb, w);
    return float4(y * tint.rgb, c.a);
}
