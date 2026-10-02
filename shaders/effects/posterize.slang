//@name Isohélie
//@category Réglages
//@param levels int 4 2 64 "Niveaux"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float n = max(float(levels), 2.0);
    float3 s = floor(LinearToSrgb(saturate(c.rgb)) * n) / (n - 1.0);
    return float4(SrgbToLinear(saturate(s)), c.a);
}
