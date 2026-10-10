//@name Rivets
//@category Texture 3D
//@spacing 1.6
//@param Ring float 0.25 0 0.6 "Anneau"

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
    float dome = 1.0 - smoothstep(0.62, 0.7, r);
    float ring = (1.0 - smoothstep(0.82, 0.88, r)) * smoothstep(0.7, 0.76, r) * Ring;
    return float4(b.color, max(dome, ring));
}
