//@name Scanlines
//@category Traitements
//@graph {"name":"Scanlines","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[540.0,0.0,0.0,0.0],"name":"Frequency","text":"Frequency","index":1,"min":1.0,"max":8192.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-10.0,"max":10.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"Sharpness","text":"Sharpness","index":1,"min":0.100000001,"max":32.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"LineColor","text":"Line Color","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Scanlines","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float scanline=0.5+0.5*cos((UV.y*Frequency+Phase)*6.2831853); scanline=pow(scanline,max(Sharpness,0.1)); float3 v=s.rgb*lerp(1.0,LineColor.rgb,scanline*Intensity); return float4(v,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Frequency",1],["Phase",1],["Sharpness",1],["Intensity",1],["LineColor",3]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Frequency float 540 1 8192 "Frequency"
//@param Phase float 0 -10 10 "Phase"
//@param Sharpness float 3 0.1 32 "Sharpness"
//@param Intensity float 0.25 0 1 "Intensity"
//@param LineColor color 0 0 0 1 "Line Color"

// Imported preset (Effect / Stylize): CRT-style scanlines with phase and tint.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Phase, float Sharpness, float Intensity, float3 LineColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float scanline=0.5+0.5*cos((UV.y*Frequency+Phase)*6.2831853); scanline=pow(scanline,max(Sharpness,0.1)); float3 v=s.rgb*lerp(1.0,LineColor.rgb,scanline*Intensity); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Phase, Sharpness, Intensity, LineColor.rgb);
}
