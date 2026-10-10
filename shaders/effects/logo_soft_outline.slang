//@name Logo Soft Outline
//@category Text and Logo Styles
//@premultiplied
//@passes 2
//@halo width 1 1
//@param width int 3 0 32 "Outline Width (px)"
//@param color color 1 1 1 1 "Outline Color"
// Square dilation preserves soft alpha rather than binarizing the edge.
float4 Effect(EffectContext ctx)
{
    int r = clamp(width, 0, 32);
    int2 axis = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
    float m = 0.0;
    for (int i = -r; i <= r; ++i)
    {
        int2 p = ctx.px + axis * i;
        float v = any(p < 0) || any(p >= int2(ctx.size)) ? 0.0 : Prev(p).a;
        m = max(m, v);
    }
    if (ctx.pass == 0) return float4(0, 0, 0, m);
    float4 c = Input(0, ctx.px);
    if (r == 0) return c;
    float alpha = saturate(m * color.a);
    return c + float4(color.rgb * alpha, alpha) * (1.0 - c.a);
}
