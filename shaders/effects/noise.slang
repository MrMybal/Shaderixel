//@name Bruit fractal
//@category Générateurs
//@generator
//@param scale float 8 0.5 200 "Échelle"
//@param octaves int 5 1 10 "Octaves"
//@param roughness float 0.5 0 1 "Rugosité"
//@param seed float 1 0 1000 "Graine"
//@param colorA color 0 0 0 1 "Couleur A"
//@param colorB color 1 1 1 1 "Couleur B"
float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = Hash12(i + seed * 17.0);
    float b = Hash12(i + float2(1, 0) + seed * 17.0);
    float c = Hash12(i + float2(0, 1) + seed * 17.0);
    float d = Hash12(i + float2(1, 1) + seed * 17.0);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float4 Effect(EffectContext ctx)
{
    float2 p = ctx.uv * float2(ctx.size.x / ctx.size.y, 1.0) * scale;
    float v = 0, amp = 0.5, norm = 0;
    for (int o = 0; o < octaves; ++o)
    {
        v += ValueNoise(p) * amp;
        norm += amp;
        p *= 2.0;
        amp *= roughness;
    }
    return lerp(colorA, colorB, v / max(norm, 1e-4));
}
