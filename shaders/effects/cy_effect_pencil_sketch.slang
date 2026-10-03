//@name Pencil Sketch
//@category Traitements
//@input Image
//@halo full
//@param Radius float 1.5 0.25 16 "Radius"
//@param EdgeStrength float 5 0 64 "Edge Strength"
//@param EdgePower float 0.7 0.05 8 "Edge Power"
//@param Shading float 0.45 0 2 "Shading"
//@param GrainScale float 512 2 4096 "Grain Scale"
//@param GrainAmount float 0.08 0 1 "Grain Amount"
//@param PreserveColor float 0 0 1 "Preserve Color"
//@param Strength float 1 0 1 "Strength"
//@param PencilColor color 0.015 0.012 0.01 1 "Pencil Color"
//@param PaperColor color 0.9 0.86 0.74 1 "Paper Color"

// Imported preset (Effect / Stylize): Pencil-sketch conversion with multi-scale edges, paper tone, grain, and source-color retention.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float EdgeStrength, float EdgePower, float Shading, float GrainScale, float GrainAmount, float PreserveColor, float Strength, float3 PencilColor, float3 PaperColor)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float l=dot(c.rgb,float3(.2126,.7152,.0722));float lx=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722))-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722));float ly=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722))-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722));float edge=pow(saturate(length(float2(lx,ly))*EdgeStrength),EdgePower);float grain=frac(sin(dot(floor(UV*GrainScale),float2(12.9898,78.233)))*43758.5453);float shade=saturate(1.0-edge-(1.0-l)*Shading-(grain-.5)*GrainAmount);float3 mono=lerp(PencilColor.rgb,PaperColor.rgb,shade);float3 outColor=lerp(mono,mono*c.rgb*1.5,PreserveColor);return float4(lerp(c.rgb,outColor,Strength),c.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, EdgeStrength, EdgePower, Shading, GrainScale, GrainAmount, PreserveColor, Strength, PencilColor.rgb, PaperColor.rgb);
}
