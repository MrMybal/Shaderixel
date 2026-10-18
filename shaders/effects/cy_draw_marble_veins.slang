//@name Marble Veins
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Marble Veins","nextId":19,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":64,"min":0.100000001,"name":"Scale","op":"param","pos":[40, 220],"text":"Scale","value":[4, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":-4,"name":"Direction","op":"param","pos":[40, 440],"text":"Direction","value":[0.349999994, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0,"name":"Warp","op":"param","pos":[40, 550],"text":"Warp","value":[1.39999998, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":64,"min":0.100000001,"name":"VeinFrequency","op":"param","pos":[40, 660],"text":"Vein Frequency","value":[4.5, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":8,"min":0.100000001,"name":"VeinWidth","op":"param","pos":[40, 770],"text":"Vein Width","value":[1.25, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":32,"min":0.100000001,"name":"VeinSharpness","op":"param","pos":[40, 880],"text":"Vein Sharpness","value":[3, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":16,"min":0.00999999978,"name":"MicroScale","op":"param","pos":[40, 990],"text":"Micro Scale","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0,"name":"MicroAmount","op":"param","pos":[40, 1100],"text":"Micro Amount","value":[0.119999997, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":16,"min":0,"name":"Intensity","op":"param","pos":[40, 1210],"text":"Intensity","value":[1, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1320],"text":"Opacity","value":[1, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"BaseColor","op":"paramcolor","pos":[40, 1430],"text":"Base Color","value":[0.0799999982, 0.0900000036, 0.100000001, 1]},{"id":15,"in":[],"index":1,"name":"SecondaryColor","op":"paramcolor","pos":[40, 1540],"text":"Secondary Color","value":[0.280000001, 0.300000012, 0.319999993, 1]},{"id":16,"in":[],"index":1,"name":"VeinColor","op":"paramcolor","pos":[40, 1650],"text":"Vein Color","value":[0.850000024, 0.819999993, 0.720000029, 1]},{"id":17,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0],[16, 0]],"name":"Marble Veins","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Direction",1],["Warp",1],["VeinFrequency",1],["VeinWidth",1],["VeinSharpness",1],["MicroScale",1],["MicroAmount",1],["Intensity",1],["Opacity",1],["BaseColor",3],["SecondaryColor",3],["VeinColor",3]],"pos":[420, 40],"text":"    float2 p = (UV - .5) * max(Scale, .01);\n    float n = sin(p.x * 2.1 + sin(p.y * 3.7 + Seed)) * 0.5 + sin(p.y * 5.3 - p.x * 1.7) * .25 + sin((p.x + p.y) * 11.7) * .125;\n    float warp = p.x + p.y * Direction + n * Warp;\n    float vein = pow(saturate(1.0 - abs(sin(warp * VeinFrequency)) * VeinWidth), VeinSharpness);\n    float micro = 0.5 + 0.5 * sin((p.x * 17.3 - p.y * 13.1 + Seed) * MicroScale);\n    vein = saturate(vein + micro * MicroAmount * vein);\n    float3 base = lerp(BaseColor.rgb, SecondaryColor.rgb, saturate(n * .5 + .5));\n    float3 rgb = lerp(base, VeinColor.rgb, vein) * Intensity;\n    return float4(rgb, Opacity);","value":[0, 0, 0, 0]},{"id":18,"in":[[17, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Scale float 4 0.1 64 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Direction float 0.35 -4 4 "Direction"
//@param Warp float 1.4 0 8 "Warp"
//@param VeinFrequency float 4.5 0.1 64 "Vein Frequency"
//@param VeinWidth float 1.25 0.1 8 "Vein Width"
//@param VeinSharpness float 3 0.1 32 "Vein Sharpness"
//@param MicroScale float 1 0.01 16 "Micro Scale"
//@param MicroAmount float 0.12 0 2 "Micro Amount"
//@param Intensity float 1 0 16 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param BaseColor color 0.08 0.09 0.1 1 "Base Color"
//@param SecondaryColor color 0.28 0.3 0.32 1 "Secondary Color"
//@param VeinColor color 0.85 0.82 0.72 1 "Vein Color"
// Imported preset (Draw / Surface): Layered marble veins with turbulent domain warping and HDR mineral highlights.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Direction, float Warp, float VeinFrequency, float VeinWidth, float VeinSharpness, float MicroScale, float MicroAmount, float Intensity, float Opacity, float3 BaseColor, float3 SecondaryColor, float3 VeinColor)
{
    float2 p = (UV - .5) * max(Scale, .01);
    float n = sin(p.x * 2.1 + sin(p.y * 3.7 + Seed)) * 0.5 + sin(p.y * 5.3 - p.x * 1.7) * .25 + sin((p.x + p.y) * 11.7) * .125;
    float warp = p.x + p.y * Direction + n * Warp;
    float vein = pow(saturate(1.0 - abs(sin(warp * VeinFrequency)) * VeinWidth), VeinSharpness);
    float micro = 0.5 + 0.5 * sin((p.x * 17.3 - p.y * 13.1 + Seed) * MicroScale);
    vein = saturate(vein + micro * MicroAmount * vein);
    float3 base = lerp(BaseColor.rgb, SecondaryColor.rgb, saturate(n * .5 + .5));
    float3 rgb = lerp(base, VeinColor.rgb, vein) * Intensity;
    return float4(rgb, Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Direction, Warp, VeinFrequency, VeinWidth, VeinSharpness, MicroScale, MicroAmount, Intensity, Opacity, BaseColor.rgb, SecondaryColor.rgb, VeinColor.rgb);
}
