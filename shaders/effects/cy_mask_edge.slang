//@name Edge
//@category Masques
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 2 0 32 "Strength"

// Imported preset (Mask / Mask): Sobel luminance edge mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength)
{
    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0))*Radius; float tl=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,-t.y)).rgb,float3(.2126,.7152,.0722)); float tc=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,-t.y)).rgb,float3(.2126,.7152,.0722)); float tr=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,-t.y)).rgb,float3(.2126,.7152,.0722)); float ml=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,0)).rgb,float3(.2126,.7152,.0722)); float mr=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float bl=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,t.y)).rgb,float3(.2126,.7152,.0722)); float bc=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float br=dot(Texture2DSample(CyInput,CyInputSampler,UV+t).rgb,float3(.2126,.7152,.0722)); float gx=-tl-2*ml-bl+tr+2*mr+br; float gy=-tl-2*tc-tr+bl+2*bc+br; float v=saturate(length(float2(gx,gy))*Strength); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength);
}
