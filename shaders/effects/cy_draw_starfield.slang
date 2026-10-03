//@name Starfield
//@category CyTexture Dessin
//@generator
//@halo 0
//@param CellCount float 48 4 512 "Cell Count"
//@param Seed float 0 -1000 1000 "Seed"
//@param Density float 0.18 0 1 "Density"
//@param Jitter float 0.8 0 1 "Jitter"
//@param MinRadius float 0.025 0.001 0.5 "Min Radius"
//@param MaxRadius float 0.13 0.001 0.7 "Max Radius"
//@param Falloff float 2 0.05 16 "Falloff"
//@param SpikeWidth float 0.15 0.001 1 "Spike Width"
//@param SpikeLength float 4 1 16 "Spike Length"
//@param SpikeSharpness float 5 0.1 32 "Spike Sharpness"
//@param SpikeIntensity float 0.3 0 8 "Spike Intensity"
//@param Intensity float 4 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundOpacity float 1 0 1 "Background Opacity"
//@param ColorA color 0.4 0.7 2 1 "Color A"
//@param ColorB color 2 0.8 0.35 1 "Color B"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported from CyTexture (Draw / Light & Space): Seeded HDR star field with variable size, density, diffraction spikes, and color variation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float CellCount, float Seed, float Density, float Jitter, float MinRadius, float MaxRadius, float Falloff, float SpikeWidth, float SpikeLength, float SpikeSharpness, float SpikeIntensity, float Intensity, float Opacity, float BackgroundOpacity, float3 ColorA, float3 ColorB, float3 BackgroundColor)
{
    float2 q=UV*max(CellCount,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); float h2=frac(sin(dot(id+Seed+17.3,float2(269.5,183.3)))*43758.5453); float2 o=float2(h-0.5,h2-0.5)*Jitter; float2 d=p-o; float active=step(1.0-saturate(Density),h); float radius=lerp(MinRadius,MaxRadius,h2); float core=pow(saturate(1.0-length(d)/max(radius,0.0001)),max(Falloff,0.05)); float spikes=pow(saturate(1.0-min(abs(d.x),abs(d.y))/max(radius*SpikeWidth,0.0001)),SpikeSharpness)*pow(saturate(1.0-length(d)/(radius*SpikeLength)),2.0); float v=active*(core+spikes*SpikeIntensity); float3 color=lerp(ColorA.rgb,ColorB.rgb,h2); return float4(BackgroundColor.rgb+color*v*Intensity,saturate(BackgroundOpacity+v*Opacity));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), CellCount, Seed, Density, Jitter, MinRadius, MaxRadius, Falloff, SpikeWidth, SpikeLength, SpikeSharpness, SpikeIntensity, Intensity, Opacity, BackgroundOpacity, ColorA.rgb, ColorB.rgb, BackgroundColor.rgb);
}
