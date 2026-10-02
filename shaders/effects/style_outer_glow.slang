//@name Lueur externe
//@category Styles de calque
//@halo size 1.5 4
//@passes 2
//@premultiplied
//@param color color 1 0.9 0.45 1 "Couleur"
//@param opacity float 0.8 0 1 "Opacité"
//@param size float 20 0 250 "Taille (px)"
//@param spread float 0.2 0 1 "Grossi"
float Blur1(EffectContext ctx, int2 dir)
{
    float sigma = max(size * 0.5, 0.01);
    int n = int(ceil(sigma * 3.0));
    int stride = max(1, n / 64);
    float s = 0, w = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float k = exp(-0.5 * (i * i) / (sigma * sigma));
        float a = ctx.pass == 0 ? Input(0, ctx.px + dir * i).a : Prev(ctx.px + dir * i).a;
        s += a * k;
        w += k;
    }
    return s / w;
}
float4 Effect(EffectContext ctx)
{
    if (ctx.pass == 0) return float4(0, 0, 0, Blur1(ctx, int2(1, 0)));
    float g = saturate(Blur1(ctx, int2(0, 1)) * (1.0 + spread * 8.0));
    float4 glow = float4(color.rgb * color.a, color.a) * g * opacity;
    float4 o = Input(0, ctx.px);
    return o + glow * (1.0 - o.a);
}
