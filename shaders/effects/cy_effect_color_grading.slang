//@name Color Grading
//@category Traitements
//@graph {"name":"Color Grading","category":"Traitements","inputs":1,"premultiplied":false,"nextId":18,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"LiftLuma","text":"Lift Luma","index":1,"min":-2.0,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"GammaLuma","text":"Gamma Luma","index":1,"min":-0.949999988,"max":4.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"GainLuma","text":"Gain Luma","index":1,"min":-1.0,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Temperature","text":"Temperature","index":1,"min":-1.0,"max":1.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Tint","text":"Tint","index":1,"min":-1.0,"max":1.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Saturation","text":"Saturation","index":1,"min":0.0,"max":4.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":4.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"ContrastPivot","text":"Contrast Pivot","index":1,"min":0.0,"max":2.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[0.0,0.0,1.0,1.0],"name":"LiftWheel","text":"Lift Wheel","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.0,0.0,1.0,1.0],"name":"GammaWheel","text":"Gamma Wheel","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.0,0.0,1.0,1.0],"name":"GainWheel","text":"Gain Wheel","index":1},{"id":16,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0]],"value":[0,0,0,0],"name":"Color Grading","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 lp=abs(frac(LiftWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 gp=abs(frac(GammaWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 ap=abs(frac(GainWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 lc=lerp(1.0.xxx,saturate(lp-1.0),saturate(LiftWheel.g)); float3 gc=lerp(1.0.xxx,saturate(gp-1.0),saturate(GammaWheel.g)); float3 ac=lerp(1.0.xxx,saturate(ap-1.0),saturate(GainWheel.g)); lc-=dot(lc,float3(.333333,.333333,.333333)); gc-=dot(gc,float3(.333333,.333333,.333333)); ac-=dot(ac,float3(.333333,.333333,.333333)); float3 v=max(s.rgb+LiftLuma+lc*0.25,0.0); v=pow(v,1.0/max(1.0+GammaLuma+gc*0.35,0.05)); v*=max(1.0+GainLuma+ac*0.5,0.0); v*=max(float3(1.0+Temperature*.12-Tint*.04,1.0+Tint*.08,1.0-Temperature*.12-Tint*.04),.001); float l=dot(v,float3(.2126,.7152,.0722)); v=lerp(l.xxx,v,Saturation); v=(v-ContrastPivot)*Contrast+ContrastPivot; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["LiftLuma",1],["GammaLuma",1],["GainLuma",1],["Temperature",1],["Tint",1],["Saturation",1],["Contrast",1],["ContrastPivot",1],["Strength",1],["LiftWheel",3],["GammaWheel",3],["GainWheel",3]],"outs":[["Resultat",4]]},{"id":17,"op":"output","pos":[780,40],"in":[[16,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param LiftLuma float 0 -2 2 "Lift Luma"
//@param GammaLuma float 0 -0.95 4 "Gamma Luma"
//@param GainLuma float 0 -1 8 "Gain Luma"
//@param Temperature float 0 -1 1 "Temperature"
//@param Tint float 0 -1 1 "Tint"
//@param Saturation float 1 0 4 "Saturation"
//@param Contrast float 1 0 4 "Contrast"
//@param ContrastPivot float 0.5 0 2 "Contrast Pivot"
//@param Strength float 1 0 1 "Strength"
//@param LiftWheel color 0 0 1 1 "Lift Wheel"
//@param GammaWheel color 0 0 1 1 "Gamma Wheel"
//@param GainWheel color 0 0 1 1 "Gain Wheel"

// Imported preset (Effect / Color & Tone): Professional lift, gamma, and gain color wheels with temperature, tint, saturation, and contrast.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LiftLuma, float GammaLuma, float GainLuma, float Temperature, float Tint, float Saturation, float Contrast, float ContrastPivot, float Strength, float3 LiftWheel, float3 GammaWheel, float3 GainWheel)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 lp=abs(frac(LiftWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 gp=abs(frac(GammaWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 ap=abs(frac(GainWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 lc=lerp(1.0.xxx,saturate(lp-1.0),saturate(LiftWheel.g)); float3 gc=lerp(1.0.xxx,saturate(gp-1.0),saturate(GammaWheel.g)); float3 ac=lerp(1.0.xxx,saturate(ap-1.0),saturate(GainWheel.g)); lc-=dot(lc,float3(.333333,.333333,.333333)); gc-=dot(gc,float3(.333333,.333333,.333333)); ac-=dot(ac,float3(.333333,.333333,.333333)); float3 v=max(s.rgb+LiftLuma+lc*0.25,0.0); v=pow(v,1.0/max(1.0+GammaLuma+gc*0.35,0.05)); v*=max(1.0+GainLuma+ac*0.5,0.0); v*=max(float3(1.0+Temperature*.12-Tint*.04,1.0+Tint*.08,1.0-Temperature*.12-Tint*.04),.001); float l=dot(v,float3(.2126,.7152,.0722)); v=lerp(l.xxx,v,Saturation); v=(v-ContrastPivot)*Contrast+ContrastPivot; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LiftLuma, GammaLuma, GainLuma, Temperature, Tint, Saturation, Contrast, ContrastPivot, Strength, LiftWheel.rgb, GammaWheel.rgb, GainWheel.rgb);
}
