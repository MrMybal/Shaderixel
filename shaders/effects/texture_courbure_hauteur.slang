//@name Curvature from Height
//@category PBR Textures
//@input Height
//@halo rayon 1 2
//@param rayon int 2 1 16 "Radius (px)"
//@param force float 4 0 32 "Curvature Strength"
//@param mode enum 0 "Signed|Ridges|Cavities" "Map Type"
float CurvatureHeight(int2 p) { return LinearToSrgb1(Input(0, p).r); }
float4 Effect(EffectContext ctx)
{
    int r = max(1, (int)round(rayon));
    float h = CurvatureHeight(ctx.px);
    float avg = (CurvatureHeight(ctx.px + int2(r, 0)) + CurvatureHeight(ctx.px - int2(r, 0))
               + CurvatureHeight(ctx.px + int2(0, r)) + CurvatureHeight(ctx.px - int2(0, r))) * 0.25;
    float k = (h - avg) * force;
    float v = mode < 0.5 ? saturate(0.5 + k) : mode < 1.5 ? saturate(k) : saturate(-k);
    return float4(SrgbToLinear(v.xxx), 1.0);
}
