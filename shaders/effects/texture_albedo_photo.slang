//@name Albedo from Photo
//@category PBR Textures
//@input Photo
//@input Blurred Lighting
//@param correction float 0.8 0 1 "Lighting Correction"
//@param cible float 0.35 0.01 1 "Target Luminance (Linear)"
//@param limite float 4 1 16 "Maximum Gain"
//@param saturation float 1 0 2 "Saturation"
// Approximation: low-frequency photo content also includes material color.
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = HasInput(1) ? Luma(Input(1, ctx.px).rgb) : Luma(c.rgb);
    float gain = clamp(cible / max(y, 0.001), 1.0 / max(limite, 1.0), max(limite, 1.0));
    float3 rgb = max(c.rgb * lerp(1.0, gain, saturate(correction)), 0.0);
    return float4(max(lerp(Luma(rgb).xxx, rgb, saturation), 0.0), c.a);
}
