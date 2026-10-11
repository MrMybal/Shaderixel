//@name Camera Raw
//@category Réglages
//@halo full
//@passes 3
//@group Base
//@param temperature float 0 -100 100 "Température"
//@param tint float 0 -100 100 "Teinte"
//@param exposure float 0 -5 5 "Exposition (IL)"
//@param contrast float 0 -100 100 "Contraste"
//@param highlights float 0 -100 100 "Hautes lumières"
//@param shadows float 0 -100 100 "Ombres"
//@param whites float 0 -100 100 "Blancs"
//@param blacks float 0 -100 100 "Noirs"
//@group Présence
//@param fine_detail float 0 -100 100 "Texture"
//@param clarity float 0 -100 100 "Clarté"
//@param dehaze float 0 -100 100 "Correction du voile"
//@param vibrance float 0 -100 100 "Vibrance"
//@param saturation float 0 -100 100 "Saturation"
//@group Détail
//@param sharpen float 0 0 150 "Netteté"
//@param radius float 1 0.5 3 "Rayon de netteté"
//@group Effets
//@param vignette float 0 -100 100 "Vignettage"
//@param vig_mid float 50 0 100 "Milieu du vignettage"
//@param grain float 0 0 100 "Grain"
//@group Courbe des tonalités
//@param tone curve "Courbe RVB"
//@group Mélangeur : Teinte
//@param mix_h_red float 0 -100 100 "Rouges"
//@param mix_h_orange float 0 -100 100 "Orangés"
//@param mix_h_yellow float 0 -100 100 "Jaunes"
//@param mix_h_green float 0 -100 100 "Verts"
//@param mix_h_aqua float 0 -100 100 "Cyans"
//@param mix_h_blue float 0 -100 100 "Bleus"
//@param mix_h_purple float 0 -100 100 "Violets"
//@param mix_h_magenta float 0 -100 100 "Magentas"
//@group Mélangeur : Saturation
//@param mix_s_red float 0 -100 100 "Rouges"
//@param mix_s_orange float 0 -100 100 "Orangés"
//@param mix_s_yellow float 0 -100 100 "Jaunes"
//@param mix_s_green float 0 -100 100 "Verts"
//@param mix_s_aqua float 0 -100 100 "Cyans"
//@param mix_s_blue float 0 -100 100 "Bleus"
//@param mix_s_purple float 0 -100 100 "Violets"
//@param mix_s_magenta float 0 -100 100 "Magentas"
//@group Mélangeur : Luminance
//@param mix_l_red float 0 -100 100 "Rouges"
//@param mix_l_orange float 0 -100 100 "Orangés"
//@param mix_l_yellow float 0 -100 100 "Jaunes"
//@param mix_l_green float 0 -100 100 "Verts"
//@param mix_l_aqua float 0 -100 100 "Cyans"
//@param mix_l_blue float 0 -100 100 "Bleus"
//@param mix_l_purple float 0 -100 100 "Violets"
//@param mix_l_magenta float 0 -100 100 "Magentas"
//@group Étalonnage des couleurs
//@param cg_sh_hue float 220 0 360 "Ombres : teinte"
//@param cg_sh_sat float 0 0 100 "Ombres : saturation"
//@param cg_mid_hue float 40 0 360 "Tons moyens : teinte"
//@param cg_mid_sat float 0 0 100 "Tons moyens : saturation"
//@param cg_hl_hue float 45 0 360 "Hautes lumières : teinte"
//@param cg_hl_sat float 0 0 100 "Hautes lumières : saturation"
//@param cg_balance float 0 -100 100 "Équilibre"
//@group Réduction du bruit
//@param nr_luma float 0 0 100 "Luminance"
//@param nr_color float 0 0 100 "Couleur"
// Raw development style controls on linear light. Passes 0 and 1 blur the image
// (radius ~ 1/60 of the image) for clarity / dehaze; pass 2 develops the pixel.


float4 Blur1D(EffectContext ctx, int2 dir)
{
    float sigma = max(4.0, max(ctx.size.x, ctx.size.y) / 60.0);
    int n = int(ceil(sigma * 2.5));
    int stride = max(1, n / 48);
    float4 sum = 0;
    float wsum = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float w = exp(-0.5 * (i * i) / (sigma * sigma));
        sum += Prev(ctx.px + dir * i) * w;
        wsum += w;
    }
    return sum / wsum;
}

float3 CrRgbToHsv(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-6)), d / (q.x + 1e-6), q.x);
}

float3 CrHsvToRgb(float3 c)
{
    float3 p = abs(frac(c.xxx + float3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return c.z * lerp(float3(1, 1, 1), saturate(p - 1.0), c.y);
}

// Color mixer: per hue band (red 0, orange 30, yellow 60, green 120, aqua 180, blue 240, purple 270,
// magenta 300 degrees) hue shift / saturation / luminance, interpolated between the two nearest bands.
float3 ColorMixer(float3 p)
{
    float hs[8] = {mix_h_red, mix_h_orange, mix_h_yellow, mix_h_green, mix_h_aqua, mix_h_blue, mix_h_purple, mix_h_magenta};
    float ss[8] = {mix_s_red, mix_s_orange, mix_s_yellow, mix_s_green, mix_s_aqua, mix_s_blue, mix_s_purple, mix_s_magenta};
    float ls[8] = {mix_l_red, mix_l_orange, mix_l_yellow, mix_l_green, mix_l_aqua, mix_l_blue, mix_l_purple, mix_l_magenta};
    float centers[9] = {0, 30, 60, 120, 180, 240, 270, 300, 360};
    float3 hsv = CrRgbToHsv(saturate(p));
    float deg = hsv.x * 360.0;
    int i = 0;
    for (int k = 0; k < 8; ++k)
        if (deg >= centers[k]) i = k;
    float t = saturate((deg - centers[i]) / (centers[i + 1] - centers[i]));
    int j = (i + 1) % 8;
    float dh = lerp(hs[i], hs[j], t), ds = lerp(ss[i], ss[j], t), dl = lerp(ls[i], ls[j], t);
    float colored = smoothstep(0.02, 0.25, hsv.y);  // grays are not touched
    float3 o = hsv;
    o.x = frac(hsv.x + dh / 100.0 * (30.0 / 360.0) * colored + 1.0);
    o.y = saturate(hsv.y * (1.0 + ds / 100.0 * colored));
    float3 rgb = CrHsvToRgb(o) + max(p - saturate(p), 0.0);  // keep values above 1
    float L0 = Luma(rgb);
    float L1 = L0 * (1.0 + dl / 100.0 * 0.6 * colored);
    return L0 > 1e-5 ? rgb * (L1 / L0) : rgb;
}

// Color grading wheels: tints of the shadows / midtones / highlights (luminance preserved).
float3 ColorGrade(float3 p)
{
    float L = saturate(Luma(p));
    float b = cg_balance / 100.0;
    float ws = 1.0 - smoothstep(0.0, 0.5 + 0.35 * b, L);
    float wh = smoothstep(0.5 + 0.35 * b, 1.0, L);
    float wm = saturate(1.0 - ws - wh);
    float3 tint = 0;
    float3 c0 = CrHsvToRgb(float3(cg_sh_hue / 360.0, 1, 1)), c1 = CrHsvToRgb(float3(cg_mid_hue / 360.0, 1, 1)), c2 = CrHsvToRgb(float3(cg_hl_hue / 360.0, 1, 1));
    tint += (c0 - Luma(c0)) * cg_sh_sat / 100.0 * ws;
    tint += (c1 - Luma(c1)) * cg_mid_sat / 100.0 * wm;
    tint += (c2 - Luma(c2)) * cg_hl_sat / 100.0 * wh;
    float3 o = max(p + tint * 0.35, 0.0);
    float Lo = Luma(o);
    return Lo > 1e-5 ? o * (Luma(p) / Lo) : o;
}

// Noise reduction: edge-preserving average of the luminance (5x5, range weights from the noise level)
// and wide chroma average (9x9, step 2) guided by the luminance.
float3 Denoise(EffectContext ctx, float3 c)
{
    float L = Luma(c);
    float3 chroma = c - L;
    float sl = nr_luma / 100.0, sc = nr_color / 100.0;
    if (sl > 0.0)
    {
        float sigma = (0.004 + 0.06 * sl * sl) * (sqrt(max(L, 0.0)) + 0.05);
        float acc = 0, wsum = 0;
        for (int y = -2; y <= 2; ++y)
            for (int x = -2; x <= 2; ++x)
            {
                float n = Luma(max(Input(0, ctx.px + int2(x, y)).rgb, 0.0));
                float d = (n - L) / sigma;
                float w = exp(-0.5 * d * d) * exp(-0.18 * (x * x + y * y));
                acc += n * w;
                wsum += w;
            }
        L = lerp(L, acc / wsum, saturate(sl * 1.5));
    }
    if (sc > 0.0)
    {
        float3 acc = 0;
        float wsum = 0;
        float sigma = 0.02 + 0.1 * sc;
        for (int y = -4; y <= 4; y += 2)
            for (int x = -4; x <= 4; x += 2)
            {
                float3 n = max(Input(0, ctx.px + int2(x, y)).rgb, 0.0);
                float nl = Luma(n);
                float d = (nl - Luma(c)) / (sigma * (Luma(c) + 0.05) * 4.0);
                float w = exp(-0.5 * d * d);
                acc += (n - nl) * w;
                wsum += w;
            }
        chroma = lerp(chroma, acc / wsum, saturate(sc * 1.2));
    }
    return max(L + chroma, 0.0);
}

float4 Effect(EffectContext ctx)
{
    if (ctx.pass == 0) return Blur1D(ctx, int2(1, 0));
    if (ctx.pass == 1) return Blur1D(ctx, int2(0, 1));

    float4 src = Input(0, ctx.px);
    float3 c = max(src.rgb, 0.0);
    float3 blurred = max(Prev(ctx.px).rgb, 0.0);
    if (nr_luma > 0.0 || nr_color > 0.0) c = Denoise(ctx, c);

    // Sharpening (unsharp mask, small radius) and texture (fine detail) on the original.
    if (sharpen > 0.0 || fine_detail != 0.0)
    {
        float r = radius;
        float3 n4 = 0;
        n4 += Input(0, ctx.px + int2(int(round(r)), 0)).rgb;
        n4 += Input(0, ctx.px - int2(int(round(r)), 0)).rgb;
        n4 += Input(0, ctx.px + int2(0, int(round(r)))).rgb;
        n4 += Input(0, ctx.px - int2(0, int(round(r)))).rgb;
        float3 detail = c - n4 * 0.25;
        c = max(c + detail * (sharpen / 100.0 + fine_detail / 100.0), 0.0);
    }

    // White balance: temperature along blue <-> amber, tint along green <-> magenta.
    float t = temperature / 100.0, g = tint / 100.0;
    c *= float3(1.0 + 0.35 * t, 1.0 - 0.25 * g, 1.0 - 0.35 * t) * float3(1.0 + 0.1 * g, 1.0, 1.0 + 0.1 * g);
    c *= exp2(exposure);

    // Dehaze: remove a fraction of the local veil (dark channel of the blurred image).
    if (dehaze != 0.0)
    {
        float haze = min(min(blurred.r, blurred.g), blurred.b) * exp2(exposure);
        float d = dehaze / 100.0;
        c = d > 0.0 ? max(c - haze * d * 0.8, 0.0) / max(1.0 - haze * d * 0.8, 0.2) : lerp(c, c + (0.5 - c) * 0.3, -d);
    }

    // Tones on a perceptual scale.
    float3 p = pow(max(c, 0.0), 1.0 / 2.2);
    float L = Luma(p);
    float hl = smoothstep(0.45, 1.0, L), sh = 1.0 - smoothstep(0.0, 0.55, L);
    float wh = smoothstep(0.75, 1.1, L), bk = 1.0 - smoothstep(0.0, 0.25, L);
    float delta = highlights / 100.0 * 0.35 * hl + shadows / 100.0 * 0.35 * sh + whites / 100.0 * 0.25 * wh + blacks / 100.0 * 0.2 * bk;
    p += delta;
    // Contrast: S curve around middle gray.
    float k = contrast / 100.0;
    p = lerp(p, saturate(p) * saturate(p) * (3.0 - 2.0 * saturate(p)), k > 0 ? k : 0.0);
    p = k < 0 ? lerp(p, 0.5 + (p - 0.5) * 0.6, -k) : p;

    // Tone curve (on the display-referred values; above 1 keeps the slope of the end of the curve).
    {
        float3 q = saturate(p);
        float3 over = p - q;
        p = float3(tone_eval(q.r), tone_eval(q.g), tone_eval(q.b)) + over;
    }

    // Clarity: local contrast against the blurred image (midtones only).
    if (clarity != 0.0)
    {
        float bl = Luma(pow(blurred * exp2(exposure), 1.0 / 2.2));
        float mid = 1.0 - abs(L - 0.5) * 2.0;
        p += (Luma(p) - bl) * clarity / 100.0 * 0.6 * saturate(mid + 0.3);
    }

    // Vibrance (protects saturated colors) and saturation.
    float Lp = Luma(p);
    float sat = (max(p.r, max(p.g, p.b)) - min(p.r, min(p.g, p.b))) / max(max(p.r, max(p.g, p.b)), 1e-4);
    float vib = vibrance / 100.0 * (1.0 - sat);
    p = Lp + (p - Lp) * max(1.0 + saturation / 100.0 + vib, 0.0);

    float mixAny = 0;
    mixAny = abs(mix_h_red) + abs(mix_h_orange) + abs(mix_h_yellow) + abs(mix_h_green) + abs(mix_h_aqua) + abs(mix_h_blue) + abs(mix_h_purple) + abs(mix_h_magenta)
        + abs(mix_s_red) + abs(mix_s_orange) + abs(mix_s_yellow) + abs(mix_s_green) + abs(mix_s_aqua) + abs(mix_s_blue) + abs(mix_s_purple) + abs(mix_s_magenta)
        + abs(mix_l_red) + abs(mix_l_orange) + abs(mix_l_yellow) + abs(mix_l_green) + abs(mix_l_aqua) + abs(mix_l_blue) + abs(mix_l_purple) + abs(mix_l_magenta);
    if (mixAny > 0.0) p = ColorMixer(p);
    if (cg_sh_sat > 0.0 || cg_mid_sat > 0.0 || cg_hl_sat > 0.0) p = ColorGrade(p);

    // Vignette (post crop style, centered).
    if (vignette != 0.0)
    {
        float2 q = (ctx.uv - 0.5) * 2.0;
        float r = length(q * float2(1.0, ctx.size.y / ctx.size.x)) / 1.2;
        float m = smoothstep(vig_mid / 100.0, 1.0, r);
        p = vignette < 0 ? p * (1.0 + vignette / 100.0 * m) : lerp(p, 1.0, vignette / 100.0 * m);
    }

    // Grain (deterministic noise on luminance).
    if (grain > 0.0)
    {
        float n = frac(sin(dot(float2(ctx.px), float2(12.9898, 78.233))) * 43758.5453) - 0.5;
        p += n * grain / 100.0 * 0.12;
    }

    c = pow(max(p, 0.0), 2.2);
    return float4(c, src.a);
}
