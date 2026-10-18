//@name Polar Transform
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Polar Transform","nextId":14,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0.0500000007,"name":"Aspect","op":"param","pos":[40, 330],"text":"Aspect","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0.00999999978,"name":"RadialScale","op":"param","pos":[40, 440],"text":"Radial Scale","value":[2, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":-8,"name":"RadialOffset","op":"param","pos":[40, 550],"text":"Radial Offset","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":4,"min":-4,"name":"Rotation","op":"param","pos":[40, 660],"text":"Rotation","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":16,"min":-16,"name":"Twist","op":"param","pos":[40, 770],"text":"Twist","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":64,"min":1,"name":"AngularRepeat","op":"param","pos":[40, 880],"text":"Angular Repeat","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"ToPolar","op":"param","pos":[40, 990],"text":"To Polar","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1100],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Polar Transform","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Aspect",1],["RadialScale",1],["RadialOffset",1],["Rotation",1],["Twist",1],["AngularRepeat",1],["ToPolar",1],["Center",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float radius = length(p) * RadialScale + RadialOffset;\n    float angle = atan2(p.y, p.x) / 6.283185 + 0.5 + Rotation + radius * Twist;\n    float2 polar = float2(frac(angle * AngularRepeat), frac(radius));\n    float2 target = lerp(UV, polar, saturate(ToPolar));\n    float4 s = Texture2DSample(CyInput, CyInputSampler, target);\n    return s;","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Aspect float 1 0.05 16 "Aspect"
//@param RadialScale float 2 0.01 32 "Radial Scale"
//@param RadialOffset float 0 -8 8 "Radial Offset"
//@param Rotation float 0 -4 4 "Rotation"
//@param Twist float 0 -16 16 "Twist"
//@param AngularRepeat float 1 1 64 "Angular Repeat"
//@param ToPolar float 1 0 1 "To Polar"
//@param Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Effect / Distort): Cartesian-to-polar transform with repeat, twist, radial offset, and seamless angular wrapping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float RadialScale, float RadialOffset, float Rotation, float Twist, float AngularRepeat, float ToPolar, float3 Center)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float radius = length(p) * RadialScale + RadialOffset;
    float angle = atan2(p.y, p.x) / 6.283185 + 0.5 + Rotation + radius * Twist;
    float2 polar = float2(frac(angle * AngularRepeat), frac(radius));
    float2 target = lerp(UV, polar, saturate(ToPolar));
    float4 s = Texture2DSample(CyInput, CyInputSampler, target);
    return s;
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, RadialScale, RadialOffset, Rotation, Twist, AngularRepeat, ToPolar, float3(Center, 0.0));
}
