//@name Incrustation en dégradé
//@category Styles de calque
//@premultiplied
//@param colorA color 1 0.3 0.1 1 "Couleur A"
//@param colorB color 0.2 0.6 1 1 "Couleur B"
//@param angle float 90 -180 180 "Angle"
//@param opacity float 1 0 1 "Opacité"
float4 Effect(EffectContext ctx)
{
    float4 o = Input(0, ctx.px);
    float a = radians(angle);
    float2 dir = float2(cos(a), -sin(a));
    float t = saturate(dot(ctx.uv - 0.5, dir) + 0.5);
    float3 g = lerp(colorA.rgb, colorB.rgb, t);
    return lerp(o, float4(g * o.a, o.a), opacity);
}
