//@name Flou directionnel
//@category Flou
//@halo dist 0.5 2
//@premultiplied
//@param angle float 0 -180 180 "Angle"
//@param dist float 20 0 500 "Distance (px)"
float4 Effect(EffectContext ctx)
{
    float a = radians(angle);
    float2 dir = float2(cos(a), sin(a)) * dist * ctx.texel;
    int n = clamp(int(dist), 1, 128);
    float4 sum = 0;
    for (int i = 0; i <= n; ++i)
        sum += InputUV(0, ctx.uv + dir * ((float)i / n - 0.5));
    return sum / (n + 1);
}
