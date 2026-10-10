//@name Scan Edge Coverage
//@category Scan and Line Art
//@halo radius 1 1
//@param radius int 1 1 8 "Edge Radius (px)"
//@param strength float 3 0 16 "Edge Strength"
//@param threshold float 0.05 0 1 "Edge Threshold"
//@param softness float 0.1 0.001 1 "Edge Softness"
float ScanLuma(int2 p)
{
    float4 c = Input(0, p);
    return Luma(lerp(float3(1, 1, 1), saturate(LinearToSrgb(max(c.rgb, 0.0))), saturate(c.a)));
}
float4 Effect(EffectContext ctx)
{
    int r = clamp(radius, 1, 8);
    int2 last = int2(ctx.size) - 1;
    float a = ScanLuma(clamp(ctx.px + int2(-r, -r), int2(0), last));
    float b = ScanLuma(clamp(ctx.px + int2(0, -r), int2(0), last));
    float c = ScanLuma(clamp(ctx.px + int2(r, -r), int2(0), last));
    float d = ScanLuma(clamp(ctx.px + int2(-r, 0), int2(0), last));
    float f = ScanLuma(clamp(ctx.px + int2(r, 0), int2(0), last));
    float g = ScanLuma(clamp(ctx.px + int2(-r, r), int2(0), last));
    float h = ScanLuma(clamp(ctx.px + int2(0, r), int2(0), last));
    float i = ScanLuma(clamp(ctx.px + int2(r, r), int2(0), last));
    float2 grad = float2(c + 2.0*f + i - a - 2.0*d - g, g + 2.0*h + i - a - 2.0*b - c) * 0.25;
    float m = smoothstep(threshold, threshold + max(softness, 0.001), length(grad) * strength);
    return float4((m * saturate(Input(0, ctx.px).a)).xxx, 1.0);
}
