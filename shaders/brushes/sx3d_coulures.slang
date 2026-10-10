//@name Coulures
//@category Texture 3D
//@spacing 0.25
//@extent 1.6
//@param Drips float 5 1 12 "Coulures"
//@param Length float 0.9 0.2 1.5 "Longueur"

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
    float top = 1.0 - smoothstep(0.25, 0.45, length(p - float2(0, -0.45)) );
    float a = top;
    for (int i = 0; i < 12; ++i)
    {
        if (i >= (int)Drips) break;
        float x = (Hash12(float2(i, b.seed * 17.0)) - 0.5) * 0.7;
        float len = lerp(0.3, 1.0, Hash12(float2(b.seed * 29.0, i))) * Length;
        float w = lerp(0.03, 0.07, Hash12(float2(i * 7.0, b.seed)));
        float y = p.y + 0.45;
        float inside = step(0.0, y) * step(y, len);
        float wy = w * (1.0 - y / max(len, 0.01) * 0.5);
        float d = abs(p.x - x);
        float tipDrop = 1.0 - smoothstep(w * 1.1, w * 1.4, length(float2(p.x - x, y - len)));
        a = max(a, max(inside * (1.0 - smoothstep(wy * 0.8, wy, d)), tipDrop));
    }
    return float4(b.color, a);
}
