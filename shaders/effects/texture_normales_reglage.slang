//@name Normal Adjustment and Conversion
//@category PBR Textures
//@input Normals
//@param force float 1 0 8 "Normal Strength"
//@param inverser_vert bool 0 "Flip Green (OpenGL / DirectX)"
//@param inverser_rouge bool 0 "Flip Red"
float4 Effect(EffectContext ctx)
{
    float3 n = LinearToSrgb(max(Input(0, ctx.px).rgb, 0.0)) * 2.0 - 1.0;
    n.xy *= max(force, 0.0);
    n.x *= inverser_rouge > 0.5 ? -1.0 : 1.0;
    n.y *= inverser_vert > 0.5 ? -1.0 : 1.0;
    n = dot(n, n) > 0.000001 ? normalize(n) : float3(0, 0, 1);
    return float4(SrgbToLinear(n * 0.5 + 0.5), 1.0);
}
