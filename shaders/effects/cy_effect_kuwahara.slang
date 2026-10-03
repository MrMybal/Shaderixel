//@name Kuwahara
//@category Traitements
//@input Image
//@halo full
//@param Radius float 6 0.5 64 "Radius"
//@param Strength float 1 0 1 "Strength"
//@param Stylize float 0.15 0 1 "Stylize"
//@param Posterize float 0.9 0.1 4 "Posterize"

// Imported preset (Effect / Blur & Denoise): Edge-aware Kuwahara painterly filter using local color variance to preserve major forms.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength, float Stylize, float Posterize)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float3 a0=0,a1=0,a2=0,a3=0;float3 q0=0,q1=0,q2=0,q3=0;[unroll]for(int y=0;y<3;y++){[unroll]for(int x=0;x<3;x++){float2 o=float2(x,y)/2.0*t;float3 s0=Texture2DSample(CyInput,CyInputSampler,UV-o).rgb;float3 s1=Texture2DSample(CyInput,CyInputSampler,UV+float2(o.x,-o.y)).rgb;float3 s2=Texture2DSample(CyInput,CyInputSampler,UV+float2(-o.x,o.y)).rgb;float3 s3=Texture2DSample(CyInput,CyInputSampler,UV+o).rgb;a0+=s0;q0+=s0*s0;a1+=s1;q1+=s1*s1;a2+=s2;q2+=s2*s2;a3+=s3;q3+=s3*s3;}}a0/=9;a1/=9;a2/=9;a3/=9;q0=abs(q0/9-a0*a0);q1=abs(q1/9-a1*a1);q2=abs(q2/9-a2*a2);q3=abs(q3/9-a3*a3);float v0=dot(q0,float3(1,1,1)),v1=dot(q1,float3(1,1,1)),v2=dot(q2,float3(1,1,1)),v3=dot(q3,float3(1,1,1));float3 best=a0;float v=v0;if(v1<v){best=a1;v=v1;}if(v2<v){best=a2;v=v2;}if(v3<v){best=a3;}best=lerp(best,pow(saturate(best),Posterize),Stylize);return float4(lerp(c.rgb,best,saturate(Strength)),c.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength, Stylize, Posterize);
}
