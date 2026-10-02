//@name Dégradé
//@category Générateurs
//@generator
//@param colorA color 0 0 0 1 "Couleur A"
//@param colorB color 1 1 1 1 "Couleur B"
//@param shape enum 0 "Linéaire|Radial|Angle|Réfléchi|Losange" "Style"
//@param angle float 90 -180 180 "Angle"
//@param scale float 1 0.05 4 "Échelle"
//@param center float2 0.5 0.5 "Centre"
float4 Effect(EffectContext ctx)
{
    float aspect = ctx.size.x / ctx.size.y;
    float2 p = (ctx.uv - center) * float2(aspect, 1.0) / scale;
    float a = radians(angle);
    float2 dir = float2(cos(a), -sin(a));
    float t;
    if (shape == 1) t = length(p) * 2.0;
    else if (shape == 2) t = frac(atan2(p.y, p.x) / 6.2831853 + 1.0);
    else if (shape == 3) t = abs(dot(p, dir)) * 2.0;
    else if (shape == 4) t = (abs(dot(p, dir)) + abs(dot(p, float2(-dir.y, dir.x)))) * 2.0;
    else t = dot(p, dir) + 0.5;
    t = saturate(t);
    float4 c = lerp(Premultiply(colorA), Premultiply(colorB), t);
    return Unpremultiply(c);
}
