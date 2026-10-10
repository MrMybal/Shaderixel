//@name Fissures
//@category Texture 3D
//@spacing 0.5
//@param Scale float 3 1 8 "Échelle"
//@param Width float 0.05 0.01 0.2 "Épaisseur"

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
    float2 p = b.local * Scale + b.seed * 23.0;
    float2 i = floor(p), f = frac(p);
    float d1 = 9.0, d2 = 9.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            float2 g = float2(x, y);
            float2 o = float2(Hash12(i + g), Hash12(i + g + 7.3));
            float d = length(g + o - f);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    float edge = 1.0 - smoothstep(Width * 0.5, Width, d2 - d1);
    float body = 1.0 - smoothstep(0.6, 1.0, length(b.local));
    return float4(b.color, edge * body);
}
