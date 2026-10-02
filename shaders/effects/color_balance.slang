//@name Balance des couleurs
//@category Réglages
//@param lift color 0 0 0 1 "Tons foncés (lift)"
//@param gammaColor color 1 1 1 1 "Tons moyens (gamma)"
//@param gain color 1 1 1 1 "Tons clairs (gain)"
//@param saturation float 1 0 2 "Saturation"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 v = c.rgb * gain.rgb + lift.rgb * (1.0 - saturate(c.rgb));
    v = pow(max(v, 0.0), 1.0 / max(gammaColor.rgb, 0.01));
    float y = Luma(v);
    v = lerp(y.xxx, v, saturation);
    return float4(max(v, 0.0), c.a);
}
