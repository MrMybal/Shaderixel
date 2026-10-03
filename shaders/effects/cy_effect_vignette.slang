//@name Vignette
//@category Traitements
//@graph {"name":"Vignette","category":"Traitements","inputs":1,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.00999999978,"max":8.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.449999988,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00999999978,"max":2.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":0.00100000005,"max":1.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.800000012,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1},{"id":8,"op":"param2","pos":[40,770],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"VignetteColor","text":"Vignette Color","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"Vignette","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float v=smoothstep(Radius-Softness,Radius+Softness,d); float3 c=lerp(s.rgb,s.rgb*VignetteColor.rgb,v*Intensity); return float4(c,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Aspect",1],["Radius",1],["Softness",1],["Intensity",1],["Center",3],["VignetteColor",3]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Aspect float 1 0.01 8 "Aspect"
//@param Radius float 0.45 0.01 2 "Radius"
//@param Softness float 0.25 0.001 1 "Softness"
//@param Intensity float 0.8 0 1 "Intensity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param VignetteColor color 0 0 0 1 "Vignette Color"

// Imported preset (Effect / Color & Tone): Elliptical soft vignette with tint and HDR intensity.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float Softness, float Intensity, float3 Center, float3 VignetteColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float v=smoothstep(Radius-Softness,Radius+Softness,d); float3 c=lerp(s.rgb,s.rgb*VignetteColor.rgb,v*Intensity); return float4(c,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, Softness, Intensity, float3(Center, 0.0), VignetteColor.rgb);
}
