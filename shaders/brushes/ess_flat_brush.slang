//@name Pinceau plat
//@category Essentiels
//@spacing 0.04
//@extent 1.2
//@param Bristles float 18 4 60 "Poils"
//@param Dryness float 0.35 0 1 "Sécheresse"
//@param Width float 0.3 0.1 1 "Épaisseur"

// Brosse plate qui suit le sens du trait : une rangée de poils de longueurs inégales ; la sécheresse laisse des
// stries quand la peinture manque.
float4 Tip(Brush b)
{
    float2 q = b.local;  // x across the stroke, y along it (the dab follows the direction)
    float across = q.x * 0.5 + 0.5;
    float lane = floor(across * Bristles);
    float len = lerp(0.6, 1.0, Hash1(lane * 7.13 + 1.7));
    float body = (1.0 - smoothstep(0.9, 1.0, abs(q.x))) * (1.0 - smoothstep(Width * len * 0.8, Width * len, abs(q.y)));
    float dry = step(Dryness * (1.0 - b.pressure * 0.5), Hash1(lane * 3.1 + floor(b.distance / max(b.radius, 1.0) * 3.0) * 0.37));
    return float4(b.color, body * lerp(1.0, dry, Dryness));
}

void Dynamics(inout Dab d)
{
    d.angle = atan2(d.direction.y, d.direction.x);
}
