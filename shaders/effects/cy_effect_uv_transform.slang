//@name UV Transform
//@category CyTexture Effets
//@input Image
//@halo full
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Scale float 1 0.01 16 "Scale"
//@param Pivot float2 0.5 0.5 0 1 "Pivot"
//@param Offset float2 0 0 0 1 "Offset"

// Imported from CyTexture (Effect / Distort): Full-screen translate, rotate, scale, and pivot transform.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Rotation, float Scale, float3 Pivot, float3 Offset)
{
    float2 p=UV-Pivot.xy-Offset.xy; float c=cos(Rotation); float s=sin(Rotation); p=mul(float2x2(c,-s,s,c),p)/max(Scale,0.0001); float2 q=Pivot.xy+p; return Texture2DSample(CyInput,CyInputSampler,q);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Rotation, Scale, float3(Pivot, 0.0), float3(Offset, 0.0));
}
