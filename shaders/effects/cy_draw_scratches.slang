//@name Scratches
//@category Dessin
//@generator
//@halo 0
//@param Cells float 72 4 1024 "Cells"
//@param Seed float 0 -1000 1000 "Seed"
//@param Density float 0.14 0 1 "Density"
//@param BaseAngle float 0 -6.283 6.283 "Base Angle"
//@param AngleJitter float 0.35 0 6.283 "Angle Jitter"
//@param Width float 0.025 0.001 0.3 "Width"
//@param Softness float 0.008 0.0001 0.2 "Softness"
//@param MinLength float 0.12 0.01 0.7 "Min Length"
//@param MaxLength float 0.48 0.01 0.8 "Max Length"
//@param Breakup float 0.35 0 1 "Breakup"
//@param BreakupScale float 35 1 512 "Breakup Scale"
//@param BreakupAmount float 0.25 0 1 "Breakup Amount"
//@param ScratchOpacity float 1 0 1 "Scratch Opacity"
//@param BackgroundOpacity float 0 0 1 "Background Opacity"
//@param ScratchColor color 1 1 1 1 "Scratch Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Surface): Seeded multi-scale directional scratch mask with length, width, density, and edge breakup.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Cells, float Seed, float Density, float BaseAngle, float AngleJitter, float Width, float Softness, float MinLength, float MaxLength, float Breakup, float BreakupScale, float BreakupAmount, float ScratchOpacity, float BackgroundOpacity, float3 ScratchColor, float3 BackgroundColor)
{
    float2 p=UV*max(Cells,1.0);float2 id=floor(p);float2 f=frac(p)-.5;float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453);float h2=frac(sin(dot(id+Seed+9.7,float2(269.5,183.3)))*43758.5453);float angle=BaseAngle+(h-.5)*AngleJitter;float c=cos(angle),s=sin(angle);float2 r=float2(c*f.x-s*f.y,s*f.x+c*f.y);float active=step(1.0-Density,h);float scratch=(1.0-smoothstep(Width,Width+Softness,abs(r.y)))*step(abs(r.x),lerp(MinLength,MaxLength,h2));float breakup=step(Breakup,frac(sin((r.x+id.x)*BreakupScale+Seed)*43758.5453));float m=active*scratch*lerp(1.0,breakup,BreakupAmount);return float4(lerp(BackgroundColor.rgb,ScratchColor.rgb,m),lerp(BackgroundOpacity,ScratchOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Cells, Seed, Density, BaseAngle, AngleJitter, Width, Softness, MinLength, MaxLength, Breakup, BreakupScale, BreakupAmount, ScratchOpacity, BackgroundOpacity, ScratchColor.rgb, BackgroundColor.rgb);
}
