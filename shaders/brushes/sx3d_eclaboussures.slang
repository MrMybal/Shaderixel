//@name Éclaboussures
//@category Texture 3D
//@spacing 0.9
//@extent 2.5
//@param Drops float 14 3 40 "Gouttes"
//@param Spread float 0.85 0.2 1 "Étendue"

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
    float a = 0.0;
    for (int i = 0; i < 40; ++i)
    {
        if (i >= (int)Drops) break;
        float2 h = float2(Hash12(float2(i, b.seed * 37.0)), Hash12(float2(b.seed * 53.0, i)));
        float ang = h.x * 6.2831853, dist = pow(h.y, 0.7) * Spread;
        float2 c = float2(cos(ang), sin(ang)) * dist;
        float rad = lerp(0.03, 0.16, Hash12(float2(i * 3.1, b.seed))) * (1.0 - dist * 0.6);
        a = max(a, 1.0 - smoothstep(rad * 0.75, rad, length(b.local - c)));
    }
    return float4(b.color, a);
}
