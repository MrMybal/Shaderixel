//@name Luminosité / Contraste
//@category Réglages
//@param brightness float 0 -1 1 "Luminosité"
//@param contrast float 0 -1 1 "Contraste"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 s = LinearToSrgb(c.rgb);
    s += brightness;
    float k = contrast >= 0.0 ? 1.0 / max(1.0 - contrast, 0.01) : 1.0 + contrast;
    s = (s - 0.5) * k + 0.5;
    return float4(SrgbToLinear(max(s, 0.0)), c.a);
}
