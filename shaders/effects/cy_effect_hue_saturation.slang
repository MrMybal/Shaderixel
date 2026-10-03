//@name Hue Saturation
//@category Traitements
//@graph {"name":"Hue Saturation","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Hue","text":"Hue","index":1,"min":-1.0,"max":1.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Saturation","text":"Saturation","index":1,"min":0.0,"max":4.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Value","text":"Value","index":1,"min":0.0,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Hue Saturation","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float y=dot(s.rgb,float3(.299,.587,.114)); float i=dot(s.rgb,float3(.596,-.274,-.322)); float q=dot(s.rgb,float3(.211,-.523,.312)); float a=Hue*6.2831853; float ir=i*cos(a)-q*sin(a); float qr=i*sin(a)+q*cos(a); float3 h=float3(y+0.956*ir+0.621*qr,y-0.272*ir-0.647*qr,y-1.106*ir+1.703*qr); float l=dot(h,float3(.2126,.7152,.0722)); h=lerp(l.xxx,h,Saturation)*Value; return float4(lerp(s.rgb,h,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Hue",1],["Saturation",1],["Value",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Hue float 0 -1 1 "Hue"
//@param Saturation float 1 0 4 "Saturation"
//@param Value float 1 0 8 "Value"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): YIQ hue rotation with saturation and value controls.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Hue, float Saturation, float Value, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float y=dot(s.rgb,float3(.299,.587,.114)); float i=dot(s.rgb,float3(.596,-.274,-.322)); float q=dot(s.rgb,float3(.211,-.523,.312)); float a=Hue*6.2831853; float ir=i*cos(a)-q*sin(a); float qr=i*sin(a)+q*cos(a); float3 h=float3(y+0.956*ir+0.621*qr,y-0.272*ir-0.647*qr,y-1.106*ir+1.703*qr); float l=dot(h,float3(.2126,.7152,.0722)); h=lerp(l.xxx,h,Saturation)*Value; return float4(lerp(s.rgb,h,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Hue, Saturation, Value, Strength);
}
