//@name Outline
//@category Traitements
//@input Image
//@halo full
//@param Thickness float 2 0.25 128 "Thickness"
//@param Inner float 0 0 1 "Inner"
//@param Opacity float 1 0 1 "Opacity"
//@param OutlineColor color 1 1 1 1 "Outline Color"

// Imported preset (Effect / Detail): Builds an inner or outer outline from source alpha.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Thickness, float Inner, float Opacity, float3 OutlineColor)
{
    float2 t=Thickness/max(CyCanvasSize.xy,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float a0=Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).a;float a1=Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).a;float a2=Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).a;float a3=Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).a;float expanded=max(max(a0,a1),max(a2,a3));float contracted=min(min(a0,a1),min(a2,a3));float edge=lerp(saturate(expanded-s.a),saturate(s.a-contracted),saturate(Inner));float alpha=edge*Opacity*(1.0-s.a*(1.0-Inner));return float4(s.rgb+OutlineColor.rgb*alpha,s.a+alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Thickness, Inner, Opacity, OutlineColor.rgb);
}
