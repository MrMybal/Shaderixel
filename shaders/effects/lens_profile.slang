//@name Correction d'objectif
//@category Déformation
//@halo full
//@premultiplied
//@param mode enum 0 "Corriger l'objectif|Simuler l'objectif" "Mode"
//@group Distorsion
//@param dist_model enum 0 "Aucune|poly3 (k1)|poly5 (k1, k2)|ptlens (a, b, c)" "Modèle"
//@param d_a float 0 -0.5 0.5 "k1 / a"
//@param d_b float 0 -0.5 0.5 "k2 / b"
//@param d_c float 0 -0.5 0.5 "c"
//@param dist_amount float 1 0 2 "Intensité de la distorsion"
//@group Aberrations chromatiques
//@param tca_model enum 0 "Aucune|linéaire (kr, kb)|poly3 (v, c, b)" "Modèle"
//@param tca_vr float 1 0.99 1.01 "Rouge : kr / vr"
//@param tca_vb float 1 0.99 1.01 "Bleu : kb / vb"
//@param tca_cr float 0 -0.01 0.01 "Rouge : cr"
//@param tca_cb float 0 -0.01 0.01 "Bleu : cb"
//@param tca_br float 0 -0.01 0.01 "Rouge : br"
//@param tca_bb float 0 -0.01 0.01 "Bleu : bb"
//@param tca_amount float 1 0 4 "Intensité des aberrations"
//@group Vignetage
//@param vig bool 0 "Vignetage (modèle pa)"
//@param v_k1 float 0 -2 2 "k1"
//@param v_k2 float 0 -2 2 "k2"
//@param v_k3 float 0 -2 2 "k3"
//@param vig_amount float 1 0 2 "Intensité du vignetage"
//@group Cadrage
//@param crop_ratio float 1 0.3 3 "Crop de calibration / crop du boîtier"
//@param autoscale bool 1 "Remplir le cadre"
//@param zoom float 1 0.5 2 "Zoom"
// Lens profiles of the lensfun database (distortion ptlens / poly3 / poly5, TCA linear / poly3,
// vignetting pa), in lensfun's conventions: the models map an undistorted radius to the distorted one,
// r = 1 is the half short edge of a 3:2 sensor at the calibration crop factor (geometry) and the image
// corner (vignetting). "Corriger" removes the lens defects; "Simuler" adds them to any image (inverse
// mapping by Newton iterations), the image being taken as shot with that lens.

float Distort(float ru)
{
    // Rd = f(Ru), amount 0..2 blends with the identity.
    float rd = ru;
    if (dist_model == 1) rd = ru * (1.0 - d_a + d_a * ru * ru);
    else if (dist_model == 2) rd = ru * (1.0 + d_a * ru * ru + d_b * ru * ru * ru * ru);
    else if (dist_model == 3) rd = ru * (d_a * ru * ru * ru + d_b * ru * ru + d_c * ru + 1.0 - d_a - d_b - d_c);
    return ru + (rd - ru) * dist_amount;
}

float DistortInverse(float rd)
{
    // Ru with f(Ru) = Rd (Newton, numeric derivative).
    float ru = rd;
    for (int i = 0; i < 8; ++i)
    {
        float f = Distort(ru) - rd;
        float d = (Distort(ru + 1e-3) - Distort(ru - 1e-3)) / 2e-3;
        ru -= f / max(abs(d), 1e-4) * sign(d);
    }
    return ru;
}

// Distorted radius of red (c = 0) or blue (c = 1) for a green radius r.
float Tca(float r, int c)
{
    float k = 1.0;
    if (tca_model == 1) k = c == 0 ? tca_vr : tca_vb;
    else if (tca_model == 2) k = c == 0 ? (tca_br * r * r + tca_cr * r + tca_vr) : (tca_bb * r * r + tca_cb * r + tca_vb);
    return r * (1.0 + (k - 1.0) * tca_amount);
}

float Vignette(float rv)
{
    float r2 = rv * rv;
    float p = 1.0 + v_k1 * r2 + v_k2 * r2 * r2 + v_k3 * r2 * r2 * r2;
    return lerp(1.0, max(p, 0.02), vig_amount);
}

float4 Effect(EffectContext ctx)
{
    const float2 hsz = ctx.size * 0.5;
    const float halfDiag = length(hsz);
    // Pixels -> normalized radius (geometry: 3:2 hsz short edge at the calibration crop; vignetting: corner).
    const float kGeom = 21.6333 * crop_ratio / (12.0 * halfDiag);
    const float kVig = crop_ratio / halfDiag;
    float2 p = ctx.px + 0.5 - hsz;
    float scale = 1.0 / max(zoom, 0.05);
    if (mode == 0 && autoscale && dist_model != 0)
    {
        // Largest zoom-out keeping the frame filled: corner and edge middles land inside the source.
        float s = 1.0;
        float2 pts[3] = {hsz, float2(hsz.x, 0), float2(0, hsz.y)};
        for (int i = 0; i < 3; ++i)
        {
            float ru = length(pts[i]) * kGeom;
            float rd = Distort(ru);
            float2 q = pts[i] * (rd / max(ru, 1e-6));
            s = min(s, min(hsz.x / max(abs(q.x), 1e-3), hsz.y / max(abs(q.y), 1e-3)));
        }
        scale *= s;
    }
    if (mode == 0)
    {
        // Correction: output (undistorted) -> source (distorted) position, per channel.
        float2 pu = p * scale;
        float ru = length(pu) * kGeom;
        float2 pg = pu * (Distort(ru) / max(ru, 1e-6));
        float rg = length(pg) * kGeom;
        float2 pr = pg * (Tca(rg, 0) / max(rg, 1e-6));
        float2 pb = pg * (Tca(rg, 1) / max(rg, 1e-6));
        float4 cg = InputUV(0, (pg + hsz) * ctx.texel);
        float4 c = float4(InputUV(0, (pr + hsz) * ctx.texel).r, cg.g, InputUV(0, (pb + hsz) * ctx.texel).b, cg.a);
        if (vig) c.rgb /= Vignette(length(pg) * kVig);
        return c;
    }
    // Simulation: output (as photographed) -> the image (undistorted) position, per channel.
    float2 q = p * scale;
    float4 o = 0;
    for (int ch = 0; ch < 3; ++ch)
    {
        // Channel position -> green position (TCA inverse, fixed point), then distortion inverse.
        float2 g = q;
        if (ch != 1)
            for (int i = 0; i < 3; ++i)
            {
                float r = length(g) * kGeom;
                g = q * (r / max(Tca(r, ch == 0 ? 0 : 1), 1e-6));
            }
        float rd = length(g) * kGeom;
        float2 u = g * (DistortInverse(rd) / max(rd, 1e-6));
        float4 s = InputUV(0, (u + hsz) * ctx.texel);
        if (ch == 0) o.r = s.r;
        else if (ch == 1) { o.g = s.g; o.a = s.a; }
        else o.b = s.b;
    }
    if (vig) o.rgb *= Vignette(length(q) * kVig);
    return o;
}
