//@name Kaleidoscope
//@category Traitements
//@graph {"name":"Kaleidoscope","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Segments","text":"Segments","index":1,"min":1.0,"max":64.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-6.28299999,"max":6.28299999},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.00999999978,"max":8.0},{"id":7,"op":"param2","pos":[40,660],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Kaleidoscope","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float r=length(p); float a=atan2(p.y,p.x)+Rotation; float sector=6.2831853/max(Segments,1.0); a=abs(fmod(a+sector*0.5,sector)-sector*0.5); float2 q=float2(cos(a),sin(a))*r; q.x/=max(Aspect,0.001); return Texture2DSample(CyInput,CyInputSampler,Center.xy+q);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Segments",1],["Rotation",1],["Aspect",1],["Center",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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
