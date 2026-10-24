//@name Flou de diaphragme
//@category Flou
//@halo maxBlur 1 2
//@premultiplied
//@param center float2 0.5 0.5 0 1 "Centre"
//@param size float2 0.25 0.18 0.01 1 "Zone nette (rayons)"
//@param rotation float 0 -180 180 "Rotation (°)"
//@param transition float 0.6 0.01 3 "Transition"
//@param maxBlur float 18 0 96 "Flou (px)"
//@param highlights float 0.6 0 4 "Éclat du bokeh"
//@param threshold float 0.75 0 1 "Seuil des hautes lumières"
//@param blades int 0 0 9 "Lamelles (0 : disque)"
//@param samples int 64 8 256 "Qualité (échantillons)"
// Photoshop's Iris Blur: sharp inside an ellipse, blurring progressively outside (bokeh with bright highlights).

float2 ApertureOffset(int i, int n, int b, float rot)
{
    float r = sqrt((i + 0.5) / n);
    float a = i * 2.39996323;
    if (b >= 3)
    {
        // Polygonal aperture: the disc sample pushed onto the polygon of b blades.
        const float seg = 6.28318530 / b;
        const float k = fmod(a + rot + 64.0 * seg, seg) - seg * 0.5;
        r *= cos(seg * 0.5) / cos(k);
    }
    return float2(cos(a), sin(a)) * r;
}

float4 Effect(EffectContext ctx)
{
    const float2 sizePx = 1.0 / ctx.texel;
    // Elliptical distance from the center (1 on the edge of the sharp area).
    const float rad = rotation * 0.0174532925;
    const float2 d = (ctx.uv - center) * sizePx;
    const float2 l = float2(cos(rad) * d.x + sin(rad) * d.y, -sin(rad) * d.x + cos(rad) * d.y);
    const float2 axes = max(size * max(sizePx.x, sizePx.y), 1.0);
    const float e = length(l / axes);
    const float r = maxBlur * saturate((e - 1.0) / transition);
    if (r < 0.5) return Input(0, ctx.px);
    const int n = max(samples, 8);
    float4 sum = 0;
    float wsum = 0;
    for (int i = 0; i < n; ++i)
    {
        const float4 c = InputUV(0, ctx.uv + ApertureOffset(i, n, blades, rad) * r * ctx.texel);
        const float w = 1.0 + highlights * 8.0 * max(Luma(Unpremultiply(c).rgb) - threshold, 0.0);  // bright spots bloom into discs
        sum += c * w;
        wsum += w;
    }
    return sum / wsum;
}
