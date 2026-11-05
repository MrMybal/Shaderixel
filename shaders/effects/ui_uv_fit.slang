//@name UI UV Image Fit
//@category UI UV/Coordinates
//@generator
//@param position float2 0.5 0.5 -4 4 "Widget position (document UV)"
//@param anchor float2 0.5 0.5 0 1 "Widget anchor"
//@param target_size float2 0 0 0 32768 "Widget size (px; 0 = canvas)"
//@param softness float 1 0 16 "Edge softness (px)"
//@param source_size float2 256 256 1 32768 "Source aspect size (px)"
//@param fit enum 0 "Contain|Cover|Stretch" "Image fit"
//@param alignment float2 0.5 0.5 0 1 "Image alignment"

// Aspect-correct contain, cover or stretch inside an independently positioned widget.
float4 Effect(EffectContext ctx) {

float2 size = max(float2(target_size.x > 0 ? target_size.x : ctx.size.x, target_size.y > 0 ? target_size.y : ctx.size.y), 0.001);
float2 p = (ctx.uv - position) * ctx.size + anchor * size;
float2 q = p / size;
float edge = min(min(p.x, p.y), min(size.x - p.x, size.y - p.y));
float coverage = softness > 0 ? saturate(edge / softness + 0.5) : (edge >= 0 ? 1.0 : 0.0);

if (fit < 1.5) {
    float2 ratio = size / max(source_size, 0.001);
    float scale = fit < 0.5 ? min(ratio.x, ratio.y) : max(ratio.x, ratio.y);
    float2 imageSize = max(source_size * scale, 0.001);
    q = (p - (size - imageSize) * alignment) / imageSize;
    float2 imageEdge = min(q, 1 - q) * imageSize;
    float d = min(imageEdge.x, imageEdge.y);
    coverage *= softness > 0 ? saturate(d / softness + 0.5) : (d >= 0 ? 1.0 : 0.0);
}
return float4(q, coverage, 1);
}
