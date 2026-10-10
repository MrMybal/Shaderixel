//@name Sprite Transparency Checker
//@category 2D Game Art
//@generator
//@param cell_size int 16 1 128 "Checker Size (px)"
//@param color_a color 0.08 0.08 0.08 1 "Checker Color 1"
//@param color_b color 0.16 0.16 0.16 1 "Checker Color 2"
float4 Effect(EffectContext ctx)
{
    int2 p = ctx.px / clamp(cell_size, 1, 128);
    return ((p.x + p.y) & 1) == 0 ? color_a : color_b;
}
