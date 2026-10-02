//@name Niveaux
//@category Réglages
//@param inBlack float 0 0 1 "Noir en entrée"
//@param inWhite float 1 0 1 "Blanc en entrée"
//@param midGamma float 1 0.1 10 "Gamma"
//@param outBlack float 0 0 1 "Noir en sortie"
//@param outWhite float 1 0 1 "Blanc en sortie"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 s = LinearToSrgb(c.rgb);
    s = saturate((s - inBlack) / max(inWhite - inBlack, 1e-4));
    s = pow(s, 1.0 / midGamma);
    s = lerp(outBlack, outWhite, s);
    return float4(SrgbToLinear(s), c.a);
}
