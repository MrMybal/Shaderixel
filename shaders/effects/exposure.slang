//@name Exposition
//@category Réglages
//@param exposure float 0 -5 5 "Exposition (IL)"
//@param offset float 0 -0.5 0.5 "Décalage"
//@param gammaCorrection float 1 0.1 3 "Correction gamma"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 v = max(c.rgb * exp2(exposure) + offset, 0.0);
    return float4(pow(v, 1.0 / gammaCorrection), c.a);
}
