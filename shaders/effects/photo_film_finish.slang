//@name Photo Grain and Vignette
//@category Photo Essentials
//@param grain float 0.15 0 1 "Grain"
//@param vignette float 0.15 0 1 "Vignette"
//@param radius float 0.8 0.1 2 "Vignette Radius"
//@param softness float 0.5 0.01 1 "Vignette Softness"
//@param seed int 0 0 9999 "Grain Seed"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 p = LinearToSrgb(max(c.rgb, 0.0));
    float y = saturate(Luma(p));
    float noise = Hash12(float2(ctx.px) + float2(seed * 0.73, seed * 1.31)) - 0.5;
    p = max(p + noise * grain * 0.12 * (0.3 + 0.7 * 4.0 * y * (1.0 - y)), 0.0);
    float2 q = (ctx.uv - 0.5) * 2.0 * ctx.size / max(min(ctx.size.x, ctx.size.y), 1.0);
    float falloff = smoothstep(radius, radius + max(softness, 0.01), length(q));
    p *= 1.0 - saturate(vignette) * falloff;
    return float4(SrgbToLinear(p), c.a);
}
