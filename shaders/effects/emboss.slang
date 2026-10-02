//@name Estampage
//@category Esthétiques
//@param angle float 135 -180 180 "Angle"
//@param height float 2 0 10 "Hauteur"
//@param amount float 1 0 5 "Quantité"
float4 Effect(EffectContext ctx)
{
    float a = radians(angle);
    float2 d = float2(cos(a), sin(a)) * height;
    float l0 = Luma(InputUV(0, ctx.uv + d * ctx.texel).rgb);
    float l1 = Luma(InputUV(0, ctx.uv - d * ctx.texel).rgb);
    float v = saturate(0.5 + (l0 - l1) * amount);
    return float4(SrgbToLinear(v.xxx), Input(0, ctx.px).a);
}
