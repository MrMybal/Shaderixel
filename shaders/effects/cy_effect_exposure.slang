//@name Exposure
//@category Traitements
//@graph {"name":"Exposure","category":"Traitements","inputs":1,"premultiplied":false,"nextId":7,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"ExposureEV","text":"Exposure EV","index":1,"min":-16.0,"max":16.0},{"id":5,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0]],"value":[0,0,0,0],"name":"Exposure","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return float4(s.rgb*exp2(ExposureEV),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["ExposureEV",1]],"outs":[["Resultat",4]]},{"id":6,"op":"output","pos":[780,40],"in":[[5,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param ExposureEV float 0 -16 16 "Exposure EV"

// Imported preset (Effect / Color & Tone): Linear HDR exposure in photographic EV stops.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float ExposureEV)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return float4(s.rgb*exp2(ExposureEV),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ExposureEV);
}
