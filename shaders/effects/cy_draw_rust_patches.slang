//@name Rust Patches
//@category Dessin
//@graph {"name":"Rust Patches","category":"Dessin","inputs":0,"premultiplied":false,"nextId":20,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.100000001,"max":128.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.5},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[180.0,0.0,0.0,0.0],"name":"PittingScale","text":"Pitting Scale","index":1,"min":2.0,"max":2048.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Pitting","text":"Pitting","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.0399999991,0.0,0.0,0.0],"name":"PittingSoftness","text":"Pitting Softness","index":1,"min":9.99999975e-05,"max":0.5},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.119999997,0.0,0.0,0.0],"name":"EdgeWidth","text":"Edge Width","index":1,"min":0.00100000005,"max":1.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"EdgeSharpness","text":"Edge Sharpness","index":1,"min":0.100000001,"max":16.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.300000012,0.0,0.0,0.0],"name":"EdgeIntensity","text":"Edge Intensity","index":1,"min":0.0,"max":8.0},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.0700000003,0.0799999982,0.0850000009,1.0],"name":"BaseColor","text":"Base Color","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.159999996,0.0250000004,0.00499999989,1.0],"name":"RustDark","text":"Rust Dark","index":1},{"id":16,"op":"paramcolor","pos":[40,1650],"in":[],"value":[0.75,0.159999996,0.0149999997,1.0],"name":"RustBright","text":"Rust Bright","index":1},{"id":17,"op":"paramcolor","pos":[40,1760],"in":[],"value":[1.20000005,0.319999993,0.0299999993,1.0],"name":"EdgeColor","text":"Edge Color","index":1},{"id":18,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0],[16,0],[17,0]],"value":[0,0,0,0],"name":"Rust Patches","text":"    float2 p=UV*max(Scale,.01)+Seed;float n=0,amp=.55;[unroll]for(int i=0;i<5;i++){n+=(sin(p.x)+sin(p.y)+sin(p.x+p.y))*.1667*amp;p=p*2.03+7.17;amp*=.52;}n=saturate(n*.5+.5);float patch=smoothstep(Threshold-Softness,Threshold+Softness,n);float pits=frac(sin(dot(floor(UV*PittingScale)+Seed,float2(12.9898,78.233)))*43758.5453);float pitMask=smoothstep(1.0-Pitting,PittingSoftness+1.0-Pitting,pits)*patch;float edge=pow(saturate(1.0-abs(n-Threshold)/max(EdgeWidth,.0001)),EdgeSharpness);float3 rust=lerp(RustDark.rgb,RustBright.rgb,saturate(n+pitMask*.3));float3 rgb=lerp(BaseColor.rgb,rust,patch)+EdgeColor.rgb*edge*EdgeIntensity;return float4(rgb,Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Threshold",1],["Softness",1],["PittingScale",1],["Pitting",1],["PittingSoftness",1],["EdgeWidth",1],["EdgeSharpness",1],["EdgeIntensity",1],["Opacity",1],["BaseColor",3],["RustDark",3],["RustBright",3],["EdgeColor",3]],"outs":[["Resultat",4]]},{"id":19,"op":"output","pos":[780,40],"in":[[18,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Threshold, float Softness, float PittingScale, float Pitting, float PittingSoftness, float EdgeWidth, float EdgeSharpness, float EdgeIntensity, float Opacity, float3 BaseColor, float3 RustDark, float3 RustBright, float3 EdgeColor)
{
    float2 p=UV*max(Scale,.01)+Seed;float n=0,amp=.55;[unroll]for(int i=0;i<5;i++){n+=(sin(p.x)+sin(p.y)+sin(p.x+p.y))*.1667*amp;p=p*2.03+7.17;amp*=.52;}n=saturate(n*.5+.5);float patch=smoothstep(Threshold-Softness,Threshold+Softness,n);float pits=frac(sin(dot(floor(UV*PittingScale)+Seed,float2(12.9898,78.233)))*43758.5453);float pitMask=smoothstep(1.0-Pitting,PittingSoftness+1.0-Pitting,pits)*patch;float edge=pow(saturate(1.0-abs(n-Threshold)/max(EdgeWidth,.0001)),EdgeSharpness);float3 rust=lerp(RustDark.rgb,RustBright.rgb,saturate(n+pitMask*.3));float3 rgb=lerp(BaseColor.rgb,rust,patch)+EdgeColor.rgb*edge*EdgeIntensity;return float4(rgb,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Threshold, Softness, PittingScale, Pitting, PittingSoftness, EdgeWidth, EdgeSharpness, EdgeIntensity, Opacity, BaseColor.rgb, RustDark.rgb, RustBright.rgb, EdgeColor.rgb);
}
