//@name Ondulation
//@category Déformation
//@halo amplitude 1 2
//@premultiplied
//@param amplitude float 10 0 200 "Amplitude (px)"
//@param wavelength float 80 2 2000 "Longueur d'onde (px)"
//@param angle float 0 -180 180 "Angle"
float4 Effect(EffectContext ctx)
{
    float a = radians(angle);
    float2 dir = float2(cos(a), sin(a));
    float2 perp = float2(-dir.y, dir.x);
    float phase = dot(float2(ctx.px), dir) / wavelength * 6.2831853;
    float2 offset = perp * sin(phase) * amplitude;
    return InputUV(0, ctx.uv + offset * ctx.texel);
}
