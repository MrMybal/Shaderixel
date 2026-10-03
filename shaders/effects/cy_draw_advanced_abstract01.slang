//@name Advanced Abstract01
//@category CyTexture Dessin
//@generator
//@halo full
//@param RadialStep float 5.01 1.0 128.0 "Radial Step"
//@param Lenght float 0.001 0.0 1.0 "Lenght"
//@param If float 0.0001 0.0 1.0 "If"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedAbstract01.M_CyDraw_AdvancedAbstract01: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_n2(float2 UV, float RadialStep, float Length)
{
float2 Center = float2(0.5, 0.5);
float2 ShapePoint = Center;
float ShapeRadius = Length / 2;
const float PI = 3.14159265;
float Result = 1 ;
float2 Points[64] ;
float2 CurPoint = (float2 )0;
int v = (int )0;
float DotP = (float )0;

for (int i = 0; i < RadialStep; i++)
{
    float Angle = i * (2 * PI / RadialStep);
    ShapePoint.x = Center.x + ShapeRadius * cos(Angle);
    ShapePoint.y = Center.y + ShapeRadius * sin(Angle);
	Points[i] = ShapePoint ;
}
for ( int u = 0; u < RadialStep; u++ )
{
	v = u +1 ;
	if ( v > RadialStep ) {
		v = 0 ;
	}
	CurPoint = normalize(Points[v] - Points[u]) ;
	DotP = abs(dot( Points[u] - UV , float2( CurPoint.y , CurPoint.x )*float2( 1 , -1 ) ) ) ;
Result *= DotP;
}
return SXRet( Result );
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float b_n2 = (Custom_n2(UV, RadialStep, Lenght)).x;
    float v_n1_o = ((b_n2 == If) ? 1.0 : ((b_n2 > If) ? 1.0 : 0.0));
    float v_n0_o = (1.0 - v_n1_o);
    float v_n11_o = (((float4)(v_n0_o))).x;
    float v_n12_o = (((float4)(v_n0_o))).y;
    float v_n10_o = max(v_n11_o, v_n12_o);
    float v_n13_o = (((float4)(v_n0_o))).z;
    float v_n9_o = max(v_n10_o, v_n13_o);
    return float4(((float3)(v_n0_o)), v_n9_o);
}
