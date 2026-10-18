//@name Normal Whiteout
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Normal Whiteout","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0,"name":"DetailStrength","op":"param","pos":[40, 550],"text":"Detail Strength","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 660],"text":"Opacity","value":[1, 0, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Normal Whiteout","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["DetailStrength",1],["Opacity",1]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float3 n1 = normalize(bg.xyz * 2.0 - 1.0);\n    float3 n2 = normalize(fg.xyz * 2.0 - 1.0);\n    n2.xy *= DetailStrength;\n    float3 combined = normalize(float3(n1.xy + n2.xy, n1.z * n2.z));\n    float3 encoded = combined * .5 + .5;\n    float a = saturate(fg.a * mask * Opacity);\n    return float4(lerp(bg.rgb, encoded, a), bg.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param DetailStrength float 1 0 8 "Detail Strength"
//@param Opacity float 1 0 1 "Opacity"
// Imported preset (Blend / PBR): Whiteout tangent-space normal blend that retains broad surface direction while adding fine detail.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float DetailStrength, float Opacity)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float3 n1 = normalize(bg.xyz * 2.0 - 1.0);
    float3 n2 = normalize(fg.xyz * 2.0 - 1.0);
    n2.xy *= DetailStrength;
    float3 combined = normalize(float3(n1.xy + n2.xy, n1.z * n2.z));
    float3 encoded = combined * .5 + .5;
    float a = saturate(fg.a * mask * Opacity);
    return float4(lerp(bg.rgb, encoded, a), bg.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), DetailStrength, Opacity);
}
