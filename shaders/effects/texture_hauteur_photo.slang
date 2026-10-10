//@name Estimated Height from Photo
//@category PBR Textures
//@input Photo
//@input Blurred Lighting
//@param detail float 1 0 8 "Detail Height"
//@param forme float 0.25 0 2 "Large-Scale Shapes"
//@param centre float 0.5 0 1 "Midpoint Height"
//@param gamma float 1 0.1 4 "Height Curve"
//@param inverser bool 0 "Invert Height"
// SrgbToLinear encodes data to compensate for the engine PNG encoding.
// All new map effects decode these values with LinearToSrgb.
float4 Effect(EffectContext ctx)
{
    float y = Luma(LinearToSrgb(max(Input(0, ctx.px).rgb, 0.0)));
    float b = HasInput(1) ? Luma(LinearToSrgb(max(Input(1, ctx.px).rgb, 0.0))) : y;
    float h = saturate(centre + (y - b) * detail + (b - 0.5) * forme);
    h = pow(h, max(gamma, 0.1));
    h = inverser > 0.5 ? 1.0 - h : h;
    return float4(SrgbToLinear(h.xxx), 1.0);
}
