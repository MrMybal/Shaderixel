//@name Anamorphic Lens Flare
//@category Dessin
//@graph {"name":"Anamorphic Lens Flare","category":"Dessin","inputs":0,"premultiplied":false,"nextId":17,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.100000001,"max":8.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[18.0,0.0,0.0,0.0],"name":"CoreFalloff","text":"Core Falloff","index":1,"min":0.100000001,"max":128.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[6.0,0.0,0.0,0.0],"name":"CoreIntensity","text":"Core Intensity","index":1,"min":0.0,"max":64.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.00600000005,0.0,0.0,0.0],"name":"StreakWidth","text":"Streak Width","index":1,"min":9.99999975e-05,"max":0.25},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.75,0.0,0.0,0.0],"name":"StreakLength","text":"Streak Length","index":1,"min":0.00100000005,"max":4.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"StreakFalloff","text":"Streak Falloff","index":1,"min":0.100000001,"max":16.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"StreakIntensity","text":"Streak Intensity","index":1,"min":0.0,"max":64.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.00400000019,0.0,0.0,0.0],"name":"ColorSplit","text":"Color Split","index":1,"min":0.0,"max":0.100000001},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":12,"op":"param2","pos":[40,1210],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[1.0,0.75,0.349999994,1.0],"name":"CoreColor","text":"Core Color","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.180000007,0.449999988,1.0,1.0],"name":"StreakColor","text":"Streak Color","index":1},{"id":15,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0]],"value":[0,0,0,0],"name":"Anamorphic Lens Flare","text":"    float2 p=UV-Center.xy; float core=exp(-length(float2(p.x*Aspect,p.y))*CoreFalloff)*CoreIntensity; float streak=exp(-abs(p.y)/max(StreakWidth,0.0001))*pow(saturate(1.0-abs(p.x)/max(StreakLength,0.0001)),StreakFalloff)*StreakIntensity; float blue=exp(-abs(p.y+ColorSplit)/max(StreakWidth,0.0001)); float red=exp(-abs(p.y-ColorSplit)/max(StreakWidth,0.0001)); float3 rgb=CoreColor.rgb*core+StreakColor.rgb*streak*float3(red,1.0,blue); float a=saturate(max(max(rgb.r,rgb.g),rgb.b)*Opacity); return float4(rgb*Opacity,a);","pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["CoreFalloff",1],["CoreIntensity",1],["StreakWidth",1],["StreakLength",1],["StreakFalloff",1],["StreakIntensity",1],["ColorSplit",1],["Opacity",1],["Center",3],["CoreColor",3],["StreakColor",3]],"outs":[["Resultat",4]]},{"id":16,"op":"output","pos":[780,40],"in":[[15,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param CoreFalloff float 18 0.1 128 "Core Falloff"
//@param CoreIntensity float 6 0 64 "Core Intensity"
//@param StreakWidth float 0.006 0.0001 0.25 "Streak Width"
//@param StreakLength float 0.75 0.001 4 "Streak Length"
//@param StreakFalloff float 2 0.1 16 "Streak Falloff"
//@param StreakIntensity float 4 0 64 "Streak Intensity"
//@param ColorSplit float 0.004 0 0.1 "Color Split"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param CoreColor color 1 0.75 0.35 1 "Core Color"
//@param StreakColor color 0.18 0.45 1 1 "Streak Color"

// Imported preset (Draw / Light & Space): Wide cinematic anamorphic flare with horizontal streaks and spectral split.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float CoreFalloff, float CoreIntensity, float StreakWidth, float StreakLength, float StreakFalloff, float StreakIntensity, float ColorSplit, float Opacity, float3 Center, float3 CoreColor, float3 StreakColor)
{
    float2 p=UV-Center.xy; float core=exp(-length(float2(p.x*Aspect,p.y))*CoreFalloff)*CoreIntensity; float streak=exp(-abs(p.y)/max(StreakWidth,0.0001))*pow(saturate(1.0-abs(p.x)/max(StreakLength,0.0001)),StreakFalloff)*StreakIntensity; float blue=exp(-abs(p.y+ColorSplit)/max(StreakWidth,0.0001)); float red=exp(-abs(p.y-ColorSplit)/max(StreakWidth,0.0001)); float3 rgb=CoreColor.rgb*core+StreakColor.rgb*streak*float3(red,1.0,blue); float a=saturate(max(max(rgb.r,rgb.g),rgb.b)*Opacity); return float4(rgb*Opacity,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, CoreFalloff, CoreIntensity, StreakWidth, StreakLength, StreakFalloff, StreakIntensity, ColorSplit, Opacity, float3(Center, 0.0), CoreColor.rgb, StreakColor.rgb);
}
