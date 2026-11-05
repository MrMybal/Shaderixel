//@name UI UV Address Modes
//@category UI UV/Coordinates
//@input UV Map
//@param mode_x enum 1 "Clamp|Repeat|Mirror|Transparent" "U addressing"
//@param mode_y enum 1 "Clamp|Repeat|Mirror|Transparent" "V addressing"

// Independent clamp, repeat, mirrored repeat and transparent addressing; handles negative coordinates.
float Address(float p, float mode) {
    if (mode < 0.5 || mode > 2.5) return saturate(p);
    if (mode < 1.5) return frac(p);
    return 1 - abs(frac(p * 0.5) * 2 - 1);
}
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float coverage = m.b;
if (mode_x > 2.5 && (m.r < 0 || m.r > 1)) coverage = 0;
if (mode_y > 2.5 && (m.g < 0 || m.g > 1)) coverage = 0;
return float4(Address(m.r, mode_x), Address(m.g, mode_y), coverage, 1);
}
