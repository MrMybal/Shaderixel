//@name UI UV Unwrap Ring
//@category UI UV/Coordinates
//@input UV Map
//@param center float2 0.5 0.5 -4 4 "Source ring center (UV)"
//@param outer_radius float 128 0.001 32768 "Source outer radius (px)"
//@param inner_ratio float 0.65 0 1 "Source inner / outer radius"
//@param start_angle float -90 -3600 3600 "Start angle (degrees)"
//@param span float 360 0.001 360 "Arc span (degrees)"
//@param clockwise bool 1 "Clockwise"

// Inverse polar map: samples a source ring into a rectangular strip using document-pixel radii.
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float angle=radians(start_angle+(clockwise>0.5?1:-1)*m.r*span);
float radius=outer_radius*lerp(inner_ratio,1,m.g);
return float4(center+float2(cos(angle),sin(angle))*radius/ctx.size,m.b,1);
}
