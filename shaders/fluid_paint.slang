// Liquid paint (src/render/FluidPaint.cpp): a film of paint simulated in texture space.
// The paint carries a whole material (color, roughness, metal, emissive) and its
// thickness gives the relief. Gravity comes from the model (each texel's slope,
// from the UV position / normal maps) or, on a flat image, from the tilt of the support.
// Shallow-film "pipe" model: each texel sends paint to its 4 neighbors; only the paint
// above a retained film moves (so runs leave a trail and stop when they thin out), and
// the paint sets with time. State (ping-pong, simulation size):
//   F0 wet: thickness, color (linear)      F1 wet: roughness, metal, age (s), emissive part
//   D0 stain: thickness, color             D1 stain: roughness, metal, paint fallen from here (drops), emissive part
//   Flux: outflow toward +x, -x, +y, -y    Grav: gravity in texels (xy), facing down, valid
// Passes (gPass): 0 gravity map, 1 brush deposit, 2 outflows, 3 transport, 4 composite
// into a layer (document size), 5 clear.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;      // simulation size
    uint2 gDocSize;   // document size
    uint gPass;
    uint gKind;       // composite: 0 base color, 1 roughness, 2 metal, 3 height, 4 emissive; deposit: dab count
    uint gFlags;      // 1 mesh maps, 2 world dabs, 4 selection, 8 flows off the borders, 16 relief bed, 32 erase (deposit removes),
                      // 64 UV seams crossed (portal maps), 128 the fallen drops were collected (counted again from 0),
                      // 256 the base color layer before the paint is bound (t9: color picked up),
                      // 512 the layers' paint became the liquid (composite: the layer before is not kept under it),
                      // 1024 / 2048 load (pass 8): roughness (t10) / metal (t11) layers bound
    float gDt;
    float gSeed;
    float3 _pad;
};

cbuffer Params : register(b1)
{
    float4 gGravity;  // mesh: world gravity (unit) + strength; image: texel direction * slope, facing down, strength
    float4 gPhys;     // speed (texels / s at full slope), retention, spread, set time (s)
    float4 gPhys2;    // irregularity, noise scale (texels), absorption, damping (1 / s)
    float4 gLook;     // density, relief, height base, wet gloss
    float4 gLook2;    // relief bed strength, world size of a texel (mesh), drips falling from overhangs, hardness
    float4 gColor;    // paint color (linear), emissive part
    float4 gMat;      // roughness, metal, cohesion, coat thickness
    float4 gEmissive; // emissive color (linear), color picked up from the paint already in the layer (w)
    float4 gDabs[512];   // image: x, y (document px), radius; world: x, y, z, radius
    float4 gDabsEx[512]; // amount, normal (world dabs: the side painted)
};

Texture2D<float4> gF0 : register(t0);
Texture2D<float4> gF1 : register(t1);
Texture2D<float4> gD0 : register(t2);
Texture2D<float4> gD1 : register(t3);
Texture2D<float4> gFlux : register(t4);
Texture2D<float4> gGrav : register(t5);
Texture2D<float4> gBed : register(t6);     // height channel (relief the paint flows around)
Texture2D<float4> gPosMap : register(t7);  // document size
Texture2D<float4> gNrmMap : register(t8);
Texture2D<float4> gAux : register(t9);     // gravity: selection; composite: the layer before the paint
// UV seams: the texel across the seam (simulation texel index, -1 none) for each direction leaving the island,
// and, on the other side, the (at most two) texels that send through it: index, direction, index, direction.
Texture2D<float4> gPortalOut : register(t10);
Texture2D<float4> gPortalIn : register(t11);

RWTexture2D<float4> gOut0 : register(u0);
RWTexture2D<float4> gOut1 : register(u1);
RWTexture2D<float4> gOut2 : register(u2);
RWTexture2D<float4> gOut3 : register(u3);

static const int2 kDir[4] = {int2(1, 0), int2(-1, 0), int2(0, 1), int2(0, -1)};

float ValueNoise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = Hash12(i + gSeed), b = Hash12(i + float2(1, 0) + gSeed), c = Hash12(i + float2(0, 1) + gSeed), d = Hash12(i + float2(1, 1) + gSeed);
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// Two octaves, in [0, 1].
float Noise(float2 texel)
{
    float s = max(gPhys2.y, 1.0);
    return ValueNoise(texel / s) * 0.65 + ValueNoise(texel / s * 2.7 + 17.3) * 0.35;
}

int2 DocPixel(float2 uv) { return clamp(int2(uv * float2(gDocSize)), int2(0, 0), int2(gDocSize) - 1); }

// -------------------------------------------------------------- gravity map

float4 GravityAt(int2 p)
{
    float2 uv = (float2(p) + 0.5) / float2(gSize);
    if ((gFlags & 1) == 0)
    {
        float valid = 1.0;
        if (gFlags & 4) valid = gAux.SampleLevel(sLinearClamp, uv, 0).r > 0.5 ? 1.0 : 0.0;
        return float4(gGravity.xy * gGravity.w, gGravity.z, valid);
    }
    float4 P = gPosMap.Load(int3(DocPixel(uv), 0));
    if (P.a < 0.75) return 0;  // outside the islands, or their padding (dilated texels: a = 0.5)
    float3 n = normalize(gNrmMap.Load(int3(DocPixel(uv), 0)).xyz);
    // World offset of one simulation texel along x and y (one-sided at the chart borders).
    float2 st = 1.0 / float2(gSize);
    float4 xp = gPosMap.Load(int3(DocPixel(uv + float2(st.x, 0)), 0)), xm = gPosMap.Load(int3(DocPixel(uv - float2(st.x, 0)), 0));
    float4 yp = gPosMap.Load(int3(DocPixel(uv + float2(0, st.y)), 0)), ym = gPosMap.Load(int3(DocPixel(uv - float2(0, st.y)), 0));
    float3 px = xp.a > 0.75 && xm.a > 0.75 ? (xp.xyz - xm.xyz) * 0.5 : xp.a > 0.75 ? xp.xyz - P.xyz : xm.a > 0.75 ? P.xyz - xm.xyz : 0;
    float3 py = yp.a > 0.75 && ym.a > 0.75 ? (yp.xyz - ym.xyz) * 0.5 : yp.a > 0.75 ? yp.xyz - P.xyz : ym.a > 0.75 ? P.xyz - ym.xyz : 0;
    float3 g = gGravity.xyz;
    float3 gt = g - n * dot(g, n);  // along the surface
    // Least squares: a * px + b * py = gt (texels to cover the slope's world vector).
    float a11 = dot(px, px), a12 = dot(px, py), a22 = dot(py, py);
    float det = a11 * a22 - a12 * a12;
    float2 t = 0;
    if (abs(det) > 1e-20)
    {
        float r1 = dot(px, gt), r2 = dot(py, gt);
        t = float2(a22 * r1 - a12 * r2, a11 * r2 - a12 * r1) / det;
    }
    t *= gLook2.y;  // in texels for the mean texel size: |t| ~ slope
    float l = length(t);
    if (l > 2.0) t *= 2.0 / l;
    return float4(t * gGravity.w, dot(n, g), 1.0);
}

// -------------------------------------------------------------- brush deposit

void Deposit(int2 p)
{
    float4 f0 = gF0.Load(int3(p, 0)), f1 = gF1.Load(int3(p, 0)), d0 = gD0.Load(int3(p, 0)), d1 = gD1.Load(int3(p, 0));
    float4 G = gGrav.Load(int3(p, 0));
    float add = 0.0;
    if (G.w > 0)
    {
        float2 uv = (float2(p) + 0.5) / float2(gSize);
        float3 P = 0, N = 0;
        if (gFlags & 2)
        {
            int2 dp = DocPixel(uv);
            P = gPosMap.Load(int3(dp, 0)).xyz;
            N = normalize(gNrmMap.Load(int3(dp, 0)).xyz);
        }
        float2 docPx = uv * float2(gDocSize);
        float coat = 0.0;
        for (uint i = 0; i < gKind; ++i)
        {
            float4 dab = gDabs[i];
            float4 ex = gDabsEx[i];
            float d, r;
            if (gFlags & 2)
            {
                r = max(dab.w, 1e-6);
                d = length(P - dab.xyz);
                if (dot(ex.yzw, ex.yzw) > 0.01 && dot(N, ex.yzw) < 0.05) continue;
            }
            else
            {
                r = max(dab.z, 0.5);
                d = length(docPx - dab.xy);
            }
            if (d >= r) continue;
            float fall = 1.0 - smoothstep(r * saturate(gLook2.w), r, d);
            add += fall * ex.x;
            coat = max(coat, fall);
        }
        // The loaded brush lays a coat (up to its thickness), the flow adds paint on top (pools when held still);
        // uneven load: more paint here and there.
        float load = 1.0 + gPhys2.x * (Noise(float2(p) * 1.7 + 91.0) - 0.5) * 1.2;
        add = max(add * load + max(coat * gMat.w * load - f0.x, 0.0), 0.0);
    }
    if (gFlags & 32)
    {
        // Erase: lifts the wet paint and the stain.
        float k = saturate(add * 4.0);
        f0.x *= 1.0 - k;
        d0.x *= 1.0 - k;
    }
    else if (add > 0)
    {
        // The absorbent support takes its stain on top: the liquid laid still runs.
        float wetAdd = add, dryAdd = add * gPhys2.z * 0.5;
        float h = f0.x + wetAdd;
        float wa = wetAdd / max(h, 1e-6), wo = 1.0 - wa;
        f0 = float4(h, f0.yzw * wo + gColor.rgb * wa);
        f1 = float4(f1.x * wo + gMat.x * wa, f1.y * wo + gMat.y * wa, f1.z * wo, f1.w * wo + gColor.w * wa);
        float hd = d0.x + dryAdd;
        float da = dryAdd / max(hd, 1e-6), dO = 1.0 - da;
        d0 = float4(hd, d0.yzw * dO + gColor.rgb * da);
        d1 = float4(d1.x * dO + gMat.x * da, d1.y * dO + gMat.y * da, d1.z, d1.w * dO + gColor.w * da);
    }
    gOut0[p] = f0;
    gOut1[p] = f1;
    gOut2[p] = d0;
    gOut3[p] = d1;
}

// -------------------------------------------------------------- outflows

float Retention(int2 p)
{
    float ret = gPhys.y * (1.0 + gPhys2.x * (Noise(float2(p)) - 0.5) * 2.0);
    return max(ret, 0.0);
}

float Elevation(int2 p, float h)
{
    float e = h + gD0.Load(int3(p, 0)).x;
    if (gFlags & 16)
    {
        float2 uv = (float2(p) + 0.5) / float2(gSize);
        e += LinearToSrgb1(gBed.SampleLevel(sLinearClamp, uv, 0).r) * gLook2.x;
    }
    return e;
}

bool Inside(int2 q) { return all(q >= 0) && all(q < int2(gSize)); }

void Outflow(int2 p)
{
    float h = gF0.Load(int3(p, 0)).x;
    float4 G = gGrav.Load(int3(p, 0));
    float4 prev = gFlux.Load(int3(p, 0));
    if (G.w <= 0 || h <= 1e-5)
    {
        gOut0[p] = 0;
        return;
    }
    float age = gF1.Load(int3(p, 0)).z;
    float setT = gPhys.w > 0 ? saturate(1.0 - age / gPhys.w) : 1.0;
    // A run leaves a thinner trail than the film at rest holds: once the paint moves, less of it stays.
    float moving = saturate((prev.x + prev.y + prev.z + prev.w) / max(0.03 * gPhys.x, 1e-3));
    float m = max(h - Retention(p) * (1.0 - 0.7 * moving), 0.0) * setT;  // the paint that moves
    float2 g = G.xy;
    // Meander: the run hesitates left / right.
    if (gPhys2.x > 0)
    {
        float w = (Noise(float2(p) * 0.8 + 40.0) - 0.5) * gPhys2.x * 1.4;
        g += float2(-g.y, g.x) * w;
    }
    float e = Elevation(p, h);
    float mob = h > 1e-5 ? m / h : 0.0;
    float speed = gPhys.x;
    float damp = exp(-gPhys2.w * gDt);
    float out4[4];
    float total = 0;
    [unroll] for (int k = 0; k < 4; ++k)
    {
        int2 q = p + kDir[k];
        float en;
        const bool open = Inside(q) && gGrav.Load(int3(q, 0)).w > 0;
        if (open)
        {
            en = Elevation(q, gF0.Load(int3(q, 0)).x);
        }
        else
        {
            // Across a UV seam: the island on the other side of the edge, else off the image / chart / selection.
            const int t = (gFlags & 64) ? (int)gPortalOut.Load(int3(p, 0))[k] : -1;
            if (t >= 0)
            {
                const int2 tq = int2(t % (int)gSize.x, t / (int)gSize.x);
                en = Elevation(tq, gF0.Load(int3(tq, 0)).x);
            }
            else
            {
                if ((gFlags & 8) == 0) { out4[k] = 0; continue; }
                en = -1e3;
            }
        }
        float drive = max(0.0, dot(g, float2(kDir[k]))) * m + gPhys.z * max(0.0, e - max(en, e - h)) * mob;
        // Cohesion: the moving paint gathers where there is more of it (runs become rivulets, not sheets).
        if (en > -1e2) drive += gMat.z * max(0.0, en - e) * mob * length(g);
        float f = prev[k] * damp + gDt * gPhys2.w * speed * drive;
        f = m > 0 ? max(f, 0.0) : 0.0;
        out4[k] = f;
        total += f;
    }
    float s = total * gDt > m ? m / max(total * gDt, 1e-9) : 1.0;
    gOut0[p] = float4(out4[0], out4[1], out4[2], out4[3]) * s;
}

// -------------------------------------------------------------- transport

void Transport(int2 p)
{
    float4 f0 = gF0.Load(int3(p, 0)), f1 = gF1.Load(int3(p, 0)), d0 = gD0.Load(int3(p, 0)), d1 = gD1.Load(int3(p, 0));
    float4 G = gGrav.Load(int3(p, 0));
    float4 fl = gFlux.Load(int3(p, 0));
    float outV = (fl.x + fl.y + fl.z + fl.w) * gDt;
    float keep = max(f0.x - outV, 0.0);
    float h = keep;
    float3 col = f0.yzw * keep;
    float4 pr = float4(f1.x, f1.y, f1.z, f1.w) * keep;
    float inV = 0;
    // Neighbor q sends toward p with its component pointing back at p.
    const int back[4] = {1, 0, 3, 2};
    [unroll] for (int k = 0; k < 4; ++k)
    {
        int2 q = p + kDir[k];
        if (!Inside(q)) continue;
        float v = gFlux.Load(int3(q, 0))[back[k]] * gDt;
        if (v <= 0) continue;
        float4 q0 = gF0.Load(int3(q, 0)), q1 = gF1.Load(int3(q, 0));
        h += v;
        col += q0.yzw * v;
        pr += q1 * v;
        inV += v;
    }
    if (gFlags & 64)
    {
        // Paint arriving across a UV seam.
        const float4 pin = gPortalIn.Load(int3(p, 0));
        [unroll] for (int s = 0; s < 2; ++s)
        {
            const int src = (int)pin[s * 2];
            if (src < 0) continue;
            const int2 sq = int2(src % (int)gSize.x, src / (int)gSize.x);
            const float v = gFlux.Load(int3(sq, 0))[(int)pin[s * 2 + 1]] * gDt;
            if (v <= 0) continue;
            const float4 q0 = gF0.Load(int3(sq, 0)), q1 = gF1.Load(int3(sq, 0));
            h += v;
            col += q0.yzw * v;
            pr += q1 * v;
            inV += v;
        }
    }
    if (G.w <= 0) h = 0;
    if (h > 1e-6)
    {
        col /= h;
        pr /= h;
    }
    else
    {
        h = 0;
        col = f0.yzw;
        pr = f1;
    }
    pr.z += gDt;  // the paint ages (sets)
    // Wet on wet: running over the paint already in the layer, the liquid takes some of its color.
    if ((gFlags & 256) && gEmissive.w > 0 && h > 1e-5)
    {
        const float4 under = gAux.SampleLevel(sLinearClamp, (float2(p) + 0.5) / float2(gSize), 0);
        if (under.a > 0.01)
        {
            const float k = saturate(gEmissive.w * gDt * (inV > 0 ? 1.0 : 0.2)) * under.a;
            col = lerp(col, under.rgb / under.a, k);
        }
    }
    // Moving paint stains what it crosses (absorbent support: ink, watercolor).
    float stain = min(inV * gPhys2.z * saturate(gDt * 4.0), h);  // per second, whatever the substeps
    // Overhangs: the thick paint falls in drops.
    float drop = 0;
    if (G.z > 0.15 && gLook2.z > 0)
        drop = max(h - gPhys.y * 0.5, 0.0) * saturate(G.z) * saturate(gLook2.z * gDt * 3.0);
    h -= stain + drop;
    if (stain > 0)
    {
        float hd = d0.x + stain;
        float a = stain / hd, o = 1.0 - a;
        d0 = float4(hd, d0.yzw * o + col * a);
        d1 = float4(d1.x * o + pr.x * a, d1.y * o + pr.y * a, d1.z, d1.w * o + pr.w * a);
    }
    // The fallen paint is counted here; the app collects it (pass 7) and lets it land lower on the model.
    d1.z = ((gFlags & 128) ? 0.0 : d1.z) + drop;
    if (h < 1e-5) h = 0;
    gOut0[p] = float4(h, col);
    gOut1[p] = pr;
    gOut2[p] = d0;
    gOut3[p] = d1;
}

// -------------------------------------------------------------- composite into a layer

void Composite(int2 p)
{
    float2 uv = (float2(p) + 0.5) / float2(gDocSize);
    float4 pre = gAux.Load(int3(p, 0));
    // Outside the simulated texels (UV island padding, seams): the paint of the nearest simulated texel,
    // so the seams show no line.
    if (gFlags & 1)
    {
        const int2 sp = int2(uv * float2(gSize));
        if (gGrav.Load(int3(sp, 0)).w <= 0)
        {
            float best = 1e9;
            float2 buv = uv;
            for (int oy = -2; oy <= 2; ++oy)
                for (int ox = -2; ox <= 2; ++ox)
                {
                    const int2 q = sp + int2(ox, oy);
                    if (!Inside(q) || gGrav.Load(int3(q, 0)).w <= 0) continue;
                    const float dd = ox * ox + oy * oy;
                    if (dd < best) { best = dd; buv = (float2(q) + 0.5) / float2(gSize); }
                }
            uv = buv;
        }
    }
    float4 f0 = gF0.SampleLevel(sLinearClamp, uv, 0), f1 = gF1.SampleLevel(sLinearClamp, uv, 0);
    float4 d0 = gD0.SampleLevel(sLinearClamp, uv, 0), d1 = gD1.SampleLevel(sLinearClamp, uv, 0);
    if (gFlags & 512) pre = 0;  // "make the layer run": its paint is now the liquid
    float T = f0.x + d0.x;
    if (T < 1e-4)
    {
        gOut0[p] = pre;
        return;
    }
    float td = T / max(gPhys.y, 0.03) * gLook.x * 0.5;  // the retained film nearly covers
    float alpha = 1.0 - exp(-3.0 * td);
    float wf = f0.x / T, df = d0.x / T;
    float wet = wf * (gPhys.w > 0 ? saturate(1.0 - f1.z / (gPhys.w * 1.5)) : 1.0) * gLook.w;
    float3 col = f0.yzw * wf + d0.yzw * df;
    float rough = f1.x * wf + d1.x * df, metal = f1.y * wf + d1.y * df, emis = f1.w * wf + d1.w * df;
    float4 v;
    if (gKind == 0) v = float4(col * lerp(1.0, 0.82, wet), 1);
    else if (gKind == 1) v = float4(SrgbToLinear1(saturate(lerp(rough, rough * 0.12, wet))).xxx, 1);
    else if (gKind == 2) v = float4(SrgbToLinear1(saturate(metal)).xxx, 1);
    else if (gKind == 3) v = float4(SrgbToLinear1(saturate(gLook.z + gLook.y * T * 0.06)).xxx, 1);
    else v = float4(gEmissive.rgb * emis, 1);
    gOut0[p] = pre * (1.0 - alpha) + v * alpha;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    int2 p = int2(id.xy);
    if (gPass == 4)
    {
        if (any(id.xy >= gDocSize)) return;
        Composite(p);
        return;
    }
    if (gPass == 8)
    {
        // The paint of the layers becomes the liquid (Make the layer run): thickness from the coverage.
        if (any(id.xy >= gSize)) return;
        const float2 uv = (float2(p) + 0.5) / float2(gSize);
        const float4 G = gGrav.Load(int3(p, 0));
        const float4 c = gAux.SampleLevel(sLinearClamp, uv, 0);
        float4 f0 = 0, f1 = float4(gMat.x, gMat.y, 0, 0);
        if (G.w > 0 && c.a > 0.004)
        {
            f0 = float4(c.a * gMat.w, c.rgb / c.a);
            if (gFlags & 1024) { const float4 r = gPortalOut.SampleLevel(sLinearClamp, uv, 0); f1.x = r.a > 0.004 ? LinearToSrgb1(r.r / r.a) : gMat.x; }
            if (gFlags & 2048) { const float4 m = gPortalIn.SampleLevel(sLinearClamp, uv, 0); f1.y = m.a > 0.004 ? LinearToSrgb1(m.r / m.a) : gMat.y; }
        }
        gOut0[p] = f0;
        gOut1[p] = f1;
        gOut2[p] = 0;
        gOut3[p] = float4(f1.x, f1.y, 0, 0);
        return;
    }
    if (gPass == 7)
    {
        // Fallen drops of a 16 x 16 block: volume, volume * color; volume * world position, texels.
        float4 a = 0, b = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const int2 q = p * 16 + int2(x, y);
                if (!Inside(q)) continue;
                const float dv = gD1.Load(int3(q, 0)).z;
                if (dv <= 1e-6) continue;
                a += float4(dv, gF0.Load(int3(q, 0)).yzw * dv);
                const float3 P = gPosMap.Load(int3(DocPixel((float2(q) + 0.5) / float2(gSize)), 0)).xyz;
                b += float4(P * dv, 1);
            }
        gOut0[p] = a;
        gOut1[p] = b;
        return;
    }
    if (any(id.xy >= gSize)) return;
    if (gPass == 0) gOut0[p] = GravityAt(p);
    else if (gPass == 1) Deposit(p);
    else if (gPass == 2) Outflow(p);
    else if (gPass == 3) Transport(p);
    else
    {
        gOut0[p] = 0;
        gOut1[p] = float4(0.5, 0, 0, 0);
        gOut2[p] = 0;
        gOut3[p] = float4(0.5, 0, 0, 0);
    }
}
