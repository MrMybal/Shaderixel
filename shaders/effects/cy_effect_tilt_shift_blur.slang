//@name Tilt Shift Blur
//@category Traitements
//@graph {"name":"Tilt Shift Blur","category":"Traitements","inputs":1,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.0,"max":512.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"FocusAngle","text":"Focus Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"FocusWidth","text":"Focus Width","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.200000003,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":0.00100000005,"max":1.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[20.0,0.0,0.0,0.0],"name":"Steps","text":"Steps","index":1,"min":1.0,"max":128.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"HighQuality","text":"High Quality","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Dither","text":"Dither","index":1},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.75,0.0,0.0,0.0],"name":"DitherAmount","text":"Dither Amount","index":1,"min":0.0,"max":2.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"DitherSeed","text":"Dither Seed","index":1,"min":-1000.0,"max":1000.0},{"id":13,"op":"param2","pos":[40,1320],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":14,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0]],"value":[0,0,0,0],"name":"Tilt Shift Blur","text":"    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 FocusNormal=float2(-sin(FocusAngle),cos(FocusAngle));float Distance=abs(dot(UV-Center.xy,FocusNormal));float LocalRadius=Radius*saturate((Distance-FocusWidth)/max(Softness,0.0001));float4 Sum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float F=saturate((Index+0.5+Jitter)/Count);float A=Index*2.39996323;float2 Offset=float2(cos(A),sin(A))*sqrt(F)*LocalRadius/max(CyCanvasSize.xy,1.0);Sum+=Texture2DSample(CyInput,CyInputSampler,UV+Offset);}return Sum/max((float)Count,1.0);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["FocusAngle",1],["FocusWidth",1],["Softness",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1],["Center",3]],"outs":[["Resultat",4]]},{"id":15,"op":"output","pos":[780,40],"in":[[14,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 24 0 512 "Radius"
//@param FocusAngle float 0 -6.283 6.283 "Focus Angle"
//@param FocusWidth float 0.08 0 1 "Focus Width"
//@param Softness float 0.2 0.001 1 "Softness"
//@param Steps float 20 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Blur & Denoise): Focus-band blur with adjustable angle, transition, radius, and quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float FocusAngle, float FocusWidth, float Softness, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Center)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 FocusNormal=float2(-sin(FocusAngle),cos(FocusAngle));float Distance=abs(dot(UV-Center.xy,FocusNormal));float LocalRadius=Radius*saturate((Distance-FocusWidth)/max(Softness,0.0001));float4 Sum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float F=saturate((Index+0.5+Jitter)/Count);float A=Index*2.39996323;float2 Offset=float2(cos(A),sin(A))*sqrt(F)*LocalRadius/max(CyCanvasSize.xy,1.0);Sum+=Texture2DSample(CyInput,CyInputSampler,UV+Offset);}return Sum/max((float)Count,1.0);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, FocusAngle, FocusWidth, Softness, Steps, HighQuality, Dither, DitherAmount, DitherSeed, float3(Center, 0.0));
}
