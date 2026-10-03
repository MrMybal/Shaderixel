//@name Fusain
//@category Exemples
//@spacing 0.08
//@param Grain float 0.65 0 1 "Grain du papier"
//@param GrainScale float 2.5 0.5 8 "Taille du grain"

// Le grain dépend de la position sur le papier (b.px), pas de la touche :
// repasser au même endroit garde la même texture.
float Noise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = Hash1(dot(i, float2(1.0, 57.0))), b = Hash1(dot(i + float2(1, 0), float2(1.0, 57.0)));
    float c = Hash1(dot(i + float2(0, 1), float2(1.0, 57.0))), d = Hash1(dot(i + float2(1, 1), float2(1.0, 57.0)));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float4 Tip(Brush b)
{
    float shape = 1.0 - smoothstep(b.hardness * 0.8, 1.0, length(b.local));
    float paper = Noise(b.px / GrainScale) * 0.6 + Noise(b.px / (GrainScale * 3.0)) * 0.4;
    float tooth = smoothstep(Grain * 0.9, Grain * 0.9 + 0.25, paper + (b.pressure - 0.5) * 0.4);
    return float4(b.color, shape * lerp(1.0, tooth, Grain));
}
