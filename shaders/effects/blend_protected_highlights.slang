//@name Protected Highlights
//@category Creative Blends
//@blend
//@input Background
//@input Foreground
//@input Mask
//@param opacity float 1 0 1 "Foreground Opacity"
//@param protect_highlights float 1 0 1 "Highlight Protection"
//@param protect_shadows float 0 0 1 "Shadow Protection"
//@param high float 0.8 0 1 "Highlight Threshold"
//@param low float 0.15 0 1 "Shadow Threshold"
//@param softness float 0.15 0.001 0.5 "Protection Softness"

// Alpha-correct foreground compositing with independent soft highlight and shadow protection.
float4 ExtMix(float4 a, float4 b, float t)
{
    return Unpremultiply(lerp(Premultiply(a), Premultiply(b), saturate(t)));
}
float4 ExtOver(float4 a, float4 b, float coverage)
{
    b.a = saturate(b.a * coverage);
    float4 ap = Premultiply(a), bp = Premultiply(b);
    return Unpremultiply(bp + ap * (1.0 - b.a));
}

float4 Effect(EffectContext ctx)
{
    float4 a=Input(0,ctx.px), b=Input(1,ctx.px);
    float y=saturate(Luma(LinearToSrgb(max(a.rgb,0.0))));
    float hi=smoothstep(high,min(high+softness,1.000001),y)*protect_highlights;
    float lo=(1.0-smoothstep(max(low-softness,-0.000001),low,y))*protect_shadows;
    float mask=HasInput(2)?saturate(Input(2,ctx.px).r*Input(2,ctx.px).a):1.0;
    // Protection only applies where the background exists.
    return ExtOver(a,b,opacity*mask*(1.0-hi*a.a)*(1.0-lo*a.a));
}
