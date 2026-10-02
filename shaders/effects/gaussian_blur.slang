//@name Flou gaussien
//@category Flou
//@halo radius 3 2
//@passes 2
//@premultiplied
//@param radius float 4 0 250 "Rayon (px)"
float4 Effect(EffectContext ctx)
{
    float sigma = max(radius, 0.01);
    if (radius < 0.3) return Prev(ctx.px);
    int n = int(ceil(sigma * 3.0));
    int stride = max(1, n / 96);
    int2 dir = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
    float4 sum = 0;
    float wsum = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float w = exp(-0.5 * (i * i) / (sigma * sigma));
        sum += Prev(ctx.px + dir * i) * w;
        wsum += w;
    }
    return sum / wsum;
}
