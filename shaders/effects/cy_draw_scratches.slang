//@name Scratches
//@category Dessin
//@graph {"name":"Scratches","category":"Dessin","inputs":0,"premultiplied":false,"nextId":21,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[72.0,0.0,0.0,0.0],"name":"Cells","text":"Cells","index":1,"min":4.0,"max":1024.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.140000001,0.0,0.0,0.0],"name":"Density","text":"Density","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"BaseAngle","text":"Base Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"AngleJitter","text":"Angle Jitter","index":1,"min":0.0,"max":6.28299999},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0250000004,0.0,0.0,0.0],"name":"Width","text":"Width","index":1,"min":0.00100000005,"max":0.300000012},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.00800000038,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.200000003},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.119999997,0.0,0.0,0.0],"name":"MinLength","text":"Min Length","index":1,"min":0.00999999978,"max":0.699999988},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.479999989,0.0,0.0,0.0],"name":"MaxLength","text":"Max Length","index":1,"min":0.00999999978,"max":0.800000012},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"Breakup","text":"Breakup","index":1},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[35.0,0.0,0.0,0.0],"name":"BreakupScale","text":"Breakup Scale","index":1,"min":1.0,"max":512.0},{"id":14,"op":"param","pos":[40,1430],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"BreakupAmount","text":"Breakup Amount","index":1},{"id":15,"op":"param","pos":[40,1540],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"ScratchOpacity","text":"Scratch Opacity","index":1},{"id":16,"op":"param","pos":[40,1650],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"BackgroundOpacity","text":"Background Opacity","index":1},{"id":17,"op":"paramcolor","pos":[40,1760],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ScratchColor","text":"Scratch Color","index":1},{"id":18,"op":"paramcolor","pos":[40,1870],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"BackgroundColor","text":"Background Color","index":1},{"id":19,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0],[16,0],[17,0],[18,0]],"value":[0,0,0,0],"name":"Scratches","text":"    float2 p=UV*max(Cells,1.0);float2 id=floor(p);float2 f=frac(p)-.5;float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453);float h2=frac(sin(dot(id+Seed+9.7,float2(269.5,183.3)))*43758.5453);float angle=BaseAngle+(h-.5)*AngleJitter;float c=cos(angle),s=sin(angle);float2 r=float2(c*f.x-s*f.y,s*f.x+c*f.y);float active=step(1.0-Density,h);float scratch=(1.0-smoothstep(Width,Width+Softness,abs(r.y)))*step(abs(r.x),lerp(MinLength,MaxLength,h2));float breakup=step(Breakup,frac(sin((r.x+id.x)*BreakupScale+Seed)*43758.5453));float m=active*scratch*lerp(1.0,breakup,BreakupAmount);return float4(lerp(BackgroundColor.rgb,ScratchColor.rgb,m),lerp(BackgroundOpacity,ScratchOpacity,m));","pins":[["UV",2],["CyCanvasSize",4],["Cells",1],["Seed",1],["Density",1],["BaseAngle",1],["AngleJitter",1],["Width",1],["Softness",1],["MinLength",1],["MaxLength",1],["Breakup",1],["BreakupScale",1],["BreakupAmount",1],["ScratchOpacity",1],["BackgroundOpacity",1],["ScratchColor",3],["BackgroundColor",3]],"outs":[["Resultat",4]]},{"id":20,"op":"output","pos":[780,40],"in":[[19,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Cells float 72 4 1024 "Cells"
//@param Seed float 0 -1000 1000 "Seed"
//@param Density float 0.14 0 1 "Density"
//@param BaseAngle float 0 -6.283 6.283 "Base Angle"
//@param AngleJitter float 0.35 0 6.283 "Angle Jitter"
//@param Width float 0.025 0.001 0.3 "Width"
//@param Softness float 0.008 0.0001 0.2 "Softness"
//@param MinLength float 0.12 0.01 0.7 "Min Length"
//@param MaxLength float 0.48 0.01 0.8 "Max Length"
//@param Breakup float 0.35 0 1 "Breakup"
//@param BreakupScale float 35 1 512 "Breakup Scale"
//@param BreakupAmount float 0.25 0 1 "Breakup Amount"
//@param ScratchOpacity float 1 0 1 "Scratch Opacity"
//@param BackgroundOpacity float 0 0 1 "Background Opacity"
//@param ScratchColor color 1 1 1 1 "Scratch Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Surface): Seeded multi-scale directional scratch mask with length, width, density, and edge breakup.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Cells, float Seed, float Density, float BaseAngle, float AngleJitter, float Width, float Softness, float MinLength, float MaxLength, float Breakup, float BreakupScale, float BreakupAmount, float ScratchOpacity, float BackgroundOpacity, float3 ScratchColor, float3 BackgroundColor)
{
    float2 p=UV*max(Cells,1.0);float2 id=floor(p);float2 f=frac(p)-.5;float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453);float h2=frac(sin(dot(id+Seed+9.7,float2(269.5,183.3)))*43758.5453);float angle=BaseAngle+(h-.5)*AngleJitter;float c=cos(angle),s=sin(angle);float2 r=float2(c*f.x-s*f.y,s*f.x+c*f.y);float active=step(1.0-Density,h);float scratch=(1.0-smoothstep(Width,Width+Softness,abs(r.y)))*step(abs(r.x),lerp(MinLength,MaxLength,h2));float breakup=step(Breakup,frac(sin((r.x+id.x)*BreakupScale+Seed)*43758.5453));float m=active*scratch*lerp(1.0,breakup,BreakupAmount);return float4(lerp(BackgroundColor.rgb,ScratchColor.rgb,m),lerp(BackgroundOpacity,ScratchOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Cells, Seed, Density, BaseAngle, AngleJitter, Width, Softness, MinLength, MaxLength, Breakup, BreakupScale, BreakupAmount, ScratchOpacity, BackgroundOpacity, ScratchColor.rgb, BackgroundColor.rgb);
}
