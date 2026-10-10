//@name Logo Soft Glow
//@category Text and Logo Styles
//@premultiplied
//@passes 2
//@halo radius 3 2
//@param radius float 8 0 64 "Glow Radius (px)"
//@param color color 0.02 0.6 1 1 "Glow Color"
//@param strength float 1 0 4 "Glow Strength"
float4 Effect(EffectContext ctx)
{
    float sigma = clamp(radius, 0.0, 64.0);
    if (sigma < 0.01) return ctx.pass == 0 ? float4(0, 0, 0, 0) : Input(0, ctx.px);
    int n = (int)ceil(sigma * 3.0);
    int2 axis = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
    float sum = 0.0, weights = 0.0;
    for (int i = -n; i <= n; ++i)
    {
        int2 p = ctx.px + axis * i;
        float v = any(p < 0) || any(p >= int2(ctx.size)) ? 0.0 : Prev(p).a;
        float w = exp(-0.5 * float(i * i) / (sigma * sigma));
        sum += v * w;
        weights += w;
    }
    float m = sum / weights;
    if (ctx.pass == 0) return float4(0, 0, 0, m);
    float4 c = Input(0, ctx.px);
    float alpha = saturate(m * color.a * strength);
    return c + float4(color.rgb * alpha, alpha) * (1.0 - c.a);
}
