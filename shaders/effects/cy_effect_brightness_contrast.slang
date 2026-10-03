//@name Brightness Contrast
//@category Traitements
//@graph {"name":"Brightness Contrast","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Brightness","text":"Brightness","index":1,"min":-4.0,"max":4.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":8.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Pivot","text":"Pivot","index":1,"min":0.0,"max":2.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Brightness Contrast","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=(s.rgb-Pivot)*Contrast+Pivot+Brightness; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Brightness",1],["Contrast",1],["Pivot",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Brightness float 0 -4 4 "Brightness"
//@param Contrast float 1 0 8 "Contrast"
//@param Pivot float 0.5 0 2 "Pivot"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): HDR-safe brightness and pivoted contrast adjustment.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Brightness, float Contrast, float Pivot, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=(s.rgb-Pivot)*Contrast+Pivot+Brightness; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Brightness, Contrast, Pivot, Strength);
}
