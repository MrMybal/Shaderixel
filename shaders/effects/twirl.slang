//@name Tourbillon
//@category Déformation
//@premultiplied
//@param angle float 180 -999 999 "Angle"
//@param radius float 0.5 0.01 1.5 "Rayon"
//@param center float2 0.5 0.5 "Centre"
float4 Effect(EffectContext ctx)
{
    float aspect = ctx.size.x / ctx.size.y;
    float2 d = (ctx.uv - center) * float2(aspect, 1.0);
    float r = length(d);
    float t = saturate(1.0 - r / radius);
    float a = radians(angle) * t * t;
    float s = sin(a), c = cos(a);
    d = float2(c * d.x - s * d.y, s * d.x + c * d.y);
    return InputUV(0, center + d / float2(aspect, 1.0));
}
