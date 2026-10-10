//@name Apply Coverage Mask
//@category Masks and Compositing
//@input Image
//@input Mask
//@param mode enum 0 "Multiply Alpha|Replace Alpha" "Alpha Mode"
//@param invert bool 0 "Invert Mask"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    if (!HasInput(1)) return c;
    float m = saturate(Input(1, ctx.px).r * Input(1, ctx.px).a);
    m = invert != 0 ? 1.0 - m : m;
    return float4(c.rgb, mode == 0 ? c.a * m : m);
}
