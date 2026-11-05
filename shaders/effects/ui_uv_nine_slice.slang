//@name UI UV Nine Slice / Eight Slice
//@category UI UV/Slicing
//@generator
//@param source_origin float2 0 0 0 1 "Source region origin (UV)"
//@param source_extent float2 1 1 1e-05 1 "Source region size (UV)"
//@param source_size float2 0 0 0 32768 "Logical source size (px; 0 = region texels)"
//@param margin_units enum 0 "Fraction|Pixels" "Source margin units"
//@param left float 0.25 0 32768 "Left source margin"
//@param top float 0.25 0 32768 "Top source margin"
//@param right float 0.25 0 32768 "Right source margin"
//@param bottom float 0.25 0 32768 "Bottom source margin"
//@param position float2 0.5 0.5 -4 4 "Widget position (document UV)"
//@param anchor float2 0.5 0.5 0 1 "Widget anchor"
//@param target_size float2 0 0 0 32768 "Widget size (px; 0 = canvas)"
//@param softness float 1 0 16 "Edge softness (px)"
//@param border_scale float 1 0 64 "Border scale"
//@param center_mode enum 0 "Stretch|Tile|Mirror tile|Transparent" "Center mode"
//@param edge_mode enum 0 "Stretch|Tile|Mirror tile" "Edge mode"
//@param tile_scale float2 1 1 0.001 100 "Tile scale (target pixels per source pixel)"
//@param phase float2 0 0 -1000 1000 "Tile phase (cycles)"
//@param tile_fit enum 0 "Natural partial tiles|Round to whole tiles" "Tile fitting"
//@param output_mode enum 0 "UV coordinates|Coverage|Center mask|Border mask|Corner mask|Slice IDs" "Output"

// Nine-region mapping with independent center and edge tiling, pixel or fractional margins, atlas region, masks and hollow eight-slice borders. Tiny destinations compress borders proportionally.
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
float2 src=max(float2(source_size.x>0?source_size.x:actual.x, source_size.y>0?source_size.y:actual.y),1);
float2 low=float2(left,top), high=float2(right,bottom);
if (margin_units < 0.5) { low*=src; high*=src; }
float2 marginScale=min(float2(1,1),max(src-1,0)/max(low+high,0.0001));
low*=marginScale; high*=marginScale;
float2 dl=low*border_scale, dh=high*border_scale;
float2 shrink=min(float2(1,1),size/max(dl+dh,0.0001)); dl*=shrink; dh*=shrink;
bool middleX=p.x>=dl.x && p.x<=size.x-dh.x;
bool middleY=p.y>=dl.y && p.y<=size.y-dh.y;
bool center=middleX&&middleY;
bool corner=!middleX&&!middleY;
float mode=center?center_mode:edge_mode;
float u=SliceAxis(p.x,size.x,low.x,high.x,src.x,border_scale,mode,tile_scale.x,phase.x,tile_fit,actual.x);
float v=SliceAxis(p.y,size.y,low.y,high.y,src.y,border_scale,mode,tile_scale.y,phase.y,tile_fit,actual.y);
float centerCoverage=0;
float inner=min(min(p.x-dl.x,p.y-dl.y),min(size.x-dh.x-p.x,size.y-dh.y-p.y));
if (size.x>dl.x+dh.x+0.0001 && size.y>dl.y+dh.y+0.0001)
    centerCoverage=softness>0?saturate(inner/softness+0.5):(center?1.0:0.0);
float resultCoverage=coverage*(center_mode>2.5?1-centerCoverage:1);
float value=coverage;
if (output_mode>1.5 && output_mode<2.5) value=coverage*centerCoverage;
else if (output_mode>2.5 && output_mode<3.5) value=coverage*(1-centerCoverage);
else if (output_mode>3.5 && output_mode<4.5) value=coverage*(corner?1.0:0.0);
else if (output_mode>4.5) value=coverage*((p.x<dl.x?0:(middleX?1:2))+3*(p.y<dl.y?0:(middleY?1:2))+1)/9.0;
else if (output_mode>0.5) value=resultCoverage;
if (output_mode>0.5) return float4(value,value,value,1);
return float4(source_origin+float2(u,v)*region,resultCoverage,1);
}
