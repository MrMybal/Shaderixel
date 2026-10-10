//@name Constant Data Map
//@category Texture Generators
//@generator
//@param valeur float 0 0 1 "Map Value"
float4 Effect(EffectContext ctx)
{
    return float4(SrgbToLinear(saturate(valeur).xxx), 1.0);
}
