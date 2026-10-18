//@name Hue Saturation
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Hue Saturation","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":-1,"name":"Hue","op":"param","pos":[40, 330],"text":"Hue","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":0,"name":"Saturation","op":"param","pos":[40, 440],"text":"Saturation","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0,"name":"Value","op":"param","pos":[40, 550],"text":"Value","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 660],"text":"Strength","value":[1, 0, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Hue Saturation","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Hue",1],["Saturation",1],["Value",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float y = dot(s.rgb, float3(.299, .587, .114));\n    float i = dot(s.rgb, float3(.596, -.274, -.322));\n    float q = dot(s.rgb, float3(.211, -.523, .312));\n    float a = Hue * 6.2831853;\n    float ir = i * cos(a) - q * sin(a);\n    float qr = i * sin(a) + q * cos(a);\n    float3 h = float3(y + 0.956 * ir + 0.621 * qr, y - 0.272 * ir - 0.647 * qr, y - 1.106 * ir + 1.703 * qr);\n    float l = dot(h, float3(.2126, .7152, .0722));\n    h = lerp(l.xxx, h, Saturation) * Value;\n    return float4(lerp(s.rgb, h, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Hue float 0 -1 1 "Hue"
//@param Saturation float 1 0 4 "Saturation"
//@param Value float 1 0 8 "Value"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): YIQ hue rotation with saturation and value controls.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Hue, float Saturation, float Value, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float y = dot(s.rgb, float3(.299, .587, .114));
    float i = dot(s.rgb, float3(.596, -.274, -.322));
    float q = dot(s.rgb, float3(.211, -.523, .312));
    float a = Hue * 6.2831853;
    float ir = i * cos(a) - q * sin(a);
    float qr = i * sin(a) + q * cos(a);
    float3 h = float3(y + 0.956 * ir + 0.621 * qr, y - 0.272 * ir - 0.647 * qr, y - 1.106 * ir + 1.703 * qr);
    float l = dot(h, float3(.2126, .7152, .0722));
    h = lerp(l.xxx, h, Saturation) * Value;
    return float4(lerp(s.rgb, h, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Hue, Saturation, Value, Strength);
}
