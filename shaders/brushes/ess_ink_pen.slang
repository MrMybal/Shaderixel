//@name Plume d'encre
//@category Essentiels
//@spacing 0.05
//@param Taper float 0.85 0 1 "Effilé selon la pression"
//@param SpeedThin float 0.3 0 1 "Affinement avec la vitesse"

// Encre : trait net et opaque, plus fin quand la pression baisse ou que le trait accélère (pleins et déliés).
float4 Tip(Brush b)
{
    float r = length(b.local);
    float aa = 1.5 / max(b.radius, 1.0);  // (one and a half pixels of soft edge)
    return float4(b.color, 1.0 - smoothstep(1.0 - aa, 1.0, r));
}

void Dynamics(inout Dab d)
{
    float k = lerp(1.0, max(d.pressure, 0.08), Taper);
    k *= lerp(1.0, 1.0 / (1.0 + d.speed / 1500.0), SpeedThin);
    d.radius *= k;
    d.opacity = max(d.opacity, 0.9);
}
