//@name Aérographe granuleux
//@category Exemples
//@spacing 0.25
//@airbrush 40
//@param Density float 0.35 0.02 1 "Densité"
//@param Falloff float 1.5 0.2 6 "Atténuation"

// Peinture en continu : tant que le bouton est appuyé, 40 touches par seconde
// s'accumulent au même endroit (//@airbrush). Chaque touche est un nuage de
// points différent (b.seed, b.time).
float4 Tip(Brush b)
{
    float r = length(b.local);
    float radial = pow(saturate(1.0 - r), Falloff);
    float n = Hash1(dot(floor(b.px), float2(0.1031, 0.3071)) + b.seed * 17.0 + b.time);
    float speck = step(1.0 - Density, n);
    return float4(b.color, radial * speck);
}
