//@name Saleté (grunge)
//@category Texture 3D
//@spacing 0.3
//@param Scale float 5 1 14 "Échelle"
//@param Contrast float 0.55 0.1 0.95 "Contraste"

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
    float n = SXFbm(b.local * Scale + b.seed * 61.0, 6) * 0.7 + SXN(b.local * Scale * 6.0) * 0.3;
    float body = 1.0 - smoothstep(0.35, 1.0, r);
    return float4(b.color, body * smoothstep(Contrast - 0.2, Contrast + 0.2, n));
}
