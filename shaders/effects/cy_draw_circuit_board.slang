//@name Circuit Board
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Grid float 18 2 256 "Grid"
//@param Seed float 0 -1000 1000 "Seed"
//@param TraceWidth float 0.055 0.001 0.45 "Trace Width"
//@param PadRadius float 0.14 0.001 0.7 "Pad Radius"
//@param Softness float 0.015 0.0001 0.2 "Softness"
//@param BranchDensity float 0.35 0 1 "Branch Density"
//@param PadDensity float 0.72 0 1 "Pad Density"
//@param PulseFrequency float 1.5 0 32 "Pulse Frequency"
//@param Phase float 0 -20 20 "Phase"
//@param PulseAmount float 0 0 1 "Pulse Amount"
//@param TraceOpacity float 1 0 1 "Trace Opacity"
//@param BoardOpacity float 1 0 1 "Board Opacity"
//@param TraceColor color 0.05 1.6 0.45 1 "Trace Color"
//@param BoardColor color 0.005 0.025 0.012 1 "Board Color"

// Imported from CyTexture (Draw / Pattern): Procedural circuit traces, pads, and substrate for technical masks and graphic design.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Grid, float Seed, float TraceWidth, float PadRadius, float Softness, float BranchDensity, float PadDensity, float PulseFrequency, float Phase, float PulseAmount, float TraceOpacity, float BoardOpacity, float3 TraceColor, float3 BoardColor)
{
    float2 q=UV*max(Grid,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float r=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); float axis=lerp(abs(p.x),abs(p.y),step(0.5,r)); float trace=1.0-smoothstep(TraceWidth,TraceWidth+Softness,axis); float pad=1.0-smoothstep(PadRadius,PadRadius+Softness,length(p)); float gate=step(BranchDensity,frac(r*17.31)); float m=max(trace*gate,pad*step(PadDensity,r)); float pulse=lerp(1.0,0.55+0.45*sin((q.x+q.y)*PulseFrequency+Phase),PulseAmount); return float4(lerp(BoardColor.rgb,TraceColor.rgb,m*pulse),lerp(BoardOpacity,TraceOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Grid, Seed, TraceWidth, PadRadius, Softness, BranchDensity, PadDensity, PulseFrequency, Phase, PulseAmount, TraceOpacity, BoardOpacity, TraceColor.rgb, BoardColor.rgb);
}
