//@name Retouche locale
//@category Réglages
//@param kind enum 0 "Dégradé linéaire|Radial" "Forme"
//@param p0 float2 0.5 0.25 0 1 "Point A / centre"
//@param invert bool 0 "Inverser"
//@param amount float 100 0 100 "Intensité"
//@param show_mask bool 0 "Afficher le masque"
//@group Dégradé
//@param p1 float2 0.5 0.6 0 1 "Point B"
//@group Radial
//@param radius float2 0.25 0.3 0.01 1.5 "Rayons"
//@param angle float 0 -180 180 "Angle"
//@param feather float 50 0 100 "Contour progressif"
//@group Réglages
//@param temperature float 0 -100 100 "Température"
//@param tint float 0 -100 100 "Teinte"
//@param exposure float 0 -4 4 "Exposition (IL)"
//@param contrast float 0 -100 100 "Contraste"
//@param highlights float 0 -100 100 "Hautes lumières"
//@param shadows float 0 -100 100 "Ombres"
//@param whites float 0 -100 100 "Blancs"
//@param blacks float 0 -100 100 "Noirs"
//@param saturation float 0 -100 100 "Saturation"
//@param haze float 0 -100 100 "Correction du voile"
//@param color color 1 1 1 1 "Couleur (teinte ajoutée)"
//@param color_amount float 0 0 100 "Force de la couleur"
// Local adjustment of the Photo RAW workspace (graduated / radial filter): a mask from the shape, the
// adjustments applied through it on the linear image. Linear: full effect at A, none at B.
// Radial: ellipse centered on A, effect inside (outside when inverted).

float LocalMask(EffectContext ctx)
{
    float2 px = float2(ctx.px) + 0.5;
    float m;
    if (kind == 0)
    {
        float2 a = p0 * ctx.size, b = p1 * ctx.size;
        float2 ab = b - a;
        float t = dot(px - a, ab) / max(dot(ab, ab), 1e-3);
        m = 1.0 - smoothstep(0.0, 1.0, t);
    }
    else
    {
        float r = radians(angle);
        float2 q = px - p0 * ctx.size;
        q = float2(q.x * cos(r) + q.y * sin(r), -q.x * sin(r) + q.y * cos(r));
        float2 rr = max(radius * ctx.size, 1.0);
        float e = length(q / rr);
        float f = max(feather / 100.0, 0.001);
        m = 1.0 - smoothstep(1.0 - f, 1.0, e);
    }
    if (invert) m = 1.0 - m;
    return saturate(m) * amount / 100.0;
}

float4 Effect(EffectContext ctx)
{
    float4 src = Input(0, ctx.px);
    float m = LocalMask(ctx);
    if (show_mask) return float4(lerp(src.rgb, float3(1.0, 0.1, 0.1), m * 0.6), src.a);
    if (m <= 0.0) return src;
    float3 c = max(src.rgb, 0.0);
    float t = temperature / 100.0, g = tint / 100.0;
    c *= float3(1.0 + 0.35 * t, 1.0 - 0.25 * g, 1.0 - 0.35 * t) * float3(1.0 + 0.1 * g, 1.0, 1.0 + 0.1 * g);
    c *= exp2(exposure);
    if (haze != 0.0)
    {
        float veil = min(c.r, min(c.g, c.b));
        float d = haze / 100.0;
        c = d > 0.0 ? max(c - veil * d * 0.7, 0.0) / max(1.0 - veil * d * 0.7, 0.3) : lerp(c, c + (0.5 - c) * 0.3, -d);
    }
    float3 p = pow(c, 1.0 / 2.2);
    float L = Luma(p);
    float hl = smoothstep(0.45, 1.0, L), sh = 1.0 - smoothstep(0.0, 0.55, L);
    float wh = smoothstep(0.75, 1.1, L), bk = 1.0 - smoothstep(0.0, 0.25, L);
    p += highlights / 100.0 * 0.35 * hl + shadows / 100.0 * 0.35 * sh + whites / 100.0 * 0.25 * wh + blacks / 100.0 * 0.2 * bk;
    float k = contrast / 100.0;
    p = k > 0 ? lerp(p, saturate(p) * saturate(p) * (3.0 - 2.0 * saturate(p)), k) : lerp(p, 0.5 + (p - 0.5) * 0.6, -k);
    float Lp = Luma(p);
    p = Lp + (p - Lp) * max(1.0 + saturation / 100.0, 0.0);
    if (color_amount > 0.0)
    {
        float3 tc = pow(max(color.rgb, 0.0), 1.0 / 2.2);
        p = lerp(p, p * tc / max(Luma(tc), 1e-3), color_amount / 100.0 * 0.5);
    }
    c = pow(max(p, 0.0), 2.2);
    return float4(lerp(src.rgb, c, m), src.a);
}
