//@name Lens Flare Ghosts
//@category CyTexture Effets
//@input Image
//@halo full
//@param GhostScale float 1.2 0.05 8 "Ghost Scale"
//@param Threshold float 0.75 0 16 "Threshold"
//@param Softness float 0.25 0.001 4 "Softness"
//@param Intensity float 1.5 0 32 "Intensity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param GhostColorA color 1 0.2 0.05 1 "Ghost Color A"
//@param GhostColorB color 0.05 0.3 1 1 "Ghost Color B"

// Imported from CyTexture (Effect / Color & Tone): Extracts bright regions into mirrored chromatic lens ghosts.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float GhostScale, float Threshold, float Softness, float Intensity, float3 Center, float3 GhostColorA, float3 GhostColorB)
{
    float4 src=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=UV-Center.xy; float2 g1=saturate(Center.xy-p*GhostScale); float2 g2=saturate(Center.xy+p*GhostScale*0.55); float3 a=Texture2DSample(CyInput,CyInputSampler,g1).rgb; float3 b=Texture2DSample(CyInput,CyInputSampler,g2).rgb; float la=dot(a,float3(.2126,.7152,.0722)); float lb=dot(b,float3(.2126,.7152,.0722)); a*=saturate((la-Threshold)/max(Softness,0.001)); b*=saturate((lb-Threshold)/max(Softness,0.001)); float3 ghosts=(a*GhostColorA.rgb+b*GhostColorB.rgb)*Intensity; return float4(src.rgb+ghosts,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), GhostScale, Threshold, Softness, Intensity, float3(Center, 0.0), GhostColorA.rgb, GhostColorB.rgb);
}
