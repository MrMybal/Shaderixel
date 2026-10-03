//@name Bokeh Rings
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param Radius float 0.22 0.001 2 "Radius"
//@param RingWidth float 0.025 0.001 0.5 "Ring Width"
//@param Softness float 2 0.1 16 "Softness"
//@param Intensity float 2 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Offset float2 0.2 0.07 0 1 "Offset"
//@param ColorA color 1 0.45 0.12 1 "Color A"
//@param ColorB color 0.1 0.4 1 1 "Color B"

// Imported from CyTexture (Draw / Light & Space): Procedural bokeh ring cluster for flare and depth-of-field overlays.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float RingWidth, float Softness, float Intensity, float Opacity, float3 Center, float3 Offset, float3 ColorA, float3 ColorB)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float ring1=pow(saturate(1.0-abs(length(p)-Radius)/max(RingWidth,0.0001)),Softness); float ring2=pow(saturate(1.0-abs(length(p-Offset.xy)-Radius*0.55)/max(RingWidth*0.7,0.0001)),Softness); float ring3=pow(saturate(1.0-abs(length(p+Offset.xy*1.4)-Radius*0.35)/max(RingWidth*0.55,0.0001)),Softness); float3 rgb=ColorA.rgb*ring1+ColorB.rgb*(ring2+ring3); float a=saturate((ring1+ring2+ring3)*Opacity); return float4(rgb*Intensity,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, RingWidth, Softness, Intensity, Opacity, float3(Center, 0.0), float3(Offset, 0.0), ColorA.rgb, ColorB.rgb);
}
