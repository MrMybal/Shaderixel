//@name Curvature From Normal
//@category CyTexture Effets
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 3 0 64 "Strength"
//@param Bias float 0 -1 1 "Bias"
//@param Concave float 0 0 1 "Concave"
//@param Power float 1 0.05 8 "Power"

// Imported from CyTexture (Effect / PBR): Extracts convex and concave curvature masks from a tangent-space normal map.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength, float Bias, float Concave, float Power)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float3 c=normalize(Texture2DSample(CyInput,CyInputSampler,UV).xyz*2.0-1.0);float3 l=normalize(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).xyz*2.0-1.0);float3 r=normalize(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).xyz*2.0-1.0);float3 u=normalize(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).xyz*2.0-1.0);float3 d=normalize(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).xyz*2.0-1.0);float curvature=((r.x-l.x)+(d.y-u.y))*Strength;float convex=saturate(curvature+Bias),concave=saturate(-curvature-Bias);float value=lerp(convex,concave,Concave);value=pow(value,Power);return float4(value.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength, Bias, Concave, Power);
}
