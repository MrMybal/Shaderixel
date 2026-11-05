//@name UI Draw Slice Test Card
//@category UI UV/Draw
//@generator
//@param margins float2 0.25 0.25 0 0.49 "Horizontal / vertical margin fraction"
//@param checks float2 8 8 1 256 "Checker count X / Y"
//@param grid_width float 0.035 0 0.3 "Grid line width (UV)"
//@param atlas bool 0 "Draw 4 by 4 atlas"

// Diagnostic skin with distinct corners, edge colors, center checkers and optional 4 by 4 atlas.
float4 Effect(EffectContext ctx) {
float2 q=ctx.uv; float2 cell=floor(q*4);
if (atlas>.5) q=frac(q*4);
float2 edge=min(q,1-q); bool x=edge.x<margins.x, y=edge.y<margins.y;
float3 color=float3(.045,.08,.14);
if (x&&y) color=q.y<.5?(q.x<.5?float3(1,.12,.06):float3(.1,1,.12)):(q.x<.5?float3(.1,.25,1):float3(1,.75,.03));
else if (y) color=float3(.12,.5,.75);
else if (x) color=float3(.5,.15,.65);
else { float check=fmod(floor(q.x*checks.x)+floor(q.y*checks.y),2); color=lerp(float3(.04,.07,.1),float3(.2,.45,.55),check); }
float2 line=min(frac(q*checks),1-frac(q*checks))/checks;
if (min(line.x,line.y)<grid_width*.2) color*=.45;
if (atlas>.5) color*=.35+.65*(cell.x+cell.y*4)/15;
return float4(color,1);
}
