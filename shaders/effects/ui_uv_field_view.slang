//@name UI UV Field Preview
//@category UI UV/Debug
//@input UV Map
//@param view enum 0 "Coverage|U|V|UV colors|Checker|Grid" "Preview mode"
//@param grid float2 8 8 1 256 "Grid count X / Y"
//@param width float 0.04 0.001 0.5 "Grid line fraction"

// Diagnostic view only. Its output is a regular image, not a coordinate field.
float4 Effect(EffectContext ctx) {
    float4 m=HasInput(0)?Input(0,ctx.px):float4(ctx.uv,1,1);
    float value=m.b;
    if (view==1) value=m.r;
    if (view==2) value=m.g;
    if (view==3) return float4(frac(m.rg),saturate(m.b),1);
    if (view==4) value=fmod(abs(floor(m.r*grid.x)+floor(m.g*grid.y)),2)*saturate(m.b);
    if (view==5) { float2 edge=min(frac(m.rg*grid),1-frac(m.rg*grid)); value=(min(edge.x,edge.y)<width?1.0:0.0)*saturate(m.b); }
    return float4(value,value,value,1);
}
