//@name Print Paper
//@category Print and Halftone
//@generator
//@param color color 0.92 0.86 0.72 1 "Paper Color"
//@param grain float 0.035 0 0.3 "Fine Grain"
//@param mottling float 0.04 0 0.3 "Mottling"
//@param scale float 64 4 512 "Mottling Scale (px)"
//@param fibers float 0.015 0 0.2 "Fiber Strength"
//@param seed int 1 0 65535 "Seed"
float4 Effect(EffectContext ctx)
{
    float2 p = float2(ctx.px) + float2(seed * 17.0, seed * 31.0);
    float2 q = p / max(scale, 4.0), base = floor(q), f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float broad = lerp(lerp(Hash12(base), Hash12(base + float2(1, 0)), f.x),
                       lerp(Hash12(base + float2(0, 1)), Hash12(base + 1.0), f.x), f.y);
    float fiber = Hash12(floor(p / float2(24.0, 1.0)));
    float variation = (Hash12(p) - 0.5) * grain + (broad - 0.5) * mottling + (fiber - 0.5) * fibers;
    float3 rgb = SrgbToLinear(saturate(LinearToSrgb(max(color.rgb, 0.0)) + variation));
    return float4(rgb, saturate(color.a));
}
