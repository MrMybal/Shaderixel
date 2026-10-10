//@name Estimated Occlusion from Height
//@category PBR Textures
//@input Height
//@halo rayon 1 2
//@param rayon float 16 1 128 "Occlusion Radius (px)"
//@param force float 4 0 32 "Occlusion Strength"
//@param biais float 0.01 0 0.2 "Bias"
//@param echantillons int 24 8 48 "Samples"
float4 Effect(EffectContext ctx)
{
    float h = LinearToSrgb1(Input(0, ctx.px).r);
    float occ = 0.0;
    int count = clamp((int)round(echantillons), 8, 48);
    for (int i = 0; i < count; ++i)
    {
        float f = sqrt((i + 0.5) / count);
        float a = i * 2.39996323;
        float2 o = float2(cos(a), sin(a)) * rayon * f * ctx.texel;
        float tap = LinearToSrgb1(InputUV(0, ctx.uv + o).r);
        occ += saturate((tap - h - biais) * force / max(f, 0.1));
    }
    float ao = saturate(1.0 - occ / count);
    return float4(SrgbToLinear(ao.xxx), 1.0);
}
