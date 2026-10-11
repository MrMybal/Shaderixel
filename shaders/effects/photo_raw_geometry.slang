//@name Géométrie
//@category Déformation
//@halo full
//@premultiplied
//@param vertical float 0 -100 100 "Perspective verticale"
//@param horizontal float 0 -100 100 "Perspective horizontale"
//@param rotate float 0 -45 45 "Rotation (°)"
//@param aspect float 0 -100 100 "Aspect"
//@param scale float 100 50 150 "Échelle (%)"
//@param offset_x float 0 -100 100 "Décalage X"
//@param offset_y float 0 -100 100 "Décalage Y"
//@param autoscale bool 1 "Remplir le cadre"
//@param grid bool 0 "Afficher une grille"
// Perspective / straightening of the Photo RAW workspace (Geometry panel): output pixel -> source
// position through scale / offset, aspect, rotation and a keystone (projective) term, in coordinates
// centered on the image and normalized by its half height. Vertical > 0 brings converging verticals
// (camera tilted up) back parallel.

float2 SourcePos(float2 q)
{
    // q: output position (centered, / half height) -> source position, same space.
    q.x *= exp2(-aspect / 100.0 * 0.5);
    q.y *= exp2(aspect / 100.0 * 0.5);
    float r = radians(rotate);
    q = float2(q.x * cos(r) - q.y * sin(r), q.x * sin(r) + q.y * cos(r));
    float kv = vertical / 100.0 * 0.45, kh = horizontal / 100.0 * 0.45;
    float w = 1.0 - kv * q.y - kh * q.x;  // top (q.y < 0) magnified when vertical > 0
    return q / max(w, 0.05);
}

float4 Effect(EffectContext ctx)
{
    const float2 hsz = ctx.size * 0.5;
    const float n = hsz.y;
    const float2 ext = hsz / n;  // half extent in normalized units
    float2 p = (float2(ctx.px) + 0.5 - hsz) / n;
    // Output -> source; "fill" divides by the largest overshoot of the corners and edge middles.
    float k = max(scale / 100.0, 0.05);
    if (autoscale)
    {
        float need = 0.0;
        float2 pts[8] = {float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, 1), float2(0, -1), float2(0, 1), float2(-1, 0), float2(1, 0)};
        for (int i = 0; i < 8; ++i)
        {
            float2 sp = SourcePos(pts[i] * ext);
            need = max(need, max(abs(sp.x) / ext.x, abs(sp.y) / ext.y));
        }
        k *= max(need, 1e-3);
    }
    float2 q = p - float2(offset_x, offset_y) / 100.0 * ext;
    float2 src = SourcePos(q) / k;
    float2 uv = (src * n + hsz) * ctx.texel;
    float4 c = (any(uv < 0.0) || any(uv > 1.0)) ? float4(0, 0, 0, 0) : InputUV(0, uv);
    if (grid)
    {
        float2 g = abs(frac((float2(ctx.px) + 0.5) / (ctx.size / 8.0) + 0.5) - 0.5) * (ctx.size / 8.0);
        if (min(g.x, g.y) < 1.0) c = lerp(c, float4(1, 1, 1, 1), 0.5);
    }
    return c;
}
