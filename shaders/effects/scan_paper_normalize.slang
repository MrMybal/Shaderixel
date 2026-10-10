//@name Scan Paper Normalize
//@category Scan and Line Art
//@input Scan
//@input Blurred Paper Reference
//@param amount float 1 0 1 "Paper Correction"
//@param black float 0 0 0.9 "Black Point"
//@param white float 1 0.1 1 "White Point"
//@param gamma float 1 0.1 4 "Tone Curve"
//@param monochrome bool 1 "Monochrome"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 rgb = saturate(LinearToSrgb(max(c.rgb, 0.0)));
    float4 ref = HasInput(1) ? Input(1, ctx.px) : float4(1, 1, 1, 1);
    float bg = Luma(lerp(float3(1, 1, 1), saturate(LinearToSrgb(max(ref.rgb, 0.0))), saturate(ref.a)));
    rgb = lerp(rgb, saturate(rgb / max(bg, 0.05)), saturate(amount));
    if (monochrome != 0) rgb = Luma(rgb).xxx;
    float span = max(white - black, 0.0001);
    rgb = pow(saturate((rgb - black) / span), max(gamma, 0.1));
    return float4(SrgbToLinear(rgb), c.a);
}
