// Photoshop-compatible blend modes (separable + non-separable), operating on
// STRAIGHT colors. Indices match sx::BlendMode in src/doc/Document.h.
// Math ported from CyTexture (CyTextureLayer.usf).

float BlendLum(float3 c) { return dot(c, float3(0.3, 0.59, 0.11)); }
float BlendSat(float3 c) { return max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b)); }

float3 ClipBlendColor(float3 c)
{
    float l = BlendLum(c);
    float n = min(c.r, min(c.g, c.b));
    float x = max(c.r, max(c.g, c.b));
    if (n < 0.0) c = l + ((c - l) * l) / max(l - n, 1e-5);
    if (x > 1.0) c = l + ((c - l) * (1.0 - l)) / max(x - l, 1e-5);
    return c;
}

float3 SetBlendLum(float3 c, float l) { return ClipBlendColor(c + (l - BlendLum(c))); }

float3 SetBlendSat(float3 c, float s)
{
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    return mx > mn ? (c - mn) * s / (mx - mn) : float3(0, 0, 0);
}

float SoftLight1(float b, float f)
{
    return f <= 0.5 ? b - (1.0 - 2.0 * f) * b * (1.0 - b)
                    : b + (2.0 * f - 1.0) * ((b <= 0.25 ? ((16.0 * b - 12.0) * b + 4.0) * b : sqrt(max(b, 0.0))) - b);
}

float3 ColorDodge(float3 b, float3 f) { return select(b <= 0.0, 0.0, min(1.0, b / max(1.0 - f, 1e-5))); }
float3 ColorBurn(float3 b, float3 f) { return select(b >= 1.0, 1.0, 1.0 - min(1.0, (1.0 - b) / max(f, 1e-5))); }
float3 VividLight(float3 b, float3 f) { return lerp(ColorBurn(b, 2.0 * f), ColorDodge(b, 2.0 * (f - 0.5)), step(0.5, f)); }

// b = backdrop, f = source (straight colors)
float3 BlendColor(float3 b, float3 f, uint mode)
{
    switch (mode)
    {
    case 1: return b * f;                                                       // Multiply
    case 2: return b + f - b * f;                                               // Screen
    case 3: return lerp(2.0 * b * f, 1.0 - 2.0 * (1.0 - b) * (1.0 - f), step(0.5, b)); // Overlay
    case 4: return b + f;                                                       // Linear Dodge (Add)
    case 5: return b - f;                                                       // Subtract
    case 6: return min(b, f);                                                   // Darken
    case 7: return max(b, f);                                                   // Lighten
    case 8: return abs(b - f);                                                  // Difference
    case 9: return ColorDodge(b, f);                                            // Color Dodge
    case 10: return ColorBurn(b, f);                                            // Color Burn
    case 11: return lerp(2.0 * b * f, 1.0 - 2.0 * (1.0 - b) * (1.0 - f), step(0.5, f)); // Hard Light
    case 12: return float3(SoftLight1(b.r, f.r), SoftLight1(b.g, f.g), SoftLight1(b.b, f.b)); // Soft Light
    case 13: return f;                                                          // Dissolve (alpha handled by caller)
    case 14: return b + f - 1.0;                                                // Linear Burn
    case 15: return VividLight(b, f);                                           // Vivid Light
    case 16: return b + 2.0 * f - 1.0;                                          // Linear Light
    case 17: return lerp(min(b, 2.0 * f), max(b, 2.0 * (f - 0.5)), step(0.5, f)); // Pin Light
    case 18: return step(1.0, b + f);                                           // Hard Mix
    case 19: return b + f - 2.0 * b * f;                                        // Exclusion
    case 20: return b / max(f, 1e-5);                                           // Divide
    case 21: return SetBlendLum(SetBlendSat(f, BlendSat(b)), BlendLum(b));      // Hue
    case 22: return SetBlendLum(SetBlendSat(b, BlendSat(f)), BlendLum(b));      // Saturation
    case 23: return SetBlendLum(f, BlendLum(b));                                // Color
    case 24: return SetBlendLum(b, BlendLum(f));                                // Luminosity
    case 25: return BlendLum(f) < BlendLum(b) ? f : b;                          // Darker Color
    case 26: return BlendLum(f) > BlendLum(b) ? f : b;                          // Lighter Color
    default: return f;                                                          // Normal
    }
}
