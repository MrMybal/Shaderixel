//@name Bombe de peinture
//@category Essentiels
//@spacing 0.1
//@airbrush 50
//@param Density float 0.12 0.01 0.6 "Densité des gouttelettes"
//@param DotSize float 1.2 0.5 4 "Taille des gouttelettes"

// Bombe : une pluie de fines gouttelettes, plus dense au centre, qui s'accumule tant que le bouton est appuyé.
float4 Tip(Brush b)
{
    float r = length(b.local);
    float2 cell = floor(b.px / DotSize);
    float n = Hash1(dot(cell, float2(0.1031, 0.3071)) + b.seed * 31.0 + b.time * 7.0);
    float density = Density * exp(-r * r * 2.5);
    return float4(b.color, step(1.0 - density, n) * (1.0 - smoothstep(0.9, 1.0, r)));
}
