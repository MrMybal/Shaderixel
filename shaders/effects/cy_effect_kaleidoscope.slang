//@name Kaleidoscope
//@category Traitements
//@input Image
//@halo full
//@param Segments float 8 1 64 "Segments"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Distort): Radial kaleidoscope UV transform.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Segments, float Rotation, float Aspect, float3 Center)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float r=length(p); float a=atan2(p.y,p.x)+Rotation; float sector=6.2831853/max(Segments,1.0); a=abs(fmod(a+sector*0.5,sector)-sector*0.5); float2 q=float2(cos(a),sin(a))*r; q.x/=max(Aspect,0.001); return Texture2DSample(CyInput,CyInputSampler,Center.xy+q);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Segments, Rotation, Aspect, float3(Center, 0.0));
}
