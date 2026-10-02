//@name Mélangeur de couches
//@category Réglages
//@param red color 1 0 0 1 "Rouge en sortie (R,V,B)"
//@param green color 0 1 0 1 "Vert en sortie (R,V,B)"
//@param blue color 0 0 1 1 "Bleu en sortie (R,V,B)"
//@param monochrome bool 0 "Monochrome"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 v = float3(dot(c.rgb, red.rgb), dot(c.rgb, green.rgb), dot(c.rgb, blue.rgb));
    if (monochrome != 0) v = v.rrr;
    return float4(max(v, 0.0), c.a);
}
