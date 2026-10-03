//@name High Pass
//@category Traitements
//@graph {"name":"High Pass","category":"Traitements","inputs":1,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":256.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":8.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Steps","text":"Steps","index":1,"min":1.0,"max":128.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"HighQuality","text":"High Quality","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Dither","text":"Dither","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.75,0.0,0.0,0.0],"name":"DitherAmount","text":"Dither Amount","index":1,"min":0.0,"max":2.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"DitherSeed","text":"Dither Seed","index":1,"min":-1000.0,"max":1000.0},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"High Pass","text":"    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 src=Texture2DSample(CyInput,CyInputSampler,UV);float4 avg=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float f=saturate((Index+0.5+Jitter)/Count);float a=Index*2.39996323;float2 o=float2(cos(a),sin(a))*sqrt(f)*Radius/max(CyCanvasSize.xy,1.0);avg+=Texture2DSample(CyInput,CyInputSampler,UV+o);}avg/=max((float)Count,1.0);return float4(0.5+(src.rgb-avg.rgb)*Strength,src.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 8 0.25 256 "Radius"
//@param Strength float 1 0 8 "Strength"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported preset (Effect / Detail): High-pass detail isolation with up to 128 samples, deterministic radius dithering, and a one-click high-quality mode.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 src=Texture2DSample(CyInput,CyInputSampler,UV);float4 avg=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float f=saturate((Index+0.5+Jitter)/Count);float a=Index*2.39996323;float2 o=float2(cos(a),sin(a))*sqrt(f)*Radius/max(CyCanvasSize.xy,1.0);avg+=Texture2DSample(CyInput,CyInputSampler,UV+o);}avg/=max((float)Count,1.0);return float4(0.5+(src.rgb-avg.rgb)*Strength,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
