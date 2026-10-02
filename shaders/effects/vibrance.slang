//@name Vibrance
//@category Réglages
//@param vibrance float 0 -1 1 "Vibrance"
//@param saturation float 0 -1 1 "Saturation"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float sat = mx > 0.0 ? (mx - mn) / mx : 0.0;
    float y = Luma(c.rgb);
    float amount = 1.0 + saturation + vibrance * (1.0 - sat);
    return float4(max(lerp(y.xxx, c.rgb, amount), 0.0), c.a);
}
