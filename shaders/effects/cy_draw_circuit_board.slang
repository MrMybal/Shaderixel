//@name Circuit Board
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Circuit Board","nextId":19,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":256,"min":2,"name":"Grid","op":"param","pos":[40, 220],"text":"Grid","value":[18, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.449999988,"min":0.00100000005,"name":"TraceWidth","op":"param","pos":[40, 440],"text":"Trace Width","value":[0.0549999997, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.699999988,"min":0.00100000005,"name":"PadRadius","op":"param","pos":[40, 550],"text":"Pad Radius","value":[0.140000001, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":0.200000003,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 660],"text":"Softness","value":[0.0149999997, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"BranchDensity","op":"param","pos":[40, 770],"text":"Branch Density","value":[0.349999994, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"PadDensity","op":"param","pos":[40, 880],"text":"Pad Density","value":[0.720000029, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":32,"min":0,"name":"PulseFrequency","op":"param","pos":[40, 990],"text":"Pulse Frequency","value":[1.5, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":20,"min":-20,"name":"Phase","op":"param","pos":[40, 1100],"text":"Phase","value":[0, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"PulseAmount","op":"param","pos":[40, 1210],"text":"Pulse Amount","value":[0, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"TraceOpacity","op":"param","pos":[40, 1320],"text":"Trace Opacity","value":[1, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"BoardOpacity","op":"param","pos":[40, 1430],"text":"Board Opacity","value":[1, 0, 0, 0]},{"id":15,"in":[],"index":1,"name":"TraceColor","op":"paramcolor","pos":[40, 1540],"text":"Trace Color","value":[0.0500000007, 1.60000002, 0.449999988, 1]},{"id":16,"in":[],"index":1,"name":"BoardColor","op":"paramcolor","pos":[40, 1650],"text":"Board Color","value":[0.00499999989, 0.0250000004, 0.0120000001, 1]},{"id":17,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0],[16, 0]],"name":"Circuit Board","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Grid",1],["Seed",1],["TraceWidth",1],["PadRadius",1],["Softness",1],["BranchDensity",1],["PadDensity",1],["PulseFrequency",1],["Phase",1],["PulseAmount",1],["TraceOpacity",1],["BoardOpacity",1],["TraceColor",3],["BoardColor",3]],"pos":[420, 40],"text":"    float2 q = UV * max(Grid, 1.0);\n    float2 id = floor(q);\n    float2 p = frac(q) - 0.5;\n    float r = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);\n    float axis = lerp(abs(p.x), abs(p.y), step(0.5, r));\n    float trace = 1.0 - smoothstep(TraceWidth, TraceWidth + Softness, axis);\n    float pad = 1.0 - smoothstep(PadRadius, PadRadius + Softness, length(p));\n    float gate = step(BranchDensity, frac(r * 17.31));\n    float m = max(trace * gate, pad * step(PadDensity, r));\n    float pulse = lerp(1.0, 0.55 + 0.45 * sin((q.x + q.y) * PulseFrequency + Phase), PulseAmount);\n    return float4(lerp(BoardColor.rgb, TraceColor.rgb, m * pulse), lerp(BoardOpacity, TraceOpacity, m));","value":[0, 0, 0, 0]},{"id":18,"in":[[17, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Grid float 18 2 256 "Grid"
//@param Seed float 0 -1000 1000 "Seed"
//@param TraceWidth float 0.055 0.001 0.45 "Trace Width"
//@param PadRadius float 0.14 0.001 0.7 "Pad Radius"
//@param Softness float 0.015 0.0001 0.2 "Softness"
//@param BranchDensity float 0.35 0 1 "Branch Density"
//@param PadDensity float 0.72 0 1 "Pad Density"
//@param PulseFrequency float 1.5 0 32 "Pulse Frequency"
//@param Phase float 0 -20 20 "Phase"
//@param PulseAmount float 0 0 1 "Pulse Amount"
//@param TraceOpacity float 1 0 1 "Trace Opacity"
//@param BoardOpacity float 1 0 1 "Board Opacity"
//@param TraceColor color 0.05 1.6 0.45 1 "Trace Color"
//@param BoardColor color 0.005 0.025 0.012 1 "Board Color"
// Imported preset (Draw / Pattern): Procedural circuit traces, pads, and substrate for technical masks and graphic design.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Grid, float Seed, float TraceWidth, float PadRadius, float Softness, float BranchDensity, float PadDensity, float PulseFrequency, float Phase, float PulseAmount, float TraceOpacity, float BoardOpacity, float3 TraceColor, float3 BoardColor)
{
    float2 q = UV * max(Grid, 1.0);
    float2 id = floor(q);
    float2 p = frac(q) - 0.5;
    float r = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);
    float axis = lerp(abs(p.x), abs(p.y), step(0.5, r));
    float trace = 1.0 - smoothstep(TraceWidth, TraceWidth + Softness, axis);
    float pad = 1.0 - smoothstep(PadRadius, PadRadius + Softness, length(p));
    float gate = step(BranchDensity, frac(r * 17.31));
    float m = max(trace * gate, pad * step(PadDensity, r));
    float pulse = lerp(1.0, 0.55 + 0.45 * sin((q.x + q.y) * PulseFrequency + Phase), PulseAmount);
    return float4(lerp(BoardColor.rgb, TraceColor.rgb, m * pulse), lerp(BoardOpacity, TraceOpacity, m));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Grid, Seed, TraceWidth, PadRadius, Softness, BranchDensity, PadDensity, PulseFrequency, Phase, PulseAmount, TraceOpacity, BoardOpacity, TraceColor.rgb, BoardColor.rgb);
}
