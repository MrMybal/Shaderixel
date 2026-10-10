//@name Estimated Roughness from Photo
//@category PBR Textures
//@input Photo
//@input Blurred Photo
//@param base float 0.6 0 1 "Base Roughness"
//@param details float 1.5 0 8 "Detail Weight"
//@param tonalite float 0.15 -1 1 "Dark Tone Weight"
//@param minimum float 0.04 0 1 "Minimum Roughness"
//@param maximum float 1 0 1 "Maximum Roughness"
//@param inverser bool 0 "Output Glossiness"
// Physical roughness cannot be measured from this photo alone.
float4 Effect(EffectContext ctx)
{
    float y = Luma(LinearToSrgb(max(Input(0, ctx.px).rgb, 0.0)));
    float b = HasInput(1) ? Luma(LinearToSrgb(max(Input(1, ctx.px).rgb, 0.0))) : y;
    float r = clamp(base + abs(y - b) * details + (0.5 - b) * tonalite,
                    min(minimum, maximum), max(minimum, maximum));
    r = inverser > 0.5 ? 1.0 - r : r;
    return float4(SrgbToLinear(saturate(r).xxx), 1.0);
}
