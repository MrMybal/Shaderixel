//@name Contours (Sobel)
//@category Esthétiques
//@param strength float 1 0 10 "Intensité"
//@param invert bool 0 "Inverser"
float4 Effect(EffectContext ctx)
{
    float l[9];
    int k = 0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            l[k++] = Luma(Input(0, ctx.px + int2(x, y)).rgb);
    float gx = -l[0] - 2 * l[3] - l[6] + l[2] + 2 * l[5] + l[8];
    float gy = -l[0] - 2 * l[1] - l[2] + l[6] + 2 * l[7] + l[8];
    float e = saturate(length(float2(gx, gy)) * strength);
    if (invert != 0) e = 1.0 - e;
    return float4(e.xxx, Input(0, ctx.px).a);
}
