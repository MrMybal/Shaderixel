//@name Alpha Erase
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Alpha Erase","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 550],"text":"Opacity","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"ReplaceRGB","op":"param","pos":[40, 660],"text":"Replace RGB","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"EraseColor","op":"paramcolor","pos":[40, 770],"text":"Erase Color","value":[0, 0, 0, 1]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Alpha Erase","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["Opacity",1],["ReplaceRGB",1],["EraseColor",3]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float erase = saturate(fg.a * mask * Opacity);\n    float outAlpha = bg.a * (1.0 - erase);\n    float3 rgb = lerp(bg.rgb, EraseColor.rgb, erase * ReplaceRGB);\n    return float4(rgb, outAlpha);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Opacity float 1 0 1 "Opacity"
//@param ReplaceRGB float 0 0 1 "Replace RGB"
//@param EraseColor color 0 0 0 1 "Erase Color"
// Imported preset (Blend / Utility): Uses foreground alpha and mask as a nondestructive eraser while optionally retaining RGB data.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity, float ReplaceRGB, float3 EraseColor)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float erase = saturate(fg.a * mask * Opacity);
    float outAlpha = bg.a * (1.0 - erase);
    float3 rgb = lerp(bg.rgb, EraseColor.rgb, erase * ReplaceRGB);
    return float4(rgb, outAlpha);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity, ReplaceRGB, EraseColor.rgb);
}
