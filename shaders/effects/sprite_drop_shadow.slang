//@name Sprite Drop Shadow
//@category 2D Game Art
//@premultiplied
//@halo full
//@param offset float2 4 4 -128 128 "Shadow Offset (px)"
//@param softness int 0 0 4 "Shadow Softness (px)"
//@param color color 0 0 0 0.5 "Shadow Color"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int2 source = ctx.px - int2(round(offset));
    int r = clamp(softness, 0, 4);
    float sum = 0.0;
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
        {
            int2 p = source + int2(x, y);
            sum += any(p < 0) || any(p >= int2(ctx.size)) ? 0.0 : Input(0, p).a;
        }
    float a = saturate(sum / float((2 * r + 1) * (2 * r + 1)) * color.a);
    return c + float4(color.rgb * a, a) * (1.0 - c.a);
}
