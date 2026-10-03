//@name Brick Bond
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Columns float 8 1 128 "Columns"
//@param Rows float 16 1 256 "Rows"
//@param Seed float 0 -1000 1000 "Seed"
//@param MortarWidth float 0.055 0.001 0.45 "Mortar Width"
//@param Softness float 0.01 0.0001 0.2 "Softness"
//@param BevelWidth float 0.12 0.001 0.5 "Bevel Width"
//@param BevelDarkening float 0.65 0 1 "Bevel Darkening"
//@param GrainScale float 12 1 256 "Grain Scale"
//@param GrainAmount float 0.12 0 1 "Grain Amount"
//@param Opacity float 1 0 1 "Opacity"
//@param BrickColorA color 0.28 0.055 0.025 1 "Brick Color A"
//@param BrickColorB color 0.55 0.14 0.055 1 "Brick Color B"
//@param MortarColor color 0.28 0.25 0.2 1 "Mortar Color"

// Imported from CyTexture (Draw / Surface): Seamless running-bond brick generator with mortar, bevel, variation, and surface grain.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Columns, float Rows, float Seed, float MortarWidth, float Softness, float BevelWidth, float BevelDarkening, float GrainScale, float GrainAmount, float Opacity, float3 BrickColorA, float3 BrickColorB, float3 MortarColor)
{
    float2 q=UV*float2(max(Columns,1.0),max(Rows,1.0));float row=floor(q.y);q.x+=fmod(row,2.0)*.5;float2 id=floor(q);float2 p=frac(q);float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453);float2 edge=min(p,1.0-p);float mortar=1.0-smoothstep(MortarWidth,MortarWidth+Softness,min(edge.x,edge.y));float bevel=saturate(min(edge.x,edge.y)/max(BevelWidth,.0001));float grain=frac(sin(dot(floor(q*GrainScale)+Seed,float2(12.9898,78.233)))*43758.5453);float3 brick=lerp(BrickColorA.rgb,BrickColorB.rgb,h)*lerp(1.0-GrainAmount,1.0+GrainAmount,grain)*lerp(BevelDarkening,1.0,bevel);return float4(lerp(brick,MortarColor.rgb,mortar),Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Columns, Rows, Seed, MortarWidth, Softness, BevelWidth, BevelDarkening, GrainScale, GrainAmount, Opacity, BrickColorA.rgb, BrickColorB.rgb, MortarColor.rgb);
}
