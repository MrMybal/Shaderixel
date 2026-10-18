//@name Rust Patches
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Rust Patches","nextId":20,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":128,"min":0.100000001,"name":"Scale","op":"param","pos":[40, 220],"text":"Scale","value":[4, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Threshold","op":"param","pos":[40, 440],"text":"Threshold","value":[0.5, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.0799999982, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":2048,"min":2,"name":"PittingScale","op":"param","pos":[40, 660],"text":"Pitting Scale","value":[180, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Pitting","op":"param","pos":[40, 770],"text":"Pitting","value":[0.0799999982, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"PittingSoftness","op":"param","pos":[40, 880],"text":"Pitting Softness","value":[0.0399999991, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"EdgeWidth","op":"param","pos":[40, 990],"text":"Edge Width","value":[0.119999997, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":16,"min":0.100000001,"name":"EdgeSharpness","op":"param","pos":[40, 1100],"text":"Edge Sharpness","value":[2, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":8,"min":0,"name":"EdgeIntensity","op":"param","pos":[40, 1210],"text":"Edge Intensity","value":[0.300000012, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1320],"text":"Opacity","value":[1, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"BaseColor","op":"paramcolor","pos":[40, 1430],"text":"Base Color","value":[0.0700000003, 0.0799999982, 0.0850000009, 1]},{"id":15,"in":[],"index":1,"name":"RustDark","op":"paramcolor","pos":[40, 1540],"text":"Rust Dark","value":[0.159999996, 0.0250000004, 0.00499999989, 1]},{"id":16,"in":[],"index":1,"name":"RustBright","op":"paramcolor","pos":[40, 1650],"text":"Rust Bright","value":[0.75, 0.159999996, 0.0149999997, 1]},{"id":17,"in":[],"index":1,"name":"EdgeColor","op":"paramcolor","pos":[40, 1760],"text":"Edge Color","value":[1.20000005, 0.319999993, 0.0299999993, 1]},{"id":18,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0],[16, 0],[17, 0]],"name":"Rust Patches","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Threshold",1],["Softness",1],["PittingScale",1],["Pitting",1],["PittingSoftness",1],["EdgeWidth",1],["EdgeSharpness",1],["EdgeIntensity",1],["Opacity",1],["BaseColor",3],["RustDark",3],["RustBright",3],["EdgeColor",3]],"pos":[420, 40],"text":"    float2 p = UV * max(Scale, .01) + Seed;\n    float n = 0, amp = .55;\n    [unroll] for (int i = 0; i < 5; i++) {\n        n += (sin(p.x) + sin(p.y) + sin(p.x + p.y)) * .1667 * amp;\n        p = p * 2.03 + 7.17;\n        amp *= .52;\n    }\n    n = saturate(n * .5 + .5);\n    float patch = smoothstep(Threshold - Softness, Threshold + Softness, n);\n    float pits = frac(sin(dot(floor(UV * PittingScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);\n    float pitMask = smoothstep(1.0 - Pitting, PittingSoftness + 1.0 - Pitting, pits) * patch;\n    float edge = pow(saturate(1.0 - abs(n - Threshold) / max(EdgeWidth, .0001)), EdgeSharpness);\n    float3 rust = lerp(RustDark.rgb, RustBright.rgb, saturate(n + pitMask * .3));\n    float3 rgb = lerp(BaseColor.rgb, rust, patch) + EdgeColor.rgb * edge * EdgeIntensity;\n    return float4(rgb, Opacity);","value":[0, 0, 0, 0]},{"id":19,"in":[[18, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Scale float 4 0.1 128 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Threshold float 0.5 0 1 "Threshold"
//@param Softness float 0.08 0.0001 0.5 "Softness"
//@param PittingScale float 180 2 2048 "Pitting Scale"
//@param Pitting float 0.08 0 1 "Pitting"
//@param PittingSoftness float 0.04 0.0001 0.5 "Pitting Softness"
//@param EdgeWidth float 0.12 0.001 1 "Edge Width"
//@param EdgeSharpness float 2 0.1 16 "Edge Sharpness"
//@param EdgeIntensity float 0.3 0 8 "Edge Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param BaseColor color 0.07 0.08 0.085 1 "Base Color"
//@param RustDark color 0.16 0.025 0.005 1 "Rust Dark"
//@param RustBright color 0.75 0.16 0.015 1 "Rust Bright"
//@param EdgeColor color 1.2 0.32 0.03 1 "Edge Color"
// Imported preset (Draw / Surface): Layered corrosion patches with pitting, scale variation, edge oxidation, and PBR-friendly masks.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Threshold, float Softness, float PittingScale, float Pitting, float PittingSoftness, float EdgeWidth, float EdgeSharpness, float EdgeIntensity, float Opacity, float3 BaseColor, float3 RustDark, float3 RustBright, float3 EdgeColor)
{
    float2 p = UV * max(Scale, .01) + Seed;
    float n = 0, amp = .55;
    [unroll] for (int i = 0; i < 5; i++) {
        n += (sin(p.x) + sin(p.y) + sin(p.x + p.y)) * .1667 * amp;
        p = p * 2.03 + 7.17;
        amp *= .52;
    }
    n = saturate(n * .5 + .5);
    float patch = smoothstep(Threshold - Softness, Threshold + Softness, n);
    float pits = frac(sin(dot(floor(UV * PittingScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);
    float pitMask = smoothstep(1.0 - Pitting, PittingSoftness + 1.0 - Pitting, pits) * patch;
    float edge = pow(saturate(1.0 - abs(n - Threshold) / max(EdgeWidth, .0001)), EdgeSharpness);
    float3 rust = lerp(RustDark.rgb, RustBright.rgb, saturate(n + pitMask * .3));
    float3 rgb = lerp(BaseColor.rgb, rust, patch) + EdgeColor.rgb * edge * EdgeIntensity;
    return float4(rgb, Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Threshold, Softness, PittingScale, Pitting, PittingSoftness, EdgeWidth, EdgeSharpness, EdgeIntensity, Opacity, BaseColor.rgb, RustDark.rgb, RustBright.rgb, EdgeColor.rgb);
}
