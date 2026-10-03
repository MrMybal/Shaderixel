//@name Match Color
//@category Traitements
//@graph {"name":"Match Color","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"LuminanceStrength","text":"Luminance Strength","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"ColorStrength","text":"Color Strength","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[0.5,0.5,0.5,1.0],"name":"SourceAverage","text":"Source Average","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[0.5,0.5,0.5,1.0],"name":"TargetAverage","text":"Target Average","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Match Color","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float sl=max(dot(SourceAverage.rgb,float3(.2126,.7152,.0722)),0.0001); float tl=dot(TargetAverage.rgb,float3(.2126,.7152,.0722)); float3 lum=s.rgb*(tl/sl); float3 chroma=s.rgb*TargetAverage.rgb/max(SourceAverage.rgb,0.0001); float3 v=lerp(s.rgb,lum,saturate(LuminanceStrength)); v=lerp(v,chroma,saturate(ColorStrength)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["LuminanceStrength",1],["ColorStrength",1],["Strength",1],["SourceAverage",3],["TargetAverage",3]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param LuminanceStrength float 1 0 1 "Luminance Strength"
//@param ColorStrength float 1 0 1 "Color Strength"
//@param Strength float 1 0 1 "Strength"
//@param SourceAverage color 0.5 0.5 0.5 1 "Source Average"
//@param TargetAverage color 0.5 0.5 0.5 1 "Target Average"

// Imported preset (Effect / Color & Tone): Matches the image toward a target average color with luminance and color strengths.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LuminanceStrength, float ColorStrength, float Strength, float3 SourceAverage, float3 TargetAverage)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float sl=max(dot(SourceAverage.rgb,float3(.2126,.7152,.0722)),0.0001); float tl=dot(TargetAverage.rgb,float3(.2126,.7152,.0722)); float3 lum=s.rgb*(tl/sl); float3 chroma=s.rgb*TargetAverage.rgb/max(SourceAverage.rgb,0.0001); float3 v=lerp(s.rgb,lum,saturate(LuminanceStrength)); v=lerp(v,chroma,saturate(ColorStrength)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LuminanceStrength, ColorStrength, Strength, SourceAverage.rgb, TargetAverage.rgb);
}
