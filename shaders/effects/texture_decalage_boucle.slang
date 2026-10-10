//@name Wrapped Texture Offset
//@category Texture Processing
//@halo full
//@premultiplied
//@param decalage float2 0.5 0.5 -1 1 "UV Offset"
float4 Effect(EffectContext ctx)
{
    // Integer pixel offset avoids incorrect interpolation across the seam.
    int2 size = max(int2(ctx.size), int2(1, 1));
    int2 p = ctx.px + int2(round(decalage * ctx.size));
    p = ((p % size) + size) % size;
    return Input(0, p);
}
