//@name Voronoi
//@category Dessin
//@generator
//@halo 0
//@param Scale float 12 1 256 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Contrast float 1.5 0 16 "Contrast"
//@param BorderContrast float 8 0 64 "Border Contrast"
//@param BorderMode float 1 0 1 "Border Mode"
//@param Opacity float 1 0 1 "Opacity"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Noise): Voronoi cell distance and border generator.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Contrast, float BorderContrast, float BorderMode, float Opacity, float3 ColorA, float3 ColorB)
{
    float2 p=UV*max(Scale,1.0); float2 ip=floor(p); float2 fp=frac(p); float nearest=10.0; float second=10.0; [unroll] for(int y=-1;y<=1;y++){[unroll] for(int x=-1;x<=1;x++){float2 o=float2(x,y); float2 cell=ip+o; float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+Seed)*43758.5453); float d=length(o+rnd-fp); if(d<nearest){second=nearest;nearest=d;}else if(d<second){second=d;}}} float border=saturate((second-nearest)*BorderContrast); float v=lerp(nearest,border,saturate(BorderMode)); v=saturate(v*Contrast); return float4(lerp(ColorA.rgb,ColorB.rgb,v),Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Contrast, BorderContrast, BorderMode, Opacity, ColorA.rgb, ColorB.rgb);
}
