//@name Edge-Preserving Texture Denoise
//@category Texture Processing
//@halo rayon 1 2
//@param rayon int 2 1 5 "Radius (px)"
//@param seuil float 0.08 0.001 1 "Color Tolerance"
//@param force float 0.7 0 1 "Denoise Strength"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float4 sum = 0.0;
    float weights = 0.0;
    int r = max(1, (int)round(rayon));
    float sigma = max(seuil, 0.001);
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
        {
            float4 tap = Input(0, ctx.px + int2(x, y));
            float3 delta = LinearToSrgb(max(tap.rgb, 0.0)) - LinearToSrgb(max(c.rgb, 0.0));
            float w = exp(-dot(delta, delta) / (2.0 * sigma * sigma)
                          - float(x * x + y * y) / (2.0 * r * r)) * saturate(tap.a);
            sum += Premultiply(tap) * w;
            weights += w;
        }
    float4 filtered = weights > 0.00001 ? Unpremultiply(sum / weights) : c;
    return float4(lerp(c.rgb, filtered.rgb, saturate(force)), c.a);
}
