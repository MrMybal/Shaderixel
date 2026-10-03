//@name VHS Glitch
//@category Traitements
//@graph {"name":"VHS Glitch","category":"Traitements","inputs":1,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[240.0,0.0,0.0,0.0],"name":"LineDensity","text":"Line Density","index":1,"min":8.0,"max":4096.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-100.0,"max":100.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.870000005,0.0,0.0,0.0],"name":"GlitchThreshold","text":"Glitch Threshold","index":1,"min":0.0,"max":0.999000013},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"GlitchSharpness","text":"Glitch Sharpness","index":1,"min":0.100000001,"max":16.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"HorizontalJitter","text":"Horizontal Jitter","index":1,"min":0.0,"max":0.5},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[85.0,0.0,0.0,0.0],"name":"TrackingFrequency","text":"Tracking Frequency","index":1,"min":0.0,"max":2048.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.00300000003,0.0,0.0,0.0],"name":"TrackingAmount","text":"Tracking Amount","index":1,"min":0.0,"max":0.100000001},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"ColorDelay","text":"Color Delay","index":1,"min":0.0,"max":64.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Scanlines","text":"Scanlines","index":1},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"Dropout","text":"Dropout","index":1},{"id":14,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0]],"value":[0,0,0,0],"name":"VHS Glitch","text":"    float scanLine=floor(UV.y*max(LineDensity,1.0));float n=frac(sin(scanLine*12.9898+Phase*7.13)*43758.5453);float burst=pow(saturate((n-GlitchThreshold)/max(1.0-GlitchThreshold,.0001)),GlitchSharpness);float jitter=(n-.5)*HorizontalJitter+sin(UV.y*TrackingFrequency+Phase)*TrackingAmount;float2 uv=UV+float2(jitter*burst,0);float delay=ColorDelay/max(CyCanvasSize.x,1.0);float r=Texture2DSample(CyInput,CyInputSampler,uv+float2(delay,0)).r;float g=Texture2DSample(CyInput,CyInputSampler,uv).g;float b=Texture2DSample(CyInput,CyInputSampler,uv-float2(delay,0)).b;float scan=lerp(1.0,0.75+0.25*sin(UV.y*CyCanvasSize.y*3.14159),Scanlines);float dropout=1.0-burst*Dropout;float4 src=Texture2DSample(CyInput,CyInputSampler,uv);return float4(float3(r,g,b)*scan*dropout,src.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["LineDensity",1],["Phase",1],["GlitchThreshold",1],["GlitchSharpness",1],["HorizontalJitter",1],["TrackingFrequency",1],["TrackingAmount",1],["ColorDelay",1],["Scanlines",1],["Dropout",1]],"outs":[["Resultat",4]]},{"id":15,"op":"output","pos":[780,40],"in":[[14,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param LineDensity float 240 8 4096 "Line Density"
//@param Phase float 0 -100 100 "Phase"
//@param GlitchThreshold float 0.87 0 0.999 "Glitch Threshold"
//@param GlitchSharpness float 2 0.1 16 "Glitch Sharpness"
//@param HorizontalJitter float 0.08 0 0.5 "Horizontal Jitter"
//@param TrackingFrequency float 85 0 2048 "Tracking Frequency"
//@param TrackingAmount float 0.003 0 0.1 "Tracking Amount"
//@param ColorDelay float 3 0 64 "Color Delay"
//@param Scanlines float 0.25 0 1 "Scanlines"
//@param Dropout float 0.35 0 1 "Dropout"

// Imported preset (Effect / Stylize): Analog VHS displacement with tracking noise, color delay, dropout, and scanline controls.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LineDensity, float Phase, float GlitchThreshold, float GlitchSharpness, float HorizontalJitter, float TrackingFrequency, float TrackingAmount, float ColorDelay, float Scanlines, float Dropout)
{
    float scanLine=floor(UV.y*max(LineDensity,1.0));float n=frac(sin(scanLine*12.9898+Phase*7.13)*43758.5453);float burst=pow(saturate((n-GlitchThreshold)/max(1.0-GlitchThreshold,.0001)),GlitchSharpness);float jitter=(n-.5)*HorizontalJitter+sin(UV.y*TrackingFrequency+Phase)*TrackingAmount;float2 uv=UV+float2(jitter*burst,0);float delay=ColorDelay/max(CyCanvasSize.x,1.0);float r=Texture2DSample(CyInput,CyInputSampler,uv+float2(delay,0)).r;float g=Texture2DSample(CyInput,CyInputSampler,uv).g;float b=Texture2DSample(CyInput,CyInputSampler,uv-float2(delay,0)).b;float scan=lerp(1.0,0.75+0.25*sin(UV.y*CyCanvasSize.y*3.14159),Scanlines);float dropout=1.0-burst*Dropout;float4 src=Texture2DSample(CyInput,CyInputSampler,uv);return float4(float3(r,g,b)*scan*dropout,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LineDensity, Phase, GlitchThreshold, GlitchSharpness, HorizontalJitter, TrackingFrequency, TrackingAmount, ColorDelay, Scanlines, Dropout);
}
