//@name Normalize Range
//@category Traitements
//@graph {"name":"Normalize Range","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"InputMinimum","text":"Input Minimum","index":1,"min":-32.0,"max":32.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"InputMaximum","text":"Input Maximum","index":1,"min":-32.0,"max":32.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"OutputMinimum","text":"Output Minimum","index":1,"min":-32.0,"max":32.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OutputMaximum","text":"Output Maximum","index":1,"min":-32.0,"max":32.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Normalize Range","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=(s.rgb-InputMinimum)/max(InputMaximum-InputMinimum,.0001); v=lerp(OutputMinimum,OutputMaximum,v); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["InputMinimum",1],["InputMaximum",1],["OutputMinimum",1],["OutputMaximum",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param InputMinimum float 0 -32 32 "Input Minimum"
//@param InputMaximum float 1 -32 32 "Input Maximum"
//@param OutputMinimum float 0 -32 32 "Output Minimum"
//@param OutputMaximum float 1 -32 32 "Output Maximum"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Remaps a chosen input range to a configurable output range.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float InputMinimum, float InputMaximum, float OutputMinimum, float OutputMaximum, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=(s.rgb-InputMinimum)/max(InputMaximum-InputMinimum,.0001); v=lerp(OutputMinimum,OutputMaximum,v); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum, InputMaximum, OutputMinimum, OutputMaximum, Strength);
}
