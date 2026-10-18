//@name Roughness Energy
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Roughness Energy","nextId":13,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"OutputAlpha","op":"param","pos":[40, 550],"text":"Output Alpha","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 660],"text":"Opacity","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"BackgroundChannel","op":"paramcolor","pos":[40, 770],"text":"Background Channel","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"ForegroundChannel","op":"paramcolor","pos":[40, 880],"text":"Foreground Channel","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"OutputChannels","op":"paramcolor","pos":[40, 990],"text":"Output Channels","value":[1, 1, 1, 0]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Roughness Energy","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["OutputAlpha",1],["Opacity",1],["BackgroundChannel",4],["ForegroundChannel",4],["OutputChannels",3]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float rb = saturate(dot(bg, BackgroundChannel));\n    float rf = saturate(dot(fg, ForegroundChannel));\n    float a = saturate(fg.a * mask * Opacity);\n    float rough = sqrt(max(lerp(rb * rb, rf * rf, a), 0.0));\n    float3 rgb = lerp(bg.rgb, rough.xxx, OutputChannels.rgb);\n    float alpha = lerp(bg.a, rough, OutputAlpha * a);\n    return float4(rgb, alpha);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param OutputAlpha float 0 0 1 "Output Alpha"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundChannel color 1 0 0 0 "Background Channel"
//@param ForegroundChannel color 1 0 0 0 "Foreground Channel"
//@param OutputChannels color 1 1 1 0 "Output Channels"
// Imported preset (Blend / PBR): Blends roughness in squared perceptual space to avoid an unnaturally linear material transition.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float OutputAlpha, float Opacity, float4 BackgroundChannel, float4 ForegroundChannel, float3 OutputChannels)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float rb = saturate(dot(bg, BackgroundChannel));
    float rf = saturate(dot(fg, ForegroundChannel));
    float a = saturate(fg.a * mask * Opacity);
    float rough = sqrt(max(lerp(rb * rb, rf * rf, a), 0.0));
    float3 rgb = lerp(bg.rgb, rough.xxx, OutputChannels.rgb);
    float alpha = lerp(bg.a, rough, OutputAlpha * a);
    return float4(rgb, alpha);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), OutputAlpha, Opacity, BackgroundChannel, ForegroundChannel, OutputChannels.rgb);
}
