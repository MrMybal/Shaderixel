//@name Normal Strength
//@category Traitements
//@graph {"name":"Normal Strength","category":"Traitements","inputs":1,"premultiplied":false,"nextId":8,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":8.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"FlipGreen","text":"Flip Green","index":1},{"id":6,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0]],"value":[0,0,0,0],"name":"Normal Strength","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 n=s.xyz*2.0-1.0;n.xy*=Strength;n.y*=lerp(1.0,-1.0,FlipGreen);n.z=sqrt(saturate(1.0-dot(n.xy,n.xy)));n=normalize(n);return float4(n*.5+.5,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Strength",1],["FlipGreen",1]],"outs":[["Resultat",4]]},{"id":7,"op":"output","pos":[780,40],"in":[[6,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Strength float 1 0 8 "Strength"
//@param FlipGreen float 0 0 1 "Flip Green"

// Imported preset (Effect / PBR): Adjusts tangent-space normal strength while reconstructing a valid positive Z component.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength, float FlipGreen)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 n=s.xyz*2.0-1.0;n.xy*=Strength;n.y*=lerp(1.0,-1.0,FlipGreen);n.z=sqrt(saturate(1.0-dot(n.xy,n.xy)));n=normalize(n);return float4(n*.5+.5,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength, FlipGreen);
}
