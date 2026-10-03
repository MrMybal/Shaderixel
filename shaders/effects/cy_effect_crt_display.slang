//@name CRT Display
//@category Traitements
//@input Image
//@halo full
//@param Curvature float 0.08 0 0.75 "Curvature"
//@param ChromaticOffset float 1.5 0 32 "Chromatic Offset"
//@param ScanlineAmount float 0.35 0 1 "Scanline Amount"
//@param PhosphorAmount float 0.3 0 1 "Phosphor Amount"
//@param Vignette float 0.32 0 2 "Vignette"
//@param VignettePower float 1.4 0.05 8 "Vignette Power"
//@param Brightness float 1.15 0 8 "Brightness"

// Imported preset (Effect / Stylize): Curved CRT display simulation with RGB phosphors, scanlines, vignette, and chromatic convergence.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Curvature, float ChromaticOffset, float ScanlineAmount, float PhosphorAmount, float Vignette, float VignettePower, float Brightness)
{
    float2 p=UV*2.0-1.0;float2 curved=p*(1.0+Curvature*dot(p,p));float2 uv=curved*.5+.5;float2 px=float2(ChromaticOffset/max(CyCanvasSize.x,1.0),0);float r=Texture2DSample(CyInput,CyInputSampler,uv+px).r;float g=Texture2DSample(CyInput,CyInputSampler,uv).g;float b=Texture2DSample(CyInput,CyInputSampler,uv-px).b;float scan=lerp(1.0,0.5+0.5*sin(uv.y*CyCanvasSize.y*3.14159),ScanlineAmount);float3 triad=lerp(1.0,0.72+0.28*cos(uv.x*CyCanvasSize.x*2.0944+float3(0,2.0944,4.1888)),PhosphorAmount);float vign=pow(saturate(1.0-dot(p,p)*Vignette),VignettePower);float inside=step(0.0,uv.x)*step(uv.x,1.0)*step(0.0,uv.y)*step(uv.y,1.0);float3 v=float3(r,g,b)*scan*triad*vign*Brightness*inside;float a=Texture2DSample(CyInput,CyInputSampler,saturate(uv)).a*inside;return float4(v,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Curvature, ChromaticOffset, ScanlineAmount, PhosphorAmount, Vignette, VignettePower, Brightness);
}
