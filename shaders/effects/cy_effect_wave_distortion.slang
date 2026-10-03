//@name Wave Distortion
//@category Traitements
//@graph {"name":"Wave Distortion","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.00999999978,0.0,0.0,0.0],"name":"Amount","text":"Amount","index":1,"min":0.0,"max":0.25},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Frequency","text":"Frequency","index":1,"min":0.0,"max":256.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-12.566,"max":12.566},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Wave Distortion","text":"    float2 o=float2(sin(UV.y*Frequency+Phase),cos(UV.x*Frequency+Phase))*Amount; return Texture2DSample(CyInput,CyInputSampler,UV+o);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["Frequency",1],["Phase",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Amount float 0.01 0 0.25 "Amount"
//@param Frequency float 24 0 256 "Frequency"
//@param Phase float 0 -12.566 12.566 "Phase"

// Imported preset (Effect / Distort): Animated-ready sine distortion controlled by phase and frequency.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float Frequency, float Phase)
{
    float2 o=float2(sin(UV.y*Frequency+Phase),cos(UV.x*Frequency+Phase))*Amount; return Texture2DSample(CyInput,CyInputSampler,UV+o);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, Frequency, Phase);
}
