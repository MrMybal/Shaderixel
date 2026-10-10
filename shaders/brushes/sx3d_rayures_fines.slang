//@name Rayures fines
//@category Texture 3D
//@spacing 0.3
//@extent 1.4
//@param Count float 7 1 16 "Rayures"
//@param Width float 0.02 0.004 0.08 "Épaisseur"

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
    for (int i = 0; i < 16; ++i)
    {
        if (i >= (int)Count) break;
        float y = (Hash12(float2(i, b.seed * 31.0)) - 0.5) * 1.4;
        float len = lerp(0.3, 1.0, Hash12(float2(b.seed * 11.0, i)));
        float x0 = (Hash12(float2(i * 5.0, b.seed)) - 0.5) * 0.6;
        float t = saturate((b.local.x - x0 + len) / (2.0 * len));
        float inside = step(abs(b.local.x - x0), len);
        float w = Width * sin(t * 3.14159);
        a = max(a, inside * (1.0 - smoothstep(w * 0.6, w, abs(b.local.y - y))));
    }
    return float4(b.color, a);
}
