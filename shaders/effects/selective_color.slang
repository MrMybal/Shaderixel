//@name Correction sélective
//@category Réglages
//@param target enum 0 "Rouges|Jaunes|Verts|Cyans|Bleus|Magentas|Blancs|Neutres|Noirs" "Couleurs"
//@param cyan float 0 -1 1 "Cyan"
//@param magenta float 0 -1 1 "Magenta"
//@param yellow float 0 -1 1 "Jaune"
//@param black float 0 -1 1 "Noir"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 s = LinearToSrgb(saturate(c.rgb));
    float3 hsv = RGBToHSV(s);
    float w;
    if (target < 6)
    {
        float center = target / 6.0;
        float d = abs(frac(hsv.x - center + 0.5) - 0.5) * 6.0;
        w = saturate(1.0 - d) * hsv.y;
    }
    else if (target == 6) w = smoothstep(0.5, 1.0, hsv.z) * (1.0 - hsv.y);
    else if (target == 8) w = 1.0 - smoothstep(0.0, 0.5, hsv.z);
    else w = (1.0 - hsv.y) * (1.0 - abs(hsv.z - 0.5) * 2.0);
    // Relative CMYK-style adjustments: cyan removes red, magenta removes green, yellow removes blue.
    float3 adj = -float3(cyan, magenta, yellow) - black;
    s = saturate(s + adj * w * s);
    return float4(SrgbToLinear(s), c.a);
}
