#include "common.slang"
cbuffer Root : register(b0) { uint2 gSize; uint gCount; float gRadius; float gSpacing; uint gStart; uint gStyle; uint gHasStencil; float gLength; };
struct Segment { float4 a, b, normal; };
cbuffer Segments : register(b1) { Segment gSegments[128]; };
Texture2D<float4> gPosition : register(t0);
Texture2D<float4> gNormal : register(t1);
Texture2D<float> gStencil : register(t2);
RWTexture2D<float> gCoverage : register(u0);
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy>=gSize)) return;
    float4 p = gPosition.Load(int3(id.xy,0));
    float coverage = gStart != 0 ? 0 : gCoverage[id.xy];
    if (p.w > 0) {
        float3 normal = gNormal.Load(int3(id.xy,0)).xyz;
        for (uint i=0;i<gCount;++i) {
            Segment s=gSegments[i];
            if (dot(normal,s.normal.xyz) < 0.1) continue;
            float3 axis=s.b.xyz-s.a.xyz;
            float progress=dot(p.xyz-s.a.xyz,axis)/max(dot(axis,axis),1e-15);
            float t=saturate(progress);
            float d=length(p.xyz-lerp(s.a.xyz,s.b.xyz,t));
            float stencil=1;
            if (gStyle==1) {
                if(gLength>1e-6 && ((s.a.w<1e-6 && progress<0) || (s.b.w>=gLength-1e-6 && progress>1))) continue;
                if(gHasStencil!=0 && d<gRadius && dot(axis,axis)>1e-12) {
                    float3 right=normalize(cross(s.normal.xyz,axis));
                    float along=lerp(s.a.w,s.b.w,t);
                    float repeat=gSpacing>0?gSpacing:gRadius*4;
                    float across=dot(p.xyz-lerp(s.a.xyz,s.b.xyz,t),right)/gRadius;
                    stencil=gStencil.SampleLevel(sLinearClamp,float2(frac(along/repeat),saturate(across*0.5+0.5)),0);
                }
            } else if (gSpacing>0) {
                float along=lerp(s.a.w,s.b.w,t);
                float phase=abs(frac(along/gSpacing+0.5)-0.5)*gSpacing;
                d=sqrt(d*d+phase*phase);
            }
            coverage=max(coverage,stencil*(1-smoothstep(gRadius*0.86,gRadius,d)));
        }
    }
    gCoverage[id.xy]=coverage;
}
