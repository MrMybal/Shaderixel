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

float4 Effect(EffectContext ctx)
{
    if (ctx.pass == 0) return Blur1D(ctx, int2(1, 0));
    if (ctx.pass == 1) return Blur1D(ctx, int2(0, 1));

    float4 src = Input(0, ctx.px);
    float3 c = max(src.rgb, 0.0);
    float3 blurred = max(Prev(ctx.px).rgb, 0.0);

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
