//@name AO From Height
//@category Traitements
//@input Image
//@halo full
//@param Radius float 24 1 512 "Radius"
//@param Steps float 24 4 48 "Steps"
//@param Bias float 0.005 -1 1 "Bias"
//@param Strength float 5 0 64 "Strength"
//@param Power float 1.2 0.05 8 "Power"
//@param Invert float 0 0 1 "Invert"
//@param HeightChannel color 1 0 0 0 "Height Channel"

// Imported preset (Effect / PBR): Generates a horizon-style ambient-occlusion mask from a linear height texture with adjustable sample quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Steps, float Bias, float Strength, float Power, float Invert, float3 HeightChannel)
{
    float2 texel=1.0/max(CyCanvasSize.xy,1.0);float center=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,HeightChannel.rgb);int count=clamp((int)round(Steps),4,48);float occ=0;[loop]for(int i=0;i<48;i++){if(i>=count)break;float f=(i+.5)/count;float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*Radius*sqrt(f)*texel;float h=dot(Texture2DSample(CyInput,CyInputSampler,UV+o).rgb,HeightChannel.rgb);occ+=saturate((h-center-Bias)*Strength/(sqrt(f)+.05));}occ=pow(saturate(occ/count),Power);float ao=lerp(1.0-occ,occ,Invert);return float4(ao.xxx,Texture2DSample(CyInput,CyInputSampler,UV).a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Steps, Bias, Strength, Power, Invert, HeightChannel.rgb);
}
