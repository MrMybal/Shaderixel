//@name Bloom
//@category Traitements
//@graph {"name":"Bloom","category":"Traitements","inputs":1,"premultiplied":false,"nextId":15,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1,"min":0.0,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[12.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":256.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":32.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Steps","text":"Steps","index":1,"min":1.0,"max":128.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"HighQuality","text":"High Quality","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Dither","text":"Dither","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.75,0.0,0.0,0.0],"name":"DitherAmount","text":"Dither Amount","index":1,"min":0.0,"max":2.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"DitherSeed","text":"Dither Seed","index":1,"min":-1000.0,"max":1000.0},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"Tint","text":"Tint","index":1},{"id":13,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0]],"value":[0,0,0,0],"name":"Bloom","text":"    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 Source=Texture2DSample(CyInput,CyInputSampler,UV);float3 BloomSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float F=saturate((Index+0.5+Jitter)/Count);float A=Index*2.39996323;float2 Offset=float2(cos(A),sin(A))*sqrt(F)*Radius/max(CyCanvasSize.xy,1.0);BloomSum+=max(Texture2DSample(CyInput,CyInputSampler,UV+Offset).rgb-Threshold,0.0);}return float4(Source.rgb+BloomSum/max((float)Count,1.0)*Intensity*Tint.rgb,Source.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Threshold",1],["Radius",1],["Intensity",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1],["Tint",3]],"outs":[["Resultat",4]]},{"id":14,"op":"output","pos":[780,40],"in":[[13,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Threshold float 1 0 16 "Threshold"
//@param Radius float 12 0.25 256 "Radius"
//@param Intensity float 1 0 32 "Intensity"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Tint color 1 1 1 1 "Tint"

// Imported preset (Effect / Color & Tone): HDR threshold bloom with adjustable radius, deterministic dither, and up to 128 samples.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Radius, float Intensity, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Tint)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 Source=Texture2DSample(CyInput,CyInputSampler,UV);float3 BloomSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float F=saturate((Index+0.5+Jitter)/Count);float A=Index*2.39996323;float2 Offset=float2(cos(A),sin(A))*sqrt(F)*Radius/max(CyCanvasSize.xy,1.0);BloomSum+=max(Texture2DSample(CyInput,CyInputSampler,UV+Offset).rgb-Threshold,0.0);}return float4(Source.rgb+BloomSum/max((float)Count,1.0)*Intensity*Tint.rgb,Source.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Radius, Intensity, Steps, HighQuality, Dither, DitherAmount, DitherSeed, Tint.rgb);
}
