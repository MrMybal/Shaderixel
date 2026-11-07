//@name Rond doux
//@category Essentiels
//@spacing 0.1
//@param Softness float 1.0 0.3 3 "Douceur"

// Pointe ronde au bord fondu (gaussien), pour les ombres et les dégradés peints.
float4 Tip(Brush b)
{
    float r = length(b.local);
    float g = exp(-r * r * 3.0 / Softness) * (1.0 - smoothstep(0.8, 1.0, r));
    return float4(b.color, g);
}
