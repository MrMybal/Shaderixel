//@name Lavis
//@category Essentiels
//@spacing 0.06
//@param Flow float 0.18 0.02 0.6 "Charge en eau"
//@param EdgeDarkening float 0.6 0 1 "Bord plus foncé"

// Lavis d'aquarelle : une couche transparente qui fonce au bord (là où le pigment se dépose en séchant).
float4 Tip(Brush b)
{
    float r = length(b.local);
    float body = 1.0 - smoothstep(0.75, 1.0, r);
    float rim = smoothstep(0.55, 0.9, r) * (1.0 - smoothstep(0.9, 1.0, r));
    return float4(b.color, body * Flow + rim * Flow * EdgeDarkening * 1.5);
}
