//@name Data Map Adjustment
//@category PBR Textures
//@input Map
//@param canal enum 0 "Red|Green|Blue|Alpha|Luminance" "Source Channel"
//@param noir float 0 0 1 "Black Point"
//@param blanc float 1 0 1 "White Point"
//@param gamma float 1 0.1 4 "Curve"
//@param inverser bool 0 "Invert"
//@param minimum float 0 0 1 "Minimum Value"
//@param maximum float 1 0 1 "Maximum Value"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 data = LinearToSrgb(max(c.rgb, 0.0));
    float v = canal < 0.5 ? data.r : canal < 1.5 ? data.g : canal < 2.5 ? data.b : canal < 3.5 ? c.a : Luma(data);
    float span = blanc - noir;
    span = abs(span) < 0.0001 ? (span < 0.0 ? -0.0001 : 0.0001) : span;
    v = pow(saturate((v - noir) / span), max(gamma, 0.1));
    v = inverser > 0.5 ? 1.0 - v : v;
    v = saturate(lerp(minimum, maximum, v));
    return float4(SrgbToLinear(v.xxx), 1.0);
}
