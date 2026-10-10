//@name Couture (points de fil)
//@category Texture 3D
//@spacing 0.9
//@param Thickness float 0.22 0.05 0.5 "Épaisseur"
//@param Slant float 0.6 -1.5 1.5 "Inclinaison"

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
    float c = cos(Slant), s = sin(Slant);
    p = float2(c * p.x - s * p.y, s * p.x + c * p.y);
    float a = (1.0 - smoothstep(Thickness * 0.8, Thickness, abs(p.y))) * (1.0 - smoothstep(0.7, 0.8, abs(p.x)));
    return float4(b.color, a);
}
