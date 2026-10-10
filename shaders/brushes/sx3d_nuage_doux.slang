//@name Nuage doux (aérographe)
//@category Texture 3D
//@spacing 0.15
//@param Falloff float 2 0.5 5 "Atténuation"

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
    float r = saturate(length(b.local));
    return float4(b.color, pow(1.0 - r, Falloff) * (0.85 + 0.15 * SXN(b.local * 4.0 + b.seed * 7.0)));
}
