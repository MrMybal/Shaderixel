//@name Hautes fréquences (séparation)
//@category Flou
//@halo radius 3 2
//@passes 2
//@premultiplied
//@param radius float 4 0.3 250 "Rayon (px)"
//@param gamma bool 1 "Calcul en sRGB (fusions gamma)"
// Frequency separation, high layer: (image - gaussian blur) / 2 + 0.5, the same blur as the Gaussian Blur effect,
// so that this layer in Linear Light over the blurred copy rebuilds the image exactly.
float4 Effect(EffectContext ctx)
{
    float sigma = max(radius, 0.01);
    int n = int(ceil(sigma * 3.0));
    int stride = max(1, n / 96);
    int2 dir = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
    float4 sum = 0;
    float wsum = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float w = exp(-0.5 * (i * i) / (sigma * sigma));
        sum += Prev(ctx.px + dir * i) * w;
        wsum += w;
    }
    const float4 blurred = sum / wsum;
    if (ctx.pass == 0) return blurred;
    // The same premultiplied blur as the low layer's Gaussian Blur, compared in straight colors.
    const float4 src = Input(0, ctx.px);
    const float3 s = Unpremultiply(src).rgb, b = Unpremultiply(blurred).rgb;
    // In the space the layers blend in (the document's gamma blending: sRGB), so Linear Light rebuilds the image.
    const float3 h = gamma ? SrgbToLinear((LinearToSrgb(saturate(s)) - LinearToSrgb(saturate(b))) * 0.5 + 0.5) : (s - b) * 0.5 + 0.5;
    return float4(h * src.a, src.a);
}
