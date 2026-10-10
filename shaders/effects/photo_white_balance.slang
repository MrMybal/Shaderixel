//@name Photo White Balance
//@category Photo Essentials
//@param temperature float 0 -1 1 "Temperature"
//@param tint float 0 -1 1 "Green / Magenta Tint"
//@param preserve_luminance bool 1 "Preserve Luminance"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 gain = exp2(float3(0.5 * temperature + 0.15 * tint, -0.3 * tint, -0.5 * temperature + 0.15 * tint));
    float3 rgb = max(c.rgb, 0.0) * gain;
    if (preserve_luminance != 0 && Luma(rgb) > 0.000001)
        rgb *= Luma(max(c.rgb, 0.0)) / Luma(rgb);
    return float4(rgb, c.a);
}
