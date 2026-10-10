//@name Channel to Coverage Mask
//@category Masks and Compositing
//@param channel enum 3 "Red|Green|Blue|Alpha|Luminance" "Source Channel"
//@param decode_srgb bool 0 "Decode PNG Numeric Values"
//@param invert bool 0 "Invert Mask"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 rgb = decode_srgb != 0 ? LinearToSrgb(max(c.rgb, 0.0)) : c.rgb;
    float v = channel == 0 ? rgb.r : channel == 1 ? rgb.g : channel == 2 ? rgb.b : channel == 3 ? c.a : Luma(rgb);
    v = saturate(v);
    v = invert != 0 ? 1.0 - v : v;
    return float4(v.xxx, 1.0);
}
