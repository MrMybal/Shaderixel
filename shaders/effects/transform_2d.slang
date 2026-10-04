//@name Transformation
//@category Traitements
//@input Image
//@halo full
//@premultiplied
//@param position float2 0 0 -8192 8192 "Position (px)"
//@param scale float 1 0.01 16 "Échelle"
//@param scaleXY float2 1 1 0.01 16 "Échelle X / Y"
//@param rotation float 0 -180 180 "Rotation (°)"
//@param pivot float2 0.5 0.5 0 1 "Pivot"
// Moves, scales and turns the input around a pivot (fraction of the document); outside: transparent.
float4 Effect(EffectContext ctx)
{
    float2 c = ctx.size * pivot;
    float2 p = (float2(ctx.px) + 0.5) - c - position;
    float a = -radians(rotation);
    float s = sin(a), co = cos(a);
    p = float2(co * p.x - s * p.y, s * p.x + co * p.y) / max(scale * scaleXY, 1e-4);
    float2 q = (p + c) / ctx.size;
    if (any(q < 0.0) || any(q > 1.0)) return float4(0, 0, 0, 0);
    return InputUV(0, q);
}
