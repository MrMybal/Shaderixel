//@name Inclinaison-décalage
//@category Flou
//@halo maxBlur 1 2
//@premultiplied
//@param center float2 0.5 0.55 0 1 "Centre de la bande nette"
//@param angle float 0 -90 90 "Angle (°)"
//@param focus float 0.08 0 0.5 "Demi-largeur nette"
//@param transition float 0.25 0.01 1 "Transition"
//@param maxBlur float 16 0 96 "Flou (px)"
//@param symmetric bool 1 "Flou des deux côtés"
//@param highlights float 0.4 0 4 "Éclat du bokeh"
//@param threshold float 0.8 0 1 "Seuil des hautes lumières"
//@param saturation float 0.15 0 1 "Saturation (effet maquette)"
//@param samples int 48 8 256 "Qualité (échantillons)"
// Photoshop's Tilt-Shift: a sharp band across the image, blurring away from it (miniature look).

float4 Effect(EffectContext ctx)
{
    const float2 sizePx = 1.0 / ctx.texel;
    const float rad = angle * 0.0174532925;
    // Signed distance across the band, in image heights.
    const float2 d = (ctx.uv - center) * sizePx / sizePx.y;
    const float across = -sin(rad) * d.x + cos(rad) * d.y;
    float t = (abs(across) - focus) / transition;
    if (!symmetric && across < 0) t = 0;
    const float r = maxBlur * saturate(t);
    float4 c0 = Input(0, ctx.px);
    float4 result = c0;
    if (r >= 0.5)
    {
        const int n = max(samples, 8);
        float4 sum = 0;
        float wsum = 0;
        for (int i = 0; i < n; ++i)
        {
            const float rr = sqrt((i + 0.5) / n) * r;
            const float a = i * 2.39996323;
            const float4 c = InputUV(0, ctx.uv + float2(cos(a), sin(a)) * rr * ctx.texel);
            const float w = 1.0 + highlights * 8.0 * max(Luma(Unpremultiply(c).rgb) - threshold, 0.0);
            sum += c * w;
            wsum += w;
        }
        result = sum / wsum;
    }
    if (saturation > 0)
    {
        // A little extra color, like the toy-like photos the effect imitates.
        float4 u = Unpremultiply(result);
        const float g = Luma(u.rgb);
        u.rgb = max(lerp(float3(g, g, g), u.rgb, 1.0 + saturation), 0.0);
        result = float4(u.rgb * u.a, u.a);
    }
    return result;
}
