//@name Logo Chrome Fill
//@category Text and Logo Styles
//@param dark color 0.01 0.025 0.06 1 "Dark Reflection (RGB)"
//@param light color 0.85 0.95 1 1 "Light Reflection (RGB)"
//@param angle float 90 -180 180 "Reflection Angle (degrees)"
//@param bands float 3 1 12 "Reflection Bands"
//@param phase float 0 0 1 "Reflection Phase"
//@param sharpness float 2 0.25 8 "Highlight Sharpness"
//@param amount float 1 0 1 "Fill Strength"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float a = radians(angle);
    float u = dot(ctx.uv - 0.5, float2(cos(a), sin(a))) * bands + phase;
    float t = pow(0.5 + 0.5 * sin(u * 6.2831853), max(sharpness, 0.25));
    return float4(lerp(c.rgb, lerp(dark.rgb, light.rgb, t), saturate(amount)), c.a);
}
