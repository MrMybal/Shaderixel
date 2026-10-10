//@name Masked Grade Mix
//@category Photo Essentials
//@blend
//@input Original
//@input Processed
//@input Mask
//@param amount float 1 0 1 "Effect Strength"
// Crossfade in premultiplied linear light; this is a grade mix, not source-over.
float4 Effect(EffectContext ctx)
{
    float4 a = Input(0, ctx.px);
    if (!HasInput(1)) return a;
    float4 b = Input(1, ctx.px);
    float mask = HasInput(2) ? saturate(Input(2, ctx.px).r * Input(2, ctx.px).a) : 1.0;
    return Unpremultiply(lerp(Premultiply(a), Premultiply(b), saturate(amount * mask)));
}
