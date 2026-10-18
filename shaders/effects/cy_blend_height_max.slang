//@name Height Max
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Height Max","nextId":13,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":2,"min":-2,"name":"HeightBias","op":"param","pos":[40, 550],"text":"Height Bias","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":1,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 660],"text":"Softness","value":[0.0500000007, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 770],"text":"Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"BackgroundHeightChannel","op":"paramcolor","pos":[40, 880],"text":"Background Height Channel","value":[0, 0, 0, 1]},{"id":10,"in":[],"index":1,"name":"ForegroundHeightChannel","op":"paramcolor","pos":[40, 990],"text":"Foreground Height Channel","value":[0, 0, 0, 1]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Height Max","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["HeightBias",1],["Softness",1],["Opacity",1],["BackgroundHeightChannel",4],["ForegroundHeightChannel",4]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float hb = dot(bg, BackgroundHeightChannel);\n    float hf = dot(fg, ForegroundHeightChannel) + HeightBias;\n    float selector = smoothstep(-Softness, Softness, hf - hb);\n    float a = saturate(selector * mask * Opacity);\n    float4 combined = lerp(bg, fg, a);\n    return float4(combined.rgb, lerp(bg.a, max(bg.a, fg.a), a));","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param HeightBias float 0 -2 2 "Height Bias"
//@param Softness float 0.05 0.0001 1 "Softness"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundHeightChannel color 0 0 0 1 "Background Height Channel"
//@param ForegroundHeightChannel color 0 0 0 1 "Foreground Height Channel"
// Imported preset (Blend / PBR): Selects the material with the greater packed height while preserving a controllable soft transition.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float HeightBias, float Softness, float Opacity, float4 BackgroundHeightChannel, float4 ForegroundHeightChannel)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float hb = dot(bg, BackgroundHeightChannel);
    float hf = dot(fg, ForegroundHeightChannel) + HeightBias;
    float selector = smoothstep(-Softness, Softness, hf - hb);
    float a = saturate(selector * mask * Opacity);
    float4 combined = lerp(bg, fg, a);
    return float4(combined.rgb, lerp(bg.a, max(bg.a, fg.a), a));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), HeightBias, Softness, Opacity, BackgroundHeightChannel, ForegroundHeightChannel);
}
