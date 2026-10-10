//@name Normals from Height
//@category PBR Textures
//@input Height
//@halo rayon 1 2
//@param rayon int 1 1 16 "Derivative Radius (px)"
//@param force float 4 0 64 "Height Strength"
//@param directx bool 0 "DirectX Convention (Flip Green)"
float HeightAt(int2 p) { return LinearToSrgb1(Input(0, p).r); }
float4 Effect(EffectContext ctx)
{
    int r = max(1, (int)round(rayon));
    float dx = (HeightAt(ctx.px + int2(r, 0)) - HeightAt(ctx.px - int2(r, 0))) / (2.0 * r);
    float dy = (HeightAt(ctx.px + int2(0, r)) - HeightAt(ctx.px - int2(0, r))) / (2.0 * r);
    // Image UVs: y points down; OpenGL tangent +Y points up.
    float3 n = normalize(float3(-dx * force, dy * force * (directx > 0.5 ? -1.0 : 1.0), 1.0));
    return float4(SrgbToLinear(n * 0.5 + 0.5), 1.0);
}
