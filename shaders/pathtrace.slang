// Render mode: progressive path tracer (GPU twin of PtTrace in src/render/PathTracer.cpp).
// Geometry and BVH live in a float texture (any GPU, no ray tracing hardware
// needed): triangle t uses texels 8t..8t+6, BVH node n texels nodeBase + 2n.
// GGX + Lambert + rough dielectric, next-event estimation (one light or the
// HDRI per bounce) with multiple importance sampling, Russian roulette.
// Each pass adds one sample per pixel of a band of rows into the accumulation.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gRowStart;
    uint gPass;      // 0: overwrite the accumulation
    uint gSeed;
};

cbuffer Scene : register(b1)
{
    float4 gCamPos;    // xyz, lens radius
    float4 gCamRight;  // xyz scaled by the half width at distance 1, focus distance
    float4 gCamUp;     // xyz scaled by the half height
    float4 gCamFwd;    // xyz, environment rotation (turns)
    uint gMaxBounces;
    uint gNumLights;
    uint gNumMaterials;
    uint gFlags;       // 1 HDRI, 2 HDRI visible, 4 project textures, 8 geometry present
    uint gNodeBase;
    uint gEnvW;
    uint gEnvH;
    uint gPad0;
    float4 gBackground;  // rgb, HDRI intensity
    float4 gEnvInfo;     // luminance total, indirect clamp
    float4 gMat[96];     // 32 materials: (base, roughness) (emission, metallic) (transmission, ior, project, 0)
    float4 gLight[64];   // 16 lights: (pos, type) (dir, size) (radiance, cos outer) (cos inner, 0, 0, 0)
};

Texture2D<float4> gGeo : register(t0);
Texture2D<float4> gEnv : register(t1);
Texture2D<float4> gEnvCdf : register(t2);  // rows 0..H-1: conditional CDFs, row H: marginal
Texture2D<float4> gBase : register(t3);    // straight linear base color, a = coverage
Texture2D<float4> gOrm : register(t4);     // roughness, metallic (visual), occlusion
Texture2D<float4> gEmis : register(t5);    // linear emission
Texture2D<float4> gNrm : register(t6);     // tangent-space normal * 0.5 + 0.5
RWTexture2D<float4> gAccum : register(u0);

static const float PI = 3.14159265;
static const float kFar = 1e30;

// ---------------------------------------------------------------- random numbers
uint Pcg(inout uint s)
{
    s = s * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float Rnd(inout uint s) { return (Pcg(s) >> 8) * (1.0 / 16777216.0); }

float4 Geo(uint i) { return gGeo.Load(int3(i & 4095u, i >> 12, 0)); }
float Lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

// ---------------------------------------------------------------- intersection
struct Hit
{
    float t;
    int tri;
    float b1, b2;
    int light;
};

float BoxHit(float3 o, float3 inv, float3 bmin, float3 bmax, float tmax)
{
    float3 t0 = (bmin - o) * inv, t1 = (bmax - o) * inv;
    float3 tn = min(t0, t1), tf = max(t0, t1);
    float a = max(max(tn.x, tn.y), max(tn.z, 0.0));
    float b = min(min(tf.x, tf.y), min(tf.z, tmax));
    return a <= b ? a : kFar;
}

void TestTriangle(uint tri, float3 o, float3 d, inout Hit h)
{
    float3 p0 = Geo(tri * 8).xyz, p1 = Geo(tri * 8 + 1).xyz, p2 = Geo(tri * 8 + 2).xyz;
    float3 e1 = p1 - p0, e2 = p2 - p0;
    float3 pv = cross(d, e2);
    float det = dot(e1, pv);
    if (abs(det) < 1e-12) return;
    float id = 1.0 / det;
    float3 tv = o - p0;
    float u = dot(tv, pv) * id;
    if (u < 0 || u > 1) return;
    float3 qv = cross(tv, e1);
    float v = dot(d, qv) * id;
    if (v < 0 || u + v > 1) return;
    float t = dot(e2, qv) * id;
    if (t > 1e-4 && t < h.t)
    {
        h.t = t;
        h.tri = (int)tri;
        h.b1 = u;
        h.b2 = v;
    }
}

Hit Trace(float3 o, float3 d, float tmax, bool anyHit)
{
    Hit h;
    h.t = tmax;
    h.tri = -1;
    h.b1 = 0;
    h.b2 = 0;
    h.light = -1;
    if (!(gFlags & 8)) return h;
    float3 inv = float3(1.0 / (abs(d.x) > 1e-12 ? d.x : 1e-12), 1.0 / (abs(d.y) > 1e-12 ? d.y : 1e-12), 1.0 / (abs(d.z) > 1e-12 ? d.z : 1e-12));
    if (BoxHit(o, inv, Geo(gNodeBase).xyz, Geo(gNodeBase + 1).xyz, h.t) >= kFar) return h;
    uint stack[40];
    int sp = 0;
    uint node = 0;
    [loop]
    for (int guard = 0; guard < 100000; ++guard)
    {
        float4 a = Geo(gNodeBase + node * 2), b = Geo(gNodeBase + node * 2 + 1);
        uint first = asuint(a.w), count = asuint(b.w);
        if (count > 0)
        {
            [loop]
            for (uint i = 0; i < count; ++i)
            {
                TestTriangle(first + i, o, d, h);
                if (anyHit && h.tri >= 0) return h;
            }
            if (sp == 0) break;
            node = stack[--sp];
            continue;
        }
        uint c0 = first, c1 = first + 1;
        float d0 = BoxHit(o, inv, Geo(gNodeBase + c0 * 2).xyz, Geo(gNodeBase + c0 * 2 + 1).xyz, h.t);
        float d1 = BoxHit(o, inv, Geo(gNodeBase + c1 * 2).xyz, Geo(gNodeBase + c1 * 2 + 1).xyz, h.t);
        if (d1 < d0)
        {
            float td = d0; d0 = d1; d1 = td;
            uint tc = c0; c0 = c1; c1 = tc;
        }
        if (d0 >= kFar)
        {
            if (sp == 0) break;
            node = stack[--sp];
            continue;
        }
        node = c0;
        if (d1 < kFar && sp < 40) stack[sp++] = c1;
    }
    return h;
}

// ---------------------------------------------------------------- lights
float3 LightPos(uint i) { return gLight[i * 4].xyz; }
uint LightType(uint i) { return (uint)gLight[i * 4].w; }  // 0 point, 1 spot, 2 area, 3 sun
float3 LightDir(uint i) { return gLight[i * 4 + 1].xyz; }
float LightSize(uint i) { return gLight[i * 4 + 1].w; }
float3 LightRadiance(uint i) { return gLight[i * 4 + 2].rgb; }

void LightSquare(float3 n, out float3 u, out float3 v)
{
    u = normalize(abs(n.y) < 0.99 ? cross(float3(0, 1, 0), n) : cross(float3(1, 0, 0), n));
    v = cross(n, u);
}

bool AreaLightHit(uint i, float3 o, float3 d, out float t)
{
    t = 0;
    float3 n = LightDir(i);
    float dn = dot(d, n);
    if (dn >= -1e-6) return false;
    t = dot(LightPos(i) - o, n) / dn;
    if (t <= 1e-4) return false;
    float3 p = o + d * t - LightPos(i);
    float3 u, v;
    LightSquare(n, u, v);
    float hs = LightSize(i) * 0.5;
    return abs(dot(p, u)) <= hs && abs(dot(p, v)) <= hs;
}

void Basis(float3 n, out float3 t, out float3 b)
{
    t = normalize(abs(n.x) > 0.5 ? cross(float3(0, 1, 0), n) : cross(float3(1, 0, 0), n));
    b = cross(n, t);
}

// ---------------------------------------------------------------- environment
int2 EnvTexel(float3 d)
{
    float u = atan2(d.x, -d.z) / (2.0 * PI) + 0.5 + gCamFwd.w;
    u -= floor(u);
    float th = acos(clamp(d.y, -1.0, 1.0));
    return int2(clamp((int)(u * gEnvW), 0, (int)gEnvW - 1), clamp((int)(th / PI * gEnvH), 0, (int)gEnvH - 1));
}
float3 EnvLookup(float3 d)
{
    if (!(gFlags & 1)) return gBackground.rgb;
    return gEnv.Load(int3(EnvTexel(d), 0)).rgb * gBackground.w;
}
float EnvPdf(float3 d)
{
    float l = max(1e-6, Lum(gEnv.Load(int3(EnvTexel(d), 0)).rgb));
    return gEnvInfo.x > 0 ? l * gEnvW * gEnvH / (gEnvInfo.x * 2.0 * PI * PI) : 1.0 / (4.0 * PI);
}
uint LowerBound(uint row, uint n, float r)
{
    uint lo = 0, hi = n - 1;
    [loop]
    while (lo < hi)
    {
        uint mid = (lo + hi) >> 1;
        if (gEnvCdf.Load(int3(mid, row, 0)).r < r) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}
float3 EnvSampleDir(inout uint rng, out float pdf)
{
    uint y = LowerBound(gEnvH, gEnvH, Rnd(rng));
    uint x = LowerBound(y, gEnvW, Rnd(rng));
    float u = (x + Rnd(rng)) / gEnvW - gCamFwd.w, v = (y + Rnd(rng)) / gEnvH;
    float phi = (u - 0.5) * 2.0 * PI, th = v * PI;
    float3 d = float3(sin(th) * sin(phi), cos(th), -sin(th) * cos(phi));
    pdf = EnvPdf(d);
    return d;
}

// ---------------------------------------------------------------- BSDF
float D_GGX(float nh, float a)
{
    float a2 = a * a, d = nh * nh * (a2 - 1) + 1;
    return a2 / (PI * d * d + 1e-12);
}
float G1(float nx, float a)
{
    float k = a * 0.5;
    return nx / (nx * (1 - k) + k + 1e-12);
}
float3 FSchlick(float3 f0, float c) { return f0 + (1.0 - f0) * pow(1.0 - saturate(c), 5.0); }

struct Surf
{
    float3 p, n, ng, base, emission;
    float rough, metal, transmission, ior;
};

float3 BsdfEval(Surf s, float3 wo, float3 wi, float pSpec, out float pdf)
{
    pdf = 0;
    float nl = dot(s.n, wi), nv = dot(s.n, wo);
    if (nl <= 0 || nv <= 0) return 0;
    float a = max(s.rough * s.rough, 0.002);
    float3 h = normalize(wi + wo);
    float nh = max(dot(s.n, h), 0.0), vh = max(dot(wo, h), 0.0);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), s.base, s.metal);
    float3 F = FSchlick(f0, vh);
    float D = D_GGX(nh, a), G = G1(nl, a) * G1(nv, a);
    float3 spec = F * (D * G / (4 * nv + 1e-6));
    float3 diff = s.base * ((1 - s.metal) / PI) * (1.0 - F) * nl;
    pdf = pSpec * D * nh / (4 * vh + 1e-6) + (1 - pSpec) * nl / PI;
    return spec + diff;
}

float FresnelDielectric(float cosi, float eta)
{
    float s2 = (1 - cosi * cosi) / (eta * eta);
    if (s2 >= 1) return 1;
    float cost = sqrt(1 - s2);
    float rs = (cosi - eta * cost) / (cosi + eta * cost), rp = (eta * cosi - cost) / (eta * cosi + cost);
    return 0.5 * (rs * rs + rp * rp);
}

float Power(float a, float b) { return a * a / (a * a + b * b + 1e-12); }

float3 Clamped(float3 c, uint bounce)
{
    if (bounce > 0 && gEnvInfo.y > 0)
    {
        float l = Lum(c);
        if (l > gEnvInfo.y) c *= gEnvInfo.y / l;
    }
    return c;
}

// ---------------------------------------------------------------- integrator
float3 Radiance(float3 o, float3 d, inout uint rng)
{
    float3 L = 0, beta = 1;
    float lastPdf = 0;
    bool specularBounce = true;
    const uint nLights = gNumLights + ((gFlags & 1) ? 1 : 0);
    [loop]
    for (uint bounce = 0; bounce <= gMaxBounces; ++bounce)
    {
        Hit h = Trace(o, d, kFar, false);
        for (uint li = 0; li < gNumLights; ++li)
        {
            float t;
            if (bounce > 0 && LightType(li) == 2 && AreaLightHit(li, o, d, t) && t < h.t)
            {
                h.t = t;
                h.light = (int)li;
                h.tri = -1;
            }
        }
        if (h.light >= 0)
        {
            uint li = (uint)h.light;
            float area = LightSize(li) * LightSize(li);
            float3 Le = LightRadiance(li) / (PI * area);
            float w = 1;
            if (!specularBounce)
            {
                float cosl = max(-dot(d, LightDir(li)), 1e-4);
                w = Power(lastPdf, h.t * h.t / (cosl * area) / nLights);
            }
            L += Clamped(beta * Le * w, bounce);
            break;
        }
        if (h.tri < 0)
        {
            if (bounce == 0)
            {
                if ((gFlags & 2) || !(gFlags & 1)) L += EnvLookup(d);
                break;
            }
            if (!(gFlags & 1)) break;
            float w = specularBounce ? 1.0 : Power(lastPdf, EnvPdf(d) / nLights);
            L += Clamped(beta * EnvLookup(d) * w, bounce);
            break;
        }
        // Surface attributes.
        uint tb = (uint)h.tri * 8;
        float4 T0 = Geo(tb), T1 = Geo(tb + 1), T2 = Geo(tb + 2), T3 = Geo(tb + 3), T4 = Geo(tb + 4), T5 = Geo(tb + 5), T6 = Geo(tb + 6);
        float b0 = 1 - h.b1 - h.b2;
        float2 uv0 = float2(T1.w, T2.w), uv1 = float2(T3.w, T4.w), uv2 = float2(T5.w, T6.x);
        float2 uv = uv0 * b0 + uv1 * h.b1 + uv2 * h.b2;
        uint mi = min((uint)T0.w, 31u);
        float4 m0 = gMat[mi * 3], m1 = gMat[mi * 3 + 1], m2 = gMat[mi * 3 + 2];
        Surf su;
        su.p = o + d * h.t;
        su.ng = normalize(cross(T1.xyz - T0.xyz, T2.xyz - T0.xyz));
        su.n = normalize(T3.xyz * b0 + T4.xyz * h.b1 + T5.xyz * h.b2);
        su.base = m0.rgb;
        su.rough = m0.w;
        su.metal = m1.w;
        su.emission = m1.rgb;
        su.transmission = m2.x;
        su.ior = m2.y;
        if (m2.z > 0.5 && (gFlags & 4))
        {
            float4 b = gBase.SampleLevel(sLinearWrap, uv, 0);
            if (b.a > 1e-4) su.base = b.rgb;
            float4 orm = gOrm.SampleLevel(sLinearWrap, uv, 0);
            su.rough = orm.r;
            su.metal = orm.g;
            su.emission += gEmis.SampleLevel(sLinearWrap, uv, 0).rgb;
            float3 tn = gNrm.SampleLevel(sLinearWrap, uv, 0).rgb * 2.0 - 1.0;
            float3 e1 = T1.xyz - T0.xyz, e2 = T2.xyz - T0.xyz;
            float du1 = uv1.x - uv0.x, dv1 = uv1.y - uv0.y, du2 = uv2.x - uv0.x, dv2 = uv2.y - uv0.y;
            float r = du1 * dv2 - du2 * dv1;
            if (abs(r) > 1e-12)
            {
                float3 T = (e1 * dv2 - e2 * dv1) / r, B = (e2 * du1 - e1 * du2) / r;
                T = normalize(T - su.n * dot(su.n, T));
                B = normalize(B - su.n * dot(su.n, B) - T * dot(T, B));
                su.n = normalize(T * tn.x + B * tn.y + su.n * tn.z);
            }
        }
        su.rough = clamp(su.rough, 0.02, 1.0);
        float3 wo = -d;
        if (dot(su.ng, T3.xyz + T4.xyz + T5.xyz) < 0) su.ng = -su.ng;  // outward like the vertex normals (any winding)
        bool entering = dot(su.ng, wo) > 0;
        if (!entering && su.transmission <= 0)
        {
            su.n = -su.n;
            su.ng = -su.ng;
        }
        if (dot(su.n, wo) <= 0) su.n = normalize(su.n + su.ng * (0.01 - dot(su.n, wo)));
        if (Lum(su.emission) > 0) L += Clamped(beta * su.emission, bounce);

        // Rough dielectric: reflect or refract through a sampled microfacet.
        if (su.transmission > 0 && Rnd(rng) < su.transmission)
        {
            float3 n = entering ? su.ng : -su.ng;
            float3 sn = dot(su.n, n) > 0 ? su.n : -su.n;
            float a = su.rough * su.rough;
            float3 t, b;
            Basis(sn, t, b);
            float r1 = Rnd(rng), r2 = Rnd(rng);
            float th = atan(a * sqrt(r1 / (1 - r1 + 1e-6))), ph = 2 * PI * r2;
            float3 hm = normalize(t * (sin(th) * cos(ph)) + b * (sin(th) * sin(ph)) + sn * cos(th));
            float eta = entering ? su.ior : 1.0 / su.ior;
            float cosi = max(dot(wo, hm), 1e-4);
            if (Rnd(rng) < FresnelDielectric(cosi, eta)) d = normalize(hm * (2 * dot(wo, hm)) - wo);
            else
            {
                float k = 1 - (1 - cosi * cosi) / (eta * eta);
                d = normalize(-wo / eta + hm * (cosi / eta - sqrt(max(k, 0.0))));
                beta *= su.base;
            }
            o = su.p + d * 1e-3;
            specularBounce = true;
            continue;
        }
        float3 f0 = lerp(float3(0.04, 0.04, 0.04), su.base, su.metal);
        float pSpec = clamp(Lum(FSchlick(f0, max(dot(su.n, wo), 0.0))) * 1.5 + su.metal * 0.5, 0.1, 0.9);

        // Next-event estimation.
        if (nLights > 0)
        {
            uint pick = min((uint)(Rnd(rng) * nLights), nLights - 1);
            float3 wi = float3(0, 1, 0), Li = 0;
            float dist = kFar, lpdf = 0;
            bool delta = false;
            if (pick < gNumLights)
            {
                uint type = LightType(pick);
                float size = LightSize(pick);
                if (type == 2)
                {
                    float3 n = LightDir(pick), u, v;
                    LightSquare(n, u, v);
                    float3 q = LightPos(pick) + u * ((Rnd(rng) - 0.5) * size) + v * ((Rnd(rng) - 0.5) * size);
                    float3 dv = q - su.p;
                    dist = length(dv);
                    wi = dv / dist;
                    float cosl = -dot(wi, n);
                    if (cosl > 1e-4)
                    {
                        float area = size * size;
                        lpdf = dist * dist / (cosl * area);
                        Li = LightRadiance(pick) / (PI * area);
                    }
                }
                else if (type == 3)
                {
                    float3 dir = -LightDir(pick), u, v;
                    Basis(dir, u, v);
                    float r = sqrt(Rnd(rng)) * tan(size * 0.5 * PI / 180.0), ph = 2 * PI * Rnd(rng);
                    wi = normalize(dir + u * (r * cos(ph)) + v * (r * sin(ph)));
                    Li = LightRadiance(pick);
                    lpdf = 1;
                    delta = true;
                }
                else
                {
                    float3 c = normalize(LightPos(pick) - su.p), u, v;
                    Basis(c, u, v);
                    float r = sqrt(Rnd(rng)) * size, ph = 2 * PI * Rnd(rng);
                    float3 dq = LightPos(pick) + u * (r * cos(ph)) + v * (r * sin(ph)) - su.p;
                    dist = length(dq);
                    wi = dq / dist;
                    float fall = 1;
                    if (type == 1)
                    {
                        float cosA = dot(-wi, LightDir(pick)), outer = gLight[pick * 4 + 2].w, inner = gLight[pick * 4 + 3].x;
                        fall = saturate((cosA - outer) / max(inner - outer, 1e-4));
                        fall = fall * fall * (3 - 2 * fall);
                    }
                    Li = LightRadiance(pick) * (fall / (4 * PI * max(dist * dist, 1e-4)));
                    lpdf = 1;
                    delta = true;
                }
            }
            else
            {
                wi = EnvSampleDir(rng, lpdf);
                Li = EnvLookup(wi);
            }
            if (lpdf > 0 && Lum(Li) > 0)
            {
                float bpdf;
                float3 f = BsdfEval(su, wo, wi, pSpec, bpdf);
                if (Lum(f) > 0)
                {
                    Hit sh = Trace(su.p + su.ng * 1e-3, wi, dist - 2e-3, true);
                    if (sh.tri < 0)
                    {
                        float w = delta ? 1.0 : Power(lpdf / nLights, bpdf);
                        L += Clamped(beta * f * Li * (w * nLights / (delta ? 1.0 : lpdf)), bounce);
                    }
                }
            }
        }
        // BSDF sampling.
        float3 t, b;
        Basis(su.n, t, b);
        float3 wi;
        if (Rnd(rng) < pSpec)
        {
            float a = max(su.rough * su.rough, 0.002);
            float r1 = Rnd(rng), r2 = Rnd(rng);
            float th = atan(a * sqrt(r1 / (1 - r1 + 1e-6))), ph = 2 * PI * r2;
            float3 hm = t * (sin(th) * cos(ph)) + b * (sin(th) * sin(ph)) + su.n * cos(th);
            wi = normalize(hm * (2 * dot(wo, hm)) - wo);
        }
        else
        {
            float r1 = Rnd(rng), r2 = Rnd(rng), r = sqrt(r1), ph = 2 * PI * r2;
            wi = t * (r * cos(ph)) + b * (r * sin(ph)) + su.n * sqrt(max(0.0, 1 - r1));
        }
        float pdf;
        float3 f = BsdfEval(su, wo, wi, pSpec, pdf);
        if (pdf <= 1e-8 || Lum(f) <= 0) break;
        beta *= f / pdf;
        lastPdf = pdf;
        specularBounce = false;
        o = su.p + su.ng * (dot(wi, su.ng) > 0 ? 1e-3 : -1e-3);
        d = wi;
        if (bounce >= 3)
        {
            float q = min(max(beta.x, max(beta.y, beta.z)), 0.95);
            if (Rnd(rng) > q) break;
            beta /= q;
        }
    }
    return L;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint2 p = uint2(id.x, id.y + gRowStart);
    if (any(p >= gSize)) return;
    uint rng = p.x * 1973u + p.y * 9277u + gSeed * 26699u;
    Pcg(rng);
    Pcg(rng);
    float px = (p.x + Rnd(rng)) / gSize.x * 2.0 - 1.0, py = 1.0 - (p.y + Rnd(rng)) / gSize.y * 2.0;
    float3 fwd = gCamFwd.xyz;
    float3 dir = normalize(fwd + gCamRight.xyz * px + gCamUp.xyz * py);
    float3 org = gCamPos.xyz;
    if (gCamPos.w > 0)
    {
        // Thin lens: sharp plane at the focus distance, aperture disc of radius w.
        float3 focus = org + dir * (gCamRight.w / max(dot(dir, fwd), 1e-4));
        float r = sqrt(Rnd(rng)) * gCamPos.w, ph = 2 * PI * Rnd(rng);
        org += normalize(gCamRight.xyz) * (r * cos(ph)) + normalize(gCamUp.xyz) * (r * sin(ph));
        dir = normalize(focus - org);
    }
    float3 c = Radiance(org, dir, rng);
    if (any(isnan(c)) || any(isinf(c))) c = 0;
    float4 prev = gPass == 0 ? float4(0, 0, 0, 0) : gAccum[p];
    gAccum[p] = prev + float4(c, 1.0);
}
