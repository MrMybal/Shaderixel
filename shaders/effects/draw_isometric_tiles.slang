//@name Isometric Tiles
//@category Procedural Patterns
//@generator
//@param size float 32 4 256 "Tile Radius (px)"
//@param mode enum 1 "Diamonds|Cubes" "Tile Style"
//@param border_width float 0.8 0 8 "Border Width (px)"
//@param offset float2 0 0 -2048 2048 "Offset (px)"
//@param top color 0.46 0.73 0.83 1 "Top Color"
//@param left color 0.1 0.28 0.4 1 "Left Color"
//@param right color 0.2 0.46 0.6 1 "Right Color"
//@param border color 0.015 0.06 0.09 1 "Border Color"

// Isometric diamond or three-face cube tiling with independent palette, border and pixel offset.
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
    float r=max(size,4.0); float2 p=float2(ctx.px)+0.5-offset;
    if(mode==0)
    {
        float2 q=float2(p.x/1.7320508+p.y,-p.x/1.7320508+p.y)/r;
        float2 f=frac(q); float parity=frac((floor(q.x)+floor(q.y))*0.5);
        float4 face=parity<0.25?top:right;
        float edge=min(min(f.x,1.0-f.x),min(f.y,1.0-f.y))*r*0.8660254;
        return ExtMix(face,border,border_width<=0.0?0.0:1.0-smoothstep(border_width,border_width+0.7,edge));
    }
    float row=floor(p.y/(1.5*r)), col=floor(p.x/(1.7320508*r)-row*0.5);
    float nearest=1e20; float2 chosen=0.0;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    {
        float rr=row+y; float2 c=float2((col+x+rr*0.5)*1.7320508*r,rr*1.5*r);
        float d=dot(p-c,p-c); if(d<nearest) {nearest=d; chosen=c;}
    }
    float2 local=p-chosen;
    float4 face=local.y<-abs(local.x)*0.57735027?top:(local.x<0.0?left:right);
    float edge=1e20;
    for(int i=0;i<6;++i)
    {
        float a=radians(60.0*i); float2 n=float2(cos(a),sin(a));
        edge=min(edge,r*0.8660254-dot(local,n));
    }
    return ExtMix(face,border,border_width<=0.0?0.0:1.0-smoothstep(border_width,border_width+0.7,edge));
}
