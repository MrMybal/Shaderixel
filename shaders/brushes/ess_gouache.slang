//@name Gouache
//@category Essentiels
//@spacing 0.06
//@param Texture float 0.25 0 1 "Texture"

// Gouache : peinture mate et couvrante, au bord légèrement irrégulier, avec un peu de texture de pinceau.
float4 Tip(Brush b)
{
    float2 p = b.local;
    float a = atan2(p.y, p.x);
    float r = length(p) / (0.92 + 0.08 * sin(a * 7.0 + b.seed * 11.0));
    float edge = 1.0 - smoothstep(0.8, 1.0, r);
    float n = Hash1(dot(floor(b.px / 2.0), float2(0.1031, 0.3071)));
    return float4(b.color, edge * lerp(1.0, 0.75 + 0.25 * n, Texture));
}
