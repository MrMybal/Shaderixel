//@name Alpha Remap
//@category Traitements
//@graph {"name":"Alpha Remap","category":"Traitements","inputs":1,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Minimum","text":"Minimum","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Maximum","text":"Maximum","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Feather","text":"Feather","index":1,"min":0.0,"max":0.5},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Gamma","text":"Gamma","index":1,"min":0.00999999978,"max":8.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Invert","text":"Invert","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"Alpha Remap","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float a=smoothstep(Minimum-Feather,Maximum+Feather,s.a); a=pow(saturate(a),max(Gamma,0.001)); a=lerp(a,1.0-a,saturate(Invert)); return float4(s.rgb,lerp(s.a,a,saturate(Strength)));","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Minimum",1],["Maximum",1],["Feather",1],["Gamma",1],["Invert",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Minimum float 0 0 1 "Minimum"
//@param Maximum float 1 0 1 "Maximum"
//@param Feather float 0 0 0.5 "Feather"
//@param Gamma float 1 0.01 8 "Gamma"
//@param Invert float 0 0 1 "Invert"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Remaps, feathers, inverts, and gamma-corrects alpha without changing RGB.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Minimum, float Maximum, float Feather, float Gamma, float Invert, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float a=smoothstep(Minimum-Feather,Maximum+Feather,s.a); a=pow(saturate(a),max(Gamma,0.001)); a=lerp(a,1.0-a,saturate(Invert)); return float4(s.rgb,lerp(s.a,a,saturate(Strength)));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Minimum, Maximum, Feather, Gamma, Invert, Strength);
}
