//@name Procedural Noise
//@category Dessin
//@generator
//@halo 0
//@param Scale float 16 1 1024 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Contrast float 1 0 8 "Contrast"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Noise): Fast deterministic value noise for masks, grain, and procedural sources.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Contrast, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p=UV*max(Scale,1.0)+Seed; float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f); float h00=frac(sin(dot(i,float2(127.1,311.7)))*43758.5453); float h10=frac(sin(dot(i+float2(1,0),float2(127.1,311.7)))*43758.5453); float h01=frac(sin(dot(i+float2(0,1),float2(127.1,311.7)))*43758.5453); float h11=frac(sin(dot(i+1.0,float2(127.1,311.7)))*43758.5453); float n=lerp(lerp(h00,h10,f.x),lerp(h01,h11,f.x),f.y); n=saturate((n-0.5)*Contrast+0.5); return float4(lerp(ColorA,ColorB,n),lerp(OpacityA,OpacityB,n));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Contrast, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
