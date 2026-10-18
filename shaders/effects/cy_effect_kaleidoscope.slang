//@name Kaleidoscope
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Kaleidoscope","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":64,"min":1,"name":"Segments","op":"param","pos":[40, 330],"text":"Segments","value":[8, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Rotation","op":"param","pos":[40, 440],"text":"Rotation","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 550],"text":"Aspect","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 660],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Kaleidoscope","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Segments",1],["Rotation",1],["Aspect",1],["Center",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float r = length(p);\n    float a = atan2(p.y, p.x) + Rotation;\n    float sector = 6.2831853 / max(Segments, 1.0);\n    a = abs(fmod(a + sector * 0.5, sector) - sector * 0.5);\n    float2 q = float2(cos(a), sin(a)) * r;\n    q.x /= max(Aspect, 0.001);\n    return Texture2DSample(CyInput, CyInputSampler, Center.xy + q);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Segments float 8 1 64 "Segments"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Effect / Distort): Radial kaleidoscope UV transform.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Segments, float Rotation, float Aspect, float3 Center)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float r = length(p);
    float a = atan2(p.y, p.x) + Rotation;
    float sector = 6.2831853 / max(Segments, 1.0);
    a = abs(fmod(a + sector * 0.5, sector) - sector * 0.5);
    float2 q = float2(cos(a), sin(a)) * r;
    q.x /= max(Aspect, 0.001);
    return Texture2DSample(CyInput, CyInputSampler, Center.xy + q);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Segments, Rotation, Aspect, float3(Center, 0.0));
}
