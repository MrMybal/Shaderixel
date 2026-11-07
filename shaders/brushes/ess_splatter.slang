//@name Éclaboussures
//@category Essentiels
//@spacing 0.9
//@extent 3
//@param Scatter float 2.0 0 3 "Dispersion"
//@param SizeJitter float 0.8 0 1 "Variation de taille"

// Gouttes de peinture projetées autour du trait, de tailles variées, aux bords légèrement irréguliers.
float4 Tip(Brush b)
{
    float2 p = b.local;
    float a = atan2(p.y, p.x);
    float wobble = 0.85 + 0.15 * sin(a * 5.0 + b.seed * 20.0);
    float r = length(p) / wobble;
    return float4(b.color, 1.0 - smoothstep(0.85, 1.0, r));
}

void Dynamics(inout Dab d)
{
    d.pos += (Rand2(d.seed) - 0.5) * 2.0 * d.radius * Scatter;
    d.radius *= lerp(1.0, pow(Rand(d.seed, 3.0), 2.0) * 0.9 + 0.1, SizeJitter);
    d.angle = Rand(d.seed, 4.0) * 6.2831853;
}
