//@name Sprite Silhouette
//@category 2D Game Art
//@param color color 1 1 1 1 "Silhouette Color"
//@param hard_alpha bool 0 "Hard Alpha"
//@param alpha_threshold float 0.5 0 1 "Alpha Threshold"
float4 Effect(EffectContext ctx)
{
    float a = Input(0, ctx.px).a;
    if (hard_alpha != 0) a = a > 0.0 && a >= alpha_threshold ? 1.0 : 0.0;
    return float4(color.rgb, a * color.a);
}
