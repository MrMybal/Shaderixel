//@name UI Effect Interaction State
//@category UI UV/Effects
//@premultiplied
//@input Image
//@param hover float 0 0 1 "Hover amount"
//@param pressed float 0 0 1 "Pressed amount"
//@param disabled float 0 0 1 "Disabled amount"
//@param hover_tint color 1.2 1.35 1.5 1 "Hover tint"
//@param pressed_tint color 0.65 0.7 0.8 1 "Pressed tint"
//@param disabled_saturation float 0 0 1 "Disabled saturation"
//@param disabled_opacity float 0.45 0 1 "Disabled opacity"
//@param opacity float 1 0 1 "Overall opacity"

// Continuous hover, pressed and disabled controls for graph-driven UI skin variants; states are explicit inputs.
float4 Effect(EffectContext ctx) {
float4 color=Input(0,ctx.px);
float3 rgb=color.a>0.000001?color.rgb/color.a:float3(0,0,0);
rgb*=lerp(float3(1,1,1),hover_tint.rgb,hover);
rgb*=lerp(float3(1,1,1),pressed_tint.rgb,pressed);
float gray=dot(rgb,float3(.2126,.7152,.0722));
rgb=lerp(rgb,lerp(float3(gray,gray,gray),rgb,disabled_saturation),disabled);
float alpha=color.a*lerp(1,hover_tint.a,hover)*lerp(1,pressed_tint.a,pressed)*lerp(1,disabled_opacity,disabled)*opacity;
return float4(rgb*alpha,alpha);
}
