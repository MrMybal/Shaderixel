//@name Radial
//@category Masques
//@graph {"name":"Radial","category":"Masques","inputs":0,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00100000005,"max":2.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.100000001,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":1.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.00999999978,"max":8.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Invert","text":"Invert","index":1},{"id":7,"op":"param2","pos":[40,660],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Radial","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float v=1.0-smoothstep(Radius-Softness,Radius+Softness,d); v=lerp(v,1.0-v,saturate(Invert)); return float4(v.xxx,1);","pins":[["UV",2],["CyCanvasSize",4],["Radius",1],["Softness",1],["Aspect",1],["Invert",1],["Center",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Radius float 0.35 0.001 2 "Radius"
//@param Softness float 0.1 0.0001 1 "Softness"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Invert float 0 0 1 "Invert"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Mask / Mask): Configurable radial mask independent of source color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Softness, float Aspect, float Invert, float3 Center)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float v=1.0-smoothstep(Radius-Softness,Radius+Softness,d); v=lerp(v,1.0-v,saturate(Invert)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Softness, Aspect, Invert, float3(Center, 0.0));
}
