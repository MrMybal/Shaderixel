//@name Flou de l'objectif
//@category Flou
//@premultiplied
//@param radius float 6 0 64 "Rayon (px)"
//@param samples int 48 8 256 "Échantillons"
float4 Effect(EffectContext ctx)
{
    // Golden-angle disc sampling (bokeh-like lens blur).
    float4 sum = 0;
    int n = max(samples, 1);
    for (int i = 0; i < n; ++i)
    {
        float r = sqrt((i + 0.5) / n) * radius;
        float a = i * 2.39996323;
        sum += InputUV(0, ctx.uv + float2(cos(a), sin(a)) * r * ctx.texel);
    }
    return sum / n;
}
