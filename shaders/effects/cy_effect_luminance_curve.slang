//@name Luminance Curve
//@category Traitements
//@input Image
//@halo 0
//@param Strength float 1 0 1 "Strength"
//@param InputRange float 1 0.01 32 "Input Range"
//@param OutputRange float 1 0 32 "Output Range"
//@param CurvePoint0 float2 0 0 0 1 "Curve Point0"
//@param CurvePoint1 float2 0.333333 0.333333 0 1 "Curve Point1"
//@param CurvePoint2 float2 0.666667 0.666667 0 1 "Curve Point2"
//@param CurvePoint3 float2 1 1 0 1 "Curve Point3"
//@param CurveHandle0Out float2 0.111111 0.111111 0 1 "Curve Handle0 Out"
//@param CurveHandle1In float2 0.222222 0.222222 0 1 "Curve Handle1 In"
//@param CurveHandle1Out float2 0.444444 0.444444 0 1 "Curve Handle1 Out"
//@param CurveHandle2In float2 0.555556 0.555556 0 1 "Curve Handle2 In"
//@param CurveHandle2Out float2 0.777778 0.777778 0 1 "Curve Handle2 Out"
//@param CurveHandle3In float2 0.888889 0.888889 0 1 "Curve Handle3 In"

// Imported preset (Effect / Color & Tone): HDR-aware luminance remapping with a dedicated interactive four-point curve editor.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength, float InputRange, float OutputRange, float3 CurvePoint0, float3 CurvePoint1, float3 CurvePoint2, float3 CurvePoint3, float3 CurveHandle0Out, float3 CurveHandle1In, float3 CurveHandle1Out, float3 CurveHandle2In, float3 CurveHandle2Out, float3 CurveHandle3In)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float x=saturate(l/max(InputRange,0.00001)); float2 p0=CurvePoint0.xy,p1=CurvePoint1.xy,p2=CurvePoint2.xy,p3=CurvePoint3.xy; float2 a,b,c,d; if(x<=p1.x){a=p0;b=CurveHandle0Out.xy;c=CurveHandle1In.xy;d=p1;}else if(x<=p2.x){a=p1;b=CurveHandle1Out.xy;c=CurveHandle2In.xy;d=p2;}else{a=p2;b=CurveHandle2Out.xy;c=CurveHandle3In.xy;d=p3;} float lo=0,hi=1,t=.5; [unroll]for(int j=0;j<10;j++){t=(lo+hi)*.5;float o=1-t;float bx=o*o*o*a.x+3*o*o*t*b.x+3*o*t*t*c.x+t*t*t*d.x;if(bx<x)lo=t;else hi=t;} t=(lo+hi)*.5;float o=1-t;float y=o*o*o*a.y+3*o*o*t*b.y+3*o*t*t*c.y+t*t*t*d.y; float mapped=y*OutputRange; float3 curved=l>0.00001?s.rgb*(mapped/l):mapped.xxx; return float4(lerp(s.rgb,curved,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength, InputRange, OutputRange, float3(CurvePoint0, 0.0), float3(CurvePoint1, 0.0), float3(CurvePoint2, 0.0), float3(CurvePoint3, 0.0), float3(CurveHandle0Out, 0.0), float3(CurveHandle1In, 0.0), float3(CurveHandle1Out, 0.0), float3(CurveHandle2In, 0.0), float3(CurveHandle2Out, 0.0), float3(CurveHandle3In, 0.0));
}
