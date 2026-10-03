//@name Lens Distortion
//@category CyTexture Effets
//@input Image
//@halo full
//@param Aspect float 1 0.01 8 "Aspect"
//@param Distortion float 0.2 -4 4 "Distortion"
//@param Quartic float 0 -8 8 "Quartic"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported from CyTexture (Effect / Distort): Barrel or pincushion lens distortion with edge clamping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Distortion, float Quartic, float3 Center)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float r2=dot(p,p); p*=1.0+Distortion*r2+Quartic*r2*r2; p.x/=max(Aspect,0.001); float2 q=saturate(Center.xy+p); return Texture2DSample(CyInput,CyInputSampler,q);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Distortion, Quartic, float3(Center, 0.0));
}
