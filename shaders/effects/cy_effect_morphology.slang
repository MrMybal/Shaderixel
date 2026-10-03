//@name Morphology
//@category Traitements
//@graph {"name":"Morphology","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":128.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Mode","text":"Mode","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"UseAlpha","text":"Use Alpha","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"OutputMask","text":"Output Mask","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Morphology","text":"    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float best=lerp(1.0,0.0,step(0.5,Mode));[unroll]for(int i=0;i<24;i++){float a=i*6.283185/24.0;float4 s=Texture2DSample(CyInput,CyInputSampler,UV+float2(cos(a),sin(a))*t);float v=lerp(dot(s.rgb,float3(.2126,.7152,.0722)),s.a,saturate(UseAlpha));best=lerp(min(best,v),max(best,v),step(.5,Mode));}float original=lerp(dot(c.rgb,float3(.2126,.7152,.0722)),c.a,saturate(UseAlpha));float result=lerp(original,best,saturate(Strength));float3 rgb=lerp(c.rgb,result.xxx,saturate(OutputMask));float alpha=lerp(c.a,result,saturate(UseAlpha));return float4(rgb,alpha);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Mode",1],["UseAlpha",1],["OutputMask",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 3 0.25 128 "Radius"
//@param Mode float 1 0 1 "Mode"
//@param UseAlpha float 1 0 1 "Use Alpha"
//@param OutputMask float 0 0 1 "Output Mask"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Detail): GPU dilate/erode filter for alpha, luminance, masks, and cleanup workflows.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Mode, float UseAlpha, float OutputMask, float Strength)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float best=lerp(1.0,0.0,step(0.5,Mode));[unroll]for(int i=0;i<24;i++){float a=i*6.283185/24.0;float4 s=Texture2DSample(CyInput,CyInputSampler,UV+float2(cos(a),sin(a))*t);float v=lerp(dot(s.rgb,float3(.2126,.7152,.0722)),s.a,saturate(UseAlpha));best=lerp(min(best,v),max(best,v),step(.5,Mode));}float original=lerp(dot(c.rgb,float3(.2126,.7152,.0722)),c.a,saturate(UseAlpha));float result=lerp(original,best,saturate(Strength));float3 rgb=lerp(c.rgb,result.xxx,saturate(OutputMask));float alpha=lerp(c.a,result,saturate(UseAlpha));return float4(rgb,alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Mode, UseAlpha, OutputMask, Strength);
}
