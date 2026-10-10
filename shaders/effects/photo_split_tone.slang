//@name Photo Split Tone
//@category Photo Essentials
//@param shadow_color color 0.12 0.22 0.42 1 "Shadow Tint"
//@param highlight_color color 0.65 0.42 0.2 1 "Highlight Tint"
//@param amount float 0.2 0 1 "Tint Strength"
//@param balance float 0.5 0.05 0.95 "Tone Balance"
//@param preserve_luminance bool 1 "Preserve Luminance"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = LinearToSrgb1(Luma(max(c.rgb, 0.0)));
    float w = smoothstep(balance - 0.25, balance + 0.25, y);
    float3 shade = lerp(shadow_color.rgb / max(Luma(shadow_color.rgb), 0.0001),
                        highlight_color.rgb / max(Luma(highlight_color.rgb), 0.0001), w);
    float alpha = lerp(shadow_color.a, highlight_color.a, w);
    float3 rgb = max(c.rgb, 0.0) * lerp(1.0, shade, saturate(amount * alpha));
    if (preserve_luminance != 0 && Luma(rgb) > 0.000001)
        rgb *= Luma(max(c.rgb, 0.0)) / Luma(rgb);
    return float4(rgb, c.a);
}
