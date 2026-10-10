//@name Edge Color Despill
//@category Masks and Compositing
//@input Image
//@input Mask
//@param key_color color 0 1 0 1 "Spill Color"
//@param amount float 0.6 0 1 "Despill Strength"
//@param edge_only bool 1 "Only Semi-Transparent Edges"
//@param preserve_luminance bool 1 "Preserve Luminance"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 rgb = max(c.rgb, 0.0);
    float3 key = max(key_color.rgb, 0.0);
    float biggest = max(key.r, max(key.g, key.b));
    if (biggest < 0.0001) return c;
    key /= biggest;
    float coverage = HasInput(1) ? saturate(Input(1, ctx.px).r) : c.a;
    float edge = edge_only != 0 ? 1.0 - smoothstep(0.8, 1.0, coverage) : 1.0;
    float dominant = key.r >= key.g && key.r >= key.b ? rgb.r - max(rgb.g, rgb.b)
                   : key.g >= key.b ? rgb.g - max(rgb.r, rgb.b) : rgb.b - max(rgb.r, rgb.g);
    float3 cleaned = max(rgb - key * max(dominant, 0.0) * saturate(amount) * edge, 0.0);
    if (preserve_luminance != 0 && Luma(cleaned) > 0.000001)
        cleaned *= min(4.0, Luma(rgb) / Luma(cleaned));
    return float4(cleaned, c.a);
}
