// Viewport/thumbnail presentation: zoom/pan, checkerboard, linear -> sRGB.
#include "common.slang"

cbuffer View : register(b1)
{
    float2 gOrigin;     // document pixel at view pixel (0,0)
    float gInvZoom;     // document pixels per view pixel
    uint gFlags;        // 1 = has image, 2 = nearest when magnified
    uint2 gViewSize;
    uint2 gDocSize;
    float4 gOutside;    // sRGB color outside the document
    float gChecker;     // checker square size in view pixels
    float gExposure;    // stops
    float gTime;        // seconds (marching ants)
    uint gSelFlags;     // 1 = selection, 2 = quick mask overlay
    uint gLutSize;      // color management / soft proofing LUT (0 = none)
    uint gGamut;        // 1 = gray out colors outside the proofing gamut
    uint gScope;        // analysis overlay: 1 zebras, 2 clipping, 3 false color, 4 focus peaking, 5 luminance
    float gScopeLevel;  // zebras: luminance threshold; focus peaking: edge threshold
    float4 gOv0;        // overflow (transform tool): document pixel -> layer contents UV
    float4 gOv1;
    float4 gOv2;
    uint gOvFlags;      // 1 = show the layer contents outside the document
    uint gInpaint;      // 1 = AI inpainting mask tint
    uint gCompare;      // before / after: 1 split (snapshot left of gSplit), 2 snapshot only, 3 difference
    float gSplit;       // split position (view pixels)
};

Texture2D<float4> gImage : register(t0);
Texture2D<float> gSel : register(t1);
Texture2D<float4> gLut : register(t2);  // n*n x n: x = b * n + r, y = g; alpha 1 = in gamut
Texture2D<float4> gOverflow : register(t3);  // contents of the layer being transformed (premultiplied linear)
Texture2D<float4> gInpaintMask : register(t4);  // AI inpainting mask (r: 1 = to regenerate)
Texture2D<float4> gSnap : register(t5);  // snapshot of the image (before / after), possibly reduced

float4 LutAt(float3 s)
{
    float n = float(gLutSize);
    float3 p = saturate(s) * (n - 1.0);
    float b0 = floor(p.z), b1 = min(b0 + 1.0, n - 1.0), fb = p.z - b0;
    float2 size = float2(n * n, n);
    float2 uv0 = (float2(b0 * n + p.x, p.y) + 0.5) / size;
    float2 uv1 = (float2(b1 * n + p.x, p.y) + 0.5) / size;
    return lerp(gLut.SampleLevel(sLinearClamp, uv0, 0), gLut.SampleLevel(sLinearClamp, uv1, 0), fb);
}
RWTexture2D<float4> gOut : register(u0);

float LumaS(float3 s) { return dot(s, float3(0.2126, 0.7152, 0.0722)); }

// Displayed (sRGB) luminance of a document pixel, for focus peaking.
// t: texel of the displayed image (reduced when zoomed out, so not always document pixels).
float LumaAt(float2 t)
{
    uint w, h;
    gImage.GetDimensions(w, h);
    float4 c = gImage.Load(int3(clamp(int2(floor(t)), int2(0, 0), int2(w, h) - 1), 0));
    return LumaS(LinearToSrgb(saturate(c.rgb)));
}

// Analysis overlays on the displayed image (document values, before the monitor profile).
float3 ScopeOverlay(float3 s, float3 shown, float alpha, float2 d, float2 v)
{
    float l = LumaS(s);
    if (gScope == 1)
    {
        // Zebras: diagonal stripes over the luminances above the threshold.
        if (l >= gScopeLevel && alpha > 0.01)
            return frac((v.x + v.y - gTime * 24.0) / 10.0) < 0.5 ? float3(0.05, 0.05, 0.05) : float3(0.95, 0.95, 0.95);
        return shown;
    }
    if (gScope == 2)
    {
        // Clipping: a channel at 0 (blue) or at 1 (red), both (magenta).
        bool low = alpha > 0.01 && any(s <= 0.5 / 255.0);
        bool high = alpha > 0.01 && any(s >= 254.5 / 255.0);
        if (low && high) return float3(1.0, 0.2, 1.0);
        if (high) return float3(1.0, 0.15, 0.1);
        if (low) return float3(0.1, 0.35, 1.0);
        return lerp(float3(l, l, l), shown, 0.35);
    }
    if (gScope == 3)
    {
        // False color (exposure bands); the rest in gray.
        if (alpha <= 0.01) return shown;
        if (l < 0.02) return float3(0.47, 0.12, 0.63);
        if (l < 0.10) return float3(0.12, 0.31, 0.9);
        if (l < 0.20) return float3(0.16, 0.63, 0.75);
        if (l >= 0.38 && l < 0.45) return float3(0.24, 0.75, 0.27);
        if (l >= 0.52 && l < 0.58) return float3(0.92, 0.55, 0.67);
        if (l >= 0.97) return float3(0.9, 0.16, 0.16);
        if (l >= 0.90) return float3(0.94, 0.86, 0.16);
        float g = l * 0.78 + 0.08;
        return float3(g, g, g);
    }
    if (gScope == 4)
    {
        // Focus peaking: strong luminance gradients (Sobel at the document resolution) in red over a dimmed image.
        uint iw, ih;
        gImage.GetDimensions(iw, ih);
        const float2 t = d * float2(iw, ih) / float2(gDocSize);  // the image may be reduced (zoomed out)
        float a = LumaAt(t + float2(-1, -1)), b = LumaAt(t + float2(0, -1)), c = LumaAt(t + float2(1, -1));
        float e = LumaAt(t + float2(-1, 0)), f = LumaAt(t + float2(1, 0));
        float g = LumaAt(t + float2(-1, 1)), h = LumaAt(t + float2(0, 1)), i = LumaAt(t + float2(1, 1));
        float gx = (c + 2.0 * f + i) - (a + 2.0 * e + g), gy = (g + 2.0 * h + i) - (a + 2.0 * b + c);
        float m = length(float2(gx, gy));
        float3 dim = lerp(float3(l, l, l), shown, 0.5) * 0.75;
        return m > gScopeLevel ? float3(1.0, 0.1, 0.1) : dim;
    }
    if (gScope == 5) return float3(l, l, l);
    return shown;
}

float SelAt(float2 d)
{
    int2 p = clamp(int2(floor(d)), int2(0, 0), int2(gDocSize) - 1);
    return gSel.Load(int3(p, 0));
}

float4 Fetch(float2 docPos)
{
    float2 uv = docPos / float2(gDocSize);
    if (gInvZoom <= 0.5 && (gFlags & 2))
        return gImage.Load(int3(int2(floor(docPos)), 0));
    return gImage.SampleLevel(sLinearClamp, uv, 0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gViewSize)) return;
    float2 v = float2(id.xy) + 0.5;
    float2 d = gOrigin + v * gInvZoom;
    if (any(d < 0.0) || any(d >= float2(gDocSize)))
    {
        float4 o = gOutside;
        if (gOvFlags & 1)
        {
            // What the transformed layer covers outside the document, dimmed (it is not in the image).
            float3 h = float3(dot(gOv0.xyz, float3(d, 1)), dot(gOv1.xyz, float3(d, 1)), dot(gOv2.xyz, float3(d, 1)));
            float2 q = h.xy / max(h.z, 1e-6);
            if (h.z > 1e-6 && all(q >= 0.0) && all(q <= 1.0))
            {
                float4 c = gOverflow.SampleLevel(sLinearClamp, q, 0);
                float k = 0.55;
                float3 rgb = c.rgb * k + SrgbToLinear(o.rgb) * (1.0 - c.a * k);
                o = float4(LinearToSrgb(rgb), 1.0);
            }
        }
        gOut[id.xy] = o;
        return;
    }

    float4 c = float4(0, 0, 0, 0);
    if (gFlags & 1)
    {
        // Minification: box-filter n x n taps over the footprint of the view pixel.
        int n = clamp(int(ceil(gInvZoom)), 1, 4);
        if (n == 1)
            c = Fetch(d);
        else
        {
            float step = gInvZoom / n;
            float2 start = gOrigin + (v - 0.5) * gInvZoom + step * 0.5;
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                    c += gImage.SampleLevel(sLinearClamp, (start + float2(x, y) * step) / float2(gDocSize), 0);
            c /= float(n * n);
        }
    }
    if (gCompare != 0)
    {
        float4 s = gSnap.SampleLevel(sLinearClamp, d / float2(gDocSize), 0);
        if ((gCompare == 1 && v.x < gSplit) || gCompare == 2) c = s;
        else if (gCompare == 3)
        {
            // Difference of the displayed colors, amplified.
            float3 a = LinearToSrgb(saturate(c.rgb + (1.0 - saturate(c.a)))), b = LinearToSrgb(saturate(s.rgb + (1.0 - saturate(s.a))));
            c = float4(SrgbToLinear(saturate(abs(a - b) * 4.0)), 1.0);
        }
    }
    c.rgb *= exp2(gExposure);

    int2 cell = int2(floor(v / max(gChecker, 1.0)));
    float checker = ((cell.x + cell.y) & 1) ? 0.80 : 1.0;
    float3 back = SrgbToLinear(float3(checker, checker, checker));
    float3 rgb = c.rgb + back * (1.0 - saturate(c.a));
    float3 srgb = LinearToSrgb(rgb);
    const float3 docSrgb = srgb;
    if (gLutSize >= 2)
    {
        // Document profile -> (proofing profile) -> monitor.
        float4 m = LutAt(srgb);
        srgb = m.rgb;
        if ((gGamut & 1) && m.a < 0.5 && c.a > 0.01) srgb = float3(0.5, 0.5, 0.5);
    }
    if (gInpaint)
    {
        int2 ip = clamp(int2(floor(d)), int2(0, 0), int2(gDocSize) - 1);
        float m = saturate(gInpaintMask.Load(int3(ip, 0)).r);
        srgb = lerp(srgb, float3(1.0, 0.25, 0.85), m * 0.45);
    }
    if (gSelFlags & 1)
    {
        float s0 = SelAt(d);
        if (gSelFlags & 2) srgb = lerp(srgb, float3(1.0, 0.15, 0.2), (1.0 - s0) * 0.5);
        // Marching ants on the 50% iso-line, one view pixel wide.
        bool in0 = s0 >= 0.5;
        bool edge = (SelAt(d + float2(gInvZoom, 0)) >= 0.5) != in0 || (SelAt(d + float2(0, gInvZoom)) >= 0.5) != in0 ||
                    (SelAt(d - float2(gInvZoom, 0)) >= 0.5) != in0 || (SelAt(d - float2(0, gInvZoom)) >= 0.5) != in0;
        if (edge && in0)
        {
            float phase = frac((v.x + v.y) / 8.0 - gTime * 1.5);
            srgb = phase < 0.5 ? float3(0, 0, 0) : float3(1, 1, 1);
        }
    }
    if (gScope != 0) srgb = ScopeOverlay(saturate(docSrgb), srgb, c.a, d, v);
    gOut[id.xy] = float4(srgb, 1.0);
}
