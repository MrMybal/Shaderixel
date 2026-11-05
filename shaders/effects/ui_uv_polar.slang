//@name UI UV Ring and Arc
//@category UI UV/Coordinates
//@generator
//@param center float2 0.5 0.5 -4 4 "Ring center (document UV)"
//@param outer_radius float 128 0.001 32768 "Outer radius (px)"
//@param inner_ratio float 0.65 0 0.9999 "Inner / outer radius"
//@param start_angle float -90 -3600 3600 "Start angle (degrees)"
//@param span float 360 0.001 360 "Arc span (degrees)"
//@param clockwise bool 1 "Clockwise"
//@param softness float 1 0 16 "Edge softness (px)"
//@param phase float 0 -1000 1000 "Angular UV offset"

// Maps a texture strip around an aspect-correct ring or arc; U follows angle and V follows radius.
float4 Effect(EffectContext ctx) {
float2 p=(ctx.uv-center)*ctx.size; float radius=length(p);
float angle=atan2(p.y,p.x)-radians(start_angle);
if (clockwise<0.5) angle=-angle;
float turn=frac(angle/6.28318530718);
float arc=max(span/360.0,0.000001), inner=outer_radius*inner_ratio;
float d=min(radius-inner,outer_radius-radius);
float coverage=softness>0?saturate(d/softness+0.5):(d>=0?1.0:0.0);
if (span<359.999) {
    float angularEdge=min(turn,arc-turn)*6.28318530718*max(radius,1);
    coverage*=softness>0?saturate(angularEdge/softness+0.5):(turn<=arc?1.0:0.0);
}
return float4(turn/arc+phase,(radius-inner)/max(outer_radius-inner,.0001),coverage,1);
}
