//@name Mask Feather and Levels
//@category Masks and Compositing
//@input Mask
//@halo radius 3 2
//@passes 3
//@param radius float 1 0 16 "Feather Radius (px)"
//@param black float 0 0 1 "Black Point"
//@param white float 1 0 1 "White Point"
//@param gamma float 1 0.1 4 "Coverage Curve"
//@param invert bool 0 "Invert Mask"
float4 Effect(EffectContext ctx)
{
    if (ctx.pass < 2)
    {
        float sigma = clamp(radius, 0.0, 16.0);
        if (sigma < 0.01) return float4(saturate(Prev(ctx.px).r).xxx, 1.0);
        int r = (int)ceil(sigma * 3.0);
        int2 axis = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
        float sum = 0.0, weights = 0.0;
        for (int i = -r; i <= r; ++i)
        {
            int2 p = ctx.px + axis * i;
            float v = any(p < 0) || any(p >= int2(ctx.size)) ? 0.0 : saturate(Prev(p).r);
            float w = exp(-0.5 * float(i * i) / (sigma * sigma));
            sum += v * w;
            weights += w;
        }
        return float4((sum / weights).xxx, 1.0);
    }
    float span = white - black;
    span = abs(span) < 0.0001 ? (span < 0.0 ? -0.0001 : 0.0001) : span;
    float m = pow(saturate((Prev(ctx.px).r - black) / span), max(gamma, 0.1));
    m = invert != 0 ? 1.0 - m : m;
    return float4(m.xxx, 1.0);
}
