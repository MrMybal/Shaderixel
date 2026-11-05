//@name UI Alpha Refine and Matte Removal
//@category UI Toolkit/Alpha
//@premultiplied
//@input Image
//@input Mask
//@param low float 0 0 1 "Alpha black point"
//@param high float 1 0 1 "Alpha white point"
//@param gamma float 1 0.001 16 "Alpha gamma"
//@param contrast float 1 0 16 "Alpha contrast"
//@param invert bool 0 "Invert alpha"
//@param matte color 1 1 1 1 "Matte color"
//@param remove_matte float 0 0 1 "Matte removal amount"
//@param opacity float 1 0 1 "Opacity"

// Alpha levels, gamma, contrast and inversion with optional matte-color decontamination and a region mask.
float4 Effect(EffectContext ctx) {
 float4 c=Input(0,ctx.px);float4 straight=Unpremultiply(c);
 float alpha=saturate((c.a-low)/max(high-low,.00001));alpha=pow(alpha,1/max(gamma,.00001));alpha=saturate((alpha-.5)*contrast+.5);
 if(invert>.5) alpha=1-alpha;alpha*=opacity;
 float3 rgb=straight.rgb;
 if(c.a>.00001 && c.a<1) rgb=lerp(rgb,max((rgb-matte.rgb*(1-c.a))/c.a,0),remove_matte);
 float4 corrected=float4(rgb*alpha,alpha);float mask=HasInput(1)?saturate(Input(1,ctx.px).r):1;
 return lerp(c,corrected,mask);
}
