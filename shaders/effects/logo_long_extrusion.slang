//@name Logo Long Extrusion
//@category Text and Logo Styles
//@premultiplied
//@halo distance 1 2
//@param distance int 24 0 128 "Extrusion Length (px)"
//@param angle float 45 -180 180 "Extrusion Angle (degrees)"
//@param color color 0.08 0.01 0.04 1 "Extrusion Color"
//@param fade float 0 0 1 "Fade Along Extrusion"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int n = clamp(distance, 0, 128);
    float a = radians(angle), coverage = 0.0;
    float2 dir = float2(cos(a), sin(a));
    for (int i = 1; i <= n; ++i)
    {
        int2 p = ctx.px - int2(round(dir * i));
        float v = any(p < 0) || any(p >= int2(ctx.size)) ? 0.0 : Input(0, p).a;
        coverage = max(coverage, v * (1.0 - saturate(fade) * float(i) / max(float(n), 1.0)));
    }
    float alpha = saturate(coverage * color.a);
    return c + float4(color.rgb * alpha, alpha) * (1.0 - c.a);
}
