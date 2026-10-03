//@name Kuwahara
//@category Traitements
//@graph {"name":"Kuwahara","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[6.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.5,"max":64.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.150000006,0.0,0.0,0.0],"name":"Stylize","text":"Stylize","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.899999976,0.0,0.0,0.0],"name":"Posterize","text":"Posterize","index":1,"min":0.100000001,"max":4.0},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Kuwahara","text":"    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float3 a0=0,a1=0,a2=0,a3=0;float3 q0=0,q1=0,q2=0,q3=0;[unroll]for(int y=0;y<3;y++){[unroll]for(int x=0;x<3;x++){float2 o=float2(x,y)/2.0*t;float3 s0=Texture2DSample(CyInput,CyInputSampler,UV-o).rgb;float3 s1=Texture2DSample(CyInput,CyInputSampler,UV+float2(o.x,-o.y)).rgb;float3 s2=Texture2DSample(CyInput,CyInputSampler,UV+float2(-o.x,o.y)).rgb;float3 s3=Texture2DSample(CyInput,CyInputSampler,UV+o).rgb;a0+=s0;q0+=s0*s0;a1+=s1;q1+=s1*s1;a2+=s2;q2+=s2*s2;a3+=s3;q3+=s3*s3;}}a0/=9;a1/=9;a2/=9;a3/=9;q0=abs(q0/9-a0*a0);q1=abs(q1/9-a1*a1);q2=abs(q2/9-a2*a2);q3=abs(q3/9-a3*a3);float v0=dot(q0,float3(1,1,1)),v1=dot(q1,float3(1,1,1)),v2=dot(q2,float3(1,1,1)),v3=dot(q3,float3(1,1,1));float3 best=a0;float v=v0;if(v1<v){best=a1;v=v1;}if(v2<v){best=a2;v=v2;}if(v3<v){best=a3;}best=lerp(best,pow(saturate(best),Posterize),Stylize);return float4(lerp(c.rgb,best,saturate(Strength)),c.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1],["Stylize",1],["Posterize",1]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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
