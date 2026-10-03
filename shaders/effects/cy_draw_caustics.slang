//@name Caustics
//@category Dessin
//@generator
//@halo 0
//@param Scale float 3.5 0.01 64 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Threshold float 0.7 0 4 "Threshold"
//@param Contrast float 2.2 0 16 "Contrast"
//@param Sharpness float 2.5 0.05 16 "Sharpness"
//@param Intensity float 2 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundOpacity float 1 0 1 "Background Opacity"
//@param DeepColor color 0 0.015 0.04 1 "Deep Color"
//@param LightColor color 0.05 0.85 2.5 1 "Light Color"

// Imported preset (Draw / Light & Space): Animated-looking layered caustic field for water light, energy, and emissive masks.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Threshold, float Contrast, float Sharpness, float Intensity, float Opacity, float BackgroundOpacity, float3 DeepColor, float3 LightColor)
{
    float2 p=(UV-0.5)*max(Scale,0.01); float v=0.0; float amp=0.55; [unroll]for(int i=0;i<5;i++){p=float2(p.x*1.62-p.y*1.17,p.x*1.17+p.y*1.62)+float2(Seed*.17+i*1.31,Seed*.11-i*.73); float w=sin(p.x+sin(p.y*1.37))+sin(p.y+sin(p.x*1.21)); v+=abs(w)*amp; amp*=0.52;} v=pow(saturate(1.0-abs(v-Threshold)*Contrast),max(Sharpness,0.05)); float3 rgb=lerp(DeepColor.rgb,LightColor.rgb,v)*Intensity; return float4(rgb,lerp(BackgroundOpacity,Opacity,v));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Threshold, Contrast, Sharpness, Intensity, Opacity, BackgroundOpacity, DeepColor.rgb, LightColor.rgb);
}
