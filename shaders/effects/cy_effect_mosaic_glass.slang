//@name Mosaic Glass
//@category CyTexture Effets
//@input Image
//@halo full
//@param Cells float 24 2 512 "Cells"
//@param Seed float 0 -1000 1000 "Seed"
//@param CellJitter float 0.55 0 1 "Cell Jitter"
//@param GroutWidth float 0.06 0.001 0.45 "Grout Width"
//@param Softness float 0.015 0.0001 0.2 "Softness"
//@param BevelPower float 0.7 0.05 8 "Bevel Power"
//@param BevelAmount float 0.25 0 2 "Bevel Amount"
//@param GroutOpacity float 1 0 1 "Grout Opacity"
//@param GroutColor color 0.015 0.015 0.018 1 "Grout Color"

// Imported from CyTexture (Effect / Stylize): Cellular stained-glass mosaic with seeded sampling, bevel shading, and grout color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Cells, float Seed, float CellJitter, float GroutWidth, float Softness, float BevelPower, float BevelAmount, float GroutOpacity, float3 GroutColor)
{
    float2 q=UV*max(Cells,1.0);float2 id=floor(q);float2 f=frac(q)-.5;float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453);float2 jitter=float2(h-.5,frac(h*19.17)-.5)*CellJitter;float2 sampleUV=(id+.5+jitter)/max(Cells,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,sampleUV);float edge=max(abs(f.x),abs(f.y));float grout=smoothstep(.5-GroutWidth,.5-GroutWidth+Softness,edge);float bevel=pow(saturate(1.0-edge*2.0),BevelPower);float3 glass=s.rgb*lerp(1.0,bevel,BevelAmount);return float4(lerp(glass,GroutColor.rgb,grout),lerp(s.a,GroutOpacity,grout));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Cells, Seed, CellJitter, GroutWidth, Softness, BevelPower, BevelAmount, GroutOpacity, GroutColor.rgb);
}
