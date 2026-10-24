// Liquid paint: a film of paint simulated on the GPU in texture space (shaders/fluid_paint.slang),
// composited live into the layers of a stroke (one per material channel). The stroke stays open
// while the paint moves; ending it records one undo step for every layer.
#include "render/Renderer.h"
#include "render/Dispatch.h"
#include "render/MeshPaint.h"
#include "doc/History.h"

#include <unordered_map>

namespace sx {

namespace {

struct FluidRoot {
    u32 w, h, docW, docH;
    u32 pass, kind, flags;
    float dt, seed;
    i32 ox = 0, oy = 0;  // first texel of the dispatch (active area)
    u32 area = 0;        // its size, width | height << 16 (0: everything)
};

u32 PackArea(const Rect& r) { return (u32)r.Width() | ((u32)r.Height() << 16); }

Rect GrowRect(const Rect& r, int k, int W, int H) {
    if (r.Empty()) return r;
    return Rect{r.x0 - k, r.y0 - k, r.x1 + k, r.y1 + k}.Clipped(W, H);
}

struct FluidCB {
    float4 gravity, phys, phys2, look, look2, color, mat, emissive, bristle;
    float4 dabs[512];
    float4 dabsEx[512];
};

void FillCB(FluidCB& cb, const Renderer::FluidParams& p) {
    cb.gravity = {p.gravity.x, p.gravity.y, p.gravity.z, p.strength};
    cb.phys = {p.speed, p.retention, p.spread, p.setTime};
    cb.phys2 = {p.noise, p.noiseScale, p.absorb, p.damping};
    cb.look = {p.density, p.relief, p.heightBase, p.wetGloss};
    cb.look2 = {p.bed, 1.0f, p.dripOff, p.hardness};
    cb.color = p.color;
    cb.mat = {p.rough, p.metal, p.cohesion, p.coat};
    cb.emissive = {p.emissive.x, p.emissive.y, p.emissive.z, p.pickup};
    cb.bristle = {p.bristle, p.bristleCount, p.bristleNoise, 0};
}

}  // namespace

bool Renderer::FluidBegin(Document& doc, gpu::CommandContext& ctx, const std::vector<Id>& surfaces, const std::vector<int>& kinds, MeshScene* mesh,
                          u32 simW, u32 simH, gpu::Texture* selection, gpu::Texture* bed) {
    if (!Kernel("fluid_paint.slang")) return false;
    if (mesh && (!MeshPrepare(doc, ctx, *mesh) || !mesh->posMap)) return false;
    fluid.beginning = true;
    const bool begun = BeginStrokeMulti(doc, ctx, surfaces);
    fluid.beginning = false;
    if (!begun) return false;
    FluidState& F = fluid;
    F.kinds = kinds;
    F.mesh = mesh;
    // Own copies: the selection and the relief may change while the paint runs.
    F.selection.reset();
    F.bed.reset();
    if (selection) {
        F.selection = gpu::Dev().CreateTexture(selection->Width(), selection->Height(), selection->GetFormat(), "FluidSelection");
        ctx.CopyTexture(F.selection.get(), selection);
    }
    if (bed) {
        F.bed = gpu::Dev().CreateTexture(bed->Width(), bed->Height(), bed->GetFormat(), "FluidBed");
        ctx.CopyTexture(F.bed.get(), bed);
    }
    F.w = std::max(simW, 8u);
    F.h = std::max(simH, 8u);
    const char* names[4][2] = {{"FluidF0a", "FluidF0b"}, {"FluidF1a", "FluidF1b"}, {"FluidD0a", "FluidD0b"}, {"FluidD1a", "FluidD1b"}};
    for (int k = 0; k < 2; ++k) {
        Ensure(F.f0[k], F.w, F.h, gpu::Format::RGBA32F, names[0][k]);
        Ensure(F.f1[k], F.w, F.h, gpu::Format::RGBA32F, names[1][k]);  // age: half floats stop counting after a few seconds
        Ensure(F.d0[k], F.w, F.h, gpu::Format::RGBA16F, names[2][k]);
        Ensure(F.d1[k], F.w, F.h, gpu::Format::RGBA32F, names[3][k]);  // z: fallen paint, summed
        Ensure(F.flux[k], F.w, F.h, gpu::Format::RGBA32F, k ? "FluidFluxB" : "FluidFluxA");
    }
    Ensure(F.grav, F.w, F.h, gpu::Format::RGBA16F, "FluidGravity");
    F.cur = 0;
    F.fluxCur = 0;
    // World size of a simulation texel (mean over the model): converts slopes to texels.
    F.texelWorld = 1.0f;
    if (mesh) {
        const auto& V = mesh->data.vertices;
        const auto& I = mesh->data.indices;
        double a3 = 0, auv = 0;
        for (size_t t = 0; t + 2 < I.size(); t += 3) {
            const MeshVertex &A = V[I[t]], &B = V[I[t + 1]], &C = V[I[t + 2]];
            a3 += Length(Cross(B.pos - A.pos, C.pos - A.pos)) * 0.5;
            const float2 e1 = B.uv - A.uv, e2 = C.uv - A.uv;
            auv += std::fabs(e1.x * e2.y - e1.y * e2.x) * 0.5;
        }
        if (auv > 1e-12 && a3 > 0) F.texelWorld = (float)(std::sqrt(a3 / auv) / std::sqrt((double)F.w * F.h));
    }
    // Clear the state.
    gpu::DispatchDesc d;
    d.pipeline = Kernel("fluid_paint.slang");
    for (int k = 0; k < 2; ++k) {
        d.uav[0] = F.f0[k].get();
        d.uav[1] = F.f1[k].get();
        d.uav[2] = F.d0[k].get();
        d.uav[3] = F.d1[k].get();
        SetConstants(d, FluidRoot{F.w, F.h, doc.Width(), doc.Height(), 5, 0, 0, 0, 0});
        Dispatch2D(ctx, d, F.w, F.h);
        d.uav[0] = F.flux[k].get();
        d.uav[1] = F.grav.get();
        d.uav[2] = F.d0[1 - k].get();
        d.uav[3] = F.d1[1 - k].get();
        Dispatch2D(ctx, d, F.w, F.h);
    }
    F.docW = doc.Width();
    F.docH = doc.Height();
    F.active = true;
    F.replace = false;
    F.time = 0;
    F.windowed = mesh == nullptr;  // models: portals and drops reach anywhere in the texture
    F.simRect = F.paintRect = F.dabsSince = {};
    F.rbBounds.reset();
    F.boundsFrame = F.boundsGrowth = 0;
    F.finalComposite = false;
    return true;
}

void Renderer::FluidDispatch(gpu::CommandContext& ctx, u32 pass, u32 kind, float dt, const FluidParams& p, const std::vector<float4>* dabs,
                             const std::vector<float4>* ex, u32 extraFlags) {
    FluidState& F = fluid;
    auto* k = Kernel("fluid_paint.slang");
    if (!k) return;
    static FluidCB cb;
    FillCB(cb, p);
    cb.look2.y = F.texelWorld;
    size_t n = 0;
    if (dabs) {
        n = std::min<size_t>(dabs->size(), 512);
        for (size_t i = 0; i < n; ++i) {
            cb.dabs[i] = (*dabs)[i];
            cb.dabsEx[i] = ex && i < ex->size() ? (*ex)[i] : float4{1, 0, 0, 0};
        }
        kind = (u32)n;
    }
    u32 flags = extraFlags;
    if (F.mesh) flags |= 1;
    if (!F.mesh && F.selection) flags |= 4;
    if (p.offEdges) flags |= 8;
    if (F.bed && p.bed > 0) flags |= 16;
    if (F.mesh && F.portalOut && F.portalKey && F.portalKey == F.portalPending) flags |= 64;
    if (pass == 3 && F.resetDrops) {
        flags |= 128;
        F.resetDrops = false;
    }
    const int c = F.cur, o = 1 - F.cur;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = F.f0[c].get();
    d.srv[1] = F.f1[c].get();
    d.srv[2] = F.d0[c].get();
    d.srv[3] = F.d1[c].get();
    d.srv[4] = F.flux[F.fluxCur].get();
    d.srv[5] = F.grav.get();
    d.srv[6] = F.bed.get();
    d.srv[7] = F.mesh ? F.mesh->posMap.get() : nullptr;
    d.srv[8] = F.mesh ? F.mesh->nrmMap.get() : nullptr;
    d.srv[9] = pass == 0 ? F.selection.get() : nullptr;
    if (pass == 3 && p.pickup > 0) {
        // The base color layer as it was before the paint (color picked up).
        gpu::Texture* under = nullptr;
        for (size_t i = 0; i < F.kinds.size() && !under; ++i)
            if (F.kinds[i] == 0) under = i == 0 ? strokePre.get() : i - 1 < strokeSlots.size() ? strokeSlots[i - 1].pre.get() : nullptr;
        if (under) {
            d.srv[9] = under;
            flags |= 256;
            SetConstants(d, FluidRoot{F.w, F.h, F.docW, F.docH, pass, kind, flags, dt, p.seed});
        }
    }
    if (flags & 64) {
        d.srv[10] = F.portalOut.get();
        d.srv[11] = F.portalIn.get();
    }
    if (pass == 0) {
        d.srv[5] = nullptr;
        d.uav[0] = F.grav.get();
    } else if (pass == 7) {
        d.uav[0] = F.dropA.get();
        d.uav[1] = F.dropB.get();
    } else if (pass == 2) {
        d.uav[0] = F.flux[1 - F.fluxCur].get();
    } else {
        d.uav[0] = F.f0[o].get();
        d.uav[1] = F.f1[o].get();
        d.uav[2] = F.d0[o].get();
        d.uav[3] = F.d1[o].get();
    }
    // Passes 1-3 on the active area only (image documents: where the paint is, grown while it may move).
    Rect area = {0, 0, (int)F.w, (int)F.h};
    if (pass >= 1 && pass <= 3 && !F.simRect.Empty()) area = F.simRect;
    FluidRoot root{F.w, F.h, F.docW, F.docH, pass, kind, flags, dt, p.seed, area.x0, area.y0, PackArea(area)};
    SetConstants(d, root);
    d.cbData = &cb;
    d.cbSize = n ? sizeof(FluidCB) : offsetof(FluidCB, dabs);  // the extras follow the full dab array
    if (pass == 7) {
        Dispatch2D(ctx, d, (F.w + 15) / 16, (F.h + 15) / 16);
        return;
    }
    Dispatch2D(ctx, d, (u32)area.Width(), (u32)area.Height());
    if (pass == 2) F.fluxCur = 1 - F.fluxCur;
    else if (pass != 0) F.cur = o;
}

std::vector<Id> Renderer::FluidSurfaces() const {
    std::vector<Id> out;
    if (!fluid.active || !strokeSurface) return out;
    out.push_back(strokeSurface);
    for (auto& s : strokeSlots) out.push_back(s.surface);
    return out;
}

void Renderer::FluidGravity(gpu::CommandContext& ctx, const FluidParams& p) {
    if (!fluid.active) return;
    FluidDispatch(ctx, 0, 0, 0, p, nullptr, nullptr, 0);
}

void Renderer::FluidDeposit(gpu::CommandContext& ctx, const std::vector<float4>& dabs, const std::vector<float4>& ex, bool world, bool erase,
                            const FluidParams& p) {
    if (!fluid.active || dabs.empty()) return;
    if (fluid.windowed) {
        if (world) {
            fluid.simRect = {0, 0, (int)fluid.w, (int)fluid.h};  // (model space: anywhere in the texture)
        } else {
            const float sx = (float)fluid.w / std::max(fluid.docW, 1u), sy = (float)fluid.h / std::max(fluid.docH, 1u);
            for (auto& dab : dabs) {
                const Rect r = Rect{(int)std::floor((dab.x - dab.z) * sx) - 2, (int)std::floor((dab.y - dab.z) * sy) - 2, (int)std::ceil((dab.x + dab.z) * sx) + 3,
                                    (int)std::ceil((dab.y + dab.z) * sy) + 3}
                                   .Clipped((int)fluid.w, (int)fluid.h);
                if (fluid.simRect.Empty()) fluid.simRect = r;
                else fluid.simRect.Merge(r);
                if (fluid.dabsSince.Empty()) fluid.dabsSince = r;  // (kept when the bounds read back come in)
                else fluid.dabsSince.Merge(r);
            }
        }
        if (fluid.paintRect.Empty()) fluid.paintRect = fluid.simRect;
        else fluid.paintRect.Merge(fluid.simRect);
    }
    for (size_t i = 0; i < dabs.size(); i += 512) {
        std::vector<float4> d(dabs.begin() + i, dabs.begin() + std::min(dabs.size(), i + 512));
        std::vector<float4> e(ex.begin() + std::min(ex.size(), i), ex.begin() + std::min(ex.size(), i + 512));
        FluidDispatch(ctx, 1, 0, 0, p, &d, &e, (world ? 2u : 0u) | (erase ? 32u : 0u));
    }
}

void Renderer::FluidPortals(gpu::CommandContext& ctx) {
    FluidState& F = fluid;
    if (!F.mesh || !F.mesh->posMap || !F.mesh->nrmMap) return;
    const u64 key = ((u64)F.mesh->revision * 1000003ull) ^ ((u64)F.w << 40) ^ ((u64)F.h << 20) ^ (u64)F.docW * 7919ull ^ 0x51ED;
    if (F.portalKey == key && F.portalPending == key) return;  // built
    if (F.portalPending != key) {
        // Position and normal maps to the CPU (once per model and size).
        F.rbPos = ctx.ReadbackTexture(F.mesh->posMap.get());
        F.rbNrm = ctx.ReadbackTexture(F.mesh->nrmMap.get());
        F.portalPending = key;
        F.portalKey = 0;
        return;
    }
    if (!F.rbPos || !F.rbNrm || !F.rbPos->ready || !F.rbNrm->ready) return;
    const u32 W = F.w, H = F.h, DW = F.rbPos->width, DH = F.rbPos->height;
    const float* pos = (const float*)F.rbPos->data.data();
    const float* nrm = (const float*)F.rbNrm->data.data();
    if (F.rbPos->data.size() < (size_t)DW * DH * 16 || F.rbNrm->data.size() < (size_t)DW * DH * 16) return;
    // Simulation texel -> document pixel, as the shader (DocPixel).
    auto docIndex = [&](int x, int y) {
        const int dx = std::clamp((int)((x + 0.5f) / W * DW), 0, (int)DW - 1), dy = std::clamp((int)((y + 0.5f) / H * DH), 0, (int)DH - 1);
        return (size_t)dy * DW + dx;
    };
    const size_t N = (size_t)W * H;
    std::vector<u8> valid(N, 0);
    for (u32 y = 0; y < H; ++y)
        for (u32 x = 0; x < W; ++x) valid[(size_t)y * W + x] = pos[docIndex(x, y) * 4 + 3] > 0.75f ? 1 : 0;  // not the padding
    auto P = [&](size_t i) { const float* q = pos + docIndex((int)(i % W), (int)(i / W)) * 4; return float3{q[0], q[1], q[2]}; };
    auto Nn = [&](size_t i) { const float* q = nrm + docIndex((int)(i % W), (int)(i / W)) * 4; return Normalize(float3{q[0], q[1], q[2]}); };
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    auto isBorder = [&](size_t i) {
        if (!valid[i]) return false;
        const int x = (int)(i % W), y = (int)(i / W);
        for (int k = 0; k < 4; ++k) {
            const int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= (int)W || ny >= (int)H || !valid[(size_t)ny * W + nx]) return true;
        }
        return false;
    };
    // Island border texels in a hash grid of their world positions (cell: about two texels).
    std::vector<size_t> border;
    for (size_t i = 0; i < N; ++i) if (isBorder(i)) border.push_back(i);
    const float cell = std::max(F.texelWorld * 2.0f, 1e-6f);
    auto cellKey = [&](float3 p) {
        const long long cx = (long long)std::floor(p.x / cell), cy = (long long)std::floor(p.y / cell), cz = (long long)std::floor(p.z / cell);
        return (u64)((cx * 73856093ll) ^ (cy * 19349663ll) ^ (cz * 83492791ll));
    };
    std::unordered_map<u64, std::vector<u32>> grid;
    grid.reserve(border.size());
    for (size_t b = 0; b < border.size(); ++b) grid[cellKey(P(border[b]))].push_back((u32)b);
    std::vector<float> out(N * 4, -1.0f), in(N * 4, -1.0f);
    int made = 0;
    for (size_t i : border) {
        const int x = (int)(i % W), y = (int)(i / W);
        const float3 pi = P(i), ni = Nn(i);
        for (int k = 0; k < 4; ++k) {
            const int nx = x + dx[k], ny = y + dy[k];
            if (nx >= 0 && ny >= 0 && nx < (int)W && ny < (int)H && valid[(size_t)ny * W + nx]) continue;
            // Where the step would land on the model: one texel further along the surface.
            const int bx = x - dx[k], by = y - dy[k];
            float3 step = {0, 0, 0};
            if (bx >= 0 && by >= 0 && bx < (int)W && by < (int)H && valid[(size_t)by * W + bx]) step = pi - P((size_t)by * W + bx);
            if (Length(step) < 1e-9f) continue;
            const float3 target = pi + step;
            const float reach = std::max(Length(step) * 1.6f, F.texelWorld * 1.5f);
            float best = reach;
            long long bestT = -1;
            const long long cx = (long long)std::floor(target.x / cell), cy = (long long)std::floor(target.y / cell), cz = (long long)std::floor(target.z / cell);
            const int rr = (int)std::ceil(reach / cell);
            for (int ox = -rr; ox <= rr; ++ox)
                for (int oy = -rr; oy <= rr; ++oy)
                    for (int oz = -rr; oz <= rr; ++oz) {
                        const u64 key2 = (u64)(((cx + ox) * 73856093ll) ^ ((cy + oy) * 19349663ll) ^ ((cz + oz) * 83492791ll));
                        auto it = grid.find(key2);
                        if (it == grid.end()) continue;
                        for (u32 b : it->second) {
                            const size_t t = border[b];
                            const int tx = (int)(t % W), ty = (int)(t / W);
                            if (std::abs(tx - x) <= 3 && std::abs(ty - y) <= 3) continue;  // same side of the seam
                            if (Dot(Nn(t), ni) < -0.2f) continue;                         // a thin shell's back
                            const float dd = Length(P(t) - target);
                            if (dd < best) { best = dd; bestT = (long long)t; }
                        }
                    }
            if (bestT < 0) continue;
            // The target lists its senders (two at most): the paint is gathered there.
            float* slot = &in[(size_t)bestT * 4];
            const int s = slot[0] < 0 ? 0 : slot[2] < 0 ? 1 : -1;
            if (s < 0) continue;
            slot[s * 2] = (float)i;
            slot[s * 2 + 1] = (float)k;
            out[i * 4 + k] = (float)bestT;
            ++made;
        }
    }
    Ensure(F.portalOut, W, H, gpu::Format::RGBA32F, "FluidPortalOut");
    Ensure(F.portalIn, W, H, gpu::Format::RGBA32F, "FluidPortalIn");
    ctx.Upload(F.portalOut.get(), out.data(), W * 16);
    ctx.Upload(F.portalIn.get(), in.data(), W * 16);
    F.portals = made;
    Log("[fluid] seams: %zu island border texels, %d crossings, texel %.5f, maps %ux%u (%zu bytes)", border.size(), made, F.texelWorld, DW, DH, F.rbPos->data.size());
    F.portalKey = key;
    F.rbPos.reset();
    F.rbNrm.reset();
}

void Renderer::FluidLoadLayers(gpu::CommandContext& ctx, const FluidParams& p, float thickness) {
    FluidState& F = fluid;
    auto* k = Kernel("fluid_paint.slang");
    if (!F.active || !k) return;
    auto preOf = [&](int kind) -> gpu::Texture* {
        for (size_t i = 0; i < F.kinds.size(); ++i)
            if (F.kinds[i] == kind) return i == 0 ? strokePre.get() : i - 1 < strokeSlots.size() ? strokeSlots[i - 1].pre.get() : nullptr;
        return nullptr;
    };
    gpu::Texture* color = preOf(0);
    if (!color) return;
    static FluidCB cb;
    FillCB(cb, p);
    cb.mat.w = thickness;
    u32 flags = 0;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[5] = F.grav.get();
    d.srv[9] = color;
    if (gpu::Texture* r = preOf(1)) { d.srv[10] = r; flags |= 1024; }
    if (gpu::Texture* m = preOf(2)) { d.srv[11] = m; flags |= 2048; }
    const int o = 1 - F.cur;
    d.uav[0] = F.f0[o].get();
    d.uav[1] = F.f1[o].get();
    d.uav[2] = F.d0[o].get();
    d.uav[3] = F.d1[o].get();
    SetConstants(d, FluidRoot{F.w, F.h, F.docW, F.docH, 8, 0, flags, 0, p.seed});
    d.cbData = &cb;
    d.cbSize = offsetof(FluidCB, dabs);
    Dispatch2D(ctx, d, F.w, F.h);
    F.cur = o;
    F.replace = true;
    F.windowed = false;  // the layer's paint may be anywhere
}

bool Renderer::FluidCollectDrops(gpu::CommandContext& ctx) {
    FluidState& F = fluid;
    if (!F.active || !F.mesh || F.rbDropA) return false;
    const u32 gw = (F.w + 15) / 16, gh = (F.h + 15) / 16;
    Ensure(F.dropA, gw, gh, gpu::Format::RGBA32F, "FluidDropA");
    Ensure(F.dropB, gw, gh, gpu::Format::RGBA32F, "FluidDropB");
    FluidParams none;
    FluidDispatch(ctx, 7, 0, 0, none, nullptr, nullptr, 0);
    F.rbDropA = ctx.ReadbackTexture(F.dropA.get());
    F.rbDropB = ctx.ReadbackTexture(F.dropB.get());
    F.resetDrops = true;  // the next transport counts from 0
    return true;
}

bool Renderer::FluidDrops(std::vector<float4>& volColor, std::vector<float4>& posCount) {
    FluidState& F = fluid;
    if (!F.rbDropA || !F.rbDropB || !F.rbDropA->ready || !F.rbDropB->ready) return false;
    const size_t n = (size_t)F.rbDropA->width * F.rbDropA->height;
    volColor.resize(n);
    posCount.resize(n);
    if (F.rbDropA->data.size() >= n * 16 && F.rbDropB->data.size() >= n * 16) {
        memcpy(volColor.data(), F.rbDropA->data.data(), n * 16);
        memcpy(posCount.data(), F.rbDropB->data.data(), n * 16);
    } else {
        volColor.clear();
        posCount.clear();
    }
    F.rbDropA.reset();
    F.rbDropB.reset();
    return true;
}

int Renderer::FluidStep(gpu::CommandContext& ctx, float dt, const FluidParams& p) {
    if (!fluid.active || dt <= 0) return 0;
    if (fluid.mesh) FluidPortals(ctx);
    // At most one texel per substep: enough substeps for the fastest paint.
    const float fastest = p.speed * std::max(p.strength, 0.05f) * 1.6f;
    const int n = std::clamp((int)std::ceil(fastest * dt) + 1, 2, 32);
    const float h = dt / n;
    FluidState& F = fluid;
    if (F.windowed) {
        // The paint's real extent (read back a few frames ago) tightens the area, grown since by the substeps run.
        if (F.rbBounds && F.rbBounds->ready) {
            const u32* b = (const u32*)F.rbBounds->data.data();
            if (F.rbBounds->data.size() >= 16) {
                Rect r = b[2] > b[0] && b[3] > b[1] ? Rect{(int)b[0], (int)b[1], (int)b[2], (int)b[3]} : Rect{};
                r = GrowRect(r, F.boundsGrowth + 2, (int)F.w, (int)F.h);
                // Dabs laid since the request: their area, grown as far as their paint may have run since.
                const Rect since = GrowRect(F.dabsSince, F.boundsGrowth, (int)F.w, (int)F.h);
                F.simRect = r.Empty() ? since : (since.Empty() ? r : (Rect{std::min(r.x0, since.x0), std::min(r.y0, since.y0),
                                                                             std::max(r.x1, since.x1), std::max(r.y1, since.y1)}));
            }
            F.rbBounds.reset();
        }
        if (!F.rbBounds && ++F.boundsFrame >= 10 && !F.simRect.Empty()) {
            F.boundsFrame = 0;
            auto* init = Kernel("bounds_init.slang");
            auto* bk = Kernel("bounds.slang");
            if (init && bk) {
                Ensure(F.boundsTex, 4, 1, gpu::Format::R32U, "FluidBounds");
                gpu::DispatchDesc di;
                di.pipeline = init;
                di.uav[0] = F.boundsTex.get();
                di.threadsX = 4;
                di.threadsY = 1;
                ctx.Dispatch(di);
                gpu::DispatchDesc db;
                db.pipeline = bk;
                db.srv[0] = F.f0[F.cur].get();
                db.uav[0] = F.boundsTex.get();
                struct { u32 w, h, flags; } br = {F.w, F.h, 4u};  // red channel: the wet thickness
                SetConstants(db, br);
                Dispatch2D(ctx, db, F.w, F.h);
                F.rbBounds = ctx.ReadbackTexture(F.boundsTex.get());
                F.boundsGrowth = 0;
                F.dabsSince = {};
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        if (F.windowed) {
            F.simRect = GrowRect(F.simRect, 1, (int)F.w, (int)F.h);  // the paint moves one texel per substep at most
            F.paintRect.Merge(F.simRect);
            ++F.boundsGrowth;
        }
        FluidDispatch(ctx, 2, 0, h, p, nullptr, nullptr, 0);
        FluidDispatch(ctx, 3, 0, h, p, nullptr, nullptr, 0);
    }
    fluid.time += dt;
    return n;
}

void Renderer::FluidComposite(Document& doc, gpu::CommandContext& ctx, const FluidParams& p, bool final) {
    fluid.finalComposite = final;
    FluidState& F = fluid;
    auto* k = Kernel("fluid_paint.slang");
    if (!F.active || !strokeSurface || !k) return;
    static FluidCB cb;
    FillCB(cb, p);
    auto one = [&](Id surface, gpu::Texture* pre, int kind) {
        Surface* s = doc.GetSurface(surface);
        if (!s || !pre) return;
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = F.f0[F.cur].get();
        d.srv[1] = F.f1[F.cur].get();
        d.srv[2] = F.d0[F.cur].get();
        d.srv[3] = F.d1[F.cur].get();
        d.srv[5] = F.grav.get();
        d.srv[9] = pre;
        d.uav[0] = s->tex.get();
        // The layer around the active area (document pixels; the whole layer when not windowed or at the end).
        const u32 SW = s->tex->Width(), SH = s->tex->Height();
        Rect area = {0, 0, (int)SW, (int)SH};
        const Rect& src = F.finalComposite ? F.paintRect : F.simRect;
        if (F.windowed && !src.Empty()) {
            const float kx = (float)SW / F.w, ky = (float)SH / F.h;
            area = Rect{(int)std::floor(src.x0 * kx) - 2, (int)std::floor(src.y0 * ky) - 2, (int)std::ceil(src.x1 * kx) + 2, (int)std::ceil(src.y1 * ky) + 2}
                       .Clipped((int)SW, (int)SH);
        } else if (F.windowed) {
            return;  // nothing painted yet
        }
        SetConstants(d, FluidRoot{F.w, F.h, SW, SH, 4, (u32)kind, (F.mesh ? 1u : 0u) | (F.replace ? 512u : 0u), 0, p.seed, area.x0, area.y0, PackArea(area)});
        d.cbData = &cb;
        d.cbSize = offsetof(FluidCB, dabs);
        d.uavRect[0] = area;
        Dispatch2D(ctx, d, (u32)area.Width(), (u32)area.Height());
        doc.TouchSurface(surface, &area);
    };
    one(strokeSurface, strokePre.get(), F.kinds.empty() ? 0 : F.kinds[0]);
    for (size_t i = 0; i < strokeSlots.size(); ++i) one(strokeSlots[i].surface, strokeSlots[i].pre.get(), i + 1 < F.kinds.size() ? F.kinds[i + 1] : 0);
    // Undo: the painted area only (whole layer when not windowed).
    if (F.windowed && !F.paintRect.Empty()) {
        const float kx = (float)F.docW / F.w, ky = (float)F.docH / F.h;
        strokeRect = Rect{(int)std::floor(F.paintRect.x0 * kx) - 2, (int)std::floor(F.paintRect.y0 * ky) - 2, (int)std::ceil(F.paintRect.x1 * kx) + 2,
                          (int)std::ceil(F.paintRect.y1 * ky) + 2}
                         .Clipped((int)F.docW, (int)F.docH);
    } else if (!F.windowed) {
        strokeRect = {0, 0, (int)F.docW, (int)F.docH};
    }
}

void Renderer::FluidEnd(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label, bool keep) {
    FluidState& F = fluid;
    if (!F.active) return;
    F.active = false;
    if (strokeSurface) {
        if (keep) {
            EndStrokeMulti(doc, ctx, hist, label);
        } else {
            // Discarded: every layer back to its state before the paint.
            if (Surface* s = doc.GetSurface(strokeSurface)) { ctx.CopyTexture(s->tex.get(), strokePre.get()); doc.TouchSurface(strokeSurface); }
            for (auto& slot : strokeSlots)
                if (Surface* s = doc.GetSurface(slot.surface)) { ctx.CopyTexture(s->tex.get(), slot.pre.get()); doc.TouchSurface(slot.surface); }
            strokeSlots.clear();
            strokeSurface = 0;
        }
    }
    // The simulation memory goes back to the pool.
    for (int k = 0; k < 2; ++k) {
        F.f0[k].reset();
        F.f1[k].reset();
        F.d0[k].reset();
        F.d1[k].reset();
        F.flux[k].reset();
    }
    F.grav.reset();
    F.dropA.reset();
    F.dropB.reset();
    F.rbDropA.reset();
    F.rbDropB.reset();
    F.resetDrops = false;
    F.replace = false;
    F.mesh = nullptr;
    F.selection.reset();
    F.bed.reset();
}

}  // namespace sx
