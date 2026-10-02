//@name Damier
//@category Générateurs
//@generator
//@param cells float 8 1 256 "Cases"
//@param colorA color 0.05 0.05 0.05 1 "Couleur A"
//@param colorB color 0.9 0.9 0.9 1 "Couleur B"
float4 Effect(EffectContext ctx)
{
    float2 p = floor(ctx.uv * float2(cells * ctx.size.x / ctx.size.y, cells));
    return (int(p.x + p.y) & 1) ? colorB : colorA;
}
