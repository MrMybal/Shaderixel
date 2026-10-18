//@name Vibrance
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Vibrance","nextId":8,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":-2,"name":"Vibrance","op":"param","pos":[40, 330],"text":"Vibrance","value":[0.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[1, 0, 0, 0]},{"id":6,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0]],"name":"Vibrance","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Vibrance",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float mx = max(max(s.r, s.g), s.b);\n    float mn = min(min(s.r, s.g), s.b);\n    float sat = mx - mn;\n    float l = dot(s.rgb, float3(.2126, .7152, .0722));\n    float amount = Vibrance * (1.0 - saturate(sat));\n    float3 v = lerp(l.xxx, s.rgb, 1.0 + amount);\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":7,"in":[[6, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Vibrance float 0.5 -2 2 "Vibrance"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Adaptive saturation that protects already saturated colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Vibrance, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float mx = max(max(s.r, s.g), s.b);
    float mn = min(min(s.r, s.g), s.b);
    float sat = mx - mn;
    float l = dot(s.rgb, float3(.2126, .7152, .0722));
    float amount = Vibrance * (1.0 - saturate(sat));
    float3 v = lerp(l.xxx, s.rgb, 1.0 + amount);
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Vibrance, Strength);
}
