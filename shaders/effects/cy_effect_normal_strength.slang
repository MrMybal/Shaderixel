//@name Normal Strength
//@category Traitements
//@input Image
//@halo 0
//@param Strength float 1 0 8 "Strength"
//@param FlipGreen float 0 0 1 "Flip Green"

// Imported preset (Effect / PBR): Adjusts tangent-space normal strength while reconstructing a valid positive Z component.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength, float FlipGreen)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 n=s.xyz*2.0-1.0;n.xy*=Strength;n.y*=lerp(1.0,-1.0,FlipGreen);n.z=sqrt(saturate(1.0-dot(n.xy,n.xy)));n=normalize(n);return float4(n*.5+.5,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength, FlipGreen);
}
