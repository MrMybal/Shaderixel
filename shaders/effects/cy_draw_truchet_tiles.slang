//@name Truchet Tiles
//@category Dessin
//@generator
//@halo 0
//@param Tiling float 10 1 256 "Tiling"
//@param Seed float 0 -1000 1000 "Seed"
//@param Thickness float 0.035 0.001 0.25 "Thickness"
//@param Softness float 0.01 0.0001 0.1 "Softness"
//@param GlowRadius float 0.18 0.001 0.7 "Glow Radius"
//@param GlowFalloff float 3 0.1 16 "Glow Falloff"
//@param GlowIntensity float 0.25 0 16 "Glow Intensity"
//@param LineOpacity float 1 0 1 "Line Opacity"
//@param BackgroundOpacity float 1 0 1 "Background Opacity"
//@param LineColor color 0.15 0.8 1.8 1 "Line Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Pattern): Seeded Truchet arc tiles for procedural ornament, paths, and seamless patterns.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tiling, float Seed, float Thickness, float Softness, float GlowRadius, float GlowFalloff, float GlowIntensity, float LineOpacity, float BackgroundOpacity, float3 LineColor, float3 BackgroundColor)
{
    float2 q=UV*max(Tiling,1.0); float2 id=floor(q); float2 p=frac(q); float rnd=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); if(rnd>0.5)p.x=1.0-p.x; float d=min(abs(length(p)-0.5),abs(length(p-1.0)-0.5)); float lineMask=1.0-smoothstep(Thickness,Thickness+Softness,d); float glow=pow(saturate(1.0-d/max(GlowRadius,0.0001)),max(GlowFalloff,0.1)); float3 rgb=BackgroundColor.rgb+LineColor.rgb*(lineMask+glow*GlowIntensity); return float4(rgb,lerp(BackgroundOpacity,LineOpacity,max(lineMask,glow)));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tiling, Seed, Thickness, Softness, GlowRadius, GlowFalloff, GlowIntensity, LineOpacity, BackgroundOpacity, LineColor.rgb, BackgroundColor.rgb);
}
