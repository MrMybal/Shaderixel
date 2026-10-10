//@name Tileable Procedural Microrelief
//@category Texture Generators
//@generator
//@param cellules int 16 2 128 "Cells per Side"
//@param graine int 0 0 9999 "Seed"
//@param octaves int 4 1 6 "Detail Octaves"
//@param amplitude float 0.2 0 0.5 "Amplitude"
//@param centre float 0.5 0 1 "Midpoint Height"
float PeriodicNoise(float2 uv, float cells)
{
    float2 p = uv * cells;
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float2 a = i - floor(i / cells) * cells;
    float2 b = i + 1.0 - floor((i + 1.0) / cells) * cells;
    float2 seed = float2(graine * 0.137, graine * 0.719);
    float n00 = Hash12(a + seed);
    float n10 = Hash12(float2(b.x, a.y) + seed);
    float n01 = Hash12(float2(a.x, b.y) + seed);
    float n11 = Hash12(b + seed);
    return lerp(lerp(n00, n10, f.x), lerp(n01, n11, f.x), f.y);
}
float4 Effect(EffectContext ctx)
{
    float sum = 0.0, weights = 0.0, w = 1.0;
    float cells = max(2.0, round(cellules));
    for (int i = 0; i < clamp((int)round(octaves), 1, 6); ++i)
    {
        sum += PeriodicNoise(ctx.uv, cells) * w;
        weights += w;
        cells *= 2.0;
        w *= 0.5;
    }
    float h = saturate(centre + (sum / weights - 0.5) * 2.0 * amplitude);
    return float4(SrgbToLinear(h.xxx), 1.0);
}
