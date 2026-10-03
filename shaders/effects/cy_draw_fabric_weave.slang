//@name Fabric Weave
//@category CyTexture Dessin
//@generator
//@halo 0
//@param WarpThreads float 48 2 512 "Warp Threads"
//@param WeftThreads float 48 2 512 "Weft Threads"
//@param ThreadRoundness float 1.8 0.1 16 "Thread Roundness"
//@param Irregularity float 0.08 0 0.8 "Irregularity"
//@param Seed float 0 -1000 1000 "Seed"
//@param Opacity float 1 0 1 "Opacity"
//@param WarpColor color 0.08 0.18 0.34 1 "Warp Color"
//@param WeftColor color 0.55 0.62 0.7 1 "Weft Color"

// Imported from CyTexture (Draw / Surface): Procedural woven textile with over-under threads, fiber highlights, and weave irregularity.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float WarpThreads, float WeftThreads, float ThreadRoundness, float Irregularity, float Seed, float Opacity, float3 WarpColor, float3 WeftColor)
{
    float2 q=UV*float2(max(WarpThreads,1.0),max(WeftThreads,1.0));float2 cell=floor(q);float2 p=frac(q)-.5;float over=fmod(cell.x+cell.y,2.0);float warp=pow(saturate(1.0-abs(p.x)*2.0),ThreadRoundness);float weft=pow(saturate(1.0-abs(p.y)*2.0),ThreadRoundness);float h=frac(sin(dot(cell+Seed,float2(127.1,311.7)))*43758.5453);warp*=lerp(1.0-Irregularity,1.0+Irregularity,h);weft*=lerp(1.0+Irregularity,1.0-Irregularity,h);float3 rgb=lerp(WeftColor.rgb,WarpColor.rgb,lerp(weft,warp,over));float shade=lerp(.55,1.2,max(warp,weft));return float4(rgb*shade,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), WarpThreads, WeftThreads, ThreadRoundness, Irregularity, Seed, Opacity, WarpColor.rgb, WeftColor.rgb);
}
