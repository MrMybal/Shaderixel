//@name Circuit Board
//@category Dessin
//@graph {"name":"Circuit Board","category":"Dessin","inputs":0,"premultiplied":false,"nextId":19,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[18.0,0.0,0.0,0.0],"name":"Grid","text":"Grid","index":1,"min":2.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0549999997,0.0,0.0,0.0],"name":"TraceWidth","text":"Trace Width","index":1,"min":0.00100000005,"max":0.449999988},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.140000001,0.0,0.0,0.0],"name":"PadRadius","text":"Pad Radius","index":1,"min":0.00100000005,"max":0.699999988},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0149999997,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.200000003},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"BranchDensity","text":"Branch Density","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.720000029,0.0,0.0,0.0],"name":"PadDensity","text":"Pad Density","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"PulseFrequency","text":"Pulse Frequency","index":1,"min":0.0,"max":32.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-20.0,"max":20.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"PulseAmount","text":"Pulse Amount","index":1},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"TraceOpacity","text":"Trace Opacity","index":1},{"id":14,"op":"param","pos":[40,1430],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BoardOpacity","text":"Board Opacity","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.0500000007,1.60000002,0.449999988,1.0],"name":"TraceColor","text":"Trace Color","index":1},{"id":16,"op":"paramcolor","pos":[40,1650],"in":[],"value":[0.00499999989,0.0250000004,0.0120000001,1.0],"name":"BoardColor","text":"Board Color","index":1},{"id":17,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0],[16,0]],"value":[0,0,0,0],"name":"Circuit Board","text":"    float2 q=UV*max(Grid,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float r=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); float axis=lerp(abs(p.x),abs(p.y),step(0.5,r)); float trace=1.0-smoothstep(TraceWidth,TraceWidth+Softness,axis); float pad=1.0-smoothstep(PadRadius,PadRadius+Softness,length(p)); float gate=step(BranchDensity,frac(r*17.31)); float m=max(trace*gate,pad*step(PadDensity,r)); float pulse=lerp(1.0,0.55+0.45*sin((q.x+q.y)*PulseFrequency+Phase),PulseAmount); return float4(lerp(BoardColor.rgb,TraceColor.rgb,m*pulse),lerp(BoardOpacity,TraceOpacity,m));","pins":[["UV",2],["CyCanvasSize",4],["Grid",1],["Seed",1],["TraceWidth",1],["PadRadius",1],["Softness",1],["BranchDensity",1],["PadDensity",1],["PulseFrequency",1],["Phase",1],["PulseAmount",1],["TraceOpacity",1],["BoardOpacity",1],["TraceColor",3],["BoardColor",3]],"outs":[["Resultat",4]]},{"id":18,"op":"output","pos":[780,40],"in":[[17,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Grid, float Seed, float TraceWidth, float PadRadius, float Softness, float BranchDensity, float PadDensity, float PulseFrequency, float Phase, float PulseAmount, float TraceOpacity, float BoardOpacity, float3 TraceColor, float3 BoardColor)
{
    float2 q=UV*max(Grid,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float r=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); float axis=lerp(abs(p.x),abs(p.y),step(0.5,r)); float trace=1.0-smoothstep(TraceWidth,TraceWidth+Softness,axis); float pad=1.0-smoothstep(PadRadius,PadRadius+Softness,length(p)); float gate=step(BranchDensity,frac(r*17.31)); float m=max(trace*gate,pad*step(PadDensity,r)); float pulse=lerp(1.0,0.55+0.45*sin((q.x+q.y)*PulseFrequency+Phase),PulseAmount); return float4(lerp(BoardColor.rgb,TraceColor.rgb,m*pulse),lerp(BoardOpacity,TraceOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Grid, Seed, TraceWidth, PadRadius, Softness, BranchDensity, PadDensity, PulseFrequency, Phase, PulseAmount, TraceOpacity, BoardOpacity, TraceColor.rgb, BoardColor.rgb);
}
