//@name Outline
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Outline","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":128,"min":0.25,"name":"Thickness","op":"param","pos":[40, 330],"text":"Thickness","value":[2, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Inner","op":"param","pos":[40, 440],"text":"Inner","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 550],"text":"Opacity","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"OutlineColor","op":"paramcolor","pos":[40, 660],"text":"Outline Color","value":[1, 1, 1, 1]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Outline","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Thickness",1],["Inner",1],["Opacity",1],["OutlineColor",3]],"pos":[420, 40],"text":"    float2 t = Thickness / max(CyCanvasSize.xy, 1.0);\n    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float a0 = Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).a;\n    float a1 = Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).a;\n    float a2 = Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).a;\n    float a3 = Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).a;\n    float expanded = max(max(a0, a1), max(a2, a3));\n    float contracted = min(min(a0, a1), min(a2, a3));\n    float edge = lerp(saturate(expanded - s.a), saturate(s.a - contracted), saturate(Inner));\n    float alpha = edge * Opacity * (1.0 - s.a * (1.0 - Inner));\n    return float4(s.rgb + OutlineColor.rgb * alpha, s.a + alpha);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Thickness float 2 0.25 128 "Thickness"
//@param Inner float 0 0 1 "Inner"
//@param Opacity float 1 0 1 "Opacity"
//@param OutlineColor color 1 1 1 1 "Outline Color"
// Imported preset (Effect / Detail): Builds an inner or outer outline from source alpha.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Thickness, float Inner, float Opacity, float3 OutlineColor)
{
    float2 t = Thickness / max(CyCanvasSize.xy, 1.0);
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float a0 = Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).a;
    float a1 = Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).a;
    float a2 = Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).a;
    float a3 = Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).a;
    float expanded = max(max(a0, a1), max(a2, a3));
    float contracted = min(min(a0, a1), min(a2, a3));
    float edge = lerp(saturate(expanded - s.a), saturate(s.a - contracted), saturate(Inner));
    float alpha = edge * Opacity * (1.0 - s.a * (1.0 - Inner));
    return float4(s.rgb + OutlineColor.rgb * alpha, s.a + alpha);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Thickness, Inner, Opacity, OutlineColor.rgb);
}
