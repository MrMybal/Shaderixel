//@name Edge Detect
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Edge Detect","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[2, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Blend","op":"param","pos":[40, 550],"text":"Blend","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"EdgeColor","op":"paramcolor","pos":[40, 660],"text":"Edge Color","value":[1, 1, 1, 1]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Edge Detect","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1],["Blend",1],["EdgeColor",3]],"pos":[420, 40],"text":"    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0)) * Radius;\n    float tl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, -t.y)).rgb, float3(.2126, .7152, .0722));\n    float tc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, -t.y)).rgb, float3(.2126, .7152, .0722));\n    float tr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, -t.y)).rgb, float3(.2126, .7152, .0722));\n    float ml = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float mr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float bl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, t.y)).rgb, float3(.2126, .7152, .0722));\n    float bc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722));\n    float br = dot(Texture2DSample(CyInput, CyInputSampler, UV + t).rgb, float3(.2126, .7152, .0722));\n    float gx = -tl - 2 * ml - bl + tr + 2 * mr + br;\n    float gy = -tl - 2 * tc - tr + bl + 2 * bc + br;\n    float e = saturate(length(float2(gx, gy)) * Strength);\n    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);\n    return float4(lerp(src.rgb, EdgeColor.rgb * e, saturate(Blend)), src.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 2 0 32 "Strength"
//@param Blend float 1 0 1 "Blend"
//@param EdgeColor color 1 1 1 1 "Edge Color"
// Imported preset (Effect / Detail): Sobel edge detection with source overlay.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength, float Blend, float3 EdgeColor)
{
    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0)) * Radius;
    float tl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, -t.y)).rgb, float3(.2126, .7152, .0722));
    float tc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, -t.y)).rgb, float3(.2126, .7152, .0722));
    float tr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, -t.y)).rgb, float3(.2126, .7152, .0722));
    float ml = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float mr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float bl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, t.y)).rgb, float3(.2126, .7152, .0722));
    float bc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722));
    float br = dot(Texture2DSample(CyInput, CyInputSampler, UV + t).rgb, float3(.2126, .7152, .0722));
    float gx = -tl - 2 * ml - bl + tr + 2 * mr + br;
    float gy = -tl - 2 * tc - tr + bl + 2 * bc + br;
    float e = saturate(length(float2(gx, gy)) * Strength);
    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);
    return float4(lerp(src.rgb, EdgeColor.rgb * e, saturate(Blend)), src.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength, Blend, EdgeColor.rgb);
}
