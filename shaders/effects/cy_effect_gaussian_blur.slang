//@name Gaussian Blur
//@category Traitements
//@graph {"name":"Gaussian Blur","category":"Traitements","inputs":1,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[16.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.0,"max":512.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[6.0,0.0,0.0,0.0],"name":"Sigma","text":"Sigma","index":1,"min":0.00999999978,"max":256.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Angle","text":"Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Steps","text":"Steps","index":1,"min":1.0,"max":128.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"HighQuality","text":"High Quality","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Dither","text":"Dither","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.75,0.0,0.0,0.0],"name":"DitherAmount","text":"Dither Amount","index":1,"min":0.0,"max":2.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"DitherSeed","text":"Dither Seed","index":1,"min":-1000.0,"max":1000.0},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Gaussian Blur","text":"    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 Direction=float2(cos(Angle)/max(CyCanvasSize.x,1.0),sin(Angle)/max(CyCanvasSize.y,1.0));float4 Sum=0;float WeightSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float T=Count<=1?0.0:(Index/(Count-1.0)-0.5)*2.0;T+=Jitter*2.0/max((float)Count,1.0);float Pixel=T*Radius;float Weight=exp(-0.5*Pixel*Pixel/max(Sigma*Sigma,0.0001));Sum+=Texture2DSample(CyInput,CyInputSampler,UV+Direction*Pixel)*Weight;WeightSum+=Weight;}return Sum/max(WeightSum,0.00001);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Sigma",1],["Angle",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 16 0 512 "Radius"
//@param Sigma float 6 0.01 256 "Sigma"
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported preset (Effect / Blur & Denoise): High-quality directional Gaussian blur; stack horizontal and vertical instances for a full 2D Gaussian.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Sigma, float Angle, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 Direction=float2(cos(Angle)/max(CyCanvasSize.x,1.0),sin(Angle)/max(CyCanvasSize.y,1.0));float4 Sum=0;float WeightSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float T=Count<=1?0.0:(Index/(Count-1.0)-0.5)*2.0;T+=Jitter*2.0/max((float)Count,1.0);float Pixel=T*Radius;float Weight=exp(-0.5*Pixel*Pixel/max(Sigma*Sigma,0.0001));Sum+=Texture2DSample(CyInput,CyInputSampler,UV+Direction*Pixel)*Weight;WeightSum+=Weight;}return Sum/max(WeightSum,0.00001);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Sigma, Angle, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
