//@name Polar Transform
//@category Traitements
//@input Image
//@halo full
//@param Aspect float 1 0.05 16 "Aspect"
//@param RadialScale float 2 0.01 32 "Radial Scale"
//@param RadialOffset float 0 -8 8 "Radial Offset"
//@param Rotation float 0 -4 4 "Rotation"
//@param Twist float 0 -16 16 "Twist"
//@param AngularRepeat float 1 1 64 "Angular Repeat"
//@param ToPolar float 1 0 1 "To Polar"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Distort): Cartesian-to-polar transform with repeat, twist, radial offset, and seamless angular wrapping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float RadialScale, float RadialOffset, float Rotation, float Twist, float AngularRepeat, float ToPolar, float3 Center)
{
    float2 p=UV-Center.xy;p.x*=Aspect;float radius=length(p)*RadialScale+RadialOffset;float angle=atan2(p.y,p.x)/6.283185+0.5+Rotation+radius*Twist;float2 polar=float2(frac(angle*AngularRepeat),frac(radius));float2 target=lerp(UV,polar,saturate(ToPolar));float4 s=Texture2DSample(CyInput,CyInputSampler,target);return s;
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, RadialScale, RadialOffset, Rotation, Twist, AngularRepeat, ToPolar, float3(Center, 0.0));
}
