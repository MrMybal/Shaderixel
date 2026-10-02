//@name Contour
//@category Styles de calque
//@premultiplied
//@param color color 1 1 1 1 "Couleur"
//@param width float 4 0.5 64 "Taille (px)"
//@param position enum 0 "Extérieur|Intérieur|Centre" "Position"
//@param opacity float 1 0 1 "Opacité"
float4 Effect(EffectContext ctx)
{
    float4 o = Input(0, ctx.px);
    // Disc search of the max (outside) and min (inside) alpha around the pixel.
    float mx = o.a, mn = o.a;
    const int A = 24;
    int R = max(1, int(ceil(width / 4.0)));
    for (int j = 1; j <= 4; ++j)
    {
        float r = width * j / 4.0;
        for (int i = 0; i < A; ++i)
        {
            float ang = 6.2831853 * (i + 0.5 * (j & 1)) / A;
            float sa = InputUV(0, ctx.uv + float2(cos(ang), sin(ang)) * r * ctx.texel).a;
            mx = max(mx, sa);
            mn = min(mn, sa);
        }
    }
    float s;
    if (position == 1) s = o.a * (1.0 - mn);          // inside edge band
    else if (position == 2) s = max(mx * (1.0 - o.a), o.a * (1.0 - mn));
    else s = mx;                                       // outside: under the content
    float4 st = float4(color.rgb * color.a, color.a) * saturate(s) * opacity;
    if (position == 0) return o + st * (1.0 - o.a);
    return st + o * (1.0 - st.a);
}
