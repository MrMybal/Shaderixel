//@name Channel Replace
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Channel Replace","nextId":14,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ReplaceAlpha","op":"param","pos":[40, 550],"text":"Replace Alpha","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 660],"text":"Opacity","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"SourceForRed","op":"paramcolor","pos":[40, 770],"text":"Source For Red","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"SourceForGreen","op":"paramcolor","pos":[40, 880],"text":"Source For Green","value":[0, 1, 0, 0]},{"id":10,"in":[],"index":1,"name":"SourceForBlue","op":"paramcolor","pos":[40, 990],"text":"Source For Blue","value":[0, 0, 1, 0]},{"id":11,"in":[],"index":1,"name":"ReplaceChannels","op":"paramcolor","pos":[40, 1100],"text":"Replace Channels","value":[1, 1, 1, 0]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Channel Replace","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["ReplaceAlpha",1],["Opacity",1],["SourceForRed",4],["SourceForGreen",4],["SourceForBlue",4],["ReplaceChannels",3]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float3 mapped = float3(dot(fg, SourceForRed), dot(fg, SourceForGreen), dot(fg, SourceForBlue));\n    float3 weights = saturate(ReplaceChannels.rgb);\n    float a = saturate(mask * fg.a * Opacity);\n    float3 blend = lerp(bg.rgb, mapped, weights);\n    return float4(lerp(bg.rgb, blend, a), lerp(bg.a, fg.a, saturate(ReplaceAlpha * mask * Opacity)));","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param ReplaceAlpha float 0 0 1 "Replace Alpha"
//@param Opacity float 1 0 1 "Opacity"
//@param SourceForRed color 1 0 0 0 "Source For Red"
//@param SourceForGreen color 0 1 0 0 "Source For Green"
//@param SourceForBlue color 0 0 1 0 "Source For Blue"
//@param ReplaceChannels color 1 1 1 0 "Replace Channels"
// Imported preset (Blend / Utility): Replaces any weighted combination of background RGB channels with foreground channels.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float ReplaceAlpha, float Opacity, float4 SourceForRed, float4 SourceForGreen, float4 SourceForBlue, float3 ReplaceChannels)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float3 mapped = float3(dot(fg, SourceForRed), dot(fg, SourceForGreen), dot(fg, SourceForBlue));
    float3 weights = saturate(ReplaceChannels.rgb);
    float a = saturate(mask * fg.a * Opacity);
    float3 blend = lerp(bg.rgb, mapped, weights);
    return float4(lerp(bg.rgb, blend, a), lerp(bg.a, fg.a, saturate(ReplaceAlpha * mask * Opacity)));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ReplaceAlpha, Opacity, SourceForRed, SourceForGreen, SourceForBlue, ReplaceChannels.rgb);
}
