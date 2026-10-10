//@name Pinceau sec
//@category Texture 3D
//@spacing 0.12
//@param Bristles float 22 6 60 "Poils"
//@param Dry float 0.45 0 0.9 "Sécheresse"

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
    float2 p = b.local;
    float body = 1.0 - smoothstep(0.75, 1.0, length(p * float2(1.0, 1.0)));
    float lane = SXN(float2(p.y * Bristles, b.seed * 3.0));
    float gaps = smoothstep(Dry, Dry + 0.15, lane + SXN(p * float2(3.0, Bristles)) * 0.3);
    return float4(b.color, body * gaps);
}
