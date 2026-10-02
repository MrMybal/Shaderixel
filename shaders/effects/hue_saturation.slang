//@name Teinte / Saturation
//@category Réglages
//@param hue float 0 -180 180 "Teinte"
//@param saturation float 0 -100 100 "Saturation"
//@param lightness float 0 -100 100 "Luminosité"
//@param colorize bool 0 "Redéfinir"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 hsv = RGBToHSV(LinearToSrgb(c.rgb));
    if (colorize != 0)
    {
        hsv.x = frac((hue + 180.0) / 360.0);
        hsv.y = saturate(0.25 + saturation / 100.0);
    }
    else
    {
        hsv.x = frac(hsv.x + hue / 360.0 + 1.0);
        hsv.y = saturate(hsv.y * (1.0 + saturation / 100.0));
    }
    float3 rgb = HSVToRGB(hsv);
    float l = lightness / 100.0;
    rgb = l >= 0.0 ? lerp(rgb, 1.0, l) : rgb * (1.0 + l);
    return float4(SrgbToLinear(rgb), c.a);
}
