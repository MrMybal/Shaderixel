//@name Heat Haze
//@category Traitements
//@graph {"name":"Heat Haze","category":"Traitements","inputs":1,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.00999999978,"max":128.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-100.0,"max":100.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.699999988,0.0,0.0,0.0],"name":"Distortion","text":"Distortion","index":1,"min":0.0,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"ReferencePixels","text":"Reference Pixels","index":1,"min":0.0,"max":512.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"UseAlphaMask","text":"Use Alpha Mask","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"MaskPower","text":"Mask Power","index":1,"min":0.0500000007,"max":8.0},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"Heat Haze","text":"    float2 p=UV*max(Scale,0.01);float nx=sin(p.y*7.13+Phase)+sin((p.x+p.y)*13.17-Phase*.7)+sin(p.y*29.7+p.x*3.1);float ny=cos(p.x*8.31-Phase)+cos((p.x-p.y)*17.71+Phase*.4)+cos(p.x*31.3-p.y*2.7);float2 warp=float2(nx,ny)/3.0*Distortion/max(CyCanvasSize.xy,1.0)*ReferencePixels;float mask=lerp(1.0,pow(saturate(Texture2DSample(CyInput,CyInputSampler,UV).a),MaskPower),UseAlphaMask);float4 s=Texture2DSample(CyInput,CyInputSampler,UV+warp*mask);return s;","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Scale",1],["Phase",1],["Distortion",1],["ReferencePixels",1],["UseAlphaMask",1],["MaskPower",1]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Scale float 4 0.01 128 "Scale"
//@param Phase float 0 -100 100 "Phase"
//@param Distortion float 0.7 0 8 "Distortion"
//@param ReferencePixels float 24 0 512 "Reference Pixels"
//@param UseAlphaMask float 0 0 1 "Use Alpha Mask"
//@param MaskPower float 1 0.05 8 "Mask Power"

// Imported preset (Effect / Distort): Layered procedural refraction for heat shimmer, underwater distortion, and energy fields.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Phase, float Distortion, float ReferencePixels, float UseAlphaMask, float MaskPower)
{
    float2 p=UV*max(Scale,0.01);float nx=sin(p.y*7.13+Phase)+sin((p.x+p.y)*13.17-Phase*.7)+sin(p.y*29.7+p.x*3.1);float ny=cos(p.x*8.31-Phase)+cos((p.x-p.y)*17.71+Phase*.4)+cos(p.x*31.3-p.y*2.7);float2 warp=float2(nx,ny)/3.0*Distortion/max(CyCanvasSize.xy,1.0)*ReferencePixels;float mask=lerp(1.0,pow(saturate(Texture2DSample(CyInput,CyInputSampler,UV).a),MaskPower),UseAlphaMask);float4 s=Texture2DSample(CyInput,CyInputSampler,UV+warp*mask);return s;
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Phase, Distortion, ReferencePixels, UseAlphaMask, MaskPower);
}
