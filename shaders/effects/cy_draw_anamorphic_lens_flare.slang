//@name Anamorphic Lens Flare
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Anamorphic Lens Flare","nextId":17,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":8,"min":0.100000001,"name":"Aspect","op":"param","pos":[40, 220],"text":"Aspect","value":[1, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":128,"min":0.100000001,"name":"CoreFalloff","op":"param","pos":[40, 330],"text":"Core Falloff","value":[18, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":0,"name":"CoreIntensity","op":"param","pos":[40, 440],"text":"Core Intensity","value":[6, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.25,"min":9.99999975e-05,"name":"StreakWidth","op":"param","pos":[40, 550],"text":"Streak Width","value":[0.00600000005, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":4,"min":0.00100000005,"name":"StreakLength","op":"param","pos":[40, 660],"text":"Streak Length","value":[0.75, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":16,"min":0.100000001,"name":"StreakFalloff","op":"param","pos":[40, 770],"text":"Streak Falloff","value":[2, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":64,"min":0,"name":"StreakIntensity","op":"param","pos":[40, 880],"text":"Streak Intensity","value":[4, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":0.100000001,"min":0,"name":"ColorSplit","op":"param","pos":[40, 990],"text":"Color Split","value":[0.00400000019, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1100],"text":"Opacity","value":[1, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1210],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":13,"in":[],"index":1,"name":"CoreColor","op":"paramcolor","pos":[40, 1320],"text":"Core Color","value":[1, 0.75, 0.349999994, 1]},{"id":14,"in":[],"index":1,"name":"StreakColor","op":"paramcolor","pos":[40, 1430],"text":"Streak Color","value":[0.180000007, 0.449999988, 1, 1]},{"id":15,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0]],"name":"Anamorphic Lens Flare","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["CoreFalloff",1],["CoreIntensity",1],["StreakWidth",1],["StreakLength",1],["StreakFalloff",1],["StreakIntensity",1],["ColorSplit",1],["Opacity",1],["Center",3],["CoreColor",3],["StreakColor",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    float core = exp(-length(float2(p.x * Aspect, p.y)) * CoreFalloff) * CoreIntensity;\n    float streak = exp(-abs(p.y) / max(StreakWidth, 0.0001)) * pow(saturate(1.0 - abs(p.x) / max(StreakLength, 0.0001)), StreakFalloff) * StreakIntensity;\n    float blue = exp(-abs(p.y + ColorSplit) / max(StreakWidth, 0.0001));\n    float red = exp(-abs(p.y - ColorSplit) / max(StreakWidth, 0.0001));\n    float3 rgb = CoreColor.rgb * core + StreakColor.rgb * streak * float3(red, 1.0, blue);\n    float a = saturate(max(max(rgb.r, rgb.g), rgb.b) * Opacity);\n    return float4(rgb * Opacity, a);","value":[0, 0, 0, 0]},{"id":16,"in":[[15, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param CoreFalloff float 18 0.1 128 "Core Falloff"
//@param CoreIntensity float 6 0 64 "Core Intensity"
//@param StreakWidth float 0.006 0.0001 0.25 "Streak Width"
//@param StreakLength float 0.75 0.001 4 "Streak Length"
//@param StreakFalloff float 2 0.1 16 "Streak Falloff"
//@param StreakIntensity float 4 0 64 "Streak Intensity"
//@param ColorSplit float 0.004 0 0.1 "Color Split"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param CoreColor color 1 0.75 0.35 1 "Core Color"
//@param StreakColor color 0.18 0.45 1 1 "Streak Color"
// Imported preset (Draw / Light & Space): Wide cinematic anamorphic flare with horizontal streaks and spectral split.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float CoreFalloff, float CoreIntensity, float StreakWidth, float StreakLength, float StreakFalloff, float StreakIntensity, float ColorSplit, float Opacity, float3 Center, float3 CoreColor, float3 StreakColor)
{
    float2 p = UV - Center.xy;
    float core = exp(-length(float2(p.x * Aspect, p.y)) * CoreFalloff) * CoreIntensity;
    float streak = exp(-abs(p.y) / max(StreakWidth, 0.0001)) * pow(saturate(1.0 - abs(p.x) / max(StreakLength, 0.0001)), StreakFalloff) * StreakIntensity;
    float blue = exp(-abs(p.y + ColorSplit) / max(StreakWidth, 0.0001));
    float red = exp(-abs(p.y - ColorSplit) / max(StreakWidth, 0.0001));
    float3 rgb = CoreColor.rgb * core + StreakColor.rgb * streak * float3(red, 1.0, blue);
    float a = saturate(max(max(rgb.r, rgb.g), rgb.b) * Opacity);
    return float4(rgb * Opacity, a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, CoreFalloff, CoreIntensity, StreakWidth, StreakLength, StreakFalloff, StreakIntensity, ColorSplit, Opacity, float3(Center, 0.0), CoreColor.rgb, StreakColor.rgb);
}
