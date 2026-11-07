//@name Pastel tendre
//@category Essentiels
//@spacing 0.08
//@param Grain float 0.5 0 1 "Grain"
//@param GrainScale float 2.5 0.5 8 "Taille du grain"

// Pastel : un dépôt poudreux au bord irrégulier, accroché par le grain du papier.
float4 Tip(Brush b)
{
    float r = length(b.local);
    float2 p = b.px / GrainScale;
    float n = Hash1(dot(floor(p), float2(0.1031, 0.3071))) * 0.5 + Hash1(dot(floor(p * 0.37), float2(0.13, 0.71))) * 0.5;
    float edge = 1.0 - smoothstep(0.55 + n * 0.3, 1.0, r);
    float tooth = saturate((n - Grain * 0.7) * 2.5 + 0.6);
    return float4(b.color, edge * tooth * 0.8);
}
