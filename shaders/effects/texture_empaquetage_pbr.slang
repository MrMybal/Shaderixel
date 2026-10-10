//@name PBR Map Packing
//@category PBR Textures
//@input Occlusion
//@input Roughness
//@input Metalness
//@input Height
//@param disposition enum 0 "ORM: Occlusion, Roughness, Metalness|RMA: Roughness, Metalness, Occlusion|MRA: Metalness, Roughness, Occlusion" "RGB Channel Order"
//@param hauteur_alpha bool 0 "Height in Alpha"
//@param rugosite_defaut float 0.6 0 1 "Fallback Roughness"
//@param metal_defaut float 0 0 1 "Fallback Metalness"
float4 Effect(EffectContext ctx)
{
    float ao = HasInput(0) ? LinearToSrgb1(Input(0, ctx.px).r) : 1.0;
    float r = HasInput(1) ? LinearToSrgb1(Input(1, ctx.px).r) : rugosite_defaut;
    float m = HasInput(2) ? LinearToSrgb1(Input(2, ctx.px).r) : metal_defaut;
    float h = HasInput(3) ? LinearToSrgb1(Input(3, ctx.px).r) : 1.0;
    float3 packed = disposition < 0.5 ? float3(ao, r, m) : disposition < 1.5 ? float3(r, m, ao) : float3(m, r, ao);
    // Alpha is never converted by the sRGB curve.
    return float4(SrgbToLinear(saturate(packed)), hauteur_alpha > 0.5 ? saturate(h) : 1.0);
}
