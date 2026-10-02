//@name Courbes
//@category Réglages
//@param master curve "RVB"
//@param red curve "Rouge"
//@param green curve "Vert"
//@param blue curve "Bleu"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 s = LinearToSrgb(saturate(c.rgb));
    s = float3(red_eval(s.r), green_eval(s.g), blue_eval(s.b));
    s = float3(master_eval(s.r), master_eval(s.g), master_eval(s.b));
    return float4(SrgbToLinear(s), c.a);
}
