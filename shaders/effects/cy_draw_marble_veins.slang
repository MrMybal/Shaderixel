//@name Marble Veins
//@category Dessin
//@generator
//@halo 0
//@param Scale float 4 0.1 64 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Direction float 0.35 -4 4 "Direction"
//@param Warp float 1.4 0 8 "Warp"
//@param VeinFrequency float 4.5 0.1 64 "Vein Frequency"
//@param VeinWidth float 1.25 0.1 8 "Vein Width"
//@param VeinSharpness float 3 0.1 32 "Vein Sharpness"
//@param MicroScale float 1 0.01 16 "Micro Scale"
//@param MicroAmount float 0.12 0 2 "Micro Amount"
//@param Intensity float 1 0 16 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param BaseColor color 0.08 0.09 0.1 1 "Base Color"
//@param SecondaryColor color 0.28 0.3 0.32 1 "Secondary Color"
//@param VeinColor color 0.85 0.82 0.72 1 "Vein Color"

// Imported preset (Draw / Surface): Layered marble veins with turbulent domain warping and HDR mineral highlights.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Direction, float Warp, float VeinFrequency, float VeinWidth, float VeinSharpness, float MicroScale, float MicroAmount, float Intensity, float Opacity, float3 BaseColor, float3 SecondaryColor, float3 VeinColor)
{
    float2 p=(UV-.5)*max(Scale,.01);float n=sin(p.x*2.1+sin(p.y*3.7+Seed))*0.5+sin(p.y*5.3-p.x*1.7)*.25+sin((p.x+p.y)*11.7)*.125;float warp=p.x+p.y*Direction+n*Warp;float vein=pow(saturate(1.0-abs(sin(warp*VeinFrequency))*VeinWidth),VeinSharpness);float micro=0.5+0.5*sin((p.x*17.3-p.y*13.1+Seed)*MicroScale);vein=saturate(vein+micro*MicroAmount*vein);float3 base=lerp(BaseColor.rgb,SecondaryColor.rgb,saturate(n*.5+.5));float3 rgb=lerp(base,VeinColor.rgb,vein)*Intensity;return float4(rgb,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Direction, Warp, VeinFrequency, VeinWidth, VeinSharpness, MicroScale, MicroAmount, Intensity, Opacity, BaseColor.rgb, SecondaryColor.rgb, VeinColor.rgb);
}
