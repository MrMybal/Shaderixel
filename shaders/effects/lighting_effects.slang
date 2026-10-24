//@name Effets d'éclairage
//@category Lumière et lueur
//@halo full
//@param kind enum 0 "Projecteur|Point|Infini" "Type de lumière"
//@param position float2 0.35 0.3 0 1 "Position"
//@param target float2 0.6 0.65 0 1 "Visée (projecteur, infini)"
//@param color color 1 0.95 0.85 1 "Couleur"
//@param intensity float 1.4 0 4 "Intensité"
//@param reach float 0.7 0.05 2 "Portée"
//@param cone float 35 5 89 "Ouverture du projecteur (°)"
//@param feather float 0.5 0 1 "Adoucissement du bord"
//@param height float 0.25 0.01 1 "Hauteur de la lumière"
//@param second bool 0 "Seconde lumière (point)"
//@param position2 float2 0.8 0.25 0 1 "Position (seconde)"
//@param color2 color 0.4 0.6 1 1 "Couleur (seconde)"
//@param intensity2 float 0.8 0 4 "Intensité (seconde)"
//@param ambient float 0.3 0 1 "Lumière ambiante"
//@param bump enum 1 "Aucun|Luminosité|Alpha" "Relief d'après"
//@param bumpHeight float 4 0 20 "Hauteur du relief"
//@param gloss float 0.4 0 1 "Brillance"
//@param shininess float 24 2 128 "Netteté du reflet"
// Photoshop's Lighting Effects: the image lit by a spot, point or directional light (and an optional second point
// light), over a relief read from its luminance or alpha. Positions are placed on the image (target button).

float H(int2 p)
{
    const float4 c = Input(0, p);
    return bump == 2 ? c.a : bump == 1 ? Luma(c.rgb) : 0.0;
}

float3 Shade(float3 base, float3 N, float3 P, float2 lightUv, float3 lcol, float lint, uint type, float2 sizePx, float2 dir)
{
    const float diag = length(sizePx);
    const float3 lp = float3(lightUv * sizePx, height * diag);
    float3 L;
    float att = 1.0;
    if (type == 2)
    {
        // Infinite: from the position toward the target, coming down at the light's height.
        L = normalize(float3(-dir * diag, height * diag));
    }
    else
    {
        const float3 d = lp - P;
        const float dist = length(d);
        L = d / max(dist, 1e-4);
        att = saturate(1.0 - dist / max(reach * diag, 1.0));
        att *= att;
        if (type == 0)
        {
            // Spot: a cone aimed at the target point on the image.
            const float3 aim = normalize(float3(dir * diag, -height * diag));
            const float c = dot(-L, aim);
            const float outer = cos(radians(cone)), inner = cos(radians(cone * (1.0 - feather)));
            att *= smoothstep(outer, max(inner, outer + 1e-3), c);
        }
    }
    const float ndl = saturate(dot(N, L));
    const float3 H = normalize(L + float3(0, 0, 1));
    const float spec = pow(saturate(dot(N, H)), shininess) * gloss;
    return (base * ndl + spec) * lcol * lint * att;
}

float4 Effect(EffectContext ctx)
{
    const float4 src = Input(0, ctx.px);
    const float2 sizePx = 1.0 / ctx.texel;
    // Normal from the relief (central differences).
    float3 N = float3(0, 0, 1);
    if (bump != 0 && bumpHeight > 0)
    {
        const float dx = H(ctx.px + int2(1, 0)) - H(ctx.px - int2(1, 0));
        const float dy = H(ctx.px + int2(0, 1)) - H(ctx.px - int2(0, 1));
        N = normalize(float3(-dx * bumpHeight, -dy * bumpHeight, 1));
    }
    const float3 P = float3(ctx.uv * sizePx, 0);
    const float3 base = src.rgb;
    float3 lit = base * ambient;
    lit += Shade(base, N, P, position, color.rgb, intensity, (uint)kind, sizePx, target - position);
    if (second) lit += Shade(base, N, P, position2, color2.rgb, intensity2, 1u, sizePx, float2(0, 0));
    return float4(lit, src.a);
}
