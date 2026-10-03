//@name Cracks
//@category Dessin
//@graph {"name":"Cracks","category":"Dessin","inputs":0,"premultiplied":false,"nextId":17,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[14.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":1.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0350000001,0.0,0.0,0.0],"name":"Width","text":"Width","index":1,"min":0.00100000005,"max":0.5},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0120000001,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.200000003},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.219999999,0.0,0.0,0.0],"name":"Breakup","text":"Breakup","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.180000007,0.0,0.0,0.0],"name":"BreakupAmount","text":"Breakup Amount","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.879999995,0.0,0.0,0.0],"name":"FillThreshold","text":"Fill Threshold","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"FillAmount","text":"Fill Amount","index":1},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"CrackOpacity","text":"Crack Opacity","index":1},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"BackgroundOpacity","text":"Background Opacity","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"CrackColor","text":"Crack Color","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"BackgroundColor","text":"Background Color","index":1},{"id":15,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0]],"value":[0,0,0,0],"name":"Cracks","text":"    float2 p=UV*max(Scale,1.0);float2 ip=floor(p),fp=frac(p);float nearest=10.0,second=10.0,cellValue=0.0;[unroll]for(int y=-1;y<=1;y++){[unroll]for(int x=-1;x<=1;x++){float2 o=float2(x,y),cell=ip+o;float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+Seed)*43758.5453);float d=length(o+rnd-fp);if(d<nearest){second=nearest;nearest=d;cellValue=rnd.x;}else if(d<second)second=d;}}float edge=second-nearest;float crack=1.0-smoothstep(Width,Width+Softness,edge);float breakup=step(Breakup,frac(sin((ip.x+ip.y+cellValue)*37.13+Seed)*43758.5453));crack*=lerp(1.0,breakup,BreakupAmount);float fill=step(FillThreshold,cellValue)*FillAmount;float m=max(crack,fill);return float4(lerp(BackgroundColor.rgb,CrackColor.rgb,m),lerp(BackgroundOpacity,CrackOpacity,m));","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Width",1],["Softness",1],["Breakup",1],["BreakupAmount",1],["FillThreshold",1],["FillAmount",1],["CrackOpacity",1],["BackgroundOpacity",1],["CrackColor",3],["BackgroundColor",3]],"outs":[["Resultat",4]]},{"id":16,"op":"output","pos":[780,40],"in":[[15,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Scale float 14 1 256 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Width float 0.035 0.001 0.5 "Width"
//@param Softness float 0.012 0.0001 0.2 "Softness"
//@param Breakup float 0.22 0 1 "Breakup"
//@param BreakupAmount float 0.18 0 1 "Breakup Amount"
//@param FillThreshold float 0.88 0 1 "Fill Threshold"
//@param FillAmount float 0 0 1 "Fill Amount"
//@param CrackOpacity float 1 0 1 "Crack Opacity"
//@param BackgroundOpacity float 0 0 1 "Background Opacity"
//@param CrackColor color 1 1 1 1 "Crack Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Surface): Voronoi fracture network with branching breakup, variable width, and optional filled cells.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Width, float Softness, float Breakup, float BreakupAmount, float FillThreshold, float FillAmount, float CrackOpacity, float BackgroundOpacity, float3 CrackColor, float3 BackgroundColor)
{
    float2 p=UV*max(Scale,1.0);float2 ip=floor(p),fp=frac(p);float nearest=10.0,second=10.0,cellValue=0.0;[unroll]for(int y=-1;y<=1;y++){[unroll]for(int x=-1;x<=1;x++){float2 o=float2(x,y),cell=ip+o;float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+Seed)*43758.5453);float d=length(o+rnd-fp);if(d<nearest){second=nearest;nearest=d;cellValue=rnd.x;}else if(d<second)second=d;}}float edge=second-nearest;float crack=1.0-smoothstep(Width,Width+Softness,edge);float breakup=step(Breakup,frac(sin((ip.x+ip.y+cellValue)*37.13+Seed)*43758.5453));crack*=lerp(1.0,breakup,BreakupAmount);float fill=step(FillThreshold,cellValue)*FillAmount;float m=max(crack,fill);return float4(lerp(BackgroundColor.rgb,CrackColor.rgb,m),lerp(BackgroundOpacity,CrackOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Width, Softness, Breakup, BreakupAmount, FillThreshold, FillAmount, CrackOpacity, BackgroundOpacity, CrackColor.rgb, BackgroundColor.rgb);
}
