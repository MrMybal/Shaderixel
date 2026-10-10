//@name Écailles / pastilles
//@category Texture 3D
//@spacing 0.55
//@param Cells float 4 2 10 "Cellules"

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
    float2 p = b.local * Cells + b.seed * 9.0;
    float2 i = floor(p), f = frac(p) - 0.5;
    float h = Hash12(i);
    float a = (1.0 - smoothstep(0.28, 0.42, length(f))) * step(0.35, h);
    float body = 1.0 - smoothstep(0.6, 1.0, length(b.local));
    return float4(b.color, a * body);
}
