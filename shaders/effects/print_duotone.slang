//@name Print Duotone
//@category Print and Halftone
//@param ink color 0.02 0.04 0.12 1 "Dark Ink (RGB)"
//@param paper color 1 0.86 0.62 1 "Light Ink (RGB)"
//@param black float 0 0 1 "Black Point"
//@param white float 1 0 1 "White Point"
//@param gamma float 1 0.1 4 "Tone Curve"
//@param amount float 1 0 1 "Strength"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = Luma(LinearToSrgb(max(c.rgb, 0.0)));
    float span = white - black;
    span = abs(span) < 0.0001 ? (span < 0.0 ? -0.0001 : 0.0001) : span;
    float t = pow(saturate((y - black) / span), max(gamma, 0.1));
    return float4(lerp(c.rgb, lerp(ink.rgb, paper.rgb, t), saturate(amount)), c.a);
}
