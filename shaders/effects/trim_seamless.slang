//@name Raccord de bande (trim)
//@category Trim sheet
//@halo full
//@param rect float4 0 0 512 128 0 16384 "Bande (x, y, largeur, hauteur px)"
//@param blendX float 0.25 0 0.5 "Fondu horizontal (part de la largeur)"
//@param blendY float 0 0 0.5 "Fondu vertical (part de la hauteur)"
// Makes one trim (rectangle of the sheet) tileable: near its edges the content comes from the
// trim shifted by half its size (wrapped inside the trim), which is continuous across the edge.
// Pixels outside the trim are left unchanged.

float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int2 o = int2(rect.xy);
    int2 s = max(int2(rect.zw), int2(1, 1));
    int2 l = ctx.px - o;
    if (any(l < 0) || any(l >= s)) return c;
    float4 r = c;
    if (blendX > 0.0)
    {
        int2 q = int2((l.x + s.x / 2) % s.x, l.y);
        float4 shifted = Input(0, o + q);
        float d = min(l.x + 0.5, s.x - l.x - 0.5);
        float w = 1.0 - smoothstep(0.0, blendX * s.x, d);
        r = lerp(r, shifted, w);
    }
    if (blendY > 0.0)
    {
        int2 q = int2(l.x, (l.y + s.y / 2) % s.y);
        float4 shifted = Input(0, o + q);
        float d = min(l.y + 0.5, s.y - l.y - 0.5);
        float w = 1.0 - smoothstep(0.0, blendY * s.y, d);
        r = lerp(r, shifted, w);
    }
    return r;
}
