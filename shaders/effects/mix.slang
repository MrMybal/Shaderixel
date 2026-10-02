//@name Mélange A/B
//@category Composition
//@premultiplied
//@input A
//@input B
//@input Masque
//@param factor float 0.5 0 1 "Facteur"
float4 Effect(EffectContext ctx)
{
    float t = factor;
    if (HasInput(2)) t *= Luma(Unpremultiply(Input(2, ctx.px)).rgb);
    return lerp(Input(0, ctx.px), Input(1, ctx.px), t);
}
