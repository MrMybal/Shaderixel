//@name Ombre portée
//@category Styles de calque
//@halo size 1.5 dist 1 4
//@passes 2
//@premultiplied
//@param color color 0 0 0 1 "Couleur"
//@param opacity float 0.75 0 1 "Opacité"
//@param angle float 135 -180 180 "Angle de la lumière"
//@param dist float 12 0 500 "Distance (px)"
//@param size float 14 0 250 "Taille (px)"
//@param spread float 0 0 1 "Grossi"
float BlurAlpha(EffectContext ctx, int2 dir, int2 offset)
{
    float sigma = max(size * 0.5, 0.01);
    int n = int(ceil(sigma * 3.0));
    int stride = max(1, n / 64);
    float s = 0, w = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float k = exp(-0.5 * (i * i) / (sigma * sigma));
        float a = ctx.pass == 0 ? Input(0, ctx.px - offset + dir * i).a : Prev(ctx.px + dir * i).a;
        s += a * k;
        w += k;
    }
    return s / w;
}
float4 Effect(EffectContext ctx)
{
    float a = radians(angle);
    int2 offset = int2(round(float2(-cos(a), sin(a)) * dist));
    if (ctx.pass == 0)
        return float4(0, 0, 0, BlurAlpha(ctx, int2(1, 0), offset));
    float sh = saturate(BlurAlpha(ctx, int2(0, 1), int2(0, 0)) * (1.0 + spread * 8.0));
    float4 shadow = float4(color.rgb * color.a, color.a) * sh * opacity;
    float4 o = Input(0, ctx.px);
    return o + shadow * (1.0 - o.a);
}
