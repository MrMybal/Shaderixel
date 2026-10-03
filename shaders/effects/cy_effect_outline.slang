//@name Outline
//@category Traitements
//@graph {"name":"Outline","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Thickness","text":"Thickness","index":1,"min":0.25,"max":128.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Inner","text":"Inner","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"OutlineColor","text":"Outline Color","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Outline","text":"    float2 t=Thickness/max(CyCanvasSize.xy,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float a0=Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).a;float a1=Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).a;float a2=Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).a;float a3=Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).a;float expanded=max(max(a0,a1),max(a2,a3));float contracted=min(min(a0,a1),min(a2,a3));float edge=lerp(saturate(expanded-s.a),saturate(s.a-contracted),saturate(Inner));float alpha=edge*Opacity*(1.0-s.a*(1.0-Inner));return float4(s.rgb+OutlineColor.rgb*alpha,s.a+alpha);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Thickness",1],["Inner",1],["Opacity",1],["OutlineColor",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Thickness float 2 0.25 128 "Thickness"
//@param Inner float 0 0 1 "Inner"
//@param Opacity float 1 0 1 "Opacity"
//@param OutlineColor color 1 1 1 1 "Outline Color"

// Imported preset (Effect / Detail): Builds an inner or outer outline from source alpha.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Thickness, float Inner, float Opacity, float3 OutlineColor)
{
    float2 t=Thickness/max(CyCanvasSize.xy,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float a0=Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).a;float a1=Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).a;float a2=Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).a;float a3=Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).a;float expanded=max(max(a0,a1),max(a2,a3));float contracted=min(min(a0,a1),min(a2,a3));float edge=lerp(saturate(expanded-s.a),saturate(s.a-contracted),saturate(Inner));float alpha=edge*Opacity*(1.0-s.a*(1.0-Inner));return float4(s.rgb+OutlineColor.rgb*alpha,s.a+alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Thickness, Inner, Opacity, OutlineColor.rgb);
}
