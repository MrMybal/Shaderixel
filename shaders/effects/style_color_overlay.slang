//@name Incrustation couleur
//@category Styles de calque
//@premultiplied
//@param color color 1 0.2 0.3 1 "Couleur"
//@param opacity float 1 0 1 "Opacité"
float4 Effect(EffectContext ctx)
{
    float4 o = Input(0, ctx.px);
    float4 c = float4(color.rgb * o.a, o.a);
    return lerp(o, c, opacity * color.a);
}
