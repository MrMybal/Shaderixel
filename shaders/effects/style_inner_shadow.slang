//@name Ombre interne
//@category Styles de calque
//@passes 2
//@premultiplied
//@param color color 0 0 0 1 "Couleur"
//@param opacity float 0.75 0 1 "Opacité"
//@param angle float 135 -180 180 "Angle de la lumière"
//@param dist float 8 0 500 "Distance (px)"
//@param size float 10 0 250 "Taille (px)"
float BlurInv(EffectContext ctx, int2 dir, int2 offset)
{
    float sigma = max(size * 0.5, 0.01);
    int n = int(ceil(sigma * 3.0));
    int stride = max(1, n / 64);
    float s = 0, w = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float k = exp(-0.5 * (i * i) / (sigma * sigma));
        float a = ctx.pass == 0 ? 1.0 - Input(0, ctx.px - offset + dir * i).a : Prev(ctx.px + dir * i).a;
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
        return float4(0, 0, 0, BlurInv(ctx, int2(1, 0), offset));
    float4 o = Input(0, ctx.px);
    float k = saturate(BlurInv(ctx, int2(0, 1), int2(0, 0))) * opacity * color.a;
    o.rgb = lerp(o.rgb, color.rgb * o.a, k);
    return o;
}
