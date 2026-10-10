//@name Sprite Hit Flash
//@category 2D Game Art
//@param color color 1 1 1 1 "Flash Color"
//@param amount float 1 0 1 "Flash Strength"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    return float4(lerp(c.rgb, color.rgb, saturate(amount * color.a)), c.a);
}
