//@name Impulse Denoise
//@category Traitements
//@graph {"name":"Impulse Denoise","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":32.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.119999997,0.0,0.0,0.0],"name":"ColorTolerance","text":"Color Tolerance","index":1,"min":0.00100000005,"max":4.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1,"min":0.0,"max":2.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0500000007,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":1.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Impulse Denoise","text":"    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float3 sum=0;float wsum=0;[unroll]for(int i=0;i<16;i++){float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*t*sqrt((i+.5)/16.0);float3 s=Texture2DSample(CyInput,CyInputSampler,UV+o).rgb;float delta=length(s-c.rgb);float w=exp(-delta*delta/max(ColorTolerance*ColorTolerance,.000001));sum+=s*w;wsum+=w;}float3 filtered=sum/max(wsum,.0001);float centerError=length(c.rgb-filtered);float reject=smoothstep(Threshold,Threshold+Softness,centerError);return float4(lerp(c.rgb,filtered,reject*saturate(Strength)),c.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["ColorTolerance",1],["Threshold",1],["Softness",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 2 0.25 32 "Radius"
//@param ColorTolerance float 0.12 0.001 4 "Color Tolerance"
//@param Threshold float 0.08 0 2 "Threshold"
//@param Softness float 0.05 0.0001 1 "Softness"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Blur & Denoise): Edge-preserving impulse and salt-and-pepper denoiser with adaptive color rejection.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float ColorTolerance, float Threshold, float Softness, float Strength)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float3 sum=0;float wsum=0;[unroll]for(int i=0;i<16;i++){float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*t*sqrt((i+.5)/16.0);float3 s=Texture2DSample(CyInput,CyInputSampler,UV+o).rgb;float delta=length(s-c.rgb);float w=exp(-delta*delta/max(ColorTolerance*ColorTolerance,.000001));sum+=s*w;wsum+=w;}float3 filtered=sum/max(wsum,.0001);float centerError=length(c.rgb-filtered);float reject=smoothstep(Threshold,Threshold+Softness,centerError);return float4(lerp(c.rgb,filtered,reject*saturate(Strength)),c.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, ColorTolerance, Threshold, Softness, Strength);
}
