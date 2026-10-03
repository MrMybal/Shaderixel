//@name Rust Patches
//@category Dessin
//@generator
//@halo 0
//@param Scale float 4 0.1 128 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Threshold float 0.5 0 1 "Threshold"
//@param Softness float 0.08 0.0001 0.5 "Softness"
//@param PittingScale float 180 2 2048 "Pitting Scale"
//@param Pitting float 0.08 0 1 "Pitting"
//@param PittingSoftness float 0.04 0.0001 0.5 "Pitting Softness"
//@param EdgeWidth float 0.12 0.001 1 "Edge Width"
//@param EdgeSharpness float 2 0.1 16 "Edge Sharpness"
//@param EdgeIntensity float 0.3 0 8 "Edge Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param BaseColor color 0.07 0.08 0.085 1 "Base Color"
//@param RustDark color 0.16 0.025 0.005 1 "Rust Dark"
//@param RustBright color 0.75 0.16 0.015 1 "Rust Bright"
//@param EdgeColor color 1.2 0.32 0.03 1 "Edge Color"

// Imported preset (Draw / Surface): Layered corrosion patches with pitting, scale variation, edge oxidation, and PBR-friendly masks.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Threshold, float Softness, float PittingScale, float Pitting, float PittingSoftness, float EdgeWidth, float EdgeSharpness, float EdgeIntensity, float Opacity, float3 BaseColor, float3 RustDark, float3 RustBright, float3 EdgeColor)
{
    float2 p=UV*max(Scale,.01)+Seed;float n=0,amp=.55;[unroll]for(int i=0;i<5;i++){n+=(sin(p.x)+sin(p.y)+sin(p.x+p.y))*.1667*amp;p=p*2.03+7.17;amp*=.52;}n=saturate(n*.5+.5);float patch=smoothstep(Threshold-Softness,Threshold+Softness,n);float pits=frac(sin(dot(floor(UV*PittingScale)+Seed,float2(12.9898,78.233)))*43758.5453);float pitMask=smoothstep(1.0-Pitting,PittingSoftness+1.0-Pitting,pits)*patch;float edge=pow(saturate(1.0-abs(n-Threshold)/max(EdgeWidth,.0001)),EdgeSharpness);float3 rust=lerp(RustDark.rgb,RustBright.rgb,saturate(n+pitMask*.3));float3 rgb=lerp(BaseColor.rgb,rust,patch)+EdgeColor.rgb*edge*EdgeIntensity;return float4(rgb,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Threshold, Softness, PittingScale, Pitting, PittingSoftness, EdgeWidth, EdgeSharpness, EdgeIntensity, Opacity, BaseColor.rgb, RustDark.rgb, RustBright.rgb, EdgeColor.rgb);
}
