//@name Advanced Shape Lens
//@category CyTexture Dessin
//@generator
//@halo full
//@param RadialStep float 5.01 1.0 128.0 "Radial Step"
//@param Lenght float 0.4 0.0 1.0 "Lenght"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedShapeLens.M_CyDraw_AdvancedShapeLens: Native CyTexture advanced Draw preset with live GPU parameters.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }  // unconnected = white, as Unreal
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
#define Texture2DSampleLevel(T, S, UVS, LOD) CyTex(T, UVS)
static EffectContext ctx;  // for helper functions (noise) reading the document size
// Unreal view values and helpers used by Custom nodes (a document pixel = a screen pixel).
struct SXView { float4 ViewSizeAndInvSize; float4 BufferSizeAndInvSize; float4 TemporalAAParams; };
static SXView View;
static SXView ResolvedView;
float2 ViewportUVToBufferUV(float2 uv) { return uv; }
float2 BufferUVToViewportUV(float2 uv) { return uv; }
float4 SXRet(float v) { return float4(v, v, v, 1.0); }
float4 SXRet(float2 v) { return float4(v, 0.0, 1.0); }
float4 SXRet(float3 v) { return float4(v, 1.0); }
float4 SXRet(float4 v) { return v; }

float4 Custom_n0(float2 UV, float Step, float Length)
{
// Lens Shape shader
// By CyberAlien

//RadialStep
//Length
//UV

float2 Center = float2(0.5, 0.5);
float2 ShapePoint = (float2 )0;
const float PI = 3.14159265;
float RadialStep = Step ;
float RadialStepAngle = ( 2 * PI ) / RadialStep ;
float Result = 0 ;
float2 Points[12] ;
float2 CurPoint = (float2 )0;
int v = (int )0;
float DotP = (float )0;


for (int i = 0; i < RadialStep; i++)
{
    float Angle = i * RadialStepAngle ;
    ShapePoint = float2(cos(Angle),sin(Angle)) ;
	Points[i] = Center + ShapePoint * Length ;
}
for ( int u = 0; u < RadialStep; u++ )
{
	v = u + 1 ;
	if ( v > RadialStep ) {
		v = 0 ;
	}
	CurPoint = normalize( Points[v] - Points[u] ) ;
	DotP = dot(Points[u]-UV , float2(CurPoint.g,CurPoint.r)*float2(1,-1));
		if ( DotP >= 0 ) {
			Result += 1 ;
		}
		else {
			Result += 0 ;
		}
}
float OutDiv = RadialStep - frac(RadialStep);
Result /= OutDiv;
if ( Result >= 1.01000 )
{
	return SXRet( 1 );
}
return SXRet( 0 );
//return SXRet( Result);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float b_n0 = (Custom_n0(UV, RadialStep, Lenght)).x;
    float v_n6_o = (((float4)(b_n0))).x;
    float v_n7_o = (((float4)(b_n0))).y;
    float v_n5_o = max(v_n6_o, v_n7_o);
    float v_n8_o = (((float4)(b_n0))).z;
    float v_n4_o = max(v_n5_o, v_n8_o);
    return float4(((float3)(b_n0)), v_n4_o);
}
