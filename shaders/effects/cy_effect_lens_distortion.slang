//@name Lens Distortion
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Lens Distortion","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 330],"text":"Aspect","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":-4,"name":"Distortion","op":"param","pos":[40, 440],"text":"Distortion","value":[0.200000003, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":-8,"name":"Quartic","op":"param","pos":[40, 550],"text":"Quartic","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 660],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Lens Distortion","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Aspect",1],["Distortion",1],["Quartic",1],["Center",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float r2 = dot(p, p);\n    p *= 1.0 + Distortion * r2 + Quartic * r2 * r2;\n    p.x /= max(Aspect, 0.001);\n    float2 q = saturate(Center.xy + p);\n    return Texture2DSample(CyInput, CyInputSampler, q);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Aspect float 1 0.01 8 "Aspect"
//@param Distortion float 0.2 -4 4 "Distortion"
//@param Quartic float 0 -8 8 "Quartic"
//@param Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Effect / Distort): Barrel or pincushion lens distortion with edge clamping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Distortion, float Quartic, float3 Center)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float r2 = dot(p, p);
    p *= 1.0 + Distortion * r2 + Quartic * r2 * r2;
    p.x /= max(Aspect, 0.001);
    float2 q = saturate(Center.xy + p);
    return Texture2DSample(CyInput, CyInputSampler, q);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Distortion, Quartic, float3(Center, 0.0));
}
