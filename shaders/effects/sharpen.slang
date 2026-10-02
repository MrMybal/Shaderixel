//@name Netteté (masque flou)
//@category Netteté
//@premultiplied
//@param amount float 1 0 5 "Gain"
//@param radius float 1.5 0.5 10 "Rayon (px)"
//@param threshold float 0 0 0.5 "Seuil"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float4 blur = 0;
    float wsum = 0;
    int n = int(ceil(radius * 2.0));
    for (int y = -n; y <= n; ++y)
        for (int x = -n; x <= n; ++x)
        {
            float w = exp(-0.5 * (x * x + y * y) / (radius * radius));
            blur += Input(0, ctx.px + int2(x, y)) * w;
            wsum += w;
        }
    blur /= wsum;
    float4 detail = c - blur;
    float mask = step(threshold, abs(Luma(detail.rgb)));
    return float4(max(c.rgb + detail.rgb * amount * mask, 0.0), c.a);
}
