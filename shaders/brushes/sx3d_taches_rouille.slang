//@name Taches de rouille
//@category Texture 3D
//@spacing 0.5
//@param Scale float 4 1 12 "Échelle"
//@param Density float 0.5 0.1 0.9 "Densité"

// Brush for texturing models (wear, dirt, details); also usable in 2D.

float SXN(float2 p)
{
    float2 i = floor(p), f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = Hash12(i), b = Hash12(i + float2(1, 0)), c = Hash12(i + float2(0, 1)), d = Hash12(i + float2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float SXFbm(float2 p, int oct)
{
    float s = 0.0, a = 0.5;
    for (int i = 0; i < oct; ++i) { s += SXN(p) * a; p = p * 2.03 + 17.1; a *= 0.5; }
    return s;
}

float4 Tip(Brush b)
{
    float r = length(b.local);
    float n = SXFbm(b.local * Scale + b.seed * 41.0, 6);
    float body = saturate(1.0 - r);
    float a = smoothstep(1.0 - Density, 1.05 - Density + 0.15, n * (0.6 + body * 0.8)) * smoothstep(0.0, 0.25, body);
    return float4(b.color, a);
}
