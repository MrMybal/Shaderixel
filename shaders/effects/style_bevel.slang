//@name Biseautage et estampage
//@category Styles de calque
//@halo size 1.5 4
//@premultiplied
//@param depth float 3 0 20 "Profondeur"
//@param size float 6 1 40 "Taille (px)"
//@param angle float 135 -180 180 "Angle de la lumière"
//@param highlight color 1 1 1 1 "Lumières"
//@param shadow color 0 0 0 1 "Ombres"
//@param opacity float 0.75 0 1 "Opacité"
float H(EffectContext ctx, float2 o)
{
    // Height = blurred alpha (cheap 3x3 at the bevel size).
    float s = 0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            s += InputUV(0, ctx.uv + (o + float2(x, y) * size * 0.5) * ctx.texel).a;
    return s / 9.0;
}
float4 Effect(EffectContext ctx)
{
    float4 o = Input(0, ctx.px);
    if (o.a <= 0.0) return o;
    float e = max(size * 0.5, 1.0);
    float2 grad = float2(H(ctx, float2(e, 0)) - H(ctx, float2(-e, 0)), H(ctx, float2(0, e)) - H(ctx, float2(0, -e)));
    float3 n = normalize(float3(-grad * depth, 1.0));
    float a = radians(angle);
    float3 l = normalize(float3(cos(a), -sin(a), 0.8));
    float lit = dot(n, l) - l.z;  // 0 on flat areas
    float4 c = lit > 0 ? float4(highlight.rgb, 1) : float4(shadow.rgb, 1);
    float k = saturate(abs(lit) * 2.0) * opacity * o.a;
    o.rgb = lerp(o.rgb, c.rgb * o.a, k);
    return o;
}
