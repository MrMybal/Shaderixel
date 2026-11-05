//@name UI UV Three Slice
//@category UI UV/Slicing
//@generator
//@param axis enum 0 "Horizontal|Vertical" "Slice direction"
//@param start_cap float 16 0 32768 "Start cap (source px)"
//@param end_cap float 16 0 32768 "End cap (source px)"
//@param source_size float2 0 0 0 32768 "Logical source size (px; 0 = region texels)"
//@param source_origin float2 0 0 0 1 "Source region origin (UV)"
//@param source_extent float2 1 1 1e-05 1 "Source region size (UV)"
//@param position float2 0.5 0.5 -4 4 "Widget position (document UV)"
//@param anchor float2 0.5 0.5 0 1 "Widget anchor"
//@param target_size float2 0 0 0 32768 "Widget size (px; 0 = canvas)"
//@param softness float 1 0 16 "Edge softness (px)"
//@param border_scale float 1 0 64 "Cap scale"
//@param center_mode enum 1 "Stretch|Tile|Mirror tile" "Middle mode"
//@param tile_scale float 1 0.001 100 "Middle tile scale"
//@param phase float 0 -1000 1000 "Tile phase (cycles)"
//@param tile_fit enum 0 "Natural partial tiles|Round to whole tiles" "Tile fitting"

// Horizontal or vertical cap-preserving mapping for progress bars and buttons, with tiled or stretched middle.
float SliceAxis(float p, float size, float lo, float hi, float srcSize,
                float borderScale, float mode, float density, float phaseValue, float roundTiles, float texels) {
    float dl = lo * borderScale, dr = hi * borderScale;
    float shrink = min(1.0, size / max(dl + dr, 0.0001));
    dl *= shrink; dr *= shrink;
    float begin, end, t;
    if (p < dl && dl > 0.00001) { begin=0; end=lo/srcSize; t=p/dl; }
    else if (p > size-dr && dr > 0.00001) { begin=1-hi/srcSize; end=1; t=(p-size+dr)/dr; }
    else {
        begin=lo/srcSize; end=1-hi/srcSize;
        float span=max(size-dl-dr,0.00001);
        t=(p-dl)/span;
        if (mode > 0.5 && mode < 2.5) {
            float count=span / max((srcSize-lo-hi)*max(density,0.0001),0.0001);
            if (roundTiles > 0.5) count=max(1.0, floor(count+0.5));
            float cycle=t*count+phaseValue;
            t=mode < 1.5 ? frac(cycle) : 1-abs(frac(cycle*0.5)*2-1);
        }
    }
    float inset=min(0.5/max(texels,1.0),max(end-begin,0.0)*0.5);
    return clamp(lerp(begin,end,saturate(t)),begin+inset,end-inset);
}
float4 Effect(EffectContext ctx) {

float2 size = max(float2(target_size.x > 0 ? target_size.x : ctx.size.x, target_size.y > 0 ? target_size.y : ctx.size.y), 0.001);
float2 p = (ctx.uv - position) * ctx.size + anchor * size;
float2 q = p / size;
float edge = min(min(p.x, p.y), min(size.x - p.x, size.y - p.y));
float coverage = softness > 0 ? saturate(edge / softness + 0.5) : (edge >= 0 ? 1.0 : 0.0);

float2 region=min(source_extent,1-source_origin);
if (any(region<=0)) return float4(0,0,0,1);
float2 actual=max(region*ctx.size,1);
float2 src=max(float2(source_size.x>0?source_size.x:actual.x,source_size.y>0?source_size.y:actual.y),1);
float sourceLength=axis<0.5?src.x:src.y;
float factor=min(1.0,max(sourceLength-1,0)/max(start_cap+end_cap,0.0001));
float coordinate=axis<0.5?p.x:p.y, length=axis<0.5?size.x:size.y;
float mapped=SliceAxis(coordinate,length,start_cap*factor,end_cap*factor,sourceLength,border_scale,center_mode,tile_scale,phase,tile_fit,axis<0.5?actual.x:actual.y);
if (axis<0.5) q.x=mapped; else q.y=mapped;
q=clamp(q,0.5/actual,1-0.5/actual);
return float4(source_origin+q*region,coverage,1);
}
