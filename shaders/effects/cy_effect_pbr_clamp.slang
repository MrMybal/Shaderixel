//@name PBR Clamp
//@category Traitements
//@graph {"name":"PBR Clamp","category":"Traitements","inputs":1,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"AlphaMinimum","text":"Alpha Minimum","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"AlphaMaximum","text":"Alpha Maximum","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"ClampAlpha","text":"Clamp Alpha","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Minimum","text":"Minimum","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"Maximum","text":"Maximum","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"Channels","text":"Channels","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"PBR Clamp","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 clamped=clamp(s.rgb,Minimum.rgb,Maximum.rgb);float3 rgb=lerp(s.rgb,clamped,Channels.rgb);float alpha=lerp(s.a,clamp(s.a,AlphaMinimum,AlphaMaximum),ClampAlpha);return float4(rgb,alpha);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["AlphaMinimum",1],["AlphaMaximum",1],["ClampAlpha",1],["Minimum",3],["Maximum",3],["Channels",3]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param AlphaMinimum float 0 0 1 "Alpha Minimum"
//@param AlphaMaximum float 1 0 1 "Alpha Maximum"
//@param ClampAlpha float 0 0 1 "Clamp Alpha"
//@param Minimum color 0 0 0 0 "Minimum"
//@param Maximum color 1 1 1 0 "Maximum"
//@param Channels color 1 1 1 0 "Channels"

// Imported preset (Effect / PBR): Clamps selected linear PBR channels to physically plausible per-channel ranges.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float AlphaMinimum, float AlphaMaximum, float ClampAlpha, float3 Minimum, float3 Maximum, float3 Channels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 clamped=clamp(s.rgb,Minimum.rgb,Maximum.rgb);float3 rgb=lerp(s.rgb,clamped,Channels.rgb);float alpha=lerp(s.a,clamp(s.a,AlphaMinimum,AlphaMaximum),ClampAlpha);return float4(rgb,alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), AlphaMinimum, AlphaMaximum, ClampAlpha, Minimum.rgb, Maximum.rgb, Channels.rgb);
}
