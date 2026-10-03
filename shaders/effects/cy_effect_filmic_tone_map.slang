//@name Filmic Tone Map
//@category CyTexture Effets
//@input Image
//@halo 0
//@param ExposureEV float 0 -16 16 "Exposure EV"
//@param WhitePoint float 11.2 0.1 64 "White Point"
//@param A float 0.15 0.01 1 "A"
//@param B float 0.5 0.01 2 "B"
//@param C float 0.1 0 1 "C"
//@param D float 0.2 0.01 2 "D"
//@param E float 0.02 0 1 "E"
//@param F float 0.3 0.01 2 "F"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): Filmic HDR tone mapping with white point, exposure, and blend strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float ExposureEV, float WhitePoint, float A, float B, float C, float D, float E, float F, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 x=max(s.rgb*exp2(ExposureEV),0.0); float3 v=(x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F)-E/F; float w=max(WhitePoint,0.001); float white=(w*(A*w+C*B)+D*E)/(w*(A*w+B)+D*F)-E/F; v/=max(white,0.0001); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ExposureEV, WhitePoint, A, B, C, D, E, F, Strength);
}
