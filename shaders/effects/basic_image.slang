//@name Réglages de l'image
//@category Réglages
//@param exposure float 0 -5 5 "Exposition (IL)"
//@param brightness float 0 -1 1 "Luminosité"
//@param contrast float 0 -1 1 "Contraste"
//@param gamma float 1 0.2 5 "Gamma"
//@param saturation float 0 -1 1 "Saturation"
//@param vibrance float 0 -1 1 "Vibrance"
//@param hue float 0 -180 180 "Teinte (°)"
//@param temperature float 0 -1 1 "Température"
//@param tint float 0 -1 1 "Nuance"
// Base image adjustments of an image layer (Propriétés > Réglages de l'image), in one pass.
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 l = c.rgb * exp2(exposure);
    // White balance (linear): warm / cool, green / magenta.
    l *= float3(1.0 + temperature * 0.25, 1.0 - tint * 0.15, 1.0 - temperature * 0.25) * float3(1.0 + tint * 0.08, 1.0, 1.0 + tint * 0.08);
    float3 s = LinearToSrgb(max(l, 0.0));
    s += brightness;
    float k = contrast >= 0.0 ? 1.0 / max(1.0 - contrast, 0.01) : 1.0 + contrast;
    s = (s - 0.5) * k + 0.5;
    s = pow(max(s, 0.0), 1.0 / max(gamma, 0.01));
    // Saturation and vibrance (vibrance protects colors already saturated).
    float y = Luma(s);
    float sat = max(s.r, max(s.g, s.b)) - min(s.r, min(s.g, s.b));
    s = lerp(float3(y, y, y), s, 1.0 + saturation + vibrance * (1.0 - saturate(sat)));
    if (abs(hue) > 0.01)
    {
        float3 hsv = RGBToHSV(saturate(s));
        hsv.x = frac(hsv.x + hue / 360.0);
        s = HSVToRGB(hsv) + (s - saturate(s));
    }
    return float4(SrgbToLinear(max(s, 0.0)), c.a);
}
