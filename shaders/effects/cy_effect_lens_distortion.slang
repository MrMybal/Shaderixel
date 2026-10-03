//@name Lens Distortion
//@category Traitements
//@graph {"name":"Lens Distortion","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.00999999978,"max":8.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.200000003,0.0,0.0,0.0],"name":"Distortion","text":"Distortion","index":1,"min":-4.0,"max":4.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Quartic","text":"Quartic","index":1,"min":-8.0,"max":8.0},{"id":7,"op":"param2","pos":[40,660],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Lens Distortion","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float r2=dot(p,p); p*=1.0+Distortion*r2+Quartic*r2*r2; p.x/=max(Aspect,0.001); float2 q=saturate(Center.xy+p); return Texture2DSample(CyInput,CyInputSampler,q);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Aspect",1],["Distortion",1],["Quartic",1],["Center",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Aspect float 1 0.01 8 "Aspect"
//@param Distortion float 0.2 -4 4 "Distortion"
//@param Quartic float 0 -8 8 "Quartic"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Distort): Barrel or pincushion lens distortion with edge clamping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Distortion, float Quartic, float3 Center)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float r2=dot(p,p); p*=1.0+Distortion*r2+Quartic*r2*r2; p.x/=max(Aspect,0.001); float2 q=saturate(Center.xy+p); return Texture2DSample(CyInput,CyInputSampler,q);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Distortion, Quartic, float3(Center, 0.0));
}
