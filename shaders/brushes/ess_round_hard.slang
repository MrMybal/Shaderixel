//@name Rond dur
//@category Essentiels
//@spacing 0.08
//@param PressureSize float 0.6 0 1 "Taille selon la pression"

// Pointe ronde nette ; la pression du stylet réduit la taille (la souris peint à pleine taille).
float4 Tip(Brush b)
{
    float r = length(b.local);
    float aa = 1.5 / max(b.radius, 1.0);  // (one and a half pixels of soft edge)
    return float4(b.color, 1.0 - smoothstep(1.0 - aa, 1.0, r));
}

void Dynamics(inout Dab d)
{
    d.radius *= lerp(1.0, max(d.pressure, 0.05), PressureSize);
}
