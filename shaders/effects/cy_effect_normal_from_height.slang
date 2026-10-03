//@name Normal From Height
//@category Traitements
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 4 0.01 64 "Strength"

// Imported preset (Effect / Detail): Generates a tangent-space normal map from source luminance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength)
{
    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0))*Radius; float l=dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float r=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float u=dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float d=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float3 n=normalize(float3((l-r)*Strength,(u-d)*Strength,1.0)); float a=Texture2DSample(CyInput,CyInputSampler,UV).a; return float4(n*0.5+0.5,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength);
}
