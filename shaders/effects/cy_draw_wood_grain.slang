//@name Wood Grain
//@category Dessin
//@graph {"name":"Wood Grain","category":"Dessin","inputs":0,"premultiplied":false,"nextId":18,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[7.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.100000001,"max":128.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-6.28299999,"max":6.28299999},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[2.79999995,0.0,0.0,0.0],"name":"WarpFrequency","text":"Warp Frequency","index":1,"min":0.100000001,"max":64.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.219999999,0.0,0.0,0.0],"name":"WarpAmount","text":"Warp Amount","index":1,"min":0.0,"max":2.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"RingWobble","text":"Ring Wobble","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[2.20000005,0.0,0.0,0.0],"name":"RingSharpness","text":"Ring Sharpness","index":1,"min":0.100000001,"max":16.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[38.0,0.0,0.0,0.0],"name":"PoreScale","text":"Pore Scale","index":1,"min":1.0,"max":1024.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"PoreAmount","text":"Pore Amount","index":1},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"PoreStrength","text":"Pore Strength","index":1,"min":0.0,"max":2.0},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.119999997,0.0350000001,0.00800000038,1.0],"name":"EarlyWood","text":"Early Wood","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.550000012,0.219999999,0.0450000018,1.0],"name":"LateWood","text":"Late Wood","index":1},{"id":16,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0]],"value":[0,0,0,0],"name":"Wood Grain","text":"    float2 p=UV*max(Scale,0.01);float c=cos(Rotation),s=sin(Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float warp=sin(p.y*WarpFrequency+Seed)+.5*sin(p.y*WarpFrequency*2.17-p.x*.7+Seed);float rings=frac((p.x+warp*WarpAmount)+sin(p.y*1.7)*RingWobble);float band=pow(saturate(1.0-abs(rings-.5)*2.0),RingSharpness);float pore=frac(sin(dot(floor(p*PoreScale)+Seed,float2(12.9898,78.233)))*43758.5453);float pores=smoothstep(1.0-PoreAmount,1.0,pore)*PoreStrength;float3 rgb=lerp(EarlyWood.rgb,LateWood.rgb,saturate(band+pores));return float4(rgb,Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Rotation",1],["Seed",1],["WarpFrequency",1],["WarpAmount",1],["RingWobble",1],["RingSharpness",1],["PoreScale",1],["PoreAmount",1],["PoreStrength",1],["Opacity",1],["EarlyWood",3],["LateWood",3]],"outs":[["Resultat",4]]},{"id":17,"op":"output","pos":[780,40],"in":[[16,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Scale float 7 0.1 128 "Scale"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Seed float 0 -1000 1000 "Seed"
//@param WarpFrequency float 2.8 0.1 64 "Warp Frequency"
//@param WarpAmount float 0.22 0 2 "Warp Amount"
//@param RingWobble float 0.08 0 1 "Ring Wobble"
//@param RingSharpness float 2.2 0.1 16 "Ring Sharpness"
//@param PoreScale float 38 1 1024 "Pore Scale"
//@param PoreAmount float 0.08 0 1 "Pore Amount"
//@param PoreStrength float 0.25 0 2 "Pore Strength"
//@param Opacity float 1 0 1 "Opacity"
//@param EarlyWood color 0.12 0.035 0.008 1 "Early Wood"
//@param LateWood color 0.55 0.22 0.045 1 "Late Wood"

// Imported preset (Draw / Surface): Seamless procedural wood rings with warped fibers, pores, and adjustable early/late wood color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Rotation, float Seed, float WarpFrequency, float WarpAmount, float RingWobble, float RingSharpness, float PoreScale, float PoreAmount, float PoreStrength, float Opacity, float3 EarlyWood, float3 LateWood)
{
    float2 p=UV*max(Scale,0.01);float c=cos(Rotation),s=sin(Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float warp=sin(p.y*WarpFrequency+Seed)+.5*sin(p.y*WarpFrequency*2.17-p.x*.7+Seed);float rings=frac((p.x+warp*WarpAmount)+sin(p.y*1.7)*RingWobble);float band=pow(saturate(1.0-abs(rings-.5)*2.0),RingSharpness);float pore=frac(sin(dot(floor(p*PoreScale)+Seed,float2(12.9898,78.233)))*43758.5453);float pores=smoothstep(1.0-PoreAmount,1.0,pore)*PoreStrength;float3 rgb=lerp(EarlyWood.rgb,LateWood.rgb,saturate(band+pores));return float4(rgb,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Rotation, Seed, WarpFrequency, WarpAmount, RingWobble, RingSharpness, PoreScale, PoreAmount, PoreStrength, Opacity, EarlyWood.rgb, LateWood.rgb);
}
