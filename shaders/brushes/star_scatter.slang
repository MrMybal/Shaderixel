//@name Dispersion d'étoiles
//@category Exemples
//@spacing 0.6
//@extent 3
//@param Scatter float 1.5 0 3 "Dispersion"
//@param SizeJitter float 0.6 0 1 "Variation de taille"
//@param HueJitter float 0.15 0 0.5 "Variation de teinte"
//@param Points float 5 3 12 "Branches"

// Chaque touche est déplacée, tournée, redimensionnée et teintée au hasard
// (Dynamics), puis dessine une étoile (Tip).
float4 Tip(Brush b)
{
    float2 p = b.local;
    float a = atan2(p.y, p.x);
    float r = length(p);
    float k = 0.5 + 0.5 * cos(a * floor(Points));
    float edge = lerp(0.45, 0.95, pow(k, 3.0));
    float cover = 1.0 - smoothstep(edge - 0.06, edge, r);
    return float4(b.color, cover);
}

float3 HueShift(float3 c, float h)
{
    // Rotation around the gray axis.
    float3 k = float3(0.57735, 0.57735, 0.57735);
    float a = h * 6.2831853;
    return c * cos(a) + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - cos(a));
}

void Dynamics(inout Dab d)
{
    d.pos += (Rand2(d.seed) - 0.5) * 2.0 * d.radius * Scatter;
    d.radius *= lerp(1.0, Rand(d.seed, 3.0), SizeJitter);
    d.angle = Rand(d.seed, 4.0) * 6.2831853;
    d.color = max(HueShift(d.color, (Rand(d.seed, 5.0) - 0.5) * 2.0 * HueJitter), 0.0);
}
