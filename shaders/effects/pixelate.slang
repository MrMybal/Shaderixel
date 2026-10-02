//@name Mosaïque
//@category Pixellisation
//@premultiplied
//@param cell int 16 2 256 "Taille de cellule"
float4 Effect(EffectContext ctx)
{
    int s = max(cell, 1);
    int2 origin = (ctx.px / s) * s;
    float2 uv = (float2(origin) + s * 0.5) * ctx.texel;
    return InputUV(0, uv);
}
