//@name Texture Detail and High Pass
//@category Texture Processing
//@input Texture
//@input Blurred Texture
//@param force float 0.5 -2 4 "Detail Strength"
//@param seuil float 0.01 0 0.25 "Detail Threshold"
//@param mode enum 0 "Sharpen|Gray High Pass" "Output"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 rgb = LinearToSrgb(max(c.rgb, 0.0));
    float3 b = HasInput(1) ? LinearToSrgb(max(Input(1, ctx.px).rgb, 0.0)) : rgb;
    float3 d = rgb - b;
    d *= smoothstep(seuil, seuil + 0.01, max(abs(d.r), max(abs(d.g), abs(d.b))));
    float3 result = mode < 0.5 ? max(rgb + d * force, 0.0) : saturate(0.5 + d * force);
    return float4(SrgbToLinear(result), c.a);
}
