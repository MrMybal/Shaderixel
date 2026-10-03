//@name Plume calligraphique
//@category Exemples
//@spacing 0.05
//@param Flatness float 0.25 0.05 1 "Épaisseur relative"
//@param NibAngle float 0.6 -3.1416 3.1416 "Angle de la plume"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Plume plate : une ellipse fine, orientée selon un angle fixe ou selon la
// direction du trait (FollowStroke).
float4 Tip(Brush b)
{
    float2 p = b.local;
    float d = length(float2(p.x, p.y / max(Flatness, 0.01)));
    float a = 1.0 - smoothstep(lerp(0.7, 0.98, b.hardness), 1.0, d);
    return float4(b.color, a);
}

void Dynamics(inout Dab d)
{
    float along = atan2(d.direction.y, d.direction.x);
    d.angle = lerp(NibAngle, along + 1.5708, FollowStroke);
}
