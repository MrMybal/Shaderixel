//@name Vignette
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Vignette","nextId":12,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 330],"text":"Aspect","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":2,"min":0.00999999978,"name":"Radius","op":"param","pos":[40, 440],"text":"Radius","value":[0.449999988, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.25, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Intensity","op":"param","pos":[40, 660],"text":"Intensity","value":[0.800000012, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 770],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":9,"in":[],"index":1,"name":"VignetteColor","op":"paramcolor","pos":[40, 880],"text":"Vignette Color","value":[0, 0, 0, 1]},{"id":10,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0]],"name":"Vignette","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Aspect",1],["Radius",1],["Softness",1],["Intensity",1],["Center",3],["VignetteColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float d = length(p);\n    float v = smoothstep(Radius - Softness, Radius + Softness, d);\n    float3 c = lerp(s.rgb, s.rgb * VignetteColor.rgb, v * Intensity);\n    return float4(c, s.a);","value":[0, 0, 0, 0]},{"id":11,"in":[[10, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Aspect float 1 0.01 8 "Aspect"
//@param Radius float 0.45 0.01 2 "Radius"
//@param Softness float 0.25 0.001 1 "Softness"
//@param Intensity float 0.8 0 1 "Intensity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param VignetteColor color 0 0 0 1 "Vignette Color"
// Imported preset (Effect / Color & Tone): Elliptical soft vignette with tint and HDR intensity.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float Softness, float Intensity, float3 Center, float3 VignetteColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float d = length(p);
    float v = smoothstep(Radius - Softness, Radius + Softness, d);
    float3 c = lerp(s.rgb, s.rgb * VignetteColor.rgb, v * Intensity);
    return float4(c, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, Softness, Intensity, float3(Center, 0.0), VignetteColor.rgb);
}
