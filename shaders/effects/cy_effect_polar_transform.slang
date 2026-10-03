//@name Polar Transform
//@category Traitements
//@graph {"name":"Polar Transform","category":"Traitements","inputs":1,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.0500000007,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"RadialScale","text":"Radial Scale","index":1,"min":0.00999999978,"max":32.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"RadialOffset","text":"Radial Offset","index":1,"min":-8.0,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-4.0,"max":4.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Twist","text":"Twist","index":1,"min":-16.0,"max":16.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"AngularRepeat","text":"Angular Repeat","index":1,"min":1.0,"max":64.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"ToPolar","text":"To Polar","index":1},{"id":11,"op":"param2","pos":[40,1100],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Polar Transform","text":"    float2 p=UV-Center.xy;p.x*=Aspect;float radius=length(p)*RadialScale+RadialOffset;float angle=atan2(p.y,p.x)/6.283185+0.5+Rotation+radius*Twist;float2 polar=float2(frac(angle*AngularRepeat),frac(radius));float2 target=lerp(UV,polar,saturate(ToPolar));float4 s=Texture2DSample(CyInput,CyInputSampler,target);return s;","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Aspect",1],["RadialScale",1],["RadialOffset",1],["Rotation",1],["Twist",1],["AngularRepeat",1],["ToPolar",1],["Center",3]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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
