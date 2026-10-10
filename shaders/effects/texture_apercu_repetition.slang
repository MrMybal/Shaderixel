//@name Tiled Texture Preview
//@category Texture Processing
//@halo full
//@premultiplied
//@param repetitions int 3 1 12 "Repeat Count"
float4 Effect(EffectContext ctx)
{
    return InputUV(0, frac(ctx.uv * max(1.0, round(repetitions))));
}
