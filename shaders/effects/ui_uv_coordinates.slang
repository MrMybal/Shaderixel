//@name UI UV Widget Coordinates
//@category UI UV/Coordinates
//@generator
//@param position float2 0.5 0.5 -4 4 "Widget position (document UV)"
//@param anchor float2 0.5 0.5 0 1 "Widget anchor"
//@param target_size float2 0 0 0 32768 "Widget size (px; 0 = canvas)"
//@param softness float 1 0 16 "Edge softness (px)"

// Coordinate field: R=U, G=V, B=coverage, A=1. Zero size selects the document dimension.
float4 Effect(EffectContext ctx) {

float2 size = max(float2(target_size.x > 0 ? target_size.x : ctx.size.x, target_size.y > 0 ? target_size.y : ctx.size.y), 0.001);
float2 p = (ctx.uv - position) * ctx.size + anchor * size;
float2 q = p / size;
float edge = min(min(p.x, p.y), min(size.x - p.x, size.y - p.y));
float coverage = softness > 0 ? saturate(edge / softness + 0.5) : (edge >= 0 ? 1.0 : 0.0);
return float4(q, coverage, 1);
}
