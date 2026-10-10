//@name Sprite Outline
//@category 2D Game Art
//@premultiplied
//@halo width 1 1
//@param width int 2 0 16 "Outline Width (px)"
//@param color color 0.01 0.01 0.01 1 "Outline Color"
//@param shape enum 0 "Square|Round" "Outline Shape"
//@param alpha_threshold float 0.5 0 1 "Alpha Threshold"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int r = clamp(width, 0, 16);
    if (r == 0) return c;
    float coverage = 0.0;
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
        {
            if (shape != 0 && x * x + y * y > r * r) continue;
            int2 p = ctx.px + int2(x, y);
            float a = any(p < 0) || any(p >= int2(ctx.size)) ? 0.0 : Input(0, p).a;
            coverage = max(coverage, a > 0.0 && a >= alpha_threshold ? 1.0 : 0.0);
        }
    float alpha = saturate(coverage * color.a);
    float4 line = float4(color.rgb * alpha, alpha);
    return c + line * (1.0 - c.a);
}
