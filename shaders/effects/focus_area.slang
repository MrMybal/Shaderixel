//@name Zone de netteté (masque)
//@category Masques
//@halo full
//@passes 5
//@param radius float 6 1 48 "Rayon d'analyse (px)"
//@param threshold float 0.05 0 0.5 "Seuil de netteté"
//@param softness float 0.02 0.001 0.2 "Transition"
//@param fill float 12 0 96 "Combler les zones unies (px)"
//@param invert bool 0 "Inverser (zones floues)"
// White where the image is in focus: the local energy of the luminance's Laplacian (fine detail), averaged over a
// disc. Blurred backgrounds stay black; flat areas enclosed by sharp edges (a plain wall of a sharp house) are filled
// by a closing (dilate then erode by `fill`, separable). Used by Selection > Focus Area.

float Lum(int2 p) { return LinearToSrgb1(Luma(Input(0, p).rgb)); }  // (straight colors: no //@premultiplied)

float Detail(int2 p)
{
    return abs(4.0 * Lum(p) - Lum(p + int2(1, 0)) - Lum(p - int2(1, 0)) - Lum(p + int2(0, 1)) - Lum(p - int2(0, 1)));
}

// Max (dilate) or min (erode) of the previous pass along a line of radius `fill` (separable: square element).
float Morph(int2 p, int2 dir, bool dilate)
{
    float v = Prev(p).r;
    const int r = int(round(fill));
    for (int i = 1; i <= r; ++i)
    {
        const float a = Prev(p + dir * i).r, b = Prev(p - dir * i).r;
        v = dilate ? max(v, max(a, b)) : min(v, min(a, b));
    }
    return v;
}

float4 Effect(EffectContext ctx)
{
    if (ctx.pass > 0)
    {
        // Passes 1-2: dilate (rows, columns), 3-4: erode.
        float m = fill < 0.5 ? Prev(ctx.px).r : Morph(ctx.px, (ctx.pass & 1) ? int2(1, 0) : int2(0, 1), ctx.pass <= 2);
        if (ctx.pass == 4 && invert) m = 1.0 - m;
        return float4(m, m, m, 1);
    }
    const int n = 40;
    float e = 0;
    for (int i = 0; i < n; ++i)
    {
        const float r = sqrt((i + 0.5) / n) * radius;
        const float a = i * 2.39996323;
        e += Detail(ctx.px + int2(round(float2(cos(a), sin(a)) * r)));
    }
    e /= n;
    const float m = smoothstep(threshold - softness, threshold + softness, e);
    return float4(m, m, m, 1);
}
