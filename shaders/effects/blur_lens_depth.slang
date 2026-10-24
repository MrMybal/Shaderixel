//@name Flou de l'objectif (profondeur)
//@category Flou
//@halo maxBlur 1 2
//@premultiplied
//@input Image
//@input Profondeur
//@param focal float 0.2 0 1 "Distance de mise au point"
//@param depthOfField float 0.08 0 1 "Profondeur de champ"
//@param maxBlur float 20 0 96 "Flou maximal (px)"
//@param invert bool 0 "Inverser la profondeur (blanc = proche)"
//@param highlights float 0.8 0 4 "Éclat du bokeh"
//@param threshold float 0.75 0 1 "Seuil des hautes lumières"
//@param blades int 6 0 9 "Lamelles (0 : disque)"
//@param bladeRotation float 15 0 180 "Rotation des lamelles (°)"
//@param samples int 96 16 256 "Qualité (échantillons)"
// Photoshop's Lens Blur: the blur of each pixel grows with its distance from the focal plane, read from the
// Depth input (black = near, white = far; without it the image's luminance). Samples farther from the focus
// than the pixel itself spread over it (scatter approximated in a gather), so sharp subjects keep clean edges.

float Depth(float2 uv)
{
    float z = HasInput(1) ? Luma(Unpremultiply(InputUV(1, uv)).rgb) : Luma(Unpremultiply(InputUV(0, uv)).rgb);
    return invert ? 1.0 - z : z;
}

float CoC(float z) { return maxBlur * saturate((abs(z - focal) - depthOfField * 0.5) / max(0.5 - depthOfField * 0.5, 0.02)); }

float2 Aperture(int i, int n)
{
    float r = sqrt((i + 0.5) / n);
    float a = i * 2.39996323;
    if (blades >= 3)
    {
        const float seg = 6.28318530 / blades;
        const float k = fmod(a + bladeRotation * 0.0174532925 + 64.0 * seg, seg) - seg * 0.5;
        r *= cos(seg * 0.5) / cos(k);
    }
    return float2(cos(a), sin(a)) * r;
}

float4 Effect(EffectContext ctx)
{
    const float zp = Depth(ctx.uv);
    const float coc = CoC(zp);
    // Search radius: the largest blur around (a blurred background spreads over the edge of a sharp subject).
    const float reach = maxBlur;
    if (reach < 0.5) return Input(0, ctx.px);
    const int n = max(samples, 16);
    float4 sum = 0;
    float wsum = 0;
    for (int i = 0; i < n; ++i)
    {
        const float2 o = Aperture(i, n) * reach;
        const float2 uv = ctx.uv + o * ctx.texel;
        const float zs = Depth(uv);
        const float cs = CoC(zs);
        const float dist = length(o);
        // A sample reaches this pixel when its own blur covers the distance, unless it lies behind the pixel and
        // outside the pixel's own blur (a blurred background never covers a sharp subject in front of it).
        float w = saturate(cs - dist + 1.0);
        if (zs > zp + 0.01 && dist > coc) w = 0;
        if (w <= 0) continue;
        w /= max(cs * cs, 1.0);  // a wide disc spreads its light thinner
        const float4 c = InputUV(0, uv);
        w *= 1.0 + highlights * 8.0 * max(Luma(Unpremultiply(c).rgb) - threshold, 0.0);
        sum += c * w;
        wsum += w;
    }
    return wsum > 0 ? sum / wsum : Input(0, ctx.px);
}
