//@name Logo Gradient Fill
//@category Text and Logo Styles
//@param top color 1 0.4 0.02 1 "Start Color (RGB)"
//@param bottom color 0.5 0.02 0.1 1 "End Color (RGB)"
//@param angle float 90 -180 180 "Gradient Angle (degrees)"
//@param center float 0.5 -1 2 "Gradient Center"
//@param span float 1 0.01 3 "Gradient Span"
//@param amount float 1 0 1 "Fill Strength"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float a = radians(angle);
    float t = saturate((dot(ctx.uv - 0.5, float2(cos(a), sin(a))) + 0.5 - center) / max(span, 0.01) + 0.5);
    return float4(lerp(c.rgb, lerp(top.rgb, bottom.rgb, t), saturate(amount)), c.a);
}
