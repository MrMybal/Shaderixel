//@name Aérographe
//@category Essentiels
//@spacing 0.08
//@airbrush 60
//@param Softness float 1.0 0.3 3 "Douceur"
//@param Buildup float 0.25 0.02 1 "Débit par touche"

// Aérographe : un nuage doux (gaussien) qui s'accumule tant que le bouton est appuyé (60 touches par seconde au
// même endroit) ; le débit par touche règle la vitesse d'accumulation.
float4 Tip(Brush b)
{
    float r = length(b.local);
    float g = exp(-r * r * 4.5 / Softness) * (1.0 - smoothstep(0.85, 1.0, r));
    return float4(b.color, g * Buildup);
}
