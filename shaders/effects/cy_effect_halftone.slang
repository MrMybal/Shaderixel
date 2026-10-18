//@name Halftone
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Halftone","nextId":12,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2048,"min":2,"name":"Scale","op":"param","pos":[40, 330],"text":"Scale","value":[96, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Angle","op":"param","pos":[40, 440],"text":"Angle","value":[0.785000026, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.0299999993, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"PreserveColor","op":"param","pos":[40, 660],"text":"Preserve Color","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"InkColor","op":"paramcolor","pos":[40, 770],"text":"Ink Color","value":[0, 0, 0, 1]},{"id":9,"in":[],"index":1,"name":"PaperColor","op":"paramcolor","pos":[40, 880],"text":"Paper Color","value":[1, 1, 1, 1]},{"id":10,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0]],"name":"Halftone","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Scale",1],["Angle",1],["Softness",1],["PreserveColor",1],["InkColor",3],["PaperColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float c = cos(Angle), sn = sin(Angle);\n    float2 p = float2(c * UV.x - sn * UV.y, sn * UV.x + c * UV.y) * Scale;\n    float2 cell = frac(p) - .5;\n    float l = dot(s.rgb, float3(.2126, .7152, .0722));\n    float dotMask = 1.0 - smoothstep(sqrt(saturate(1.0 - l)) * .48, sqrt(saturate(1.0 - l)) * .48 + Softness, length(cell));\n    float3 mono = lerp(PaperColor.rgb, InkColor.rgb, dotMask);\n    return float4(lerp(mono, s.rgb, saturate(PreserveColor)), s.a);","value":[0, 0, 0, 0]},{"id":11,"in":[[10, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Scale float 96 2 2048 "Scale"
//@param Angle float 0.785 -6.283 6.283 "Angle"
//@param Softness float 0.03 0.0001 0.5 "Softness"
//@param PreserveColor float 0 0 1 "Preserve Color"
//@param InkColor color 0 0 0 1 "Ink Color"
//@param PaperColor color 1 1 1 1 "Paper Color"
// Imported preset (Effect / Stylize): Print-style halftone dots with angle, scale, softness, and color preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Angle, float Softness, float PreserveColor, float3 InkColor, float3 PaperColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float c = cos(Angle), sn = sin(Angle);
    float2 p = float2(c * UV.x - sn * UV.y, sn * UV.x + c * UV.y) * Scale;
    float2 cell = frac(p) - .5;
    float l = dot(s.rgb, float3(.2126, .7152, .0722));
    float dotMask = 1.0 - smoothstep(sqrt(saturate(1.0 - l)) * .48, sqrt(saturate(1.0 - l)) * .48 + Softness, length(cell));
    float3 mono = lerp(PaperColor.rgb, InkColor.rgb, dotMask);
    return float4(lerp(mono, s.rgb, saturate(PreserveColor)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Angle, Softness, PreserveColor, InkColor.rgb, PaperColor.rgb);
}
