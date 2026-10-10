//@name Texture Edge Seam Blending
//@category Texture Processing
//@halo full
//@premultiplied
//@param largeur float 0.08 0.001 0.5 "Seam Width (Fraction)"
//@param force float 1 0 1 "Seam Blend Strength"
// At strength 1, opposite edge pixels match. Derivative continuity is approximate.
float4 Effect(EffectContext ctx)
{
    float2 p = float2(ctx.px) / max(ctx.size - 1.0, 1.0);
    float2 edge = min(p, 1.0 - p);
    float2 w = 0.5 * (1.0 - smoothstep(0.0, max(largeur, 0.001), edge)) * saturate(force);
    float4 c = Input(0, ctx.px);
    float4 x = InputUV(0, float2(1.0 - ctx.uv.x, ctx.uv.y));
    float4 y = InputUV(0, float2(ctx.uv.x, 1.0 - ctx.uv.y));
    float4 xy = InputUV(0, 1.0 - ctx.uv);
    return lerp(lerp(c, x, w.x), lerp(y, xy, w.x), w.y);
}
