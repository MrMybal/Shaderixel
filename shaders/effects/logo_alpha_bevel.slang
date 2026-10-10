//@name Logo Alpha Bevel
//@category Text and Logo Styles
//@halo width 1 1
//@param width int 2 1 16 "Bevel Width (px)"
//@param depth float 3 0 12 "Bevel Depth"
//@param angle float 135 -180 180 "Light Angle (degrees)"
//@param highlight color 1 1 1 1 "Highlight Color"
//@param shadow color 0 0 0 1 "Shadow Color"
//@param amount float 0.7 0 1 "Bevel Strength"
float LogoAlpha(int2 p, float2 size)
{
    return any(p < 0) || any(p >= int2(size)) ? 0.0 : Input(0, p).a;
}
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int r = clamp(width, 1, 16);
    float2 grad = float2(LogoAlpha(ctx.px + int2(r, 0), ctx.size) - LogoAlpha(ctx.px - int2(r, 0), ctx.size),
                        LogoAlpha(ctx.px + int2(0, r), ctx.size) - LogoAlpha(ctx.px - int2(0, r), ctx.size));
    float3 n = normalize(float3(-grad * depth, 1.0));
    float a = radians(angle);
    float3 light = normalize(float3(cos(a), sin(a), 0.8));
    float lit = dot(n, light) - light.z;
    float4 tint = lit > 0.0 ? highlight : shadow;
    return float4(lerp(c.rgb, tint.rgb, saturate(abs(lit) * 2.0 * amount * tint.a)), c.a);
}
