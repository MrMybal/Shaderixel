//@name Fractal Noise
//@category Dessin
//@generator
//@halo 0
//@param Scale float 6 0.1 512 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Lacunarity float 2 1.1 4 "Lacunarity"
//@param Gain float 0.5 0.1 0.9 "Gain"
//@param Contrast float 1 0 8 "Contrast"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Noise): Multi-octave procedural noise for clouds, masks, and roughness.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Lacunarity, float Gain, float Contrast, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p=UV*max(Scale,1.0)+Seed; float n=0.0; float amp=0.5714; float sum=0.0; [unroll] for(int o=0;o<5;o++){float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f); float a=frac(sin(dot(i,float2(127.1,311.7)))*43758.5453); float b=frac(sin(dot(i+float2(1,0),float2(127.1,311.7)))*43758.5453); float c=frac(sin(dot(i+float2(0,1),float2(127.1,311.7)))*43758.5453); float d=frac(sin(dot(i+1.0,float2(127.1,311.7)))*43758.5453); n+=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y)*amp; sum+=amp; p=p*Lacunarity+13.17; amp*=Gain;} n=saturate((n/max(sum,0.001)-0.5)*Contrast+0.5); return float4(lerp(ColorA.rgb,ColorB.rgb,n),lerp(OpacityA,OpacityB,n));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Lacunarity, Gain, Contrast, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
