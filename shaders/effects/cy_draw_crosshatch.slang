//@name Crosshatch
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Frequency float 28 2 512 "Frequency"
//@param Angle float 0.45 -6.283 6.283 "Angle"
//@param Width float 0.08 0.001 0.49 "Width"
//@param Softness float 0.025 0.0001 0.25 "Softness"
//@param Tone float 0.72 0 1 "Tone"
//@param InkOpacity float 1 0 1 "Ink Opacity"
//@param PaperOpacity float 1 0 1 "Paper Opacity"
//@param InkColor color 0.015 0.012 0.01 1 "Ink Color"
//@param PaperColor color 0.75 0.68 0.55 1 "Paper Color"

// Imported from CyTexture (Draw / Pattern): Layered crosshatch generator with tone-controlled stroke density and paper color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Angle, float Width, float Softness, float Tone, float InkOpacity, float PaperOpacity, float3 InkColor, float3 PaperColor)
{
    float2 p=UV*max(Frequency,1.0); float c=cos(Angle),s=sin(Angle); float2 r=float2(c*p.x-s*p.y,s*p.x+c*p.y); float l0=1.0-smoothstep(Width,Width+Softness,abs(frac(r.x)-0.5)); float l1=1.0-smoothstep(Width,Width+Softness,abs(frac(r.y)-0.5)); float l2=1.0-smoothstep(Width,Width+Softness,abs(frac((r.x+r.y)*0.707)-0.5)); float l3=1.0-smoothstep(Width,Width+Softness,abs(frac((r.x-r.y)*0.707)-0.5)); float tone=saturate(Tone); float ink=max(l0*step(0.2,tone),max(l1*step(0.45,tone),max(l2*step(0.7,tone),l3*step(0.9,tone)))); return float4(lerp(PaperColor.rgb,InkColor.rgb,ink),lerp(PaperOpacity,InkOpacity,ink));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Angle, Width, Softness, Tone, InkOpacity, PaperOpacity, InkColor.rgb, PaperColor.rgb);
}
