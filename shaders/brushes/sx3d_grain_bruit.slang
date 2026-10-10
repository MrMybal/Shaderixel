//@name Grain (bruit fin)
//@category Texture 3D
//@spacing 0.25
//@param Scale float 18 4 60 "Finesse"
//@param Amount float 0.6 0 1 "Quantité"

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
    float soft = 1.0 - smoothstep(0.3, 1.0, r);
    float n = SXN(b.local * Scale + b.seed * 13.0);
    return float4(b.color, soft * smoothstep(1.0 - Amount, 1.0, n + 0.3));
}
