//@name Sprite Pixel Grid
//@category 2D Game Art
//@halo block_size 1 1
//@param block_size int 4 1 64 "Pixel Block Size"
//@param hard_alpha bool 1 "Hard Alpha"
//@param alpha_threshold float 0.5 0 1 "Alpha Threshold"
float4 Effect(EffectContext ctx)
{
    int block = clamp(block_size, 1, 64);
    int2 samplePx = (ctx.px / block) * block + int2(block / 2);
    float4 c = Input(0, samplePx);
    if (hard_alpha != 0) c.a = c.a > 0.0 && c.a >= alpha_threshold ? 1.0 : 0.0;
    return c;
}
