//@name Mask Expand and Contract
//@category Masks and Compositing
//@input Mask
//@halo radius 1 1
//@passes 2
//@param radius int 1 0 16 "Radius (px)"
//@param operation enum 0 "Expand|Contract" "Operation"
// Separable square morphology. Coverage outside the document is zero.
float4 Effect(EffectContext ctx)
{
    int r = clamp(radius, 0, 16);
    int2 axis = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
    float m = operation == 0 ? 0.0 : 1.0;
    for (int i = -r; i <= r; ++i)
    {
        int2 p = ctx.px + axis * i;
        float v = any(p < 0) || any(p >= int2(ctx.size)) ? 0.0 : saturate(Prev(p).r);
        m = operation == 0 ? max(m, v) : min(m, v);
    }
    return float4(m.xxx, 1.0);
}
