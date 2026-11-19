// The opacity brush stores its three-decimal visual value in a linear half-float
// layer. Snap the cutoff's own display bin before filtering into the path-tracing atlas.
float MeshOpacity(Texture2D<float4> image, float2 uv)
{
    float4 c = image.SampleLevel(sLinearWrap, uv, 0);
    float opacity = lerp(1.0, LinearToSrgb1(saturate(Unpremultiply(c).r)), saturate(c.a));
    return abs(opacity - 0.5) < 0.0005 ? 0.5 : opacity;
}
