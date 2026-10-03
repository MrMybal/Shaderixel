//@name Pixelate
//@category CyTexture Effets
//@input Image
//@halo full
//@param BlocksX float 128 1 8192 "Blocks X"
//@param BlocksY float 128 1 8192 "Blocks Y"

// Imported from CyTexture (Effect / Stylize): Pixelates the source into configurable blocks.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float BlocksX, float BlocksY)
{
    float2 count=max(float2(BlocksX,BlocksY),1.0); float2 q=(floor(UV*count)+0.5)/count; return Texture2DSample(CyInput,CyInputSampler,q);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), BlocksX, BlocksY);
}
